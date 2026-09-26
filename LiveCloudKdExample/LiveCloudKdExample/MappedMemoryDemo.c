//
// MappedMemoryDemo.c: verification and benchmark for the mapped guest memory
// path (HvmmMappedMemory.h over IOCTL_MAP_GPA_RANGE in hvmm.sys).
//
// Runs as action [2] of LiveCloudKdExample:
//   1. maps every run of the selected VM and prints coverage;
//   2. byte-compares random pages read through the mapping against
//      SdkReadPhysicalMemory (ReadInterfaceHvmmDrvInternal);
//   3. times 512 scattered 4 KB reads and one 16 MB contiguous read both ways;
//   4. optionally lets you stop the VM and checks the mapping survives.
//

#include "LiveCloudKdExample.h"
#include "HvmmMappedMemory.h"

#define DEMO_COMPARE_PAGES      10000

//
// The classic per-page read request, sent by hand to tell a driver fault from an hvlib one.
//

#define DEMO_IOCTL_VID_INTERNAL_READ_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x830, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _DEMO_GPA_INFO {
	HANDLE PartitionHandle;    // +0x00, the layout hvlib.dll sends
	ULONG64 PartitionId;       // +0x08
	ULONG64 VmwpProcessId;     // +0x10
	ULONG64 Reserved18;
	ULONG64 Reserved20;
	ULONG64 StartPage;         // +0x28, bytes
	ULONG64 BytesCount;        // +0x30, bytes
} DEMO_GPA_INFO;
#define DEMO_SCATTER_PAGES      512
#define DEMO_CONTIG_BYTES       (16ULL * 1024 * 1024)
#define DEMO_CONTIG_BLOCK       (1024 * 1024)   // what LiveCloudKd's dumper uses per read

static ULONG64 g_RngState = 0x9E3779B97F4A7C15ULL;

static ULONG64 DemoRandom()
{
	// xorshift64*: deterministic so a failing page can be reproduced.
	g_RngState ^= g_RngState >> 12;
	g_RngState ^= g_RngState << 25;
	g_RngState ^= g_RngState >> 27;
	return g_RngState * 2685821657736338717ULL;
}

static double DemoSeconds(LARGE_INTEGER Start, LARGE_INTEGER End)
{
	static LARGE_INTEGER Freq = { 0 };
	if (Freq.QuadPart == 0) {
		QueryPerformanceFrequency(&Freq);
	}
	return (double)(End.QuadPart - Start.QuadPart) / (double)Freq.QuadPart;
}

//
// Pick a random page-aligned GPA that is inside a mapped chunk.
//

static UINT64 DemoRandomMappedPage(PHVMM_MAPPED_MEMORY Mapped)
{
	PHVMM_MAPPED_CHUNK Chunk = &Mapped->Chunks[DemoRandom() % Mapped->ChunkCount];
	UINT64 Pages = Chunk->Length / DUMP_PAGE_SIZE;
	return Chunk->GpaStart + (DemoRandom() % Pages) * DUMP_PAGE_SIZE;
}

static VOID DemoPrintCoverage(PHVMM_MAPPED_MEMORY Mapped, double MapSeconds)
{
	ULONG i;

	wprintf(L"\n   Runs reported by hvlib: %lu\n", Mapped->RunCount);
	for (i = 0; i < Mapped->RunCount && i < 16; i++) {
		wprintf(L"    run %2lu  GPA 0x%012llX  length 0x%llX (%llu MB)\n", i, Mapped->Runs[i].GpaStart, Mapped->Runs[i].Length, Mapped->Runs[i].Length >> 20);
	}
	if (Mapped->RunCount > 16) {
		wprintf(L"    ... %lu more\n", Mapped->RunCount - 16);
	}

	wprintf(L"   Mapped chunks: %lu (%llu map IOCTLs, %.3f s)\n", Mapped->ChunkCount, Mapped->MapCalls, MapSeconds);
	for (i = 0; i < Mapped->ChunkCount && i < 16; i++) {
		wprintf(L"    chunk %2lu  GPA 0x%012llX  length 0x%llX  VA %p\n", i, Mapped->Chunks[i].GpaStart, Mapped->Chunks[i].Length, Mapped->Chunks[i].UserVa);
	}
	if (Mapped->ChunkCount > 16) {
		wprintf(L"    ... %lu more\n", Mapped->ChunkCount - 16);
	}

	wprintf(L"   Coverage: %llu MB of %llu MB (%.2f%%)\n",
		Mapped->MappedBytes >> 20, Mapped->RunBytes >> 20,
		Mapped->RunBytes ? 100.0 * (double)Mapped->MappedBytes / (double)Mapped->RunBytes : 0.0);
}

//
// The whole design rests on the mapping being read-only. Prove it on every run:
// a write into the first mapped page must fault.
//

static BOOLEAN DemoWriteMustFault(PHVMM_MAPPED_MEMORY Mapped)
{
	volatile UCHAR *Page = (volatile UCHAR *)Mapped->Chunks[0].UserVa;
	BOOLEAN Faulted = FALSE;

	__try {
		Page[0] = Page[0];
	}
	__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
		Faulted = TRUE;
	}

	wprintf(L"   Write to the mapping: %s\n", Faulted ? L"faulted (read-only, as required)" : L"SUCCEEDED - the mapping is writable, this is a bug");
	return Faulted;
}

//
// One page through the classic read request, once by hand and once through hvlib, against
// the mapping. Says which side fails when the byte compare cannot compare anything.
//

static VOID DemoPrintIoctlStats(PHVMM_PARTITION_LAYOUT L);

static VOID DemoClassicReadProbe(ULONG64 Partition, PHVMM_MAPPED_MEMORY Mapped)
{
	UCHAR ViaIoctl[DUMP_PAGE_SIZE];
	UCHAR ViaSdk[DUMP_PAGE_SIZE];
	UCHAR ViaMap[DUMP_PAGE_SIZE];
	DEMO_GPA_INFO Info;
	DWORD Returned = 0;
	BOOL Ok;
	UINT64 Gpa = Mapped->Chunks[0].GpaStart + 0x10000;
	HVMM_PARTITION_LAYOUT L;

	wprintf(L"\n   Classic read path check at GPA 0x%llX (partition handle %p):\n", Gpa, Mapped->VidPartitionHandle);

	if (HvmmQueryPartitionLayout(Mapped->DeviceHandle, Mapped->VidPartitionHandle, &L)) {
		wprintf(L"    layout query through this handle: %s\n", L.Source == 0 ? L"resolved" : L"handle refused");
		DemoPrintIoctlStats(&L);
	}

	HvmmMappedRead(Mapped, Gpa, ViaMap, DUMP_PAGE_SIZE);

	RtlZeroMemory(&Info, sizeof(Info));
	Info.StartPage = Gpa;
	Info.BytesCount = DUMP_PAGE_SIZE;
	Info.PartitionHandle = Mapped->VidPartitionHandle;
	RtlZeroMemory(ViaIoctl, sizeof(ViaIoctl));
	Ok = DeviceIoControl(Mapped->DeviceHandle, DEMO_IOCTL_VID_INTERNAL_READ_MEMORY, &Info, sizeof(Info), ViaIoctl, sizeof(ViaIoctl), &Returned, NULL);
	wprintf(L"    driver IOCTL by hand:  ok=%d  bytes=%lu  LastError=%lu  %s\n", Ok, Returned, Ok ? 0 : GetLastError(),
		(Ok && Returned == DUMP_PAGE_SIZE) ? (memcmp(ViaIoctl, ViaMap, DUMP_PAGE_SIZE) == 0 ? L"bytes equal the mapping" : L"bytes DIFFER from the mapping") : L"");

	RtlZeroMemory(ViaSdk, sizeof(ViaSdk));
	SetLastError(0);
	Ok = SdkReadPhysicalMemory(Partition, Gpa, DUMP_PAGE_SIZE, ViaSdk, ReadInterfaceHvmmDrvInternal);
	wprintf(L"    hvlib DrvInternal:     ok=%d  LastError=%lu  %s\n", Ok, GetLastError(),
		Ok ? (memcmp(ViaSdk, ViaMap, DUMP_PAGE_SIZE) == 0 ? L"bytes equal the mapping" : L"bytes DIFFER from the mapping") : L"");

	RtlZeroMemory(ViaSdk, sizeof(ViaSdk));
	SetLastError(0);
	Ok = SdkReadPhysicalMemory(Partition, Gpa, DUMP_PAGE_SIZE, ViaSdk, ReadInterfaceWinHv);
	wprintf(L"    hvlib WinHv:           ok=%d  LastError=%lu  %s\n", Ok, GetLastError(),
		Ok ? (memcmp(ViaSdk, ViaMap, DUMP_PAGE_SIZE) == 0 ? L"bytes equal the mapping" : L"bytes DIFFER from the mapping") : L"");
}

//
// Byte compare. The VM should be suspended for this; a running guest changes
// pages between the two reads and would show as mismatches. Pages that differ
// are re-read once so a transient change is reported separately from a stable
// one. Only stable mismatches are bugs.
//

static BOOLEAN DemoByteCompare(ULONG64 Partition, PHVMM_MAPPED_MEMORY Mapped, BOOLEAN Suspended)
{
	PUCHAR ViaSdk = (PUCHAR)malloc(DUMP_PAGE_SIZE);
	PUCHAR ViaMap = (PUCHAR)malloc(DUMP_PAGE_SIZE);
	ULONG i;
	ULONG Compared = 0, SdkFailed = 0, Transient = 0, Stable = 0;
	ULONG Pages = DEMO_COMPARE_PAGES;
	UINT64 FirstStable = 0;

	if (ViaSdk == NULL || ViaMap == NULL) {
		free(ViaSdk);
		free(ViaMap);
		return FALSE;
	}

	if (Mapped->MappedBytes / DUMP_PAGE_SIZE < Pages) {
		Pages = (ULONG)(Mapped->MappedBytes / DUMP_PAGE_SIZE);
	}

	wprintf(L"\n   Byte compare: %lu random pages, mapping vs SdkReadPhysicalMemory(ReadInterfaceHvmmDrvInternal)%s\n",
		Pages, Suspended ? L", VM suspended" : L", VM running (transient mismatches expected)");

	for (i = 0; i < Pages; i++) {
		UINT64 Gpa = DemoRandomMappedPage(Mapped);

		if (!SdkReadPhysicalMemory(Partition, Gpa, DUMP_PAGE_SIZE, ViaSdk, ReadInterfaceHvmmDrvInternal)) {
			SdkFailed++;
			continue;
		}
		if (!HvmmMappedRead(Mapped, Gpa, ViaMap, DUMP_PAGE_SIZE)) {
			wprintf(L"   ERROR: HvmmMappedRead failed at GPA 0x%llX\n", Gpa);
			break;
		}
		Compared++;

		if (memcmp(ViaSdk, ViaMap, DUMP_PAGE_SIZE) != 0) {
			//
			// Re-read both. A stable difference is a real bug.
			//
			SdkReadPhysicalMemory(Partition, Gpa, DUMP_PAGE_SIZE, ViaSdk, ReadInterfaceHvmmDrvInternal);
			HvmmMappedRead(Mapped, Gpa, ViaMap, DUMP_PAGE_SIZE);
			if (memcmp(ViaSdk, ViaMap, DUMP_PAGE_SIZE) != 0) {
				if (Stable == 0) {
					FirstStable = Gpa;
				}
				Stable++;
			}
			else {
				Transient++;
			}
		}
	}

	wprintf(L"   compared %lu  sdk-read-failed %lu  transient %lu  STABLE MISMATCH %lu\n", Compared, SdkFailed, Transient, Stable);
	if (Stable != 0) {
		ULONG Off;
		SdkReadPhysicalMemory(Partition, FirstStable, DUMP_PAGE_SIZE, ViaSdk, ReadInterfaceHvmmDrvInternal);
		HvmmMappedRead(Mapped, FirstStable, ViaMap, DUMP_PAGE_SIZE);
		wprintf(L"   first stable mismatch at GPA 0x%llX, first differing bytes:\n", FirstStable);
		for (Off = 0; Off < DUMP_PAGE_SIZE; Off++) {
			if (ViaSdk[Off] != ViaMap[Off]) {
				wprintf(L"    +0x%03lX  sdk %02X  map %02X\n", Off, ViaSdk[Off], ViaMap[Off]);
				break;
			}
		}
	}

	free(ViaSdk);
	free(ViaMap);
	return (Stable == 0 && Compared > 0) ? TRUE : FALSE;
}

static VOID DemoBenchmark(ULONG64 Partition, PHVMM_MAPPED_MEMORY Mapped)
{
	PUCHAR Buffer = (PUCHAR)malloc(DEMO_CONTIG_BLOCK);
	UINT64 *Gpas = (UINT64 *)malloc(DEMO_SCATTER_PAGES * sizeof(UINT64));
	LARGE_INTEGER T0, T1;
	double SdkScatter, MapScatter, SdkContig, MapContig;
	ULONG i;
	UINT64 ContigStart = 0, ContigAvail = 0, Off;
	volatile UCHAR Sink = 0;

	if (Buffer == NULL || Gpas == NULL) {
		free(Buffer);
		free(Gpas);
		return;
	}

	for (i = 0; i < DEMO_SCATTER_PAGES; i++) {
		Gpas[i] = DemoRandomMappedPage(Mapped);
	}

	//
	// 512 scattered pages
	//

	QueryPerformanceCounter(&T0);
	for (i = 0; i < DEMO_SCATTER_PAGES; i++) {
		SdkReadPhysicalMemory(Partition, Gpas[i], DUMP_PAGE_SIZE, Buffer, ReadInterfaceHvmmDrvInternal);
	}
	QueryPerformanceCounter(&T1);
	SdkScatter = DemoSeconds(T0, T1);

	QueryPerformanceCounter(&T0);
	for (i = 0; i < DEMO_SCATTER_PAGES; i++) {
		HvmmMappedRead(Mapped, Gpas[i], Buffer, DUMP_PAGE_SIZE);
	}
	QueryPerformanceCounter(&T1);
	MapScatter = DemoSeconds(T0, T1);

	//
	// 16 MB contiguous, from the largest chunk, in 1 MB blocks like the dumper.
	//

	for (i = 0; i < Mapped->ChunkCount; i++) {
		if (Mapped->Chunks[i].Length > ContigAvail) {
			ContigAvail = Mapped->Chunks[i].Length;
			ContigStart = Mapped->Chunks[i].GpaStart;
		}
	}
	if (ContigAvail > DEMO_CONTIG_BYTES) {
		ContigAvail = DEMO_CONTIG_BYTES;
	}

	QueryPerformanceCounter(&T0);
	for (Off = 0; Off + DEMO_CONTIG_BLOCK <= ContigAvail; Off += DEMO_CONTIG_BLOCK) {
		SdkReadPhysicalMemory(Partition, ContigStart + Off, DEMO_CONTIG_BLOCK, Buffer, ReadInterfaceHvmmDrvInternal);
	}
	QueryPerformanceCounter(&T1);
	SdkContig = DemoSeconds(T0, T1);

	QueryPerformanceCounter(&T0);
	for (Off = 0; Off + DEMO_CONTIG_BLOCK <= ContigAvail; Off += DEMO_CONTIG_BLOCK) {
		HvmmMappedRead(Mapped, ContigStart + Off, Buffer, DEMO_CONTIG_BLOCK);
		Sink ^= Buffer[0];
	}
	QueryPerformanceCounter(&T1);
	MapContig = DemoSeconds(T0, T1);

	wprintf(L"\n   Benchmark                     SdkReadPhysicalMemory     mapped        speedup\n");
	wprintf(L"   %-28s  %10.3f ms  %10.3f ms  %8.1fx\n", L"512 scattered 4 KB reads", SdkScatter * 1e3, MapScatter * 1e3, MapScatter > 0 ? SdkScatter / MapScatter : 0.0);
	wprintf(L"   %-28s  %10.3f ms  %10.3f ms  %8.1fx\n", L"16 MB contiguous read", SdkContig * 1e3, MapContig * 1e3, MapContig > 0 ? SdkContig / MapContig : 0.0);
	wprintf(L"   %-28s  %10.1f MB/s %10.1f MB/s\n", L"contiguous throughput",
		SdkContig > 0 ? (double)ContigAvail / SdkContig / 1048576.0 : 0.0,
		MapContig > 0 ? (double)ContigAvail / MapContig / 1048576.0 : 0.0);

	free(Buffer);
	free(Gpas);
}

static VOID DemoLifecycle(PHVMM_MAPPED_MEMORY Mapped)
{
	UCHAR Buffer[DUMP_PAGE_SIZE];
	int Key;

	wprintf(L"\n   Lifecycle test: stop (or turn off) the VM now, then press 'y' here. Any other key skips.\n   > ");
	Key = DemoReadKey('n');
	wprintf(L"%c\n", Key);
	if (Key != 'y' && Key != 'Y') {
		return;
	}

	wprintf(L"   HvmmMappedIsAlive: %s (expected: FALSE after the VM stopped)\n", HvmmMappedIsAlive(Mapped) ? L"TRUE" : L"FALSE");
	wprintf(L"   Reading one page through the stale mapping: ");
	if (HvmmMappedRead(Mapped, Mapped->Chunks[0].GpaStart, Buffer, DUMP_PAGE_SIZE)) {
		wprintf(L"ok (no crash; the bytes are whatever the host put in those pages now)\n");
	}
	else {
		wprintf(L"read failed (fallback path)\n");
	}
	wprintf(L"   Start the VM again and rerun action 2 to check remap.\n");
}

static LPCWSTR DemoScanWord(UINT32 Flags, UINT32 Bit)
{
	return (Flags & Bit) ? L"found" : L"MISSING";
}

static VOID DemoPrintLayout(PHVMM_PARTITION_LAYOUT L)
{
	CHAR Tag[5];

	RtlCopyMemory(Tag, &L->Signature, 4);
	Tag[4] = 0;

	wprintf(L"    signature      \"%S\" (0x%08X)%s\n", Tag, L->Signature, L->Signature == 0x6e747250 ? L"" : L"  <- not Prtn");
	wprintf(L"    name/id        %s   gpar handle %s   objMBlock %s   host-pfn array %s   mblock array %s%s\n",
		DemoScanWord(L->ScanFlags, HVMM_SCAN_NAME), DemoScanWord(L->ScanFlags, HVMM_SCAN_GPAR),
		DemoScanWord(L->ScanFlags, HVMM_SCAN_OBJMBLOCK), DemoScanWord(L->ScanFlags, HVMM_SCAN_GPA_ARRAY),
		DemoScanWord(L->ScanFlags, HVMM_SCAN_MBLOCKARRAY), (L->ScanFlags & HVMM_SCAN_CACHED) ? L"   (cached)" : L"");
	wprintf(L"    offsets        name 0x%X  id 0x%X  gparHandle 0x%X (count +0x%X)  objMBlock 0x%X  hostPfnArray 0x%X  mblockArray 0x%X\n",
		L->NameOffset, L->PartitionIdOffset, L->GparHandleOffset, L->GparCountOffset, L->ObjMblockOffset, L->GuestGpaArrayOffset, L->MblockArrayOffset);
	wprintf(L"    full VM        %s   partition id 0x%llX   user type %lu\n", L->IsFullVm ? L"yes" : L"NO", L->PartitionId, L->UsrVmType);
}

static VOID DemoPrintIoctlStats(PHVMM_PARTITION_LAYOUT L)
{
	ULONG i;

	wprintf(L"    requests seen by the driver since load (function code: calls / failures / last status):\n");
	for (i = 0; i < HVMM_IOCTL_STAT_SLOTS && L->IoctlStats[i].Code != 0; i++) {
		wprintf(L"      0x%03X: %lu / %lu / 0x%08X\n", (L->IoctlStats[i].Code >> 2) & 0xFFF, L->IoctlStats[i].Calls, L->IoctlStats[i].Failures, L->IoctlStats[i].LastStatus);
	}

	if (L->LastReadFail.Handle != 0 || L->LastReadFail.Step != 0) {
		static LPCWSTR StepName[] = {
			L"ok", L"handle refused", L"no partition context", L"not a full VM", L"length not page aligned",
			L"page outside every GPA range", L"page in a vmwp descriptor", L"objMBlock NULL", L"host-pfn array NULL",
			L"MDL allocation", L"map failed", L"copy faulted", L"no page backed" };
		UINT32 Step = L->LastReadFail.Step < 13 ? L->LastReadFail.Step : 0;
		wprintf(L"    last classic read: handle %p  GPA 0x%llX  length 0x%X  ->  %s (step %lu) at page 0x%llX, backed %lu, unbacked %lu\n",
			(PVOID)L->LastReadFail.Handle, L->LastReadFail.Gpa, L->LastReadFail.Length, StepName[Step], L->LastReadFail.Step,
			L->LastReadFail.FailPage, L->LastReadFail.Backed, L->LastReadFail.Unbacked);
		wprintf(L"    request bytes as sent:");
		for (i = 0; i < 64; i++) {
			if ((i % 16) == 0) {
				wprintf(L"\n      +0x%02X ", i);
			}
			wprintf(L"%02X ", L->LastReadFail.Raw[i]);
		}
		wprintf(L"\n");
	}
}

VOID MappedMemoryLayoutReport(ULONG64 Partition)
{
	HANDLE Device;
	HANDLE VidPartition;
	HVMM_PARTITION_LAYOUT L;

	VidPartition = (HANDLE)SdkGetData2(Partition, InfoPartitionHandle);
	Device = CreateFileW(HVMM_DEVICE_PATH, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (Device == INVALID_HANDLE_VALUE) {
		wprintf(L"   Layout report: cannot open \\\\.\\hvmm (LastError %lu)\n", GetLastError());
		return;
	}

	//
	// What the driver saw while hvlib enumerated the partitions. That call always carries a
	// handle the driver can resolve, so this is the reliable picture.
	//

	if (HvmmQueryPartitionLayout(Device, NULL, &L)) {
		wprintf(L"\n   Partition layout as the driver saw it during enumeration:\n");
		DemoPrintLayout(&L);
	}
	else {
		wprintf(L"   Layout report: the loaded hvmm.sys does not answer the layout query (LastError %lu)\n", GetLastError());
	}

	//
	// The same query through the handle hvlib reports now. Before SdkSelectPartition this
	// handle may not be one the driver can resolve; the source line says so.
	//

	if (VidPartition != NULL && HvmmQueryPartitionLayout(Device, VidPartition, &L)) {
		wprintf(L"\n   Partition layout through hvlib's handle %p (%s):\n", VidPartition,
			L.Source == 0 ? L"resolved" : L"handle refused, NTSTATUS in the id field");
		DemoPrintLayout(&L);
	}

	CloseHandle(Device);
}

#define DEMO_IOCTL_GET_FRIENDLY_PARTITION_NAME CTL_CODE(FILE_DEVICE_UNKNOWN, 0x820, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DEMO_ENUM_REPLY_BYTES 0x610

typedef struct _DEMO_PARTITION_INFO {
	HANDLE PartitionHandle;
	ULONG64 VmwpPid;
	ULONG64 ProcessPid;
	ULONG VidInformationClass;
} DEMO_PARTITION_INFO;

static VOID DemoHexRows(PUCHAR p, ULONG cb, BOOLEAN SkipZeroRows)
{
	ULONG off, i;
	for (off = 0; off < cb; off += 16) {
		BOOLEAN AllZero = TRUE;
		for (i = 0; i < 16 && off + i < cb; i++) { if (p[off + i]) { AllZero = FALSE; break; } }
		if (SkipZeroRows && AllZero) { continue; }
		wprintf(L"      +0x%04X ", off);
		for (i = 0; i < 16 && off + i < cb; i++) { wprintf(L"%02X ", p[off + i]); }
		wprintf(L"  ");
		for (i = 0; i < 16 && off + i < cb; i++) { wprintf(L"%c", (p[off + i] >= 0x20 && p[off + i] < 0x7f) ? p[off + i] : L'.'); }
		wprintf(L"\n");
	}
}

//
// Action 3: what the loaded driver answers to the enumeration request, byte by byte, and the
// start of the partition context as our driver sees it. Run it against another hvmm.sys build
// to learn what its reply carries that ours does not.
//

VOID MappedMemoryEnumDump(ULONG64 Partition)
{
	HANDLE Device;
	HANDLE VidPartition;
	DEMO_PARTITION_INFO In;
	PUCHAR Reply;
	DWORD Returned = 0;
	PHVMM_PARTITION_LAYOUT L;

	VidPartition = (HANDLE)SdkGetData2(Partition, InfoPartitionHandle);
	Device = CreateFileW(HVMM_DEVICE_PATH, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (Device == INVALID_HANDLE_VALUE) {
		wprintf(L"   cannot open the device (LastError %lu)\n", GetLastError());
		return;
	}

	Reply = (PUCHAR)malloc(DEMO_ENUM_REPLY_BYTES);
	if (Reply != NULL) {
		RtlZeroMemory(&In, sizeof(In));
		In.PartitionHandle = VidPartition;
		In.ProcessPid = GetCurrentProcessId();
		RtlZeroMemory(Reply, DEMO_ENUM_REPLY_BYTES);
		if (DeviceIoControl(Device, DEMO_IOCTL_GET_FRIENDLY_PARTITION_NAME, &In, sizeof(In), Reply, DEMO_ENUM_REPLY_BYTES, &Returned, NULL)) {
			wprintf(L"\n   Enumeration reply (IOCTL 0x820) for partition handle %p: %lu bytes, non-zero rows:\n", VidPartition, Returned);
			DemoHexRows(Reply, Returned, TRUE);
		}
		else {
			wprintf(L"\n   Enumeration reply (IOCTL 0x820): failed, LastError %lu\n", GetLastError());
		}
		free(Reply);
	}

	L = (PHVMM_PARTITION_LAYOUT)malloc(sizeof(HVMM_PARTITION_LAYOUT));
	if (L != NULL) {
		if (HvmmQueryPartitionLayout(Device, VidPartition, L) && L->Source == 0) {
			wprintf(L"\n   Partition context as our driver sees it: %lu bytes, non-zero rows:\n", L->ContextBytes);
			DemoHexRows(L->Context, L->ContextBytes, TRUE);
		}
		else {
			wprintf(L"\n   (no layout query on this driver)\n");
		}
		free(L);
	}

	CloseHandle(Device);
}

BOOLEAN MappedMemoryDemo(ULONG64 Partition)
{
	PHVMM_MAPPED_MEMORY Mapped = NULL;
	LARGE_INTEGER T0, T1;
	BOOLEAN Suspended = FALSE;
	BOOLEAN CompareOk;
	int Key;

	wprintf(L"\n   Mapped memory: opening \\\\.\\hvmm and mapping every run...\n");

	QueryPerformanceCounter(&T0);
	if (!HvmmMappedOpen(Partition, NULL, &Mapped)) {
		DWORD Error = GetLastError();
		if (Error == ERROR_NOT_SUPPORTED) {
			wprintf(L"   ERROR: the loaded hvmm.sys did not answer the mapping query, so no map request was sent.\n"
				L"   Load the hvmm.sys built from this branch ('sc qc hvmm' shows which binary the service uses).\n");
		}
		else {
			wprintf(L"   ERROR: HvmmMappedOpen failed (LastError %lu).\n"
				L"   Causes: a container partition, or the VM is not running.\n", Error);
		}
		return FALSE;
	}
	QueryPerformanceCounter(&T1);

	DemoPrintCoverage(Mapped, DemoSeconds(T0, T1));

	if (!DemoWriteMustFault(Mapped)) {
		HvmmMappedClose(Mapped);
		return FALSE;
	}

	DemoClassicReadProbe(Partition, Mapped);

	wprintf(L"\n   Suspend the VM for the byte compare? (y/n) > ");
	Key = DemoReadKey('n');
	wprintf(L"%c\n", Key);
	if (Key == 'y' || Key == 'Y') {
		Suspended = SdkControlVmState(Partition, SuspendVm, SuspendResumePowershell, FALSE);
		if (!Suspended) {
			wprintf(L"   WARNING: suspend failed, comparing against a running guest.\n");
		}
	}

	CompareOk = DemoByteCompare(Partition, Mapped, Suspended);

	if (Suspended) {
		SdkControlVmState(Partition, ResumeVm, SuspendResumePowershell, FALSE);
	}

	DemoBenchmark(Partition, Mapped);

	wprintf(L"\n   Fallback reads issued so far: %llu\n", Mapped->FallbackReads);
	wprintf(L"   Byte compare: %s\n", CompareOk ? L"PASS" : L"FAIL");

	DemoLifecycle(Mapped);
	HvmmMappedClose(Mapped);

	return CompareOk;
}
