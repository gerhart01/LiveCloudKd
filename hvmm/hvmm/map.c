//
// map.c: mapped read mode for full VMs.
//
// IOCTL_MAP_GPA_RANGE maps a guest physical range into the calling process, read-only, and
// keeps it mapped, so a read becomes a memcpy in user mode instead of one IOCTL per block.
// The host frames are found exactly the way VidGetFullVmMemoryBlock finds them, through
// VidGetGparObjectForGpa and the GPAR / memory block structures in hvmm.h, so this file has
// no layout knowledge of its own: whatever offsets vid.c uses, the mapping uses too.
//
// Limits, by design:
//  - Full VMs only (the container types read through vmmem and are refused).
//  - The mapping shows the host frames that backed the guest at map time. It is meant for
//    VMs without dynamic memory, and only while the VM runs: if the VM stops, restarts,
//    saves or the host takes pages back, the frames go back to the host and the mapping
//    shows whatever the host puts there. The client must unmap when the VM goes away.
//  - Only frames inside host RAM (MmGetPhysicalMemoryRanges) are mapped. A frame in a gap
//    between RAM ranges ends the mapped prefix, so device memory is never mapped cached.
//

#include "hvmm.h"

//
// The calling process is taken from the current thread, not PsGetCurrentProcess, because
// IOCTL_HV_PATCH_GETPROCESS can redirect PsGetCurrentProcess to the VM worker process.
//

//
// Per handle state
//

NTSTATUS MapCreateFileContext(PFILE_OBJECT FileObject)
{
	PHVMM_FILE_CONTEXT pFileContext;

	if (FileObject == NULL) {
		return STATUS_SUCCESS;
	}

	pFileContext = (PHVMM_FILE_CONTEXT)HvmmPoolAlloc(sizeof(HVMM_FILE_CONTEXT));
	if (pFileContext == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	KeInitializeSpinLock(&pFileContext->Lock);
	InitializeListHead(&pFileContext->MappingList);
	FileObject->FsContext2 = pFileContext;
	return STATUS_SUCCESS;
}

//
// Tear one mapping down. A user mapping can only be removed from its own address space, so
// for a mapping made by another process (inherited or duplicated handle) attach to it first.
// If that process is already exiting, its address space goes with it and only the MDL is
// left to free.
//

static VOID MapReleaseMapping(PHVMM_GPA_MAPPING pMapping)
{
	PEPROCESS Owner = pMapping->Process;
	BOOLEAN Attached = FALSE;
	BOOLEAN Unmap = TRUE;
	KAPC_STATE ApcState;

	if (Owner != PsGetThreadProcess(PsGetCurrentThread())) {
		if (PsGetProcessExitStatus(Owner) != STATUS_PENDING) {
			Unmap = FALSE;
		}
		else {
			KeStackAttachProcess(Owner, &ApcState);
			Attached = TRUE;
		}
	}

	if (Unmap) {
		MmUnmapLockedPages(pMapping->UserVa, pMapping->Mdl);
	}
	if (Attached) {
		KeUnstackDetachProcess(&ApcState);
	}

	pMapping->Mdl->MdlFlags &= ~MDL_PAGES_LOCKED;
	IoFreeMdl(pMapping->Mdl);
	ObDereferenceObject(pMapping->Partition);
	ObDereferenceObject(pMapping->Process);
	HvmmPoolFree(pMapping);
}

//
// Unmap everything still live on a handle. IRP_MJ_CLEANUP calls it in the context of the
// process closing the last handle; IRP_MJ_CLOSE calls it again for a map request that was
// still in flight when cleanup ran.
//

VOID MapCleanupFileContext(PFILE_OBJECT FileObject)
{
	PHVMM_FILE_CONTEXT pFileContext;
	LIST_ENTRY LocalList;
	KIRQL OldIrql;

	if (FileObject == NULL || FileObject->FsContext2 == NULL) {
		return;
	}
	pFileContext = (PHVMM_FILE_CONTEXT)FileObject->FsContext2;

	//
	// Detach the list under the lock and unmap outside it: MmUnmapLockedPages for a user
	// mapping runs at PASSIVE_LEVEL.
	//

	InitializeListHead(&LocalList);

	KeAcquireSpinLock(&pFileContext->Lock, &OldIrql);
	while (!IsListEmpty(&pFileContext->MappingList)) {
		InsertTailList(&LocalList, RemoveHeadList(&pFileContext->MappingList));
	}
	pFileContext->MappingCount = 0;
	KeReleaseSpinLock(&pFileContext->Lock, OldIrql);

	while (!IsListEmpty(&LocalList)) {
		MapReleaseMapping(CONTAINING_RECORD(RemoveHeadList(&LocalList), HVMM_GPA_MAPPING, Link));
	}
}

VOID MapFreeFileContext(PFILE_OBJECT FileObject)
{
	if (FileObject == NULL || FileObject->FsContext2 == NULL) {
		return;
	}
	MapCleanupFileContext(FileObject);
	HvmmPoolFree(FileObject->FsContext2);
	FileObject->FsContext2 = NULL;
}

//
// IOCTL_QUERY_MAPPING_SUPPORT
//

NTSTATUS MapQuerySupport(PCHAR pBuffer, ULONG inLen, ULONG outLen, PULONG pBytesReturned)
{
	MAPPING_QUERY_INPUT Input;
	MAPPING_QUERY_OUTPUT Output;

	*pBytesReturned = 0;

	if (inLen < sizeof(MAPPING_QUERY_INPUT) || outLen < sizeof(MAPPING_QUERY_OUTPUT)) {
		return STATUS_BUFFER_TOO_SMALL;
	}

	RtlCopyMemory(&Input, pBuffer, sizeof(Input));
	if (Input.Magic != HVMM_MAPPING_QUERY_MAGIC) {
		return STATUS_INVALID_PARAMETER;
	}

	Output.Signature = HVMM_MAPPING_SIGNATURE;
	Output.Version = HVMM_MAPPING_VERSION;
	Output.MaxMapLength = HVMM_MAP_GPA_MAX_LENGTH;
	RtlCopyMemory(pBuffer, &Output, sizeof(Output));
	*pBytesReturned = sizeof(Output);
	return STATUS_SUCCESS;
}

//
// True when the frame lies inside one of the host RAM ranges. Ranges is the array
// MmGetPhysicalMemoryRanges returns, ended by an all-zero entry.
//

static BOOLEAN MapIsHostRam(PPHYSICAL_MEMORY_RANGE Ranges, ULONG64 Pfn)
{
	ULONG i;

	for (i = 0; Ranges[i].BaseAddress.QuadPart != 0 || Ranges[i].NumberOfBytes.QuadPart != 0; i++) {
		ULONG64 First = (ULONG64)Ranges[i].BaseAddress.QuadPart / PAGE_SIZE;
		ULONG64 Count = (ULONG64)Ranges[i].NumberOfBytes.QuadPart / PAGE_SIZE;
		if (Pfn >= First && Pfn < First + Count) {
			return TRUE;
		}
	}
	return FALSE;
}

//
// Fill Pfns with the host frames of the longest backed prefix of [GpaStartPage,
// GpaStartPage + TotalPages). Returns the number of pages filled.
//

static ULONG64 MapResolveFrames(PVM_PROCESS_CONTEXT pPartitionHandle, PPHYSICAL_MEMORY_RANGE Ranges,
	UINT64 GpaStartPage, UINT64 TotalPages, PPFN_NUMBER Pfns)
{
	UINT64 i = 0;

	while (i < TotalPages) {
		UINT64 GpaPage = GpaStartPage + i;
		UINT64 RangeEnd;
		PGPAR_OBJECT objGpar;
		PMEMORY_BLOCK objMBlock;
		PCHAR pGuestGpaArray;

		//
		// One GPAR lookup per range, not per page. VidGetGparObjectForGpa returns the last
		// range it looked at when none contains the page, so the bounds are checked here.
		// A range whose start equals its end is a vmwp.exe descriptor, not host backed.
		//

		objGpar = VidGetGparObjectForGpa(pPartitionHandle, GpaPage);
		if (objGpar == NULL ||
			GpaPage < objGpar->GpaIndexStart ||
			GpaPage > objGpar->GpaIndexEnd ||
			objGpar->GpaIndexStart == objGpar->GpaIndexEnd) {
			break;
		}

		objMBlock = objGpar->objMBlock;
		if (objMBlock == NULL || objMBlock->pGuestGPAArray == NULL) {
			break;
		}
		pGuestGpaArray = (PCHAR)objMBlock->pGuestGPAArray;

		RangeEnd = objGpar->GpaIndexEnd;
		if (RangeEnd > GpaStartPage + TotalPages - 1) {
			RangeEnd = GpaStartPage + TotalPages - 1;
		}

		for (; GpaPage <= RangeEnd; GpaPage++, i++) {
			ULONG64 HostPfn = *(PULONG)(pGuestGpaArray + 0x10 * (GpaPage - objGpar->GpaIndexStart));
			if (HostPfn == 0 || !MapIsHostRam(Ranges, HostPfn)) {
				return i;
			}
			Pfns[i] = (PFN_NUMBER)HostPfn;
		}
	}

	return i;
}

//
// IOCTL_MAP_GPA_RANGE
//

NTSTATUS MapGpaRange(PFILE_OBJECT FileObject, PCHAR pBuffer, ULONG inLen, ULONG outLen, PULONG pBytesReturned)
{
	MAP_GPA_RANGE_INPUT Input;
	MAP_GPA_RANGE_OUTPUT Output;
	NTSTATUS Status;
	PFILE_OBJECT objVmPartition = NULL;
	PVM_PROCESS_CONTEXT pPartitionHandle;
	PHVMM_FILE_CONTEXT pFileContext;
	PHVMM_GPA_MAPPING pMapping;
	PPHYSICAL_MEMORY_RANGE Ranges = NULL;
	PMDL pMDL = NULL;
	PVOID UserVa = NULL;
	UINT64 TotalPages, MappedPages = 0;
	KIRQL OldIrql;

	*pBytesReturned = 0;

	if (inLen < sizeof(MAP_GPA_RANGE_INPUT) || outLen < sizeof(MAP_GPA_RANGE_OUTPUT)) {
		return STATUS_BUFFER_TOO_SMALL;
	}
	if (FileObject == NULL || FileObject->FsContext2 == NULL) {
		return STATUS_INVALID_DEVICE_STATE;
	}
	pFileContext = (PHVMM_FILE_CONTEXT)FileObject->FsContext2;

	RtlCopyMemory(&Input, pBuffer, sizeof(Input));

	if (Input.Length == 0 ||
		(Input.GpaStart & (PAGE_SIZE - 1)) != 0 ||
		(Input.Length & (PAGE_SIZE - 1)) != 0) {
		return STATUS_INVALID_PARAMETER;
	}
	if (Input.Length > HVMM_MAP_GPA_MAX_LENGTH) {
		Input.Length = HVMM_MAP_GPA_MAX_LENGTH;
	}
	TotalPages = Input.Length / PAGE_SIZE;

	//
	// Same lookup as every other partition request in this driver.
	//

	Status = ObReferenceObjectByHandle(Input.PartitionHandle,
		READ_CONTROL,
		*IoFileObjectType,
		KernelMode,
		&objVmPartition,
		NULL);

	if (!NT_SUCCESS(Status)) {
		KDbgLog("MapGpaRange.ObReferenceObjectByHandle failed. Status ", Status);
		return Status;
	}

	if (objVmPartition->FsContext == NULL) {
		Status = STATUS_INVALID_HANDLE;
		goto Fail;
	}

	pPartitionHandle = (PVM_PROCESS_CONTEXT)((PCHAR)objVmPartition->FsContext - 1);

	switch (pPartitionHandle->VmType)
	{
	case VidVmTypeFullWin10VM:
	case VidVmTypeFullWinSrvVMSecure:
	case VidVmTypeFullWinSrvVM:
		break;
	default:
		Status = STATUS_NOT_SUPPORTED;
		goto Fail;
	}

	Ranges = MmGetPhysicalMemoryRanges();
	if (Ranges == NULL) {
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto Fail;
	}

	pMDL = IoAllocateMdl(NULL, (ULONG)Input.Length, FALSE, FALSE, NULL);
	if (pMDL == NULL) {
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto Fail;
	}

	MappedPages = MapResolveFrames(pPartitionHandle, Ranges, Input.GpaStart / PAGE_SIZE, TotalPages, MmGetMdlPfnArray(pMDL));
	if (MappedPages == 0) {
		Status = STATUS_NO_MORE_ENTRIES;
		goto Fail;
	}

	//
	// The frames belong to vid.sys, which keeps them while the VM runs. The MDL only
	// describes them so they can be mapped; it never locks or frees them.
	//

	pMDL->ByteCount = (ULONG)(MappedPages * PAGE_SIZE);
	pMDL->MdlFlags |= MDL_PAGES_LOCKED;

	__try {
		UserVa = MmMapLockedPagesSpecifyCache(pMDL, UserMode, MmCached, NULL, FALSE,
			NormalPagePriority | MdlMappingNoWrite | MdlMappingNoExecute);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		UserVa = NULL;
	}

	if (UserVa == NULL) {
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto Fail;
	}

	pMapping = (PHVMM_GPA_MAPPING)HvmmPoolAlloc(sizeof(HVMM_GPA_MAPPING));
	if (pMapping == NULL) {
		MmUnmapLockedPages(UserVa, pMDL);
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto Fail;
	}

	pMapping->Mdl = pMDL;
	pMapping->UserVa = UserVa;
	pMapping->Process = PsGetThreadProcess(PsGetCurrentThread());
	pMapping->Partition = objVmPartition;
	pMapping->MappedBytes = MappedPages * PAGE_SIZE;
	ObReferenceObject(pMapping->Process);

	//
	// From here the mapping owns the MDL and the partition reference.
	//

	pMDL = NULL;
	objVmPartition = NULL;
	ExFreePool(Ranges);
	Ranges = NULL;

	KeAcquireSpinLock(&pFileContext->Lock, &OldIrql);
	if (pFileContext->MappingCount >= HVMM_MAX_MAPPINGS_PER_HANDLE) {
		KeReleaseSpinLock(&pFileContext->Lock, OldIrql);
		MapReleaseMapping(pMapping);
		return STATUS_QUOTA_EXCEEDED;
	}
	pFileContext->MappingCount++;
	InsertTailList(&pFileContext->MappingList, &pMapping->Link);
	KeReleaseSpinLock(&pFileContext->Lock, OldIrql);

	Output.UserVa = UserVa;
	Output.MappedBytes = MappedPages * PAGE_SIZE;
	RtlCopyMemory(pBuffer, &Output, sizeof(Output));
	*pBytesReturned = sizeof(Output);
	return STATUS_SUCCESS;

Fail:
	if (pMDL != NULL) {
		pMDL->MdlFlags &= ~MDL_PAGES_LOCKED;
		IoFreeMdl(pMDL);
	}
	if (Ranges != NULL) {
		ExFreePool(Ranges);
	}
	if (objVmPartition != NULL) {
		ObDereferenceObject(objVmPartition);
	}
	return Status;
}

//
// IOCTL_UNMAP_GPA_RANGE: unmap one range this process mapped on this handle.
//

NTSTATUS UnmapGpaRange(PFILE_OBJECT FileObject, PCHAR pBuffer, ULONG inLen)
{
	UNMAP_GPA_RANGE_INPUT Input;
	PHVMM_FILE_CONTEXT pFileContext;
	PHVMM_GPA_MAPPING pFound = NULL;
	PLIST_ENTRY pEntry;
	PEPROCESS Caller = PsGetThreadProcess(PsGetCurrentThread());
	KIRQL OldIrql;

	if (inLen < sizeof(UNMAP_GPA_RANGE_INPUT)) {
		return STATUS_BUFFER_TOO_SMALL;
	}
	if (FileObject == NULL || FileObject->FsContext2 == NULL) {
		return STATUS_INVALID_DEVICE_STATE;
	}
	pFileContext = (PHVMM_FILE_CONTEXT)FileObject->FsContext2;

	RtlCopyMemory(&Input, pBuffer, sizeof(Input));

	KeAcquireSpinLock(&pFileContext->Lock, &OldIrql);
	for (pEntry = pFileContext->MappingList.Flink; pEntry != &pFileContext->MappingList; pEntry = pEntry->Flink) {
		PHVMM_GPA_MAPPING pMapping = CONTAINING_RECORD(pEntry, HVMM_GPA_MAPPING, Link);
		if (pMapping->UserVa == Input.UserVa && pMapping->Process == Caller) {
			RemoveEntryList(pEntry);
			pFileContext->MappingCount--;
			pFound = pMapping;
			break;
		}
	}
	KeReleaseSpinLock(&pFileContext->Lock, OldIrql);

	if (pFound == NULL) {
		return STATUS_NOT_FOUND;
	}

	MapReleaseMapping(pFound);
	return STATUS_SUCCESS;
}
