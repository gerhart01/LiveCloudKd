#include "hvmm.h"

PVOID g_vmmemHandle = NULL;
EPROCESS_INTERNALS EprocessInternalData = { 0 };

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

PGPAR_OBJECT VidGetGparObjectForGpa(PVM_PROCESS_CONTEXT pPartitionHandle, UINT64 GPA)
{

	UINT32 Index = 0;
	PGPAR_BLOCK_HANDLE pGparBlockHandle;
	PUINT64 pGparArray;
	UINT64 uElement = 0;

	PGPAR_OBJECT objGpar = NULL;

	pGparBlockHandle = pPartitionHandle->pGparBlockHandle;

	if (pGparBlockHandle == 0)
	{
		pGparBlockHandle = pPartitionHandle->pGparBlockHandle20H1;
		if (pGparBlockHandle == 0) {
			KDbgPrintString("\tSomething wrong with offset of GparBlockHandle");
			return NULL;
		}
	}

	Index = pGparBlockHandle->CountInGparArray;
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
			KDbgLog16("pGparElement->GpaIndexStart", objGpar->GpaIndexStart);
			KDbgLog16("pGparElement->GpaIndexEnd",objGpar->GpaIndexEnd);

			if ((GPA >= objGpar->GpaIndexStart) && (GPA <= objGpar->GpaIndexEnd))
			{
				return objGpar;
			}
		}
		else
		{
			KDbgLog("\tGpar Element is NULL, i = ", i);
		}
	} // end for

	if (!objGpar)
		KDbgPrintString("\tGpar element wasn't found");

	return objGpar;
}

//
// Read memory block from Hyper-V container
//

BOOLEAN VidGetContainerMemoryBlock(PVM_PROCESS_CONTEXT pPartitionHandle, PCHAR pBuffer, ULONG len, ULONG64 GPA)
{
	PGPAR_OBJECT objGpar = NULL;
	BOOLEAN Ret = FALSE;
	ULONG64 SourceAddress;
	ULONG64 uBlocks, uRemainBytes, i;

	//DbgBreakPoint();

	if (g_vmmemHandle == NULL) {
		Ret = FALSE;
	}

	uBlocks = len / PAGE_SIZE;
	uRemainBytes = len % PAGE_SIZE;

	for (i = 0; i < uBlocks; i++)
	{

		objGpar = VidGetGparObjectForGpa(pPartitionHandle, GPA+i);

		if (objGpar == NULL) {
			return FALSE;
		}

		SourceAddress = (GPA + i - objGpar->GpaIndexStart - objGpar->SomeGpaOffset) * PAGE_SIZE + objGpar->VmmMemGpaOffset;
		if (objGpar->SomeGpaOffset != 0) {
			KDbgLog16("   objGpar->SomeGpaOffset", objGpar->SomeGpaOffset);
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

		objGpar = VidGetGparObjectForGpa(pPartitionHandle, GPA + uBlocks);

		if (objGpar == NULL) {
			return FALSE;
		}

		SourceAddress = (GPA + uBlocks - objGpar->GpaIndexStart - objGpar->SomeGpaOffset) * PAGE_SIZE + objGpar->VmmMemGpaOffset;
		if (objGpar->SomeGpaOffset != 0) {
			//KDbgLog16("   pGparElement->SomeGpaOffset", pGparElement->SomeGpaOffset);
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

BOOLEAN VidGetFullVmMemoryBlock(PVM_PROCESS_CONTEXT pPartitionHandle, PCHAR pBuffer, ULONG len, ULONG64 GPA)
{
	PMEMORY_BLOCK objMBlock = NULL;
	PGPAR_OBJECT objGpar = NULL;

	ULONG64 HostSPA = 0;

	PMDL pMDL = NULL;
	PVOID VirtualAddress = NULL;
	PULONG64 MdlPfnArray = NULL;

	PVOID SourceAddress = NULL;
	ULONG64 uBlocks, uRemainBytes, i;

	//DbgBreakPoint();

	uBlocks = len / PAGE_SIZE;
	uRemainBytes = len % PAGE_SIZE;

	if (uRemainBytes != 0) {
		KDbgPrintString("Buffer Length must be paged size alignment");
		return FALSE;
	}

	for (i = 0; i < uBlocks; i++)
	{

		objGpar = VidGetGparObjectForGpa(pPartitionHandle, GPA+i);

		if (objGpar == NULL) {
			return FALSE;
		}

		if (objGpar->GpaIndexStart == objGpar->GpaIndexEnd) {
			KDbgPrintString("MBlock in GPAR object is vmwp.exe descriptor");
			return FALSE;
		}

		objMBlock = objGpar->objMBlock;

		HostSPA = *(PULONG)((PCHAR)objMBlock->pGuestGPAArray + 0x10 * (GPA- objGpar->GpaIndexStart +i));

		pMDL = IoAllocateMdl(VirtualAddress, PAGE_SIZE, FALSE, FALSE, NULL);

		if (pMDL == NULL) {
			KDbgPrintString("MDL allocation false");
			return FALSE;
		}
		
		MdlPfnArray = MmGetMdlPfnArray(pMDL);
		*MdlPfnArray = HostSPA;

		__try
		{
			SourceAddress = MmMapLockedPagesSpecifyCache(pMDL, KernelMode, MmCached, NULL, FALSE, NormalPagePriority);

			if (!SourceAddress)
			{
				IoFreeMdl(pMDL);
				return FALSE;
			}
				
			RtlCopyMemory(pBuffer + i * PAGE_SIZE, SourceAddress, PAGE_SIZE);

			MmUnmapLockedPages(SourceAddress, pMDL);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			KDbgLog("   RtlCopyMemory failed", GetExceptionCode());
		}
		
		IoFreeMdl(pMDL);
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
	//PGPAR_ELEMENT pGparElement = NULL;
	UINT64 GPA;
	BOOLEAN Ret = FALSE;

	//ULONG64 SourceAddress;

	//DbgBreakPoint();

	RtlCopyMemory(&GpaInfo, pBuffer, sizeof(GpaInfo));
	memset(pBuffer, 0, len);

	GPA = GpaInfo.StartPage / PAGE_SIZE;
	//KDbgLog16("GPA = ", GPA);

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
		return FALSE;
	}

	if (objVmPartition->FsContext != NULL)
	{
		pPartitionHandle = (PVM_PROCESS_CONTEXT)((PCHAR)objVmPartition->FsContext - 1);

		switch (pPartitionHandle->VmType)
		{
		case VidVmTypeDockerHyperVContainerUserName:
		case VidVmTypeDockerHyperVContainerGUID:
		case VidVmTypeContainer:

			Ret = VidGetContainerMemoryBlock(pPartitionHandle, pBuffer, len, GPA);

			break;

		case VidVmTypeFullWin10VM:
		case VidVmTypeFullWinSrvVMSecure:
		case VidVmTypeFullWinSrvVM:

			Ret = VidGetFullVmMemoryBlock(pPartitionHandle, pBuffer, len, GPA);				
			break;

		default:
			break;
		}		
	}

	ObDereferenceObject(objVmPartition);
	return Ret;
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
		objMBlockArray = (PMEMORY_BLOCK_ARRAY) pPartitionHandle->ArrayOfMblocks;

		if (*(PULONG)objMBlockArray == 0) {

			objMBlockArray = pPartitionHandle->ArrayOfMblocks20H1;

			if (*(PULONG)objMBlockArray == 0) {
				KDbgPrintString("Something wrong with objMBlockArray offset");
				ObDereferenceObject(objVmPartition);
				return FALSE;
			}
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
    PUINT64 pGparArray;
    UINT64 uElement;
    UINT64 GPA;
    BOOLEAN Ret = FALSE;

    UINT32 Index = 0;
    LONG i;
 
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
		pGparBlockHandle = pPartitionHandle->pGparBlockHandle;

		if (pGparBlockHandle == 0)
		{
			pGparBlockHandle = pPartitionHandle->pGparBlockHandle20H1;

			if (pGparBlockHandle == 0)
			{
				KDbgPrintString("\tSomething wrong with offset of GparBlockHandle");
				ObDereferenceObject(objVmPartition);
				return FALSE;
			}
		}

        Index = pGparBlockHandle->CountInGparArray;
        pGparArray = (PUINT64)pGparBlockHandle->GparArray;

        for (i = Index-1; i >= 0; i--)
        {
           uElement = *((PUINT64)pGparArray + i);
           if (uElement != 0) 
           {
               pGparElement = (PGPAR_OBJECT)uElement;
               //KDbgLog("pGparElement->GpaIndexStart", pGparElement->GpaIndexStart);
               //KDbgLog("pGparElement->GpaIndexEnd",pGparElement->GpaIndexEnd);
               if ((GPA >= pGparElement->GpaIndexStart) && (GPA <= pGparElement->GpaIndexEnd)) 
               {
                   pGparBlockInfo->MemoryBlockPageIndex = GPA - pGparElement->GpaIndexStart;
				   pGparBlockInfo->MbHandle = (MB_HANDLE)pGparElement->objMBlock->MbHandle;
                  // pGparBlockInfo->MbHandle = VidGetMbBlockIndex(pPartitionHandle, pGparElement->objMBlockElement);
				   pGparBlockInfo->Count = Index;
                   //KDbgLog("BlockIndexInfo->Index", pGparBlockInfo->MemoryBlockPageIndex);
                   //KDbgLog("pGparBlockInfo->MbHandle", pGparBlockInfo->MbHandle);
                   Ret = TRUE;
                   break;
               }
           } 
           else
           {
               KDbgLog("pElement = 0, i = ", i);
           }
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
      	
		
		RtlCopyMemory(pBuffer, ((PCHAR)objVmPartition->FsContext - 1 + PARTITION_NAME_1803_OFFSET), sizeof(pVmInfo->FriendlyName)); // 0x78 - for Windows 10.1803. For Windows 2016 - 0x70 (but still works vid.dll). RtlGetVersion
        RtlCopyMemory(pBuffer+sizeof(pVmInfo->FriendlyName), ((PCHAR)objVmPartition->FsContext - 1 + PARTITION_ID_1803_OFFSET), sizeof(pVmInfo->PartitionId));
        
		pPartitionHandle = (PVM_PROCESS_CONTEXT)((PCHAR)objVmPartition->FsContext - 1);

		pVmInfo = (PVID_VM_INFO)pBuffer;

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