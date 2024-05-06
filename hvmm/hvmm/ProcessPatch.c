#include "hvmm.h"

BOOLEAN bIsPsGetCurrentPsPatched = FALSE;
PCHAR g_pVidLoadBase = NULL;

PVOID pPsGetCurrentProcessOrig = NULL;
PVOID pSavedPsGetCurrentProcessFunction = NULL;
PVOID pAddrOfArchNewPsGetCurrentProcess = NULL;

PEPROCESS pHandleOfLiveCloudKd = NULL;
PEPROCESS pHandleOfVmwp = NULL;





PVOID VidPsProcessCheckWorker(PVOID pCurrentProcess, PVOID pRetAddress)
{
	//KSPIN_LOCK SpinLock;
   // KLOCK_QUEUE_HANDLE QueueHandle;
	ULONG64 Address01 = (ULONG64)g_pVidLoadBase + VID_PS_PROCESS_CHECK_01;
	//ULONG64 Address02 = (ULONG64)g_pVidLoadBase + VID_PS_PROCESS_CHECK_02;

	 //if ((pRetAddress == (PVOID)Address01) | (pRetAddress == (PVOID)Address02)) {
		 //KDbgLog16("Address01", Address01);
		 //KDbgLog16("Address02", Address02);
		 //KDbgLog16("pRetAddress", pRetAddress);
		 //if (bIsPsGetCurrentPsPatched) {
		 //    //KeInitializeSpinLock(&SpinLock);
		 //    //KeAcquireInStackQueuedSpinLock(&SpinLock, &QueueHandle);

		 //    RtlCopyMemory(pPsGetCurrentProcessOrig, pSavedPsGetCurrentProcessFunction, SIZE_OF_ARCH_NEW_PS_FUNCTION);
		 //    //KeReleaseInStackQueuedSpinLock(&QueueHandle);
		 //    //ExFreePoolWithTag(pSavedPsGetCurrentProcessFunction, 'Hvmm');

		 //    DbgPrintString("PsGetCurrentPsPatched was restored");
		 //    bIsPsGetCurrentPsPatched = FALSE;
		 //}
	if ((pRetAddress == (PVOID)Address01)) {
		return pHandleOfVmwp;
	}
	return pCurrentProcess;
}

BOOLEAN VidPatchPsGetCurrentProcess(PCHAR pBuffer, ULONG len)
{
	UNREFERENCED_PARAMETER(len);
	UNICODE_STRING uFunctionName;
	char* sVidName = "Vid.sys";
	PUINT64 tmpAddr = (PUINT64)0xFFFFF78000000000ULL;
	NTSTATUS Status = 0;

	//KSPIN_LOCK SpinLock;
	//KLOCK_QUEUE_HANDLE QueueHandle;

	PPARTITION_INFO pPartitionInfo;
	KIRQL kiCurrent = 0;

	//DbgBreakPoint();

	pPartitionInfo = (PPARTITION_INFO)pBuffer;

	if (bIsPsGetCurrentPsPatched) {
		KDbgPrintString("PsGetCurrentProcess was patched already");
		KDbgLog("pPartitionInfo->ProcessPid", (ULONG)pPartitionInfo->ProcessPid);
		if (pPartitionInfo->ProcessPid == 0) {
			pHandleOfLiveCloudKd = PsGetCurrentProcess();
		}
		else
		{
			Status = PsLookupProcessByProcessId((HANDLE)pPartitionInfo->ProcessPid, &pHandleOfLiveCloudKd);
			if (Status != STATUS_SUCCESS) {
				KDbgLog("Status of PsLookupProcessByProcessId", Status);
				return FALSE;
			}
		}

		return TRUE;
	}

	pSavedPsGetCurrentProcessFunction = HvmmPoolAlloc(SIZE_OF_ARCH_NEW_PS_FUNCTION);

	if (!pSavedPsGetCurrentProcessFunction) {
		KDbgLog("ExAllocatePoolWithTag failed", Status);
		return FALSE;
	}

	memset(pSavedPsGetCurrentProcessFunction, 0, SIZE_OF_ARCH_NEW_PS_FUNCTION);


	//pHandleOfLiveCloudKd = PsGetCurrentProcess();
	Status = PsLookupProcessByProcessId((HANDLE)pPartitionInfo->ProcessPid, &pHandleOfLiveCloudKd);
	if (Status != STATUS_SUCCESS) {
		KDbgLog("Status of PsLookupProcessByProcessId", Status);
		return FALSE;
	}

	Status = PsLookupProcessByProcessId((HANDLE)pPartitionInfo->VmwpPid, &pHandleOfVmwp);
	if (Status != STATUS_SUCCESS) {
		KDbgLog("Status of PsLookupProcessByProcessId", Status);
		return FALSE;
	}

	*tmpAddr = (UINT64)ArchNewPsGetCurrentProcess02;

	if (g_pVidLoadBase == NULL) {
		g_pVidLoadBase = FindDrvBaseAddress(sVidName);
		if (g_pVidLoadBase == NULL) {
			KDbgPrintString("Vid.sys was not found");
			return FALSE;
		}
	}


	RtlInitUnicodeString(&uFunctionName, L"PsGetCurrentProcess");
	pPsGetCurrentProcessOrig = MmGetSystemRoutineAddress(&uFunctionName);

	if (pPsGetCurrentProcessOrig == NULL) {
		return FALSE;
	}

	//
	//save original PsGetCurrentProcess in buffer
	//

	RtlCopyMemory(pSavedPsGetCurrentProcessFunction, pPsGetCurrentProcessOrig, SIZE_OF_ARCH_NEW_PS_FUNCTION);

	//
	// replace PsGetCurrentProcess to custom function
	//

	//KeInitializeSpinLock(&SpinLock);
	//KeAcquireInStackQueuedSpinLock(&SpinLock, &QueueHandle);
	//RtlCopyMemory(tmpAddr, &tmpAddr2, sizeof(ULONG64));
	KeRaiseIrql(HIGH_LEVEL, &kiCurrent);

	RtlCopyMemory(pPsGetCurrentProcessOrig, &ArchPsGetCurrentProcess, SIZE_OF_ARCH_NEW_PS_FUNCTION);
	bIsPsGetCurrentPsPatched = TRUE;

	KeLowerIrql(kiCurrent);

	//KeReleaseInStackQueuedSpinLock(&QueueHandle);

	//DbgPrintString("PsGetCurrentPsPatched was patched");
	//ArchmReplacePsGetCurrentProcess(&pPsGetCurrentProcessOrig, &ArchPsGetCurrentProcess);

	return TRUE;
}


BOOLEAN VidRestorePsGetCurrentProcess()
{

	//KSPIN_LOCK SpinLock;
	//KLOCK_QUEUE_HANDLE QueueHandle;
	KIRQL kiCurrent = 0;

	if (bIsPsGetCurrentPsPatched) {
		//KeInitializeSpinLock(&SpinLock);
		//KeAcquireInStackQueuedSpinLock(&SpinLock, &QueueHandle);
		KeRaiseIrql(HIGH_LEVEL, &kiCurrent);

		RtlCopyMemory(pPsGetCurrentProcessOrig, pSavedPsGetCurrentProcessFunction, SIZE_OF_ARCH_NEW_PS_FUNCTION);
		bIsPsGetCurrentPsPatched = FALSE;

		KeLowerIrql(kiCurrent);

		//KeReleaseInStackQueuedSpinLock(&QueueHandle);
		//ExFreePoolWithTag(pSavedPsGetCurrentProcessFunction, 'Hvmm');

		//DbgPrintString("PsGetCurrentPsPatched was restored");
	}

	return TRUE;
}

BOOLEAN PatchVidIOCtlHandler()
{
	ULONG i, ModuleCount;
	PSYSTEM_MODULE_INFORMATION pSystemModuleInformation = NULL;
	ULONG Len = 0;
	PVOID pBuffer;
	PVOID pVidModuleBase = NULL;
	BOOLEAN bFound = FALSE;
	//PMDL pMdl;
	NTSTATUS Status = 0;
	PULONG64 pArrayofReg, pArrayofValues, pUnknown02;
	HV_ACCESS_GPA_CONTROL_FLAGS ControlFlags = { 0 };
	HV_ACCESS_GPA_RESULT AccessResult;

	const char* sDriverName = "Vid.sys";
	unsigned char* pVidPatchPlace;

	EnumActivePartitionID();

	pArrayofReg = HvmmPoolAlloc(PAGE_SIZE);
	pArrayofValues = HvmmPoolAlloc(PAGE_SIZE);
	pUnknown02 = HvmmPoolAlloc(PAGE_SIZE);

	if ((pArrayofReg == NULL) | (pArrayofValues == NULL) | (pUnknown02 == NULL))
	{
		KDbgPrintString("ExAllocatePoolWithTag was failed");
		return FALSE;
	}

	memset(pArrayofReg, 0, PAGE_SIZE);
	memset(pArrayofValues, 0, PAGE_SIZE);
	memset(pUnknown02, 0, PAGE_SIZE);

	ZwQuerySystemInformation(SystemModuleInformation, &pSystemModuleInformation, 0, &Len);
	KDbgLog("Length ", Len);
	pBuffer = MmAllocateNonCachedMemory(Len);
	KDbgLog16("pBuffer ", (ULONG64)pBuffer);

	if (!pBuffer)
	{
		KDbgPrintString("PatchVidIOCtlHandler. pBuffer allocation failed");
		return FALSE;
	}

	if (ZwQuerySystemInformation(SystemModuleInformation, pBuffer, Len, &Len)) {
		KDbgPrintString("PatchVidIOCtlHandler. ZwQuerySystemInformation failed");
		MmFreeNonCachedMemory(pBuffer, Len);
		return FALSE;
	}

	ModuleCount = *(UINT32*)pBuffer;
	KDbgLog("ModuleCount ", ModuleCount);
	pSystemModuleInformation = (PSYSTEM_MODULE_INFORMATION)((unsigned char*)pBuffer + sizeof(size_t));
	for (i = 0; i < ModuleCount; i++) {
		//DbgPrintEx(DPFLTR_IHVDRIVER_ID, DBG_PRINT_LEVEL,"pSystemModuleInformation->ImageName = %s\n",pSystemModuleInformation->Module->ImageName);
		if (strstr(pSystemModuleInformation->Module->ImageName, sDriverName)) //driver name is case-sensitive
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID, DBG_PRINT_LEVEL, "Driver found = %s\n", pSystemModuleInformation->Module->ImageName);
			pVidModuleBase = pSystemModuleInformation->Module->Base;
			pVidPatchPlace = (unsigned char*)pVidModuleBase + VID_IOCTL_HANDLER_PATCH_OFFSET;
			//KDbgLog16("pBuffer ", pVidPatchPlace);
			//pVidPatchPlace = 0xfffff802b7ba0de2ULL;
			//*pVidPatchPlace = 0x90;
			//pMdl = IoAllocateMdl(pVidPatchPlace, PAGE_SIZE, FALSE, FALSE, NULL);
			//MmBuildMdlForNonPagedPool(pMdl);
			//Status = MmProtectMdlSystemAddress(pMdl, PAGE_READWRITE);
			//KDbgLog("Status of MmProtectMdlSystemAddress", Status);
			//ModCR0();
			//*pVidPatchPlace = 0xeb;
			//*(pVidPatchPlace + 1) = 0xeb;
			//*pArrayofReg = (ULONG64)HvX64RegisterCr0;
			//*pArrayofReg = (ULONG64)HvX64RegisterCr0;
			//*pArrayofValues = (ULONG64)0x80040033;
			//Status = WinHvSetVpRegisters(1, 1, 0, 1, pArrayofReg, pArrayofValues, pUnknown02);
			//KDbgLog("Status of WinHvSetVpRegisters", Status);
			//IoFreeMdl(pMdl);
			//HvProp = 0x002BB9FF00003FFFuLL;

			//Status = WinHvSetPartitionProperty(GpaInfo.PartitionId, HvPartitionPropertyPrivilegeFlags, HvProp);
			//KDbgLog("Status of WinHvSetPartitionProperty", Status);
			//Status = WinHvSetPartitionProperty(1, HvPartitionPropertyPrivilegeFlags, HvProp);
			//KDbgLog("Status of WinHvSetPartitionProperty", Status);
			ControlFlags.CacheType = HvCacheTypeX64WriteBack;
			ControlFlags.InputVtl = 0;
			Status = WinHvReadGpa(3, 1, 0x10000, 0x10, ControlFlags, &AccessResult, pUnknown02);
			KDbgLog("Status of WinHvReadGpa", Status);
			//WinHvMapGpaPages(1,0x10,);
			KDbgLog("AccessResult", AccessResult.ResultCode);
			bFound = TRUE;
			KDbgLog16("vid.sys PsGetCurrentProcess check was patched ", (ULONG64)pVidPatchPlace);
		}
		pSystemModuleInformation++;
	}
	if (bFound == FALSE) {
		KDbgPrintString("vid.sys driver was not found");
	}
	//MmFreeNonCachedMemory(pBuffer, Len);
	ExFreePoolWithTag(pArrayofReg, 'Hvmm');
	ExFreePoolWithTag(pArrayofValues, 'Hvmm');
	ExFreePoolWithTag(pUnknown02, 'Hvmm');
	return TRUE;
}