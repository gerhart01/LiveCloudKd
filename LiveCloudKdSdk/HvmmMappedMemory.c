//
// HvmmMappedMemory.c: persistent read-only user-mode mapping of guest physical
// memory through hvmm.sys. See HvmmMappedMemory.h for the contract.
//
// Plain C, no dependency beyond hvlib.dll for partition data. Compile it into the
// consumer (LiveCloudKd.exe, the LeechCore plugin, the SDK example); nothing new
// ships next to hvlib.dll and hvmm.sys.
//

#include "HvmmMappedMemory.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

#define HVMM_MAPPED_MAX_CONTEXTS 16

//
// Registry of open contexts so HvmmMappedReadPhysicalMemory can find a mapping by
// hvlib partition handle. Guarded by a critical section initialised on first use.
//

static PHVMM_MAPPED_MEMORY g_MappedContexts[HVMM_MAPPED_MAX_CONTEXTS];
static CRITICAL_SECTION g_MappedLock;
static INIT_ONCE g_MappedLockInit = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK HvmmMappedInitLock(PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
	UNREFERENCED_PARAMETER(InitOnce);
	UNREFERENCED_PARAMETER(Parameter);
	UNREFERENCED_PARAMETER(Context);
	InitializeCriticalSection(&g_MappedLock);
	return TRUE;
}

static BOOLEAN HvmmMappedRegister(PHVMM_MAPPED_MEMORY Mapped)
{
	ULONG i;
	BOOLEAN Registered = FALSE;
	InitOnceExecuteOnce(&g_MappedLockInit, HvmmMappedInitLock, NULL, NULL);
	EnterCriticalSection(&g_MappedLock);
	for (i = 0; i < HVMM_MAPPED_MAX_CONTEXTS; i++) {
		if (g_MappedContexts[i] == NULL) {
			g_MappedContexts[i] = Mapped;
			Registered = TRUE;
			break;
		}
	}
	LeaveCriticalSection(&g_MappedLock);
	return Registered;
}

static VOID HvmmMappedUnregister(PHVMM_MAPPED_MEMORY Mapped)
{
	ULONG i;
	InitOnceExecuteOnce(&g_MappedLockInit, HvmmMappedInitLock, NULL, NULL);
	EnterCriticalSection(&g_MappedLock);
	for (i = 0; i < HVMM_MAPPED_MAX_CONTEXTS; i++) {
		if (g_MappedContexts[i] == Mapped) {
			g_MappedContexts[i] = NULL;
		}
	}
	LeaveCriticalSection(&g_MappedLock);
}

static PHVMM_MAPPED_MEMORY HvmmMappedLookup(ULONG64 Partition)
{
	ULONG i;
	PHVMM_MAPPED_MEMORY Found = NULL;
	InitOnceExecuteOnce(&g_MappedLockInit, HvmmMappedInitLock, NULL, NULL);
	EnterCriticalSection(&g_MappedLock);
	for (i = 0; i < HVMM_MAPPED_MAX_CONTEXTS; i++) {
		if (g_MappedContexts[i] != NULL && g_MappedContexts[i]->Partition == Partition) {
			Found = g_MappedContexts[i];
			break;
		}
	}
	LeaveCriticalSection(&g_MappedLock);
	return Found;
}

//
// Raw IOCTL wrappers
//

BOOLEAN HvmmQueryMappingSupport(_In_ HANDLE DeviceHandle, _Out_opt_ PUINT64 MaxMapLength)
{
	HVMM_MAPPING_QUERY_INPUT Input;
	HVMM_MAPPING_QUERY_OUTPUT Output;
	DWORD BytesReturned = 0;

	if (MaxMapLength != NULL) {
		*MaxMapLength = 0;
	}
	if (DeviceHandle == NULL || DeviceHandle == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	//
	// An hvmm.sys without the mapping IOCTLs completes this code with
	// STATUS_INVALID_DEVICE_REQUEST. Anything short of the exact answer means the
	// driver is not one that understands map requests, and none must be sent to it.
	//

	Input.Magic = HVMM_MAPPING_QUERY_MAGIC;
	Input.Reserved = 0;
	RtlZeroMemory(&Output, sizeof(Output));

	if (!DeviceIoControl(DeviceHandle, HVMM_IOCTL_QUERY_MAPPING_SUPPORT, &Input, sizeof(Input), &Output, sizeof(Output), &BytesReturned, NULL)) {
		return FALSE;
	}

	if (BytesReturned != sizeof(Output) ||
		Output.Signature != HVMM_MAPPING_SIGNATURE ||
		Output.Version != HVMM_MAPPING_VERSION ||
		Output.MaxMapLength < HVMM_MAP_GPA_MIN_MAX_LENGTH ||
		(Output.MaxMapLength & (PAGE_SIZE - 1)) != 0) {
		return FALSE;
	}

	if (MaxMapLength != NULL) {
		*MaxMapLength = Output.MaxMapLength;
	}
	return TRUE;
}

BOOLEAN HvmmQueryPartitionLayout(_In_ HANDLE DeviceHandle, _In_ HANDLE VidPartitionHandle, _Out_ PHVMM_PARTITION_LAYOUT Layout)
{
	HVMM_PARTITION_LAYOUT_QUERY_INPUT Input;
	DWORD BytesReturned = 0;

	RtlZeroMemory(Layout, sizeof(*Layout));

	if (DeviceHandle == NULL || DeviceHandle == INVALID_HANDLE_VALUE) {
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	Input.PartitionHandle = VidPartitionHandle;

	if (!DeviceIoControl(DeviceHandle, HVMM_IOCTL_QUERY_PARTITION_LAYOUT, &Input, sizeof(Input), Layout, sizeof(*Layout), &BytesReturned, NULL)) {
		return FALSE;
	}

	return BytesReturned == sizeof(*Layout) ? TRUE : FALSE;
}

BOOLEAN HvmmMapGpaRange(_In_ HANDLE DeviceHandle, _In_ HANDLE VidPartitionHandle, _In_ UINT64 GpaStart, _In_ UINT64 Length, _Out_ PVOID *UserVa, _Out_ PUINT64 MappedBytes)
{
	HVMM_MAP_GPA_RANGE_INPUT Input;
	HVMM_MAP_GPA_RANGE_OUTPUT Output;
	DWORD BytesReturned = 0;

	*UserVa = NULL;
	*MappedBytes = 0;

	if (DeviceHandle == NULL || DeviceHandle == INVALID_HANDLE_VALUE || VidPartitionHandle == NULL) {
		return FALSE;
	}

	if (Length == 0 || (GpaStart & (PAGE_SIZE - 1)) != 0 || (Length & (PAGE_SIZE - 1)) != 0) {
		return FALSE;
	}

	Input.PartitionHandle = VidPartitionHandle;
	Input.GpaStart = GpaStart;
	Input.Length = Length;
	RtlZeroMemory(&Output, sizeof(Output));

	if (!DeviceIoControl(DeviceHandle, HVMM_IOCTL_MAP_GPA_RANGE, &Input, sizeof(Input), &Output, sizeof(Output), &BytesReturned, NULL)) {
		return FALSE;
	}

	if (BytesReturned < sizeof(Output) || Output.UserVa == NULL || Output.MappedBytes == 0) {
		return FALSE;
	}

	*UserVa = Output.UserVa;
	*MappedBytes = Output.MappedBytes;
	return TRUE;
}

BOOLEAN HvmmUnmapGpaRange(_In_ HANDLE DeviceHandle, _In_ PVOID UserVa)
{
	HVMM_UNMAP_GPA_RANGE_INPUT Input;
	UINT64 Dummy = 0;   // the driver's dispatcher rejects a zero-length output buffer
	DWORD BytesReturned = 0;

	if (DeviceHandle == NULL || DeviceHandle == INVALID_HANDLE_VALUE || UserVa == NULL) {
		return FALSE;
	}

	Input.UserVa = UserVa;
	return DeviceIoControl(DeviceHandle, HVMM_IOCTL_UNMAP_GPA_RANGE, &Input, sizeof(Input), &Dummy, sizeof(Dummy), &BytesReturned, NULL) ? TRUE : FALSE;
}

//
// Run table
//

static BOOLEAN HvmmMappedLoadRuns(PHVMM_MAPPED_MEMORY Mapped)
{
	ULONG64 NumberOfRuns = 0;
	ULONG64 MaximumPhysicalPage = 0;
	PULONG64 RunTable = NULL;
	ULONG64 i;
	GUEST_TYPE GuestType;

	SdkGetData(Mapped->Partition, InfoMmMaximumPhysicalPage, &MaximumPhysicalPage);
	GuestType = (GUEST_TYPE)SdkGetData2(Mapped->Partition, InfoGuestOsType);

	if (GuestType == MmStandard) {
		SdkGetData(Mapped->Partition, InfoNumberOfRuns, &NumberOfRuns);
		SdkGetData(Mapped->Partition, InfoRun, &RunTable);
	}

	//
	// hvlib reports runs as { BasePage, PageCount } pairs, the same layout the
	// LeechCore plugin consumes. Anything else collapses to one run over the
	// whole physical address space, which the driver trims to what is backed.
	//

	if (NumberOfRuns == 0 || NumberOfRuns > (MAX_NUMBER_OF_RUNS_BYTES / (2 * sizeof(ULONG64))) || RunTable == NULL) {
		if (MaximumPhysicalPage == 0) {
			return FALSE;
		}
		Mapped->Runs = (PHVMM_MAPPED_RUN)LocalAlloc(LMEM_ZEROINIT, sizeof(HVMM_MAPPED_RUN));
		if (Mapped->Runs == NULL) {
			return FALSE;
		}
		Mapped->RunCount = 1;
		Mapped->Runs[0].GpaStart = 0;
		Mapped->Runs[0].Length = MaximumPhysicalPage * PAGE_SIZE;
		Mapped->RunBytes = Mapped->Runs[0].Length;
		return TRUE;
	}

	Mapped->Runs = (PHVMM_MAPPED_RUN)LocalAlloc(LMEM_ZEROINIT, (SIZE_T)(NumberOfRuns * sizeof(HVMM_MAPPED_RUN)));
	if (Mapped->Runs == NULL) {
		return FALSE;
	}

	for (i = 0; i < NumberOfRuns; i++) {
		UINT64 BasePage = RunTable[2 * i];
		UINT64 PageCount = RunTable[2 * i + 1];
		if (PageCount == 0) {
			continue;
		}
		Mapped->Runs[Mapped->RunCount].GpaStart = BasePage * PAGE_SIZE;
		Mapped->Runs[Mapped->RunCount].Length = PageCount * PAGE_SIZE;
		Mapped->RunBytes += PageCount * PAGE_SIZE;
		Mapped->RunCount++;
	}

	return Mapped->RunCount > 0 ? TRUE : FALSE;
}

//
// Chunk table
//

static BOOLEAN HvmmMappedAddChunk(PHVMM_MAPPED_MEMORY Mapped, UINT64 GpaStart, UINT64 Length, PVOID UserVa)
{
	PHVMM_MAPPED_CHUNK NewTable;
	ULONG i, Pos;

	NewTable = (PHVMM_MAPPED_CHUNK)LocalAlloc(LMEM_ZEROINIT, (Mapped->ChunkCount + 1) * sizeof(HVMM_MAPPED_CHUNK));
	if (NewTable == NULL) {
		return FALSE;
	}

	//
	// Keep the table sorted so reads can binary search it.
	//

	Pos = 0;
	while (Pos < Mapped->ChunkCount && Mapped->Chunks[Pos].GpaStart < GpaStart) {
		Pos++;
	}

	for (i = 0; i < Pos; i++) {
		NewTable[i] = Mapped->Chunks[i];
	}
	NewTable[Pos].GpaStart = GpaStart;
	NewTable[Pos].Length = Length;
	NewTable[Pos].UserVa = (PUCHAR)UserVa;
	for (i = Pos; i < Mapped->ChunkCount; i++) {
		NewTable[i + 1] = Mapped->Chunks[i];
	}

	if (Mapped->Chunks != NULL) {
		LocalFree(Mapped->Chunks);
	}
	Mapped->Chunks = NewTable;
	Mapped->ChunkCount++;
	Mapped->MappedBytes += Length;
	return TRUE;
}

//
// Find the chunk containing Gpa. Returns NULL if none.
//

static PHVMM_MAPPED_CHUNK HvmmMappedFindChunk(PHVMM_MAPPED_MEMORY Mapped, UINT64 Gpa)
{
	LONG Low = 0;
	LONG High = (LONG)Mapped->ChunkCount - 1;

	while (Low <= High) {
		LONG Mid = Low + (High - Low) / 2;
		PHVMM_MAPPED_CHUNK Chunk = &Mapped->Chunks[Mid];
		if (Gpa < Chunk->GpaStart) {
			High = Mid - 1;
		}
		else if (Gpa >= Chunk->GpaStart + Chunk->Length) {
			Low = Mid + 1;
		}
		else {
			return Chunk;
		}
	}
	return NULL;
}

//
// Map one run in chunks of the size the driver reported. The driver maps the longest
// host-backed prefix of each request; a page it cannot back ends the chunk and
// the next request starts past it. Holes are skipped with a growing stride so a
// large unbacked region does not cost one IOCTL per page; anything skipped that
// was actually backed is still readable through the fallback.
//

static VOID HvmmMappedMapRun(PHVMM_MAPPED_MEMORY Mapped, PHVMM_MAPPED_RUN Run)
{
	UINT64 Gpa = Run->GpaStart;
	UINT64 End = Run->GpaStart + Run->Length;
	UINT64 Skip = PAGE_SIZE;

	while (Gpa < End) {
		UINT64 Request = End - Gpa;
		PVOID UserVa = NULL;
		UINT64 MappedBytes = 0;

		if (Request > Mapped->MaxMapLength) {
			Request = Mapped->MaxMapLength;
		}

		Mapped->MapCalls++;

		if (HvmmMapGpaRange(Mapped->DeviceHandle, Mapped->VidPartitionHandle, Gpa, Request, &UserVa, &MappedBytes)) {
			if (!HvmmMappedAddChunk(Mapped, Gpa, MappedBytes, UserVa)) {
				HvmmUnmapGpaRange(Mapped->DeviceHandle, UserVa);
				return;
			}
			Gpa += MappedBytes;
			Skip = PAGE_SIZE;

			if (MappedBytes < Request) {
				//
				// The page at Gpa is not backed. Step over it.
				//
				Gpa += PAGE_SIZE;
			}
		}
		else {
			Gpa += Skip;
			if (Skip < 0x200000) {
				Skip *= 2;
			}
		}
	}
}

//
// Public API
//

BOOLEAN HvmmMappedOpen(_In_ ULONG64 Partition, _In_opt_ HANDLE DeviceHandle, _Out_ PHVMM_MAPPED_MEMORY *Mapped)
{
	PHVMM_MAPPED_MEMORY Ctx;
	ULONG i;

	*Mapped = NULL;

	if (Partition == 0) {
		return FALSE;
	}

	Ctx = (PHVMM_MAPPED_MEMORY)LocalAlloc(LMEM_ZEROINIT, sizeof(HVMM_MAPPED_MEMORY));
	if (Ctx == NULL) {
		return FALSE;
	}

	Ctx->Partition = Partition;
	Ctx->FallbackMethod = ReadInterfaceHvmmDrvInternal;
	Ctx->VidPartitionHandle = (HANDLE)SdkGetData2(Partition, InfoPartitionHandle);

	if (Ctx->VidPartitionHandle == NULL) {
		LocalFree(Ctx);
		return FALSE;
	}

	if (DeviceHandle != NULL && DeviceHandle != INVALID_HANDLE_VALUE) {
		Ctx->DeviceHandle = DeviceHandle;
		Ctx->OwnsDeviceHandle = FALSE;
	}
	else {
		Ctx->DeviceHandle = CreateFileW(HVMM_DEVICE_PATH, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (Ctx->DeviceHandle == INVALID_HANDLE_VALUE) {
			LocalFree(Ctx);
			return FALSE;
		}
		Ctx->OwnsDeviceHandle = TRUE;
	}

	//
	// Nothing but the query goes to a driver that has not answered it: another
	// hvmm.sys build may give the map code a different meaning.
	//

	if (!HvmmQueryMappingSupport(Ctx->DeviceHandle, &Ctx->MaxMapLength)) {
		HvmmMappedClose(Ctx);
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	if (!HvmmMappedLoadRuns(Ctx)) {
		HvmmMappedClose(Ctx);
		return FALSE;
	}

	for (i = 0; i < Ctx->RunCount; i++) {
		HvmmMappedMapRun(Ctx, &Ctx->Runs[i]);
	}

	if (Ctx->ChunkCount == 0) {
		//
		// Nothing could be mapped: a container, an old hvmm.sys without the IOCTL,
		// or a stopped VM. The caller keeps using SdkReadPhysicalMemory.
		//
		HvmmMappedClose(Ctx);
		return FALSE;
	}

	if (!HvmmMappedRegister(Ctx)) {
		HvmmMappedClose(Ctx);
		SetLastError(ERROR_TOO_MANY_OPEN_FILES);
		return FALSE;
	}

	*Mapped = Ctx;
	return TRUE;
}

static BOOLEAN HvmmMappedCopy(PVOID Destination, PVOID Source, SIZE_T Length)
{
	__try {
		RtlCopyMemory(Destination, Source, Length);
		return TRUE;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return FALSE;
	}
}

BOOLEAN HvmmMappedRead(_In_ PHVMM_MAPPED_MEMORY Mapped, _In_ UINT64 Gpa, _Out_writes_bytes_(Length) PVOID Buffer, _In_ UINT64 Length)
{
	PUCHAR Out = (PUCHAR)Buffer;
	UINT64 Done = 0;

	if (Mapped == NULL || Buffer == NULL) {
		return FALSE;
	}

	while (Done < Length) {
		UINT64 Cur = Gpa + Done;
		UINT64 Remaining = Length - Done;
		PHVMM_MAPPED_CHUNK Chunk = HvmmMappedFindChunk(Mapped, Cur);

		if (Chunk != NULL) {
			UINT64 Offset = Cur - Chunk->GpaStart;
			UINT64 Avail = Chunk->Length - Offset;
			UINT64 Take = Remaining < Avail ? Remaining : Avail;

			if (!HvmmMappedCopy(Out + Done, Chunk->UserVa + Offset, (SIZE_T)Take)) {
				return FALSE;
			}
			Done += Take;
			continue;
		}

		//
		// Not mapped: hand the gap up to the next chunk (or the end) to hvlib.
		//

		{
			UINT64 Gap = Remaining;
			ULONG i;
			for (i = 0; i < Mapped->ChunkCount; i++) {
				if (Mapped->Chunks[i].GpaStart > Cur) {
					UINT64 ToNext = Mapped->Chunks[i].GpaStart - Cur;
					if (ToNext < Gap) {
						Gap = ToNext;
					}
					break;
				}
			}

			Mapped->FallbackReads++;
			if (!SdkReadPhysicalMemory(Mapped->Partition, Cur, Gap, Out + Done, Mapped->FallbackMethod)) {
				return FALSE;
			}
			Done += Gap;
		}
	}

	return TRUE;
}

PVOID HvmmMappedGetPointer(_In_ PHVMM_MAPPED_MEMORY Mapped, _In_ UINT64 Gpa, _Out_opt_ PUINT64 AvailableBytes)
{
	PHVMM_MAPPED_CHUNK Chunk;

	if (AvailableBytes != NULL) {
		*AvailableBytes = 0;
	}
	if (Mapped == NULL) {
		return NULL;
	}

	Chunk = HvmmMappedFindChunk(Mapped, Gpa);
	if (Chunk == NULL) {
		return NULL;
	}

	if (AvailableBytes != NULL) {
		*AvailableBytes = Chunk->Length - (Gpa - Chunk->GpaStart);
	}
	return Chunk->UserVa + (Gpa - Chunk->GpaStart);
}

BOOLEAN HvmmMappedIsAlive(_In_ PHVMM_MAPPED_MEMORY Mapped)
{
	PVOID UserVa = NULL;
	UINT64 MappedBytes = 0;
	UINT64 Gpa;

	if (Mapped == NULL || Mapped->ChunkCount == 0) {
		return FALSE;
	}

	//
	// Map one already-known page again. This goes through the same partition
	// lookup the reads depend on, so it fails as soon as the partition is gone.
	//

	Gpa = Mapped->Chunks[0].GpaStart;
	if (!HvmmMapGpaRange(Mapped->DeviceHandle, Mapped->VidPartitionHandle, Gpa, PAGE_SIZE, &UserVa, &MappedBytes)) {
		return FALSE;
	}
	HvmmUnmapGpaRange(Mapped->DeviceHandle, UserVa);
	return TRUE;
}

VOID HvmmMappedClose(_In_opt_ PHVMM_MAPPED_MEMORY Mapped)
{
	ULONG i;

	if (Mapped == NULL) {
		return;
	}

	HvmmMappedUnregister(Mapped);

	if (Mapped->Chunks != NULL) {
		for (i = Mapped->ChunkCount; i > 0; i--) {
			HvmmUnmapGpaRange(Mapped->DeviceHandle, Mapped->Chunks[i - 1].UserVa);
		}
		LocalFree(Mapped->Chunks);
	}

	if (Mapped->Runs != NULL) {
		LocalFree(Mapped->Runs);
	}

	if (Mapped->OwnsDeviceHandle && Mapped->DeviceHandle != NULL && Mapped->DeviceHandle != INVALID_HANDLE_VALUE) {
		CloseHandle(Mapped->DeviceHandle);
	}

	LocalFree(Mapped);
}

BOOLEAN HvmmMappedReadPhysicalMemory(_In_ ULONG64 PartitionHandle, _In_ UINT64 StartPosition, _In_ UINT64 ReadByteCount, _Inout_ PVOID ClientBuffer, _In_ READ_MEMORY_METHOD Method)
{
	PHVMM_MAPPED_MEMORY Mapped = HvmmMappedLookup(PartitionHandle);

	if (Mapped != NULL) {
		return HvmmMappedRead(Mapped, StartPosition, ClientBuffer, ReadByteCount);
	}

	return SdkReadPhysicalMemory(PartitionHandle, StartPosition, ReadByteCount, ClientBuffer, Method);
}
