#include "hvmm.h"
#include <ntstrsafe.h>

PVOID g_vmmemHandle = NULL;
EPROCESS_INTERNALS EprocessInternalData = { 0 };

//
// Discovered vid.sys layout, found once by signature scan and reused. The moving offsets are
// vid.sys-build constants, the same for every partition on one host, so the first successful
// discovery is cached here and only the per-partition shape check is redone. Scanning by
// signature is what keeps the driver working across Windows builds.
//

static VM_LAYOUT g_DiscoveredLayout = { 0 };

static ULONG64 VidHighestHostPfn(VOID);

//
// The last classic read that failed, for the layout query. Diagnostic only.
//

static HVMM_LAST_READ_FAIL g_LastReadFail = { 0 };

//
// True when p is a canonical kernel address whose page is resident. Reject anything below
// the kernel half before touching MmIsAddressValid, so a
// stray user or non-canonical pointer never reaches it. Every candidate pointer in a scan goes
// through here before it is dereferenced, and the scans themselves run under __try.
//

static BOOLEAN VidProbe(PVOID p)
{
	if ((ULONG64)p < 0xffff800000000000ULL) {
		return FALSE;
	}
	return MmIsAddressValid(p);
}

//
// What the last enumeration (VidGetFriendlyPartitionName) saw. The layout query returns it
// when the caller has no partition handle yet, which is the case before SdkSelectPartition.
//

static PARTITION_LAYOUT_QUERY_OUTPUT g_LastEnumLayout = { 0 };

static VOID VidFillLayoutReport(PCHAR ctx, PVM_LAYOUT Layout, PPARTITION_LAYOUT_QUERY_OUTPUT Output)
{
	RtlZeroMemory(Output, sizeof(*Output));
	__try {
		if (VidProbe(ctx)) {
			Output->Signature = *(PULONG)ctx;
		}
		Output->ScanFlags = Layout->ScanFlags;
		Output->IsFullVm = Layout->IsFullVm ? 1 : 0;
		Output->UsrVmType = Layout->UsrVmType;
		Output->NameOffset = Layout->NameOffset;
		Output->PartitionIdOffset = Layout->PartitionIdOffset;
		Output->MblockArrayOffset = Layout->MblockArrayOffset;
		Output->GparHandleOffset = Layout->GparHandleOffset;
		Output->GparCountOffset = Layout->GparCountOffset;
		Output->ObjMblockOffset = Layout->ObjMblockOffset;
		Output->GuestGpaArrayOffset = Layout->GuestGpaArrayOffset;
		if (VidProbe(ctx + Layout->PartitionIdOffset)) {
			Output->PartitionId = *(PULONG64)(ctx + Layout->PartitionIdOffset);
		}
		Output->ContextBytes = 0;
		while (Output->ContextBytes < HVMM_CONTEXT_DUMP_BYTES && VidProbe(ctx + Output->ContextBytes)) {
			ULONG chunk = PAGE_SIZE - (((ULONG_PTR)ctx + Output->ContextBytes) & (PAGE_SIZE - 1));
			if (chunk > HVMM_CONTEXT_DUMP_BYTES - Output->ContextBytes) {
				chunk = HVMM_CONTEXT_DUMP_BYTES - Output->ContextBytes;
			}
			RtlCopyMemory(Output->Context + Output->ContextBytes, ctx + Output->ContextBytes, chunk);
			Output->ContextBytes += chunk;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		KDbgLog("VidFillLayoutReport: exception", GetExceptionCode());
	}
}

//
// One code unit of a partition friendly name: printable ASCII in the low byte (0x1d < c < 0x7f).
//

static BOOLEAN VidNameChar(UCHAR c)
{
	return (c > 0x1d) && (c < 0x7f);
}

//
// Find the FriendlyName offset inside the "Prtn" context. Scan [0x60..0x260) for
// two inline UTF-16 code units that look like a name (printable low byte, zero high byte) and
// whose partition id at name+0x200 is a small non-zero value. On a hit the
// PartitionId offset is name+0x200. Leaves the 2018 pair (0x78 / 0x278) if nothing fits.
//

static BOOLEAN VidScanNameOffset(PCHAR ctx, PULONG pNameOffset, PULONG pIdOffset)
{
	ULONG off;

	for (off = 0x60; off < 0x260; off += 8) {
		PCHAR p = ctx + off;
		ULONG64 id;

		if (!VidProbe(p) || !VidProbe(p + 0x200)) {
			continue;
		}
		if (p[1] != 0 || p[3] != 0 || p[5] != 0 || p[7] != 0) {
			continue;
		}
		if (!VidNameChar((UCHAR)p[0]) || !VidNameChar((UCHAR)p[2])) {
			continue;
		}
		id = *(PULONG64)(p + 0x200) - 1;
		if (id < 0xfffe) {
			*pNameOffset = off;
			*pIdOffset = off + 0x200;
			return TRUE;
		}
	}
	return FALSE;
}

//
// Find the GPAR_BLOCK_HANDLE offset inside the "Prtn" context, plus the count offset inside that
// handle and the first "Gpar" object it points at. Scan [0x800..0x4000) for a
// pointer (the handle) whose GparArray at handle+0x8 holds, at index 0, a "Gpar" object
// The count sits at handle+0x14 when the handle's owner (handle[0]) is itself a
// "Prtn" partition, else at +0x18.
//

static BOOLEAN VidScanGparHandleOffset(PCHAR ctx, PULONG pHandleOffset, PULONG pCountOffset, PGPAR_OBJECT* pFirstGpar)
{
	ULONG off;

	for (off = 0x800; off < 0x4000; off += 8) {
		PCHAR handle;
		PCHAR gparArray;
		PGPAR_OBJECT firstGpar;
		PCHAR owner;

		if (!VidProbe(ctx + off)) {
			continue;
		}
		handle = *(PCHAR*)(ctx + off);
		if (!VidProbe(handle + 8)) {
			continue;
		}
		gparArray = *(PCHAR*)(handle + 8);
		if (!VidProbe(gparArray)) {
			continue;
		}
		firstGpar = *(PGPAR_OBJECT*)gparArray;
		if (!VidProbe(firstGpar) || *(PULONG)firstGpar != VID_SIG_GPAR) {
			continue;
		}

		*pHandleOffset = off;
		owner = VidProbe(handle) ? *(PCHAR*)handle : NULL;
		if (owner != NULL && VidProbe(owner) && *(PULONG)owner == VID_SIG_PRTN) {
			*pCountOffset = 0x14;
		} else {
			*pCountOffset = 0x18;
		}
		*pFirstGpar = firstGpar;
		return TRUE;
	}
	return FALSE;
}

//
// Find the objMBlock offset inside a "Gpar" object. Scan [0x90..0x300) for a
// pointer to a "Mb  " block. SomeGpaOffset / VmmMemGpaOffset sit right after it.
//

static BOOLEAN VidScanObjMblockOffset(PCHAR gpar, PULONG pObjMblockOffset)
{
	ULONG off;

	for (off = 0x90; off < 0x300; off += 8) {
		PMEMORY_BLOCK mb;

		if (!VidProbe(gpar + off)) {
			continue;
		}
		mb = *(PMEMORY_BLOCK*)(gpar + off);
		if (VidProbe(mb) && *(PULONG)mb == VID_SIG_MB) {
			*pObjMblockOffset = off;
			return TRUE;
		}
	}
	return FALSE;
}

//
// Find the host-PFN array offset inside a "Mb  " block. Scan [0x40..0x200) for a
// pointer whose entries (0x10 stride) start 1,2,3,4,5 in their low byte - the
// shape of the guest-to-host page map.
//

static BOOLEAN VidScanGuestGpaArrayOffset(PCHAR mblock, PULONG pArrayOffset)
{
	ULONG off;

	for (off = 0x40; off < 0x200; off += 8) {
		PCHAR arr;

		if (!VidProbe(mblock + off)) {
			continue;
		}
		arr = *(PCHAR*)(mblock + off);
		if (VidProbe(arr) && VidProbe(arr + 0x50) &&
			arr[0x10] == 1 && arr[0x20] == 2 && arr[0x30] == 3 &&
			arr[0x40] == 4 && arr[0x50] == 5) {
			*pArrayOffset = off;
			return TRUE;
		}
	}
	return FALSE;
}

//
// Find the MEMORY_BLOCK_ARRAY offset inside the "Prtn" context. Scan
// [0x800..0x4000) for a pointer P whose element 1 (P+8) is a "Mb  " block.
// Best effort: only the mblock-info diagnostic path uses it, the read path does not.
//

static BOOLEAN VidScanMblockArrayOffset(PCHAR ctx, PULONG pArrayOffset)
{
	ULONG off;

	for (off = 0x800; off < 0x4000; off += 8) {
		PCHAR arr;
		PMEMORY_BLOCK mb;

		if (!VidProbe(ctx + off)) {
			continue;
		}
		arr = *(PCHAR*)(ctx + off);
		if (!VidProbe(arr + 8)) {
			continue;
		}
		mb = *(PMEMORY_BLOCK*)(arr + 8);
		if (VidProbe(mb) && *(PULONG)mb == VID_SIG_MB) {
			*pArrayOffset = off;
			return TRUE;
		}
	}
	return FALSE;
}

//
// True when this partition is a full, host-page-backed VM: a "Prtn" context whose GPAR handle
// leads to a "Gpar" object, whose objMBlock is a "Mb  " block with a host-PFN array. This is the
// shape the read path needs, and it gates the full-VM path instead of
// reading VmType at a fixed offset. Re-checked per partition against the cached offsets.
//

static BOOLEAN VidValidateFullVm(PCHAR ctx, PVM_LAYOUT lay)
{
	BOOLEAN result = FALSE;

	__try {
		PCHAR handle;
		PCHAR gparArray;
		PGPAR_OBJECT firstGpar;
		PMEMORY_BLOCK mb;
		PCHAR arr;

		if (!VidProbe(ctx) || *(PULONG)ctx != VID_SIG_PRTN) {
			return FALSE;
		}
		if (!VidProbe(ctx + lay->GparHandleOffset)) {
			return FALSE;
		}
		handle = *(PCHAR*)(ctx + lay->GparHandleOffset);
		if (!VidProbe(handle + 8)) {
			return FALSE;
		}
		gparArray = *(PCHAR*)(handle + 8);
		if (!VidProbe(gparArray)) {
			return FALSE;
		}
		firstGpar = *(PGPAR_OBJECT*)gparArray;
		if (!VidProbe(firstGpar) || *(PULONG)firstGpar != VID_SIG_GPAR) {
			return FALSE;
		}
		if (!VidProbe((PCHAR)firstGpar + lay->ObjMblockOffset)) {
			return FALSE;
		}
		mb = *(PMEMORY_BLOCK*)((PCHAR)firstGpar + lay->ObjMblockOffset);
		if (!VidProbe(mb) || *(PULONG)mb != VID_SIG_MB) {
			return FALSE;
		}
		if (!VidProbe((PCHAR)mb + lay->GuestGpaArrayOffset)) {
			return FALSE;
		}
		arr = *(PCHAR*)((PCHAR)mb + lay->GuestGpaArrayOffset);
		if (!VidProbe(arr)) {
			return FALSE;
		}
		result = TRUE;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		result = FALSE;
	}
	return result;
}

//
// Work out where every moving field of one partition sits. Full VMs (the only ones this path
// reads) are host-page backed and carry the "Prtn"/"Gpar"/"Mb  " structures; a container or exo
// partition does not, so it is left with IsFullVm FALSE and the caller falls back to the old path.
//
// The offsets are found once and cached (see g_DiscoveredLayout); anything a scan cannot find
// keeps its 2018 fallback, so a partial failure degrades to the old behaviour, never a wrong read.
// pLayout always comes back filled with usable offsets; the BOOLEAN says whether this context is a
// "Prtn" partition we could reason about.
//

BOOLEAN VidResolveLayout(PVM_PROCESS_CONTEXT pPartitionHandle, PVM_LAYOUT pLayout)
{
	PCHAR ctx = (PCHAR)pPartitionHandle;
	VM_LAYOUT local;
	BOOLEAN discovered = FALSE;

	RtlZeroMemory(pLayout, sizeof(VM_LAYOUT));
	pLayout->Context = pPartitionHandle;
	pLayout->NameOffset = PARTITION_NAME_1803_OFFSET;
	pLayout->PartitionIdOffset = PARTITION_ID_1803_OFFSET;
	pLayout->GparHandleOffset = 0x1520;
	pLayout->GparCountOffset = 0x14;
	pLayout->MblockArrayOffset = 0x1218;
	pLayout->ObjMblockOffset = 0x170;
	pLayout->SomeGpaOffset = 0x178;
	pLayout->VmmMemGpaOffset = 0x180;
	pLayout->GuestGpaArrayOffset = 0xF0;
	pLayout->UsrVmType = UsrVidVmTypeUnknown;

	if (pPartitionHandle == NULL) {
		return FALSE;
	}

	RtlZeroMemory(&local, sizeof(local));

	__try {
		if (!VidProbe(ctx) || *(PULONG)ctx != VID_SIG_PRTN) {
			KDbgPrintString("VidResolveLayout: not a Prtn partition context");
			return FALSE;
		}

		if (g_DiscoveredLayout.Resolved) {
			local = g_DiscoveredLayout;
			local.ScanFlags |= VID_SCAN_CACHED;
			discovered = TRUE;
		} else {
			PGPAR_OBJECT firstGpar = NULL;
			BOOLEAN haveName, haveGpar, haveObjMb = FALSE, haveArray = FALSE, haveMbArray;

			local.NameOffset = PARTITION_NAME_1803_OFFSET;
			local.PartitionIdOffset = PARTITION_ID_1803_OFFSET;
			local.GparHandleOffset = 0x1520;
			local.GparCountOffset = 0x14;
			local.MblockArrayOffset = 0x1218;
			local.ObjMblockOffset = 0x170;
			local.SomeGpaOffset = 0x178;
			local.VmmMemGpaOffset = 0x180;
			local.GuestGpaArrayOffset = 0xF0;

			haveName = VidScanNameOffset(ctx, &local.NameOffset, &local.PartitionIdOffset);
			haveGpar = VidScanGparHandleOffset(ctx, &local.GparHandleOffset, &local.GparCountOffset, &firstGpar);

			//
			// The two inner scans need a GPA range whose memory block is host backed. Not every
			// range is (a small one may hold no pages yet), so walk the whole GPAR array and
			// take the first range where both scans succeed.
			//

			if (haveGpar && firstGpar != NULL) {
				PCHAR handle = *(PCHAR*)(ctx + local.GparHandleOffset);
				PVOID *gparArray = *(PVOID**)(handle + 8);
				UINT32 count = *(PUINT32)(handle + local.GparCountOffset);
				UINT32 k;

				if (count > 4096) {
					count = 4096;
				}

				for (k = 0; k < count && !(haveObjMb && haveArray); k++) {
					PCHAR gpar;
					PMEMORY_BLOCK mblock;

					if (!VidProbe(gparArray + k)) {
						continue;
					}
					gpar = (PCHAR)gparArray[k];
					if (!VidProbe(gpar) || *(PULONG)gpar != VID_SIG_GPAR) {
						continue;
					}

					haveObjMb = VidScanObjMblockOffset(gpar, &local.ObjMblockOffset);
					haveArray = FALSE;
					if (!haveObjMb) {
						continue;
					}

					local.SomeGpaOffset = local.ObjMblockOffset + 0x8;
					local.VmmMemGpaOffset = local.ObjMblockOffset + 0x10;
					mblock = *(PMEMORY_BLOCK*)(gpar + local.ObjMblockOffset);
					if (VidProbe(mblock)) {
						haveArray = VidScanGuestGpaArrayOffset((PCHAR)mblock, &local.GuestGpaArrayOffset);
					}
				}
			}
			haveMbArray = VidScanMblockArrayOffset(ctx, &local.MblockArrayOffset);

			local.ScanFlags =
				(haveName ? VID_SCAN_NAME : 0) |
				(haveGpar ? VID_SCAN_GPAR : 0) |
				(haveObjMb ? VID_SCAN_OBJMBLOCK : 0) |
				(haveArray ? VID_SCAN_GPA_ARRAY : 0) |
				(haveMbArray ? VID_SCAN_MBLOCKARRAY : 0);

			if (haveName && haveGpar && haveObjMb && haveArray) {
				local.Context = pPartitionHandle;
				local.Resolved = TRUE;
				g_DiscoveredLayout = local;
				discovered = TRUE;
				KDbgPrintString("VidResolveLayout: layout discovered by signature scan");
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		KDbgLog("VidResolveLayout: exception during scan", GetExceptionCode());
		return FALSE;
	}

	if (!discovered) {
		//
		// Prtn context but the full-VM chain was not found (a container / exo partition, or a
		// vid.sys shape we do not know). Report it as resolved-but-not-full so the caller takes
		// the old path; the fallback offsets above stay in place.
		//
		pLayout->Resolved = TRUE;
		pLayout->IsFullVm = FALSE;
		pLayout->ScanFlags = local.ScanFlags;
		return TRUE;
	}

	pLayout->NameOffset = local.NameOffset;
	pLayout->PartitionIdOffset = local.PartitionIdOffset;
	pLayout->GparHandleOffset = local.GparHandleOffset;
	pLayout->GparCountOffset = local.GparCountOffset;
	pLayout->MblockArrayOffset = local.MblockArrayOffset;
	pLayout->ObjMblockOffset = local.ObjMblockOffset;
	pLayout->SomeGpaOffset = local.SomeGpaOffset;
	pLayout->VmmMemGpaOffset = local.VmmMemGpaOffset;
	pLayout->GuestGpaArrayOffset = local.GuestGpaArrayOffset;
	pLayout->ScanFlags = local.ScanFlags;
	pLayout->Resolved = TRUE;

	pLayout->IsFullVm = VidValidateFullVm(ctx, pLayout);
	pLayout->UsrVmType = pLayout->IsFullVm ? UsrVidVmTypeFullWinSrvVM : UsrVidVmTypeUnknown;

	return TRUE;
}

//
// Enable vmwp.exe process protection
//

BOOLEAN SearchEprocessOffsets(PEPROCESS eProcess)
{
	
	ULONG i = 0;
	BOOLEAN bResult01 = FALSE, bResult02 = FALSE;
	PULONG pEprocessArea = (PULONG)eProcess;

	if ((EprocessInternalData.EprocessSignaturesOffset != 0) && (EprocessInternalData.EprocessMitigationsOffset != 0))
		return TRUE;

	//
	//Searching signature offset
	//

	for (i = 0x150 / sizeof(ULONG); i < 0x21E; i++)
	{

		if (pEprocessArea[i] == 0x808)
		{
			EprocessInternalData.EprocessSignaturesOffset = i * sizeof(ULONG);
			bResult01 = TRUE;
			break;
		}
	}

	//
	//Searching mitigations offset
	//

	for (i = 0x150 / sizeof(ULONG); i < 0x21E; i++)
	{

		if (pEprocessArea[i] == 0xad31bf)
		{
			EprocessInternalData.EprocessMitigationsOffset = i * sizeof(ULONG);
			bResult02 = TRUE;
			break;
		}
	}
	
	if (!bResult01)
		KDbgPrintString("Error in signatures offset searching");

	if (!bResult02)
		KDbgPrintString("Error in mitigations offset searching");

	return (bResult01 && bResult02);
}


//
// Enable vmwp.exe process protection
//

BOOLEAN VidEnableProcessProtection(PEPROCESS eProcess)
{
	if (!SearchEprocessOffsets(eProcess))
		return FALSE;
	
	PUHALF_PTR dwSignature = (PUHALF_PTR)((PCHAR)eProcess + EprocessInternalData.EprocessSignaturesOffset);
	PUHALF_PTR dwMitigation = (PUHALF_PTR)((PCHAR)eProcess + EprocessInternalData.EprocessMitigationsOffset);

	//PUHALF_PTR dwMitigation = (PUHALF_PTR)((PCHAR)eProcess + 0x820);
	//PUHALF_PTR dwSignature = (PUHALF_PTR)((PCHAR)eProcess + 0x6c8);
	*dwMitigation = EprocessInternalData.VmwpMitigationsOriginal;
	*dwSignature = EprocessInternalData.VmwpSignaturesOriginal;
	return TRUE;
}

//
// Disable vmwp.exe process protection
//


BOOLEAN VidDisableProcessProtection(PEPROCESS eProcess)
{
	if (!SearchEprocessOffsets(eProcess))
		return FALSE;

	PUHALF_PTR dwSignature = (PUHALF_PTR)((PCHAR)eProcess + EprocessInternalData.EprocessSignaturesOffset);
	PUHALF_PTR dwMitigation = (PUHALF_PTR)((PCHAR)eProcess + EprocessInternalData.EprocessMitigationsOffset);

	EprocessInternalData.VmwpMitigationsOriginal = *dwMitigation;
	EprocessInternalData.VmwpSignaturesOriginal = *dwSignature;
	*dwMitigation = 0;
	*dwSignature = 0;
	return TRUE;
}

//
// Usermode wrapper for VidDisableProcessProtection
//

BOOLEAN VidIOCTLDisableProcessProtection(PCHAR pBuffer, ULONG len)
{
	UNREFERENCED_PARAMETER(len);
	ULONG64 ProcessId = *(PULONG64)pBuffer;
	PEPROCESS ProcessHandle = NULL;
	NTSTATUS Status;
	BOOLEAN bResult;

	Status = PsLookupProcessByProcessId((HANDLE)ProcessId, &ProcessHandle);

	if (Status != STATUS_SUCCESS) {
		KDbgLog("Status of PsLookupProcessByProcessId", Status);
		return FALSE;
	}

	bResult = VidDisableProcessProtection(ProcessHandle);

	return bResult;
}

//
// Usermode wrapper for VidEnableProcessProtection
//

BOOLEAN VidIOCTLEnableProcessProtection(PCHAR pBuffer, ULONG len)
{
	UNREFERENCED_PARAMETER(len);
	ULONG64 ProcessId = *(PULONG64)pBuffer;
	PEPROCESS ProcessHandle = NULL;
	NTSTATUS Status;
	BOOLEAN bResult;

	Status = PsLookupProcessByProcessId((HANDLE)ProcessId, &ProcessHandle);

	if (Status != STATUS_SUCCESS) {
		KDbgLog("Status of PsLookupProcessByProcessId", Status);
		return FALSE;
	}

	bResult = VidDisableProcessProtection(ProcessHandle);

	return bResult;
}


//
//Get MBlock index in array for vid.dll!VidReadWriteMemoryBlockPageRange
//

MB_HANDLE VidGetMbBlockIndex(PVM_PROCESS_CONTEXT pPartitionHandle, PMEMORY_BLOCK MbBlock)
{
	PMEMORY_BLOCK_ARRAY objMBlockArray = NULL;
	PVOID objMBlockAddress = NULL;
	ULONG64 Index = 0;
	ULONG64* pBlocks;

	DbgBreakPoint();

	objMBlockArray = pPartitionHandle->ArrayOfMblocks;

	if (objMBlockArray == 0) {

		objMBlockArray = pPartitionHandle->ArrayOfMblocks20H1;

		if (objMBlockArray == 0) {
			KDbgPrintString("Something wrong with objMBlockArray offset");
			return NULL;
		}
	}

	for (ULONG64 i = 1; i < objMBlockArray->Count; i++) // start from 2nd element, 1st element is count 
	{
		
		pBlocks = (PULONG64)objMBlockArray;
		objMBlockAddress = (PVOID)pBlocks[i];
		//objMBlockAddress = (PVOID) *((PULONG64)objMBlockArray + i);
		
		if (objMBlockAddress == (PVOID) MbBlock)
		{
			Index = i;
			return (MB_HANDLE)Index;
		}
	}
	return 0;
}

//
// Find Gpar block from PARTITION_HANDLE structure for specifica GPA Page
//

PGPAR_OBJECT VidGetGparObjectForGpa(PVM_PROCESS_CONTEXT pPartitionHandle, PVM_LAYOUT pLayout, UINT64 GPA)
{

	UINT32 Index = 0;
	PGPAR_BLOCK_HANDLE pGparBlockHandle;
	PUINT64 pGparArray;
	UINT64 uElement = 0;

	PGPAR_OBJECT objGpar = NULL;

	//
	// The GPAR handle sits at a discovered offset in the context; its GparArray is at handle+0x8
	// (confirmed) and the element count at a discovered offset (0x14 or 0x18) inside the handle.
	//

	pGparBlockHandle = (PGPAR_BLOCK_HANDLE)*(PVOID*)((PCHAR)pPartitionHandle + pLayout->GparHandleOffset);

	if (pGparBlockHandle == 0)
	{
		KDbgPrintString("\tSomething wrong with offset of GparBlockHandle");
		return NULL;
	}

	Index = *(PUINT32)((PCHAR)pGparBlockHandle + pLayout->GparCountOffset);
	pGparArray = (PUINT64)pGparBlockHandle->GparArray;

	if (pGparArray == 0)
	{
		KDbgPrintString("\tSomething wrong with offset of Gpar array");
		return NULL;
	}

	for (LONG i = Index - 1; i >= 0; i--)
	{
		uElement = *((PUINT64)pGparArray + i);
		if (uElement != 0)
		{
			objGpar = (PGPAR_OBJECT)uElement;

			if ((GPA >= objGpar->GpaIndexStart) && (GPA <= objGpar->GpaIndexEnd))
			{
				return objGpar;
			}
		}
	} // end for

	if (!objGpar)
		KDbgPrintString("\tGpar element wasn't found");

	return objGpar;
}

//
// Read memory block from Hyper-V container
//

BOOLEAN VidGetContainerMemoryBlock(PVM_PROCESS_CONTEXT pPartitionHandle, PVM_LAYOUT pLayout, PCHAR pBuffer, ULONG len, ULONG64 GPA)
{
	PGPAR_OBJECT objGpar = NULL;
	BOOLEAN Ret = FALSE;
	ULONG64 SourceAddress;
	ULONG64 SomeGpa, VmmMemGpa;
	ULONG64 uBlocks, uRemainBytes, i;

	//DbgBreakPoint();

	if (g_vmmemHandle == NULL) {
		Ret = FALSE;
	}

	uBlocks = len / PAGE_SIZE;
	uRemainBytes = len % PAGE_SIZE;

	for (i = 0; i < uBlocks; i++)
	{

		objGpar = VidGetGparObjectForGpa(pPartitionHandle, pLayout, GPA+i);

		if (objGpar == NULL) {
			return FALSE;
		}

		//
		// SomeGpaOffset / VmmMemGpaOffset are two fields that sit right after the objMBlock
		// pointer inside the GPAR object, so they move with it - read them at the discovered
		// offset, not a fixed one.
		//

		SomeGpa = *(PULONG64)((PCHAR)objGpar + pLayout->SomeGpaOffset);
		VmmMemGpa = *(PULONG64)((PCHAR)objGpar + pLayout->VmmMemGpaOffset);
		SourceAddress = (GPA + i - objGpar->GpaIndexStart - SomeGpa) * PAGE_SIZE + VmmMemGpa;
		if (SomeGpa != 0) {
			KDbgLog16("   objGpar->SomeGpaOffset", SomeGpa);
		}
		__try {
			KeReadProcessMemory((PEPROCESS)g_vmmemHandle, (PVOID)SourceAddress, pBuffer + i * PAGE_SIZE, PAGE_SIZE);
			Ret = TRUE;
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			KDbgLog("   KeReadProcessMemory excpetion", GetExceptionCode());
			Ret = FALSE;
		}
	}

	if (uRemainBytes > 0) {

		objGpar = VidGetGparObjectForGpa(pPartitionHandle, pLayout, GPA + uBlocks);

		if (objGpar == NULL) {
			return FALSE;
		}

		SomeGpa = *(PULONG64)((PCHAR)objGpar + pLayout->SomeGpaOffset);
		VmmMemGpa = *(PULONG64)((PCHAR)objGpar + pLayout->VmmMemGpaOffset);
		SourceAddress = (GPA + uBlocks - objGpar->GpaIndexStart - SomeGpa) * PAGE_SIZE + VmmMemGpa;
		if (SomeGpa != 0) {
			//KDbgLog16("   pGparElement->SomeGpaOffset", SomeGpa);
		}

		__try 
		{
			KeReadProcessMemory((PEPROCESS)g_vmmemHandle, (PVOID)SourceAddress, pBuffer + uBlocks * PAGE_SIZE, uRemainBytes);
			Ret = TRUE;
		}

		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			KDbgLog("   KeReadProcessMemory. Exception", GetExceptionCode());
			Ret = FALSE;
		}
	}
	
	return Ret;
}


//
//Read memory block from FULL VM
//

//
// Copy len bytes of guest memory starting at page GPA into pBuffer. Pages are resolved one
// GPA range at a time and every host-backed run inside the block is mapped with a single
// MDL, copied and unmapped, so a block costs a handful of kernel mappings instead of one per
// page. A page the host does not back (a hole between ranges, a vmwp.exe descriptor, a block
// with no host-PFN array, a frame past host RAM) is left zero and the walk goes on, the way
// a physical dump expects. The request fails only when no page of it was backed. The first
// unbacked page is noted for the layout query.
//

#define VID_CLASSIC_RUN_MAX_PAGES (HVMM_MAP_GPA_MAX_LENGTH / PAGE_SIZE)

static BOOLEAN VidCopyHostRun(PPFN_NUMBER Pfns, ULONG Pages, PCHAR Dest)
{
	PMDL pMDL;
	PVOID Source = NULL;
	BOOLEAN Ok = FALSE;

	pMDL = IoAllocateMdl(NULL, Pages * PAGE_SIZE, FALSE, FALSE, NULL);
	if (pMDL == NULL) {
		g_LastReadFail.Step = HVMM_READ_FAIL_MDL;
		return FALSE;
	}
	RtlCopyMemory(MmGetMdlPfnArray(pMDL), Pfns, Pages * sizeof(PFN_NUMBER));
	pMDL->MdlFlags |= MDL_PAGES_LOCKED;

	__try {
		Source = MmMapLockedPagesSpecifyCache(pMDL, KernelMode, MmCached, NULL, FALSE, NormalPagePriority | MdlMappingNoWrite);
		if (Source != NULL) {
			RtlCopyMemory(Dest, Source, Pages * PAGE_SIZE);
			MmUnmapLockedPages(Source, pMDL);
			Ok = TRUE;
		}
		else {
			g_LastReadFail.Step = HVMM_READ_FAIL_MAP;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		KDbgLog("VidCopyHostRun: copy faulted", GetExceptionCode());
		g_LastReadFail.Step = HVMM_READ_FAIL_EXCEPTION;
	}

	pMDL->MdlFlags &= ~MDL_PAGES_LOCKED;
	IoFreeMdl(pMDL);
	return Ok;
}

BOOLEAN VidGetFullVmMemoryBlock(PVM_PROCESS_CONTEXT pPartitionHandle, PVM_LAYOUT pLayout, PCHAR pBuffer, ULONG len, ULONG64 GPA)
{
	PPFN_NUMBER Pfns;
	ULONG64 TotalPages = len / PAGE_SIZE;
	ULONG64 i = 0;
	ULONG64 HighestHostPfn;
	ULONG Backed = 0, Unbacked = 0;

	if ((len % PAGE_SIZE) != 0) {
		KDbgPrintString("Buffer Length must be paged size alignment");
		g_LastReadFail.Step = HVMM_READ_FAIL_LENGTH;
		return FALSE;
	}

	HighestHostPfn = VidHighestHostPfn();
	Pfns = (PPFN_NUMBER)HvmmPoolAlloc(VID_CLASSIC_RUN_MAX_PAGES * sizeof(PFN_NUMBER));
	if (Pfns == NULL) {
		g_LastReadFail.Step = HVMM_READ_FAIL_MDL;
		return FALSE;
	}

	while (i < TotalPages) {
		ULONG64 Page = GPA + i;
		ULONG64 RangeEnd;
		PGPAR_OBJECT objGpar;
		PMEMORY_BLOCK objMBlock = NULL;
		PULONG64 pGuestGpaArray = NULL;
		ULONG Step = 0;
		ULONG Run = 0;

		objGpar = VidGetGparObjectForGpa(pPartitionHandle, pLayout, Page);

		if (objGpar == NULL || Page < objGpar->GpaIndexStart || Page > objGpar->GpaIndexEnd) {
			Step = HVMM_READ_FAIL_NO_GPAR;
		}
		else if (objGpar->GpaIndexStart == objGpar->GpaIndexEnd) {
			Step = HVMM_READ_FAIL_VMWP_RANGE;
		}
		else {
			objMBlock = (PMEMORY_BLOCK)*(PVOID*)((PCHAR)objGpar + pLayout->ObjMblockOffset);
			if (objMBlock == NULL) {
				Step = HVMM_READ_FAIL_NO_MBLOCK;
			}
			else {
				pGuestGpaArray = (PULONG64)*(PVOID*)((PCHAR)objMBlock + pLayout->GuestGpaArrayOffset);
				if (pGuestGpaArray == NULL) {
					Step = HVMM_READ_FAIL_NO_ARRAY;
				}
			}
		}

		if (Step == 0) {
			RangeEnd = objGpar->GpaIndexEnd;
			if (RangeEnd > GPA + TotalPages - 1) {
				RangeEnd = GPA + TotalPages - 1;
			}
			while (Page + Run <= RangeEnd && Run < VID_CLASSIC_RUN_MAX_PAGES) {
				ULONG64 HostPfn = *(PULONG)((PCHAR)pGuestGpaArray + 0x10 * (Page + Run - objGpar->GpaIndexStart));
				if (HostPfn == 0 || HostPfn > HighestHostPfn) {
					break;
				}
				Pfns[Run] = (PFN_NUMBER)HostPfn;
				Run++;
			}
			if (Run == 0) {
				Step = HVMM_READ_FAIL_NO_GPAR;
			}
		}

		if (Step != 0) {
			if (Unbacked == 0) {
				g_LastReadFail.Step = Step;
				g_LastReadFail.FailPage = Page;
			}
			Unbacked++;
			i++;
			continue;
		}

		if (!VidCopyHostRun(Pfns, Run, pBuffer + i * PAGE_SIZE)) {
			g_LastReadFail.FailPage = Page;
			HvmmPoolFree(Pfns);
			return FALSE;
		}
		Backed += Run;
		i += Run;
	}

	HvmmPoolFree(Pfns);

	g_LastReadFail.Backed = Backed;
	g_LastReadFail.Unbacked = Unbacked;

	if (Backed == 0) {
		if (g_LastReadFail.Step == 0) {
			g_LastReadFail.Step = HVMM_READ_FAIL_ALL_UNBACKED;
		}
		return FALSE;
	}

	return TRUE;
}

//
//	Get HANLE of vmmem process, which is child to corresponding vmwp.exe process
//


PVOID VidFindVmmemHandle(PVM_PROCESS_CONTEXT pHandle)
{
	ULONG i = 0;
	PULONG pPartitiionArea = (PULONG) pHandle;
	PVOID pResult = NULL;

	//DbgBreakPoint();

	for (i = 0x3C00 / sizeof(ULONG); i < 0x5000; i += 0x1) 
	{
		
		if (pPartitiionArea[i] == PRCS_TAG) 
		{
			pResult = (PVOID)*(PULONG64)((PUCHAR)&pPartitiionArea[i]+0x18);
			break;
		}
	}
	return pResult;
}

//
//Read guest VM memory using raw access to unmapped physical memory in host OS
//

BOOLEAN VidInternalReadMemory(PCHAR pBuffer, ULONG len)
{
	GPA_INFO GpaInfo;
	NTSTATUS Status;
	PFILE_OBJECT objVmPartition;
	PVM_PROCESS_CONTEXT pPartitionHandle;
	VM_LAYOUT Layout;
	//PGPAR_ELEMENT pGparElement = NULL;
	UINT64 GPA;
	BOOLEAN Ret = FALSE;

	//ULONG64 SourceAddress;

	//DbgBreakPoint();

	RtlCopyMemory(&GpaInfo, pBuffer, sizeof(GpaInfo));

	GPA = GpaInfo.StartPage / PAGE_SIZE;
	//KDbgLog16("GPA = ", GPA);

	RtlZeroMemory(&g_LastReadFail, sizeof(g_LastReadFail));
	g_LastReadFail.Handle = (UINT64)GpaInfo.PartitionHandle;
	g_LastReadFail.Gpa = GpaInfo.StartPage;
	g_LastReadFail.Length = len;
	RtlCopyMemory(g_LastReadFail.Raw, &GpaInfo, sizeof(GpaInfo));
	if (len >= sizeof(g_LastReadFail.Raw)) {
		RtlCopyMemory(g_LastReadFail.Raw, pBuffer, sizeof(g_LastReadFail.Raw));
	}
	memset(pBuffer, 0, len);

	Status = ObReferenceObjectByHandle(GpaInfo.PartitionHandle,
		READ_CONTROL,
		*IoFileObjectType,
		KernelMode,
		&objVmPartition,
		NULL);

	if (!NT_SUCCESS(Status))
	{
		KDbgLog("VidInternalReadMemory.ObReferenceObjectByHandle failed. Status ", Status);
		KDbgLog16("GpaInfo.PartitionHandle ", (ULONG64)GpaInfo.PartitionHandle);
		g_LastReadFail.Step = HVMM_READ_FAIL_HANDLE;
		return FALSE;
	}

	if (objVmPartition->FsContext == NULL)
	{
		g_LastReadFail.Step = HVMM_READ_FAIL_NO_CONTEXT;
	}

	if (objVmPartition->FsContext != NULL)
	{
		pPartitionHandle = (PVM_PROCESS_CONTEXT)((PCHAR)objVmPartition->FsContext - 1);

		//
		// Work out the partition layout by signature scan. A full VM is host-page backed and
		// takes the MDL-mapping read path; anything else (a container / exo partition) keeps the
		// old vmmem read path, gated on the fixed VmType field as before.
		//

		VidResolveLayout(pPartitionHandle, &Layout);

		if (Layout.IsFullVm)
		{
			Ret = VidGetFullVmMemoryBlock(pPartitionHandle, &Layout, pBuffer, len, GPA);
		}
		else
		{
			switch (pPartitionHandle->VmType)
			{
			case VidVmTypeDockerHyperVContainerUserName:
			case VidVmTypeDockerHyperVContainerGUID:
			case VidVmTypeContainer:

				Ret = VidGetContainerMemoryBlock(pPartitionHandle, &Layout, pBuffer, len, GPA);

				break;

			case VidVmTypeFullWin10VM:
			case VidVmTypeFullWinSrvVMSecure:
			case VidVmTypeFullWinSrvVM:

				Ret = VidGetFullVmMemoryBlock(pPartitionHandle, &Layout, pBuffer, len, GPA);
				break;

			default:
				g_LastReadFail.Step = HVMM_READ_FAIL_NOT_FULL_VM;
				break;
			}
		}
	}

	ObDereferenceObject(objVmPartition);
	return Ret;
}


//
// Answer the mapping handshake. Clients send it before any map request, so a map request never
// reaches an hvmm.sys build that gives the same IOCTL code another meaning.
//

BOOLEAN VidQueryMappingSupport(PCHAR pBuffer, ULONG inLen, ULONG outLen, PULONG pBytesReturned)
{
	MAPPING_QUERY_INPUT Input;
	MAPPING_QUERY_OUTPUT Output;

	*pBytesReturned = 0;

	if (inLen != sizeof(MAPPING_QUERY_INPUT) || outLen < sizeof(MAPPING_QUERY_OUTPUT)) {
		return FALSE;
	}

	RtlCopyMemory(&Input, pBuffer, sizeof(Input));
	if (Input.Magic != HVMM_MAPPING_QUERY_MAGIC) {
		return FALSE;
	}

	Output.Signature = HVMM_MAPPING_SIGNATURE;
	Output.Version = HVMM_MAPPING_VERSION;
	Output.MaxMapLength = HVMM_MAP_GPA_MAX_LENGTH;
	RtlCopyMemory(pBuffer, &Output, sizeof(Output));
	*pBytesReturned = sizeof(Output);
	return TRUE;
}

//
// Highest page frame of host RAM, from the memory manager's own range table. Learned once.
// A guest-to-host entry that points past it is not RAM (device memory, or garbage from a
// layout we misread) and is never handed to the memory manager.
//

static ULONG64 g_HighestHostPfn = 0;

static ULONG64 VidHighestHostPfn(VOID)
{
	PPHYSICAL_MEMORY_RANGE Ranges;
	ULONG i;
	ULONG64 Highest = 0;

	if (g_HighestHostPfn != 0) {
		return g_HighestHostPfn;
	}

	Ranges = MmGetPhysicalMemoryRanges();
	if (Ranges == NULL) {
		return 0;
	}
	for (i = 0; Ranges[i].BaseAddress.QuadPart != 0 || Ranges[i].NumberOfBytes.QuadPart != 0; i++) {
		ULONG64 End = ((ULONG64)Ranges[i].BaseAddress.QuadPart + (ULONG64)Ranges[i].NumberOfBytes.QuadPart - 1) / PAGE_SIZE;
		if (End > Highest) {
			Highest = End;
		}
	}
	ExFreePool(Ranges);

	g_HighestHostPfn = Highest;
	return Highest;
}

//
// Tear one mapping down and free everything it holds. A user mapping can only be unmapped
// from its own address space, so for a mapping made by another process (the handle was
// inherited or duplicated) attach to that process first. If that process has already
// exited, its address space took the mapping with it and only the MDL is left to free.
// The mapping holds a reference on its owner, so the pointer is never a reused one.
//

static VOID VidReleaseGpaMapping(PHVMM_GPA_MAPPING pMapping)
{
	PEPROCESS Owner = pMapping->Process;
	BOOLEAN Attached = FALSE;
	KAPC_STATE ApcState;

	if (Owner != PsGetThreadProcess(PsGetCurrentThread())) {
		if (PsGetProcessExitStatus(Owner) != STATUS_PENDING) {
			Owner = NULL;
		}
		else {
			KeStackAttachProcess(Owner, &ApcState);
			Attached = TRUE;
		}
	}

	if (Owner != NULL) {
		__try { MmUnmapLockedPages(pMapping->UserVa, pMapping->Mdl); } __except (EXCEPTION_EXECUTE_HANDLER) {}
	}
	if (Attached) {
		KeUnstackDetachProcess(&ApcState);
	}

	pMapping->Mdl->MdlFlags &= ~MDL_PAGES_LOCKED;
	IoFreeMdl(pMapping->Mdl);
	ObDereferenceObject(pMapping->Process);
	HvmmPoolFree(pMapping);
}

//
// Report what the layout scan found for one partition. Diagnostic only: nothing is mapped.
//

BOOLEAN VidQueryPartitionLayout(PCHAR pBuffer, ULONG inLen, ULONG outLen, PULONG pBytesReturned)
{
	PARTITION_LAYOUT_QUERY_INPUT Input;
	PARTITION_LAYOUT_QUERY_OUTPUT Output;
	NTSTATUS Status;
	PFILE_OBJECT objVmPartition = NULL;
	PCHAR ctx;
	VM_LAYOUT Layout;

	*pBytesReturned = 0;

	if (inLen < sizeof(PARTITION_LAYOUT_QUERY_INPUT) || outLen < sizeof(PARTITION_LAYOUT_QUERY_OUTPUT)) {
		return FALSE;
	}

	RtlCopyMemory(&Input, pBuffer, sizeof(Input));
	RtlZeroMemory(&Output, sizeof(Output));

	if (Input.PartitionHandle == NULL) {
		Output = g_LastEnumLayout;
		Output.Source = 1;
		HvmmCopyIoctlStats(Output.IoctlStats, HVMM_IOCTL_STAT_SLOTS);
		Output.LastReadFail = g_LastReadFail;
		RtlCopyMemory(pBuffer, &Output, sizeof(Output));
		*pBytesReturned = sizeof(Output);
		return TRUE;
	}

	Status = ObReferenceObjectByHandle(Input.PartitionHandle,
		READ_CONTROL,
		*IoFileObjectType,
		KernelMode,
		&objVmPartition,
		NULL);

	if (!NT_SUCCESS(Status)) {
		KDbgLog("VidQueryPartitionLayout.ObReferenceObjectByHandle failed. Status ", Status);
		Output.Source = 2;
		Output.PartitionId = (UINT64)(LONG)Status;
		HvmmCopyIoctlStats(Output.IoctlStats, HVMM_IOCTL_STAT_SLOTS);
		Output.LastReadFail = g_LastReadFail;
		RtlCopyMemory(pBuffer, &Output, sizeof(Output));
		*pBytesReturned = sizeof(Output);
		return TRUE;
	}

	if (objVmPartition->FsContext == NULL) {
		ObDereferenceObject(objVmPartition);
		return FALSE;
	}

	ctx = (PCHAR)objVmPartition->FsContext - 1;

	VidResolveLayout((PVM_PROCESS_CONTEXT)ctx, &Layout);
	VidFillLayoutReport(ctx, &Layout, &Output);
	HvmmCopyIoctlStats(Output.IoctlStats, HVMM_IOCTL_STAT_SLOTS);
	Output.LastReadFail = g_LastReadFail;

	ObDereferenceObject(objVmPartition);

	RtlCopyMemory(pBuffer, &Output, sizeof(Output));
	*pBytesReturned = sizeof(Output);
	return TRUE;
}

//
// Map a guest physical range into the calling process, read-only, and keep it mapped.
//
// Resolves each host page frame exactly as VidGetFullVmMemoryBlock does, but builds one
// MDL for the whole range and maps it to UserMode with MmMapLockedPagesSpecifyCache. The
// mapping outlives the IOCTL; reads then cost a pointer dereference in user mode instead of
// a round trip through the kernel. Only full VMs are backed by host pages this way.
//

BOOLEAN VidMapGpaRange(PFILE_OBJECT FileObject, PCHAR pBuffer, ULONG inLen, ULONG outLen, PULONG pBytesReturned)
{
	MAP_GPA_RANGE_INPUT Input;
	MAP_GPA_RANGE_OUTPUT Output;
	NTSTATUS Status;
	PFILE_OBJECT objVmPartition = NULL;
	PVM_PROCESS_CONTEXT pPartitionHandle;
	VM_LAYOUT Layout;
	PHVMM_FILE_CONTEXT pFileContext;
	PHVMM_GPA_MAPPING pMapping = NULL;
	PMDL pMDL = NULL;
	PPFN_NUMBER pPfnArray;
	PVOID UserVa = NULL;
	UINT64 GpaStartPage, TotalPages, MappedPages, i;
	UINT64 MappedBytes;
	UINT64 HighestHostPfn;
	KIRQL OldIrql;

	*pBytesReturned = 0;

	if (inLen < sizeof(MAP_GPA_RANGE_INPUT) || outLen < sizeof(MAP_GPA_RANGE_OUTPUT)) {
		return FALSE;
	}

	if (FileObject == NULL || FileObject->FsContext2 == NULL) {
		KDbgPrintString("VidMapGpaRange: no file context");
		return FALSE;
	}
	pFileContext = (PHVMM_FILE_CONTEXT)FileObject->FsContext2;

	RtlCopyMemory(&Input, pBuffer, sizeof(Input));

	if (Input.Length == 0 ||
		(Input.GpaStart & (PAGE_SIZE - 1)) != 0 ||
		(Input.Length & (PAGE_SIZE - 1)) != 0) {
		KDbgPrintString("VidMapGpaRange: range not page aligned");
		return FALSE;
	}

	if (Input.Length > HVMM_MAP_GPA_MAX_LENGTH) {
		Input.Length = HVMM_MAP_GPA_MAX_LENGTH;
	}

	GpaStartPage = Input.GpaStart / PAGE_SIZE;
	TotalPages = Input.Length / PAGE_SIZE;

	HighestHostPfn = VidHighestHostPfn();
	if (HighestHostPfn == 0) {
		KDbgPrintString("VidMapGpaRange: host memory ranges unavailable");
		return FALSE;
	}

	//
	// KernelMode on purpose, like every other partition lookup in this driver: hvlib hands
	// over a handle that lives in the VM worker's table, and the PsGetCurrentProcess patch
	// makes that the table the lookup sees. A UserMode lookup answers STATUS_INVALID_HANDLE.
	//

	Status = ObReferenceObjectByHandle(Input.PartitionHandle,
		READ_CONTROL,
		*IoFileObjectType,
		KernelMode,
		&objVmPartition,
		NULL);

	if (!NT_SUCCESS(Status)) {
		KDbgLog("VidMapGpaRange.ObReferenceObjectByHandle failed. Status ", Status);
		return FALSE;
	}

	if (objVmPartition->FsContext == NULL) {
		ObDereferenceObject(objVmPartition);
		return FALSE;
	}

	pPartitionHandle = (PVM_PROCESS_CONTEXT)((PCHAR)objVmPartition->FsContext - 1);

	//
	// Only a full, host-page-backed VM can be mapped this way. The layout resolver decides that
	// from the partition's "Prtn"/"Gpar"/"Mb  " shape (which survives Windows updates), not from
	// the VmType field at a fixed offset (which moves), and hands back the offsets we walk below.
	//

	VidResolveLayout(pPartitionHandle, &Layout);
	if (!Layout.IsFullVm) {
		KDbgPrintString("VidMapGpaRange: partition is not a full VM");
		ObDereferenceObject(objVmPartition);
		return FALSE;
	}

	//
	// One MDL for the full requested range. We fill its PFN array with the host frames of the
	// longest backed prefix, then shrink ByteCount to that prefix before mapping.
	//

	pMDL = IoAllocateMdl(NULL, (ULONG)Input.Length, FALSE, FALSE, NULL);
	if (pMDL == NULL) {
		KDbgPrintString("VidMapGpaRange: IoAllocateMdl failed");
		ObDereferenceObject(objVmPartition);
		return FALSE;
	}
	pPfnArray = MmGetMdlPfnArray(pMDL);

	MappedPages = 0;
	i = 0;
	__try {
		while (i < TotalPages) {
			UINT64 GpaPage = GpaStartPage + i;
			UINT64 RangeEnd;
			PGPAR_OBJECT objGpar;
			PMEMORY_BLOCK objMBlock;
			PULONG64 pGuestGpaArray;
			BOOLEAN Unbacked = FALSE;

			//
			// One GPAR lookup per range, not per page: the lookup walks the whole GPAR array.
			// It also does not return NULL for a GPA outside every range but the last range it
			// looked at, so the bounds are checked here. A descriptor whose start equals its
			// end is a vmwp.exe range, not host backed.
			//

			objGpar = VidGetGparObjectForGpa(pPartitionHandle, &Layout, GpaPage);
			if (objGpar == NULL ||
				GpaPage < objGpar->GpaIndexStart ||
				GpaPage > objGpar->GpaIndexEnd ||
				objGpar->GpaIndexStart == objGpar->GpaIndexEnd) {
				break;
			}

			//
			// objMBlock and the host-PFN array live at discovered offsets (see VidResolveLayout).
			//

			objMBlock = (PMEMORY_BLOCK)*(PVOID*)((PCHAR)objGpar + Layout.ObjMblockOffset);
			if (objMBlock == NULL) {
				break;
			}
			pGuestGpaArray = (PULONG64)*(PVOID*)((PCHAR)objMBlock + Layout.GuestGpaArrayOffset);
			if (pGuestGpaArray == NULL) {
				break;
			}

			RangeEnd = objGpar->GpaIndexEnd;
			if (RangeEnd > GpaStartPage + TotalPages - 1) {
				RangeEnd = GpaStartPage + TotalPages - 1;
			}

			//
			// A frame past host RAM is treated like an unbacked page: it ends the prefix.
			//

			for (; GpaPage <= RangeEnd; GpaPage++, i++) {
				ULONG64 HostPfn = *(PULONG)((PCHAR)pGuestGpaArray + 0x10 * (GpaPage - objGpar->GpaIndexStart));
				if (HostPfn == 0 || HostPfn > HighestHostPfn) {
					Unbacked = TRUE;
					break;
				}
				pPfnArray[i] = (PFN_NUMBER)HostPfn;
				MappedPages++;
			}
			if (Unbacked) {
				break;
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		KDbgLog("VidMapGpaRange: exception resolving host frames", GetExceptionCode());
	}

	if (MappedPages == 0) {
		IoFreeMdl(pMDL);
		ObDereferenceObject(objVmPartition);
		return FALSE;
	}

	MappedBytes = MappedPages * PAGE_SIZE;
	pMDL->ByteCount = (ULONG)MappedBytes;
	pMDL->MdlFlags |= MDL_PAGES_LOCKED;

	__try {
		UserVa = MmMapLockedPagesSpecifyCache(pMDL, UserMode, MmCached, NULL, FALSE, NormalPagePriority | MdlMappingNoWrite | MdlMappingNoExecute);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		KDbgLog("VidMapGpaRange: MmMapLockedPagesSpecifyCache raised", GetExceptionCode());
		UserVa = NULL;
	}

	if (UserVa == NULL) {
		pMDL->MdlFlags &= ~MDL_PAGES_LOCKED;
		IoFreeMdl(pMDL);
		ObDereferenceObject(objVmPartition);
		return FALSE;
	}

	pMapping = (PHVMM_GPA_MAPPING)HvmmPoolAlloc(sizeof(HVMM_GPA_MAPPING));
	if (pMapping == NULL) {
		__try { MmUnmapLockedPages(UserVa, pMDL); } __except (EXCEPTION_EXECUTE_HANDLER) {}
		pMDL->MdlFlags &= ~MDL_PAGES_LOCKED;
		IoFreeMdl(pMDL);
		ObDereferenceObject(objVmPartition);
		return FALSE;
	}

	pMapping->Mdl = pMDL;
	pMapping->UserVa = UserVa;
	pMapping->Process = PsGetThreadProcess(PsGetCurrentThread());
	pMapping->MappedBytes = MappedBytes;
	ObReferenceObject(pMapping->Process);

	KeAcquireSpinLock(&pFileContext->Lock, &OldIrql);
	if (pFileContext->MappingCount >= HVMM_MAX_MAPPINGS_PER_HANDLE) {
		KeReleaseSpinLock(&pFileContext->Lock, OldIrql);
		KDbgPrintString("VidMapGpaRange: too many live mappings on this handle");
		VidReleaseGpaMapping(pMapping);
		ObDereferenceObject(objVmPartition);
		return FALSE;
	}
	pFileContext->MappingCount++;
	InsertTailList(&pFileContext->MappingList, &pMapping->Link);
	KeReleaseSpinLock(&pFileContext->Lock, OldIrql);

	Output.UserVa = UserVa;
	Output.MappedBytes = MappedBytes;
	RtlCopyMemory(pBuffer, &Output, sizeof(Output));
	*pBytesReturned = sizeof(Output);

	KDbgLog16("VidMapGpaRange mapped bytes", MappedBytes);

	ObDereferenceObject(objVmPartition);
	return TRUE;
}

//
// Unmap one range previously returned by VidMapGpaRange on this handle.
//

BOOLEAN VidUnmapGpaRange(PFILE_OBJECT FileObject, PCHAR pBuffer, ULONG inLen)
{
	UNMAP_GPA_RANGE_INPUT Input;
	PHVMM_FILE_CONTEXT pFileContext;
	PHVMM_GPA_MAPPING pMapping = NULL;
	PHVMM_GPA_MAPPING pFound = NULL;
	PLIST_ENTRY pEntry;
	KIRQL OldIrql;

	if (inLen < sizeof(UNMAP_GPA_RANGE_INPUT)) {
		return FALSE;
	}
	if (FileObject == NULL || FileObject->FsContext2 == NULL) {
		return FALSE;
	}
	pFileContext = (PHVMM_FILE_CONTEXT)FileObject->FsContext2;

	RtlCopyMemory(&Input, pBuffer, sizeof(Input));

	KeAcquireSpinLock(&pFileContext->Lock, &OldIrql);
	for (pEntry = pFileContext->MappingList.Flink;
		pEntry != &pFileContext->MappingList;
		pEntry = pEntry->Flink) {
		pMapping = CONTAINING_RECORD(pEntry, HVMM_GPA_MAPPING, Link);
		if (pMapping->UserVa == Input.UserVa) {
			RemoveEntryList(pEntry);
			pFileContext->MappingCount--;
			pFound = pMapping;
			break;
		}
	}
	KeReleaseSpinLock(&pFileContext->Lock, OldIrql);

	if (pFound == NULL) {
		return FALSE;
	}

	VidReleaseGpaMapping(pFound);
	return TRUE;
}

//
// Unmap every range still live on a handle. Called from IRP_MJ_CLEANUP, which the kernel
// delivers in the owning process context (on an explicit close or on process exit).
//

VOID VidCleanupGpaMappings(PFILE_OBJECT FileObject)
{
	PHVMM_FILE_CONTEXT pFileContext;
	PLIST_ENTRY pEntry;
	KIRQL OldIrql;
	LIST_ENTRY LocalList;

	if (FileObject == NULL || FileObject->FsContext2 == NULL) {
		return;
	}
	pFileContext = (PHVMM_FILE_CONTEXT)FileObject->FsContext2;

	//
	// Detach the whole list under the lock, then unmap outside it: MmUnmapLockedPages runs at
	// PASSIVE_LEVEL and must not be called while holding a spin lock.
	//

	InitializeListHead(&LocalList);

	KeAcquireSpinLock(&pFileContext->Lock, &OldIrql);
	while (!IsListEmpty(&pFileContext->MappingList)) {
		pEntry = RemoveHeadList(&pFileContext->MappingList);
		InsertTailList(&LocalList, pEntry);
	}
	pFileContext->MappingCount = 0;
	KeReleaseSpinLock(&pFileContext->Lock, OldIrql);

	while (!IsListEmpty(&LocalList)) {
		pEntry = RemoveHeadList(&LocalList);
		VidReleaseGpaMapping(CONTAINING_RECORD(pEntry, HVMM_GPA_MAPPING, Link));
	}
}

//
//Get Mblock structure
//

BOOLEAN VidGetMBlockInfo(PCHAR pBuffer, ULONG len)
{
	UNREFERENCED_PARAMETER(len);
	PFILE_OBJECT objVmPartition;
    NTSTATUS Status;
    PPARTITION_INFO pPartitionInfo;
    PVM_PROCESS_CONTEXT pPartitionHandle;
	VM_LAYOUT Layout;
	ULONG64 i;
	PMEMORY_BLOCK_ARRAY objMBlockArray;
	PMEMORY_BLOCK objMBlock;

    pPartitionInfo = (PPARTITION_INFO)pBuffer;

    Status = ObReferenceObjectByHandle(pPartitionInfo->PartitionHandle,
        READ_CONTROL,
        *IoFileObjectType,
        KernelMode,
        &objVmPartition,
        NULL);

    if (!NT_SUCCESS(Status))
    {
        KDbgLog("VidGetMBlockInfo.ObReferenceObjectByHandle failed. Status ", Status);
        return FALSE;
    }
    if (objVmPartition->FsContext != NULL)
    {
        pPartitionHandle = (PVM_PROCESS_CONTEXT)((PCHAR)objVmPartition->FsContext - 1);

		VidResolveLayout(pPartitionHandle, &Layout);
		objMBlockArray = (PMEMORY_BLOCK_ARRAY)*(PVOID*)((PCHAR)pPartitionHandle + Layout.MblockArrayOffset);

		if (objMBlockArray == 0 || *(PULONG)objMBlockArray == 0) {
			KDbgPrintString("Something wrong with objMBlockArray offset");
			ObDereferenceObject(objVmPartition);
			return FALSE;
		}

		for (i = 1; i < objMBlockArray->Count; i++) // start from 2nd element
		{
			objMBlock = (PMEMORY_BLOCK) *((PULONG64)objMBlockArray + i);
			KDbgLog16("MBlock.BitMapSize ", objMBlock->BitMapSize01);
		}
    }

	ObDereferenceObject(objVmPartition);
	return FALSE;
}


BOOLEAN VidQueryInformation(PCHAR pBuffer, ULONG len)
{
    PPARTITION_INFO pPartitionInfo;
    BOOLEAN bRet = FALSE;

    pPartitionInfo = (PPARTITION_INFO)pBuffer;

	//DbgBreakPoint();

    switch (pPartitionInfo->VidInformationClass)
    {
    case VidMbBlockInfo:
        {
            bRet = VidGetMBlockInfo(pBuffer, len);
        }
        default:
            break;
    }

    return bRet;
}

//
//Get information about GPAR block
//

BOOLEAN VidGetGparBlockInfoFromGPA(PCHAR pBuffer, ULONG len)
{
	UNREFERENCED_PARAMETER(len);
	NTSTATUS Status;
    PFILE_OBJECT objVmPartition;

	PGPAR_BLOCK_INFO pGparBlockInfo;
    PVM_PROCESS_CONTEXT pPartitionHandle;
    PGPAR_BLOCK_HANDLE pGparBlockHandle;
    PGPAR_OBJECT pGparElement;
    PMEMORY_BLOCK objMBlock;
    VM_LAYOUT Layout;
    UINT64 GPA;
    BOOLEAN Ret = FALSE;

    UINT32 Index = 0;

    pGparBlockInfo = (PGPAR_BLOCK_INFO)pBuffer;
    GPA = pGparBlockInfo->GPA;

    //KDbgLog16("hPartitionDeviceHandle: ", pGparBlockInfo->PartitionHandle);
    //KDbgLog16("GPA: ", pGparBlockInfo->GPA);
    Status = ObReferenceObjectByHandle(pGparBlockInfo->PartitionHandle,
        READ_CONTROL,
        *IoFileObjectType,
        KernelMode,
        &objVmPartition,
        NULL);

    if (!NT_SUCCESS(Status)) 
    {
        KDbgLog("VidGetMemoryBlockInfoFromGPA.ObReferenceObjectByHandle failed. Status ", Status);
		KDbgLog16("pGparBlockInfo->PartitionHandle ", (ULONG64)pGparBlockInfo->PartitionHandle);
        return FALSE;
    }
    if (objVmPartition->FsContext != NULL)
    {
        pPartitionHandle = (PVM_PROCESS_CONTEXT)((PCHAR)objVmPartition->FsContext - 1);

		//
		// Resolve the layout, then let VidGetGparObjectForGpa walk the GPAR array at the
		// discovered offsets. The GPAR handle is only kept here for the reported Count.
		//

		VidResolveLayout(pPartitionHandle, &Layout);
		pGparBlockHandle = (PGPAR_BLOCK_HANDLE)*(PVOID*)((PCHAR)pPartitionHandle + Layout.GparHandleOffset);

		if (pGparBlockHandle == 0)
		{
			KDbgPrintString("\tSomething wrong with offset of GparBlockHandle");
			ObDereferenceObject(objVmPartition);
			return FALSE;
		}

        Index = *(PUINT32)((PCHAR)pGparBlockHandle + Layout.GparCountOffset);

		pGparElement = VidGetGparObjectForGpa(pPartitionHandle, &Layout, GPA);
		if (pGparElement != NULL &&
			GPA >= pGparElement->GpaIndexStart && GPA <= pGparElement->GpaIndexEnd)
		{
			objMBlock = (PMEMORY_BLOCK)*(PVOID*)((PCHAR)pGparElement + Layout.ObjMblockOffset);
			pGparBlockInfo->MemoryBlockPageIndex = GPA - pGparElement->GpaIndexStart;
			pGparBlockInfo->MbHandle = (MB_HANDLE)(objMBlock != NULL ? objMBlock->MbHandle : 0);
			pGparBlockInfo->Count = Index;
			Ret = TRUE;
		}
    } // end if

	ObDereferenceObject(objVmPartition);
    return Ret;
}

//
// Test HvMapGpaPages hypercall. It maps host pages to guest OS
//

BOOLEAN VidHvMapGpaPages(PCHAR pBuffer, ULONG len)
{
	UNREFERENCED_PARAMETER(len);
	UNREFERENCED_PARAMETER(pBuffer);
	PULONG64 Buffer,ArrayOfBuffers,pUnknownParam01;
	PHYSICAL_ADDRESS paBuffer = {0};
    NTSTATUS Status;
    PUINT32 PageCount = (PUINT32)20;

    Buffer = HvmmPoolAlloc(PAGE_SIZE);
    ArrayOfBuffers = HvmmPoolAlloc(PAGE_SIZE);
    pUnknownParam01 = HvmmPoolAlloc(PAGE_SIZE);

	if (!Buffer || !ArrayOfBuffers || !pUnknownParam01) {
		KDbgPrintString("ExAllocatePoolWithTag failed");
		return FALSE;
	}

    paBuffer = MmGetPhysicalAddress(Buffer);
    paBuffer.QuadPart = paBuffer.QuadPart / PAGE_SIZE;

    memset(Buffer, 0, PAGE_SIZE);
    memset(ArrayOfBuffers, 0, PAGE_SIZE);

    //*(ArrayOfBuffers+1) = (ULONG64)paBuffer.QuadPart;
    *ArrayOfBuffers = (ULONG64)paBuffer.QuadPart;
    //HV_MAP_GPA_READABLE | HV_MAP_GPA_EXECUTABLE

    Status = WinHvMapGpaPages(3, 0x2280, 0x1d, PageCount, ArrayOfBuffers, pUnknownParam01);
    KDbgLog("Status of WinHvMapGpaPages", Status);

    ExFreePoolWithTag(Buffer, 'Hvmm');
    ExFreePoolWithTag(ArrayOfBuffers, 'Hvmm');
    ExFreePoolWithTag(pUnknownParam01, 'Hvmm');

    return TRUE;
}

//
//HvReadGpa hypercall wrapper
//

BOOLEAN VidHvReadGpa(PCHAR pBuffer, ULONG len)
{
    GPA_INFO GpaInfo;
    HV_STATUS Status;
    HV_ACCESS_GPA_CONTROL_FLAGS ControlFlags = { 0 };
    HV_ACCESS_GPA_RESULT AccessResult;
    ULONG64 uBlocks,uRemainBytes,i;

    HV_VP_INDEX VpIndex = 0;

    XMM_ALIGN64 UINT128 MemoryOutput = {0};

    RtlCopyMemory(&GpaInfo,pBuffer,sizeof(GpaInfo));

    //KDbgLog16("GpaInfo.MbpCount", GpaInfo.BytesCount);
    //KDbgLog16("GpaInfo.StartMbp", GpaInfo.StartPage);
    //KDbgLog16("GpaInfo.PartitionId", GpaInfo.PartitionId);

    uBlocks = len / VID_READ_WRITE_GPA_BUFFER_SIZE;
    uRemainBytes = len % VID_READ_WRITE_GPA_BUFFER_SIZE;

    memset(pBuffer, 0, len);

    for (i = 0; i < uBlocks; i++)
    {
        Status = WinHvReadGpa(GpaInfo.PartitionId, VpIndex, GpaInfo.StartPage+i*VID_READ_WRITE_GPA_BUFFER_SIZE, VID_READ_WRITE_GPA_BUFFER_SIZE, ControlFlags, &AccessResult, pBuffer + i * VID_READ_WRITE_GPA_BUFFER_SIZE);

        KDbgLog("Status of WinHvReadGpa", Status);
        KDbgLog("AccessResult", AccessResult.ResultCode);
    }

    //KDbgLog("i = ", i);

    if (uRemainBytes > 0) {
        Status = WinHvReadGpa(GpaInfo.PartitionId, VpIndex, GpaInfo.StartPage + uBlocks * VID_READ_WRITE_GPA_BUFFER_SIZE, (UINT32)uRemainBytes, ControlFlags, &AccessResult, &MemoryOutput);
        RtlCopyMemory(pBuffer + uBlocks * VID_READ_WRITE_GPA_BUFFER_SIZE, &MemoryOutput, uRemainBytes);
        //KDbgLog("Status of WinHvReadGpa", Status);
        //KDbgLog("AccessResult", AccessResult.ResultCode);
    }

  return TRUE;
}

//
//HvWriteGpa hypercall wrapper
//

BOOLEAN VidHvWriteGpa(PCHAR pBuffer, ULONG len)
{
	UNREFERENCED_PARAMETER(len);
	GPA_INFO GpaInfo;
    NTSTATUS Status = 0;
   // PUCHAR pGPABuffer;
    HV_ACCESS_GPA_CONTROL_FLAGS ControlFlags = { 0 };
    HV_ACCESS_GPA_RESULT AccessResult;
    ULONG64 uBlocks, i;
	UINT32 uRemainBytes;
	PCHAR uPosition = NULL;
	ULONG64 PageBoundaryCheckLowerBorder = 0, PageBoundaryCheckHighBorder = 0;
	UINT32 PageBoundaryCheck1WriteBlockSize = 0, PageBoundaryCheck2WriteBlockSize = 0;

    HV_VP_INDEX VpIndex = 0;

	//DbgBreakPoint();

    RtlCopyMemory(&GpaInfo, pBuffer, sizeof(GpaInfo));

    KDbgLog16("GpaInfo.MbpCount", GpaInfo.BytesCount);
    KDbgLog16("GpaInfo.StartMbp", GpaInfo.StartPage);
    KDbgLog16("GpaInfo.PartitionId", GpaInfo.PartitionId);

	//
	//len = size of buffer + struct GpaInfo
	//

	//
	//All memory operations must be page aligned, therefore some additional checks for bage boundaries
	//

    uBlocks = GpaInfo.BytesCount / VID_READ_WRITE_GPA_BUFFER_SIZE;
    uRemainBytes = GpaInfo.BytesCount % VID_READ_WRITE_GPA_BUFFER_SIZE;

    for (i = 0; i < uBlocks; i++)
    {
		uPosition = sizeof(GpaInfo) + (PCHAR)pBuffer + i * VID_READ_WRITE_GPA_BUFFER_SIZE;

		PageBoundaryCheckLowerBorder = (GpaInfo.StartPage + i * VID_READ_WRITE_GPA_BUFFER_SIZE) / PAGE_SIZE;
		PageBoundaryCheckHighBorder = (GpaInfo.StartPage + i * VID_READ_WRITE_GPA_BUFFER_SIZE+ VID_READ_WRITE_GPA_BUFFER_SIZE-1) / PAGE_SIZE;

		if (PageBoundaryCheckLowerBorder == PageBoundaryCheckHighBorder)
		{
			Status = WinHvWriteGpa(GpaInfo.PartitionId, VpIndex, GpaInfo.StartPage + i * VID_READ_WRITE_GPA_BUFFER_SIZE, VID_READ_WRITE_GPA_BUFFER_SIZE, ControlFlags, (PVOID)uPosition, &AccessResult);
		}
		else
		{
			PageBoundaryCheck1WriteBlockSize = (PAGE_SIZE - ((GpaInfo.StartPage + i * VID_READ_WRITE_GPA_BUFFER_SIZE) & 0xFFF));
			PageBoundaryCheck2WriteBlockSize = VID_READ_WRITE_GPA_BUFFER_SIZE - PageBoundaryCheck1WriteBlockSize;

			Status = WinHvWriteGpa(GpaInfo.PartitionId, VpIndex, GpaInfo.StartPage + i * VID_READ_WRITE_GPA_BUFFER_SIZE, PageBoundaryCheck1WriteBlockSize, ControlFlags, (PVOID)uPosition, &AccessResult);
			Status = WinHvWriteGpa(GpaInfo.PartitionId, VpIndex, GpaInfo.StartPage + i * VID_READ_WRITE_GPA_BUFFER_SIZE+ PageBoundaryCheck1WriteBlockSize, PageBoundaryCheck2WriteBlockSize, ControlFlags, (PVOID)((PCHAR)uPosition+PageBoundaryCheck1WriteBlockSize), &AccessResult);
		}

		KDbgLog("Status of WinHvReadGpa", Status);
		KDbgLog("AccessResult", AccessResult.ResultCode);
    }

	KDbgLog("i = ", (ULONG)i);

    if (uRemainBytes > 0) {
		uPosition = sizeof(GpaInfo) + (PCHAR)pBuffer + uBlocks * VID_READ_WRITE_GPA_BUFFER_SIZE;

		PageBoundaryCheckLowerBorder = (GpaInfo.StartPage + uBlocks * VID_READ_WRITE_GPA_BUFFER_SIZE) / PAGE_SIZE;
		PageBoundaryCheckHighBorder = (GpaInfo.StartPage + uBlocks * VID_READ_WRITE_GPA_BUFFER_SIZE + uRemainBytes - 1) / PAGE_SIZE;

		if (PageBoundaryCheckLowerBorder == PageBoundaryCheckHighBorder)
		{
			Status = WinHvWriteGpa(GpaInfo.PartitionId, VpIndex, GpaInfo.StartPage + uBlocks * VID_READ_WRITE_GPA_BUFFER_SIZE, uRemainBytes, ControlFlags, (PVOID)uPosition, &AccessResult);
		}
		else
		{
			PageBoundaryCheck1WriteBlockSize = (PAGE_SIZE - ((GpaInfo.StartPage + uBlocks * VID_READ_WRITE_GPA_BUFFER_SIZE) & 0xFFF));
			PageBoundaryCheck2WriteBlockSize = uRemainBytes - PageBoundaryCheck1WriteBlockSize;

			Status = WinHvWriteGpa(GpaInfo.PartitionId, VpIndex, GpaInfo.StartPage + uBlocks * VID_READ_WRITE_GPA_BUFFER_SIZE, PageBoundaryCheck1WriteBlockSize, ControlFlags, (PVOID)uPosition, &AccessResult);
			Status = WinHvWriteGpa(GpaInfo.PartitionId, VpIndex, GpaInfo.StartPage + uBlocks * VID_READ_WRITE_GPA_BUFFER_SIZE + PageBoundaryCheck1WriteBlockSize, PageBoundaryCheck2WriteBlockSize, ControlFlags, (PVOID)((PCHAR)uPosition + PageBoundaryCheck1WriteBlockSize), &AccessResult);
		}

		KDbgLog("Status of WinHvWriteGpa", Status);
		KDbgLog("AccessResult", AccessResult.ResultCode);
    }

    return TRUE;
}

//
//ReadVpRegisters hypercall implementaion
//

BOOLEAN VidReadVpRegisters(PCHAR pBuffer, ULONG len)
{
	UNREFERENCED_PARAMETER(len);
	PULONG64 pArrayofReg, pArrayofCount, pArrayOfResult;
    NTSTATUS Status = 0;
    PREGISTER_VP_INFO pRegInfo;
    
	pArrayofReg = HvmmPoolAlloc(PAGE_SIZE);
    pArrayofCount = HvmmPoolAlloc(PAGE_SIZE);
    pArrayOfResult = HvmmPoolAlloc(PAGE_SIZE);

	if (!pArrayofReg || !pArrayofCount || !pArrayOfResult) {
		KDbgPrintString("ExAllocatePoolWithTag failed");
		return FALSE;
	}

    pRegInfo = (PREGISTER_VP_INFO)pBuffer;

    memset(pArrayofReg, 0, PAGE_SIZE);
    memset(pArrayofCount, 0, PAGE_SIZE);
    memset(pArrayOfResult, 0, PAGE_SIZE);
        
    *pArrayofReg = (ULONG64)pRegInfo->RegisterCode;
    Status = WinHvGetVpRegisters(pRegInfo->PartitionId, pRegInfo->VpIndex, 0, REGISTER_READ_WRITE_COUNT, pArrayofReg, pArrayofCount, pArrayOfResult);
    KDbgLog("Status of WinHvGetVpRegisters", Status);
    
    RtlCopyMemory(pBuffer, pArrayOfResult, sizeof(HV_REGISTER_VALUE));

    ExFreePoolWithTag(pArrayofReg, 'Hvmm');
    ExFreePoolWithTag(pArrayofCount, 'Hvmm');
    ExFreePoolWithTag(pArrayOfResult, 'Hvmm');

	if (Status != STATUS_SUCCESS) {
		return FALSE;
	}
    
    return TRUE;
}

BOOLEAN VidWriteVpRegisters(PCHAR pBuffer, ULONG len)
{
	UNREFERENCED_PARAMETER(len);

	PULONG64 pArrayofReg, pArrayofValues, pArrayOfResult;
	NTSTATUS Status = 0;
	PREGISTER_VP_INFO pRegInfo;

	//DbgBreakPoint();

	pArrayofReg = HvmmPoolAlloc(PAGE_SIZE);
	pArrayofValues = HvmmPoolAlloc(PAGE_SIZE);
	pArrayOfResult = HvmmPoolAlloc(PAGE_SIZE);

	if (!pArrayofReg || !pArrayofValues || !pArrayOfResult) {
		KDbgPrintString("ExAllocatePoolWithTag failed");
		return FALSE;
	}

	pRegInfo = (PREGISTER_VP_INFO)pBuffer;

	*pArrayofReg = (ULONG64)pRegInfo->RegisterCode;
	*pArrayofValues = (ULONG64)pRegInfo->RegisterValue.Reg64;

    //*pArrayofReg = (ULONG64)HvRegisterExplicitSuspend;
	//*pArrayofValues = 0x1;
	Status = WinHvSetVpRegisters(pRegInfo->PartitionId, pRegInfo->VpIndex, 0, REGISTER_READ_WRITE_COUNT, pArrayofReg, pArrayofValues, pArrayOfResult);

	KDbgLog("Status of WinHvSetVpRegisters", Status);

	ExFreePoolWithTag(pArrayofReg, 'Hvmm');
	ExFreePoolWithTag(pArrayofValues, 'Hvmm');
	ExFreePoolWithTag(pArrayOfResult, 'Hvmm');

	if (Status != STATUS_SUCCESS) {
		return FALSE;
	}
    
    return TRUE;
}

BOOLEAN VidTranslateGvatoGpa(PCHAR pBuffer, ULONG len)
{
    PTRANSLATE_VA_INFO pVaInfo;
    XMM_ALIGN64 HV_GPA_PAGE_NUMBER GpaPage;
    XMM_ALIGN64 HV_TRANSLATE_GVA_RESULT TranslationResult;
    UNREFERENCED_PARAMETER(len);

    pVaInfo = (PTRANSLATE_VA_INFO) pBuffer;

    WinHvTranslateVirtualAddress(pVaInfo->PartitionId, pVaInfo->VpIndex, pVaInfo->ControlFlags, pVaInfo->GvaPage, &TranslationResult, &GpaPage);

    RtlCopyMemory(pBuffer, &GpaPage, sizeof(GpaPage));
    RtlCopyMemory(pBuffer+ sizeof(GpaPage), &TranslationResult, sizeof(TranslationResult));
    
    return TRUE;
}

BOOLEAN VidGetFriendlyPartitionName(PCHAR pBuffer, ULONG len)
{
    NTSTATUS Status;
    PFILE_OBJECT objVmPartition;
    UNICODE_STRING pVmName;
    PVID_VM_INFO pVmInfo;
    PPARTITION_INFO pPartitionInfo;
	PVM_PROCESS_CONTEXT pPartitionHandle = NULL;
	VM_LAYOUT Layout;

    pPartitionInfo = (PPARTITION_INFO)pBuffer;
    EnumActivePartitionID();

    UNREFERENCED_PARAMETER(len);
    //hPartitionDeviceHandle = *(PUINT64)(pBuffer);

    //VidPatchPsGetCurrentProcess(pBuffer);

    KDbgLog16("hPartitionDeviceHandle: ", (ULONG64)pPartitionInfo->PartitionHandle);
    Status = ObReferenceObjectByHandle(pPartitionInfo->PartitionHandle,
        READ_CONTROL,
        *IoFileObjectType,
        KernelMode,
        &objVmPartition,
        NULL);

    if (!NT_SUCCESS(Status)) {
        KDbgLog("VidGetFriendlyPartitionName.ObReferenceObjectByHandle failed. Status ", Status);
        return FALSE;
    }
    if (objVmPartition->FsContext != NULL) {

		KDbgLog16("objVmPartition->FsContext ", (ULONG64)objVmPartition->FsContext);

		pPartitionHandle = (PVM_PROCESS_CONTEXT)((PCHAR)objVmPartition->FsContext - 1);

		//
		// Find where the name and id sit for this vid.sys build, then copy them from there. On a
		// full VM these come from the signature scan; on anything else the resolver leaves the
		// 2018 fallback offsets in place, so the copy still runs (same as before).
		//

		VidResolveLayout(pPartitionHandle, &Layout);
		VidFillLayoutReport((PCHAR)pPartitionHandle, &Layout, &g_LastEnumLayout);

		//
		// The reply shares the buffer with the request, and hvlib reads more of it than this
		// driver fills. Clear it all first so nothing from the pool reaches user mode.
		//

		if (len < sizeof(VID_VM_INFO)) {
			KDbgPrintString("VidGetFriendlyPartitionName: output buffer too small");
			ObDereferenceObject(objVmPartition);
			return FALSE;
		}
		RtlZeroMemory(pBuffer, len);

		pVmInfo = (PVID_VM_INFO)pBuffer;

		//
		// The reply starts with the 0x400 bytes that follow the name in the partition
		// context (the name, the id, the binary GUID), copied page by page so a context
		// that ends early cannot fault. Then the fields hvlib reads at fixed places.
		//

		{
			ULONG copied = 0;
			PCHAR src = (PCHAR)pPartitionHandle + Layout.NameOffset;
			ULONG window = len < VID_VM_INFO_NAME_WINDOW_BYTES ? len : VID_VM_INFO_NAME_WINDOW_BYTES;

			while (copied < window && VidProbe(src + copied)) {
				ULONG chunk = PAGE_SIZE - (((ULONG_PTR)src + copied) & (PAGE_SIZE - 1));
				if (chunk > window - copied) {
					chunk = window - copied;
				}
				RtlCopyMemory(pBuffer + copied, src + copied, chunk);
				copied += chunk;
			}
		}

		if (len >= sizeof(VID_VM_INFO)) {
			PUCHAR guid = (PUCHAR)pPartitionHandle + Layout.PartitionIdOffset + sizeof(HV_PARTITION_ID);

			RtlCopyMemory(&pVmInfo->PartitionId, ((PCHAR)pPartitionHandle + Layout.PartitionIdOffset), sizeof(pVmInfo->PartitionId));

			if (VidProbe(guid) && VidProbe(guid + 15)) {
				RtlStringCchPrintfW(pVmInfo->VmGuidString, RTL_NUMBER_OF(pVmInfo->VmGuidString),
					L"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
					*(PULONG)guid, *(PUSHORT)(guid + 4), *(PUSHORT)(guid + 6),
					guid[8], guid[9], guid[10], guid[11], guid[12], guid[13], guid[14], guid[15]);
			}
		}

		if (Layout.IsFullVm)
		{
			//
			// The shape says a full VM. The shape does not distinguish Win10 / Server / Secure
			// here (it defaults to Server), so we report the full-Server type. That is enough for
			// the read gate and matches the released driver.
			//

			KDbgPrintString("Partition is a FULL VM (resolved by signature scan)");
			pVmInfo->VmType = UsrVidVmTypeFullWinSrvVM;
			if (len >= sizeof(VID_VM_INFO)) {
				pVmInfo->FullVmFlag = 1;
			}
		}
		else
		{
			//
			// Not a full VM. Fall back to the fixed VmType field to tell containers apart, and
			// grab the vmmem handle for the ones that read through it, exactly as before.
			//

			switch (pPartitionHandle->VmType)
			{
			case VidVmTypeContainer:
				KDbgPrintString("Partition is container (WDAG or Windows Sandbox)");
				g_vmmemHandle = VidFindVmmemHandle(pPartitionHandle);
				pVmInfo->VmType = UsrVidVmTypeContainer;
				break;
			case VidVmTypeFullWin10VM:
				KDbgPrintString("Partition is FULL Win10 VM");
				pVmInfo->VmType = UsrVidVmTypeFullWin10VM;
				break;
			case VidVmTypeFullWinSrvVM:
				KDbgPrintString("Partition is FULL WinSrv VM");
				pVmInfo->VmType = UsrVidVmTypeFullWinSrvVM;
				break;
			case VidVmTypeFullWinSrvVMSecure:
				KDbgPrintString("Partition is FULL WinSrv VM with Secure Boot");
				pVmInfo->VmType = UsrVidVmTypeFullWinSrvVMSecure;
				break;
			case VidVmTypeDockerHyperVContainerUserName:
				KDbgPrintString("Docker username partition");
				g_vmmemHandle = VidFindVmmemHandle(pPartitionHandle);
				pVmInfo->VmType = UsrVidVmTypeDockerHyperVContainerUserName;
				break;
			case VidVmTypeDockerHyperVContainerGUID:
				KDbgPrintString("Docker named partition");
				g_vmmemHandle = VidFindVmmemHandle(pPartitionHandle);
				pVmInfo->VmType = UsrVidVmTypeDockerHyperVContainerGUID;
				break;
			case VidVmTypeLinuxContainer:
				KDbgPrintString("Linux container. Nothing to do with WinDBG");
				pVmInfo->VmType = UsrVidVmTypeLinuxContainer;
				break;
			default:
				KDbgPrintString("Partition is unknown type. Next actions are dangerous!");
				break;
			}
		}

        RtlInitUnicodeString(&pVmName, (PWCH)pVmInfo->FriendlyName);
        
        #ifdef DBG_PRINT_STRINGS
			DbgPrintUStringString("Partition friendly name: ", pVmName);
		#endif
        KDbgLog16("Partition ID: ", pVmInfo->PartitionId);

        ObDereferenceObject(objVmPartition);

        return TRUE;
    }

	KDbgPrintString("Object is NULL");
    ObDereferenceObject(objVmPartition);
    return FALSE;
}

SIZE_T EnumActivePartitionID() {
    HV_STATUS hvStatus;
    HV_PARTITION_ID PartID = 0xFF, NextPartID;
    HV_PARTITION_PROPERTY HvProp = 0;
    SIZE_T counter = 1;

    hvStatus = WinHvGetPartitionId(&PartID);
    KDbgLog(" First PartID", (ULONG)PartID);
    hvStatus = WinHvGetPartitionProperty(PartID, HvPartitionPropertyPrivilegeFlags, &HvProp);
   //KDbgLog16(" First HvProp", HvProp);
    hvStatus = WinHvGetNextChildPartition(PartID, HV_PARTITION_ID_INVALID, &NextPartID);
    //KDbgLog(" first WinHvGetNextChildPartition hvstatus", hvStatus);
    KDbgLog16(" First NextPartID", NextPartID);
    while ((NextPartID != HV_PARTITION_ID_INVALID) && (hvStatus == 0)) {
        hvStatus = WinHvGetPartitionProperty(NextPartID, HvPartitionPropertyPrivilegeFlags, &HvProp);
       // KDbgLog16("  HvProp", HvProp);
        hvStatus = WinHvGetNextChildPartition(PartID, NextPartID, &NextPartID);
        //KDbgLog("    WinHvGetNextChildPartition hvstatus", hvStatus);
        KDbgLog16("  NextPartID", NextPartID);
        counter += 1;
    }
    return counter;
}