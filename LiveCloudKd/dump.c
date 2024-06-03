/*++
	Microsoft Hyper-V Virtual Machine Physical Memory Dumper
	Copyright (C) Matt Suiche. All rights reserved.

Module Name:

	- dump.c

Abstract:

	- This header file contains definition used by LiveCloudKd (2010) and open-sourced in December 2018 after
	collaborating with Arthur Khudyaev (@gerhart_x) to revive the project.

	More information can be found on the original repository: https://github.com/comaeio/LiveCloudKd

	Original 2010 blogpost: https://blogs.technet.microsoft.com/markrussinovich/2010/10/09/livekd-for-virtual-machine-debugging/

Environment:

	- User mode

Revision History:

	- Arthur Khudyaev (@gerhart_x) - Many fixes of bugs
	- Arthur Khudyaev (@gerhart_x) - 18-Apr-2019 - Add additional methods (using Microsoft winhv.sys and own hvmm.sys driver) for reading guest memory
	- Arthur Khudyaev (@gerhart_x) - 20-Feb-2019 - Migrate parto of code to LiveCloudKd plugin
	- Arthur Khudyaev (@gerhart_x) - 26-Jan-2019 - Migration to MemProcFS/LeechCore
	- Matthieu Suiche (@msuiche) 11-Dec-2018 - Open-sourced LiveCloudKd in December 2018 on GitHub
	- Arthur Khudyaev (@gerhart_x) - 28-Oct-2018 - Add partial Windows 10 support
	- Matthieu Suiche (@msuiche) 09-Dec-2010 - Initial version from LiveCloudKd and presented at BlueHat 2010

--*/

#include "hvdd.h"

READ_MEMORY_METHOD g_MemoryReadInterfaceType = ReadInterfaceUnsupported;
WRITE_MEMORY_METHOD g_MemoryWriteInterfaceType = WriteInterfaceUnsupported;

BOOLEAN
DumpMemoryBlock(
	_In_ ULONG64 PartitionEntry,
	_In_ LPCWSTR DestinationFile,
	_In_ ULONG64 Start,
	_In_ ULONG64 Size,
	_In_ GUEST_TYPE DumpMode
)
{
	HANDLE FileHandle = INVALID_HANDLE_VALUE;

	PVOID Buffer = NULL;

	ULONG64 Index;
	BOOLEAN Ret = FALSE;

	ULONG64 lPageCountTotal;

	lPageCountTotal = Size / PAGE_SIZE;

	if (CreateDestinationFile(DestinationFile, &FileHandle) == FALSE) goto Exit;

	Buffer = malloc(BLOCK_SIZE);

	if (Buffer == NULL) goto Exit;

	White(L"   PageCountTotal = 0x%x\n", lPageCountTotal);
	Green(L"\n"
		L"   Total Size: %d MB\n", (Size / (1024 * 1024)));
	White(L"   Starting... \n");

	for (Index = Start;
		Index < Start+Size;
		Index += BLOCK_SIZE)
	{
		if (Index % (10 * 1024 * 1024) == 0) {
			printf("%I64d MBs... \n", (Index / (1024 * 1024)));
		}


		Ret = SdkReadPhysicalMemory(PartitionEntry,
			Index,
			BLOCK_SIZE,
			Buffer,
			g_MemoryReadInterfaceType
		);

		if (Ret == TRUE) {
			WriteFileSynchronous(FileHandle, Buffer, BLOCK_SIZE);
		}
		else
		{
			RtlZeroMemory(Buffer, BLOCK_SIZE);
			WriteFileSynchronous(FileHandle, Buffer, BLOCK_SIZE);
		}
	}

	Green(L"   Done.\n");

	Ret = TRUE;
Exit:
	if (FileHandle != INVALID_HANDLE_VALUE) CloseHandle(FileHandle);

	return Ret;
}

BOOLEAN
DumpVirtualMachine(
	_In_ ULONG64 PartitionEntry,
	_In_ LPCWSTR DestinationFile
)
{
    HANDLE FileHandle = INVALID_HANDLE_VALUE;

    PVOID Buffer = NULL;

    ULONG64 Index;
    BOOLEAN Ret = FALSE;

    ULONG64 lPageCountTotal = 0;

	SdkGetData(PartitionEntry, InfoMmMaximumPhysicalPage, &lPageCountTotal);

    if (CreateDestinationFile(DestinationFile, &FileHandle) == FALSE) goto Exit;

    Buffer = malloc(BLOCK_SIZE);
    if (Buffer == NULL) goto Exit;

    White(L"PageCountTotal = 0x%x\n", lPageCountTotal);
    Green(L"\n"
          L"   Total Size: %d MB\n", (ULONG)(((ULONG)lPageCountTotal * PAGE_SIZE) / (1024 * 1024)));
    White(L"   Starting... \n");

    for (Index = 0;
         Index < (ULONG)lPageCountTotal;
         Index += (BLOCK_SIZE / PAGE_SIZE))
    {
		if (Index % (10 * 1024) == 0) {
			printf("%I64d MBs... \n", (Index * PAGE_SIZE) / (1024 * 1024));
		}

		Ret = SdkReadPhysicalMemory(PartitionEntry,
			Index*PAGE_SIZE,
			BLOCK_SIZE,
			Buffer,	
			g_MemoryReadInterfaceType
		);

		if (Ret == TRUE) {
			WriteFileSynchronous(FileHandle, Buffer, BLOCK_SIZE);
		}
		else
		{
			RtlZeroMemory(Buffer, BLOCK_SIZE);
			WriteFileSynchronous(FileHandle, Buffer, BLOCK_SIZE);
		}
	}

    Green(L"Done.\n");

    Ret = TRUE;
Exit:
    if (FileHandle != INVALID_HANDLE_VALUE) CloseHandle(FileHandle);

    return Ret;
}

BOOLEAN
DumpCrashVirtualMachine(
	_In_ ULONG64 PartitionEntry,
	_In_ LPCWSTR DestinationFile
)
{
	ULONG HeaderSize = 0;
	PVOID Header = NULL;

	HANDLE FileHandle = INVALID_HANDLE_VALUE;

	ULONG64 PageCountTotal = 0;
	ULONG64 Index;

	PVOID Buffer = NULL;

	PHYSICAL_ADDRESS ContextPa;
	PULONG64 ContextVa = NULL;
	PULONG64 CpuContextVa = NULL;
	ULONG64 NumberOfCPU = 0;

	BOOLEAN Ret = FALSE;
	ULONG64 ContextPageIndex[MAX_PROCESSORS];
	ULONG ContextOffsetLow[MAX_PROCESSORS];
	CONTEXT Context;
	KDDEBUGGER_DATA64 KdDebuggerDataBlockBlock;

	ULONG64 KDBGPa;
	PHYSICAL_ADDRESS KdDebuggerDataBlockPa;
	PKDDEBUGGER_DATA64 pTmpKdBlock = NULL;

    if (DumpFillHeader(PartitionEntry, &Header, &HeaderSize) == FALSE) goto Exit;

    Buffer = malloc(BLOCK_SIZE);
    if (Buffer == NULL) goto Exit;

    if (CreateDestinationFile(DestinationFile, &FileHandle) == FALSE) goto Exit;

    if (WriteFileSynchronous(FileHandle, Header, HeaderSize) == FALSE) goto Exit;

	SdkGetData(PartitionEntry, InfoMmMaximumPhysicalPage, &PageCountTotal);
    PageCountTotal += (HeaderSize / PAGE_SIZE);

    Green(L"\n"
          L"   Total Size: %d MB\n", (ULONG)((PageCountTotal * PAGE_SIZE) / (1024 * 1024)));
    White(L"   Starting... ");

    if (SdkGetMachineType(PartitionEntry) == MACHINE_X86)
    {
		wprintf(L"MACHINE_X86 guest machine type is not supported\n");
		Ret = FALSE;
		goto Exit;
    }  

	SdkGetData(PartitionEntry, InfoNumberOfCPU, &NumberOfCPU);
	SdkGetData(PartitionEntry, InfoKdbgContext, &ContextVa);
	SdkGetData(PartitionEntry, InfoCpuContextVa, &CpuContextVa);

	for (ULONG i = 0; i < NumberOfCPU; i++)
	{
		ContextPa.QuadPart = SdkGetPhysicalAddress(PartitionEntry, ContextVa[i], MmVirtualMemory);

		ContextPageIndex[i] = ContextPa.QuadPart;
		wprintflvl1(L"FunctionTable.ContextPa 0x%I64X, CPU[%d]\n", ContextPa.QuadPart, i);
		wprintflvl1(L"FunctionTable.ContextPageIndex %llx, CPU[%d]\n", ContextPageIndex[i], i);
		wprintflvl1(L"FunctionTable.ContextOffsetLow %x, CPU[%d]\n", ContextOffsetLow[i], i);
	}

	for (ULONG i = 0; i < NumberOfCPU; i++)
	{
		SdkReadVirtualMemory(PartitionEntry, CpuContextVa[i], &Context, sizeof(CONTEXT));
		if (Context.Rip != 0)
		{
			wprintflvl1(L"GuestContext RSP = 0x%llx\n", Context.Rip);
			wprintflvl1(L"GuestContext RIP = 0x%llx\n", Context.Rsp);
			break;
		}
	}

	SdkGetData(PartitionEntry, InfoKdbgDataBlockArea, &pTmpKdBlock);
	SdkGetData(PartitionEntry, InfoKDBGPa, &KdDebuggerDataBlockPa.QuadPart);
	RtlCopyMemory(&KdDebuggerDataBlockBlock, pTmpKdBlock, sizeof(KDDEBUGGER_DATA64));

	//
	// Dump memory blocks
	//

    for (Index = 0;
         Index < (PageCountTotal * PAGE_SIZE);
         Index += BLOCK_SIZE )
    {
		
		Ret = SdkReadPhysicalMemory(PartitionEntry,	Index, BLOCK_SIZE, Buffer, g_MemoryReadInterfaceType);
		
		if (Ret)
        {		
			for (ULONG i = 0; i < NumberOfCPU; i++)
			{
				if ((ContextPageIndex[i] >= Index) && (ContextPageIndex[i] < (Index + BLOCK_SIZE)))
				{
					PUCHAR C;
					PX64_CONTEXT Context64;

					C = (PUCHAR)Buffer + ContextPageIndex[i] - Index;

					if ((Context.SegCs == 0) || (Context.Rip == 0) || (Context.Rsp == 0))
					{
						Context64 = (PX64_CONTEXT)C;
						Context64->SegCs = KGDT64_R0_CODE;
						Context64->SegDs = (KGDT64_R3_DATA | RPL_MASK);
						Context64->SegEs = (KGDT64_R3_DATA | RPL_MASK);
						Context64->SegFs = (KGDT64_R3_CMTEB | RPL_MASK);
						Context64->SegGs = 0;
						Context64->SegGs = (KGDT64_R3_DATA | RPL_MASK);
					}
					else
					{
						RtlCopyMemory(C,&Context, sizeof(CONTEXT));
					}
				}
			}

			KDBGPa = KdDebuggerDataBlockPa.QuadPart;
		
			if ((KDBGPa >= Index) && (KDBGPa + sizeof(KDDEBUGGER_DATA64) <= (Index + BLOCK_SIZE)))
			{
				ULONG64 j = KDBGPa - Index; 
				for (ULONG i = 0; i < sizeof(KDDEBUGGER_DATA64); i++)
				{
					PUCHAR pKdbg = (PUCHAR)Buffer;
					PUCHAR pKdbgBlock = (PUCHAR)&KdDebuggerDataBlockBlock;
					pKdbg[i + j] = pKdbgBlock[i]; 
				}
			}
			
            WriteFileSynchronous(FileHandle, Buffer, BLOCK_SIZE);
        }
        else
        {
            RtlZeroMemory(Buffer, BLOCK_SIZE);
            WriteFileSynchronous(FileHandle, Buffer, BLOCK_SIZE);
        }
    }

    Green(L"Done.\n");

    Ret = TRUE;

Exit:
    if (Buffer) free(Buffer);
    if (Header) free(Header);

    if (FileHandle != INVALID_HANDLE_VALUE) CloseHandle(FileHandle);

    return Ret;
}

BOOLEAN
DumpLiveVirtualMachine(
	_In_ ULONG64 PartitionEntry,
	_In_ ULONG64 VmId
)
{
    ULONG HeaderSize = 0;
    PVOID Header = NULL;

    HANDLE HvddFile = NULL;

    ULONG64 PageCountTotal = 0;

    PVOID Buffer = NULL;

    BOOLEAN Ret = FALSE;

    USHORT Color;
    HANDLE Handle;

    WCHAR WindowsDir[MAX_PATH];
	WCHAR CrashFilePath[MAX_PATH] = {0};

    PHYSICAL_ADDRESS ContextPa;
    PKDDEBUGGER_DATA64 pTmpKdBlock = NULL;

	if (g_UseEXDi == FALSE)
	{
		GetWindowsDirectory(WindowsDir, sizeof(WindowsDir) / sizeof(WindowsDir[0]));
		swprintf_s(CrashFilePath, sizeof(CrashFilePath) / sizeof(CrashFilePath[0]),
			L"%s\\hvdd.dmp", WindowsDir);

		HvddFile = CreateFile(CrashFilePath,
			GENERIC_WRITE,
			FILE_SHARE_READ,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_HIDDEN | FILE_FLAG_NO_BUFFERING,
			NULL
		);

		if (HvddFile == INVALID_HANDLE_VALUE) goto Exit;
		
		if (DumpFillHeader(PartitionEntry, &Header, &HeaderSize) == FALSE) goto Exit;
		
		FunctionTable._LoadLibrary = LoadLibraryW;
		FunctionTable._GetProcAddress = GetProcAddress;
		FunctionTable._MessageBoxW = MessageBoxW;
		FunctionTable._MessageBoxA = MessageBoxA;
		FunctionTable._CreateFileW = CreateFileW;
		FunctionTable._SetFilePointer = SetFilePointer;
		FunctionTable._VirtualAlloc = VirtualAlloc;
		FunctionTable._VirtualFree = VirtualFree;
		FunctionTable._VirtualProtect = VirtualProtect;
		FunctionTable._ReadFile = ReadFile;
		FunctionTable.VmId = VmId;
		FunctionTable.PartitionInit = FALSE;
		FunctionTable.CurrentPartitionHandle = 0;
		FunctionTable._SdkHvmmReadPhysicalMemoryHandle = SdkReadPhysicalMemory;
		FunctionTable._SdkSelectPartitionHandle = SdkSelectPartition;
		FunctionTable._SdkEnumPartitionsHandle = SdkEnumPartitions;
		FunctionTable._SdkSetData = SdkSetData;
		FunctionTable._CreateFileMappingA = CreateFileMappingA;
		FunctionTable._CreateFileMappingW = CreateFileMappingW;
		FunctionTable._MapViewOfFile = MapViewOfFile;
		FunctionTable._UnmapViewOfFile = UnmapViewOfFile;
		FunctionTable._SetLastError = SetLastError;

		FunctionTable.CrashDumpHandle = INVALID_HANDLE_VALUE;
		FunctionTable.HeaderSize = HeaderSize;
		FunctionTable.Header = Header;
		FunctionTable.MemoryHandle = NULL;

		RtlCopyMemory(&FunctionTable.VmOpsConfig, &g_VmOperationsConfig, sizeof(VM_OPERATIONS_CONFIG));

		SdkGetData(PartitionEntry, InfoKernelBase, &FunctionTable.KernelBase);
		SdkGetData(PartitionEntry, InfoPartitionHandle, &FunctionTable.PartitionHandle);
		SdkGetData(PartitionEntry, InfoPartitionHandle, &FunctionTable.PartitionHandleConst);
		SdkGetData(PartitionEntry, InfoMmMaximumPhysicalPage, &FunctionTable.FileSize.QuadPart);
		FunctionTable.FileSize.QuadPart = FunctionTable.FileSize.QuadPart * PAGE_SIZE + HeaderSize;
		SdkGetData(PartitionEntry, InfoPartitionId, &FunctionTable.PartitionId);
		SdkGetData(PartitionEntry, InfoIdleKernelStack, &FunctionTable.IdleKernelStack);

		PVOID HvddPointer = NULL;
		ULONG64 SizeOfHvdd = 0;
		SdkGetData(PartitionEntry, InfoStructure, &HvddPointer);
		SdkGetData(PartitionEntry, InfoSize, &SizeOfHvdd);
		
		RtlCopyMemory(&(FunctionTable.HvddPartition), HvddPointer, SizeOfHvdd);

		if (SizeOfHvdd > sizeof(FunctionTable.HvddPartition)) {
			wprintf(L"Error. Size mismatch. SizeOfHvdd = 0x%llx\n", SizeOfHvdd);
			return FALSE;
		}

		SdkGetData(PartitionEntry, InfoKDBGPa, &FunctionTable.KdDebuggerDataBlockPa.QuadPart);

		SdkGetData(PartitionEntry, InfoKdbgDataBlockArea, &pTmpKdBlock);
		SdkGetData(PartitionEntry, InfoNumberOfCPU, &FunctionTable.NumberOfCPU);

		PULONG64 ContextVa = NULL;
		PULONG64 CpuContextVa = NULL;

		SdkGetData(PartitionEntry, InfoKdbgContext, &ContextVa);
		SdkGetData(PartitionEntry, InfoCpuContextVa, &CpuContextVa);

		pTmpKdBlock->SavedContext = ContextVa[0];
		RtlCopyMemory(FunctionTable.KdDebuggerDataBlockBlock, pTmpKdBlock, KD_DEBUGGER_BLOCK_PAGE_SIZE);

		for (ULONG i = 0; i < FunctionTable.NumberOfCPU; i++)
		{
			ContextPa.QuadPart = SdkGetPhysicalAddress(PartitionEntry, ContextVa[i], MmVirtualMemory);
			FunctionTable.ContextPageIndex[i] = (ContextPa.QuadPart / PAGE_SIZE);
			FunctionTable.ContextOffsetLow[i] = (ContextPa.LowPart & (PAGE_SIZE - 1));
		}

		for (ULONG i = 0; i < FunctionTable.NumberOfCPU; i++)
		{
			SdkReadVirtualMemory(PartitionEntry, CpuContextVa[i], &FunctionTable.Context, sizeof(CONTEXT));
			if (FunctionTable.Context.Rip != 0)
			{
				wprintflvl1(L"GuestContext RSP = 0x%llx\n", FunctionTable.Context.Rip);
				wprintflvl1(L"GuestContext RIP = 0x%llx\n", FunctionTable.Context.Rsp);
				break;
			}		
		}

		wprintflvl1(L"FunctionTable.FileSize.QuadPart = 0x%llx\n", FunctionTable.FileSize.QuadPart);

		FunctionTable.MachineType = SdkGetMachineType(PartitionEntry);
		FunctionTable.IsDllLoad = FALSE;

		Handle = GetStdHandle(STD_OUTPUT_HANDLE);
		Color = GetConsoleTextAttribute(Handle);
		SetConsoleTextAttribute(Handle, 0xF);

		LaunchKd(CrashFilePath, PartitionEntry);
	}

	if (g_UseWinDbg == TRUE) {
		LaunchWinDbg(PartitionEntry);
	}
	if (g_UseWinDbgLive == TRUE) {
		LaunchWinDbgLive(PartitionEntry);
	}
	if (g_UseWinDbgX == TRUE) {
		LaunchWinDbgX(PartitionEntry);
	}

    getchar();

    Ret = TRUE;

Exit:
    if (Buffer) free(Buffer);
    if (Header) free(Header);

    if (HvddFile) 
		CloseHandle(HvddFile);

    DeleteFile(CrashFilePath);

    return Ret;
}

BOOLEAN
DumpFillHeader(
	_In_ ULONG64 PartitionEntry,
	_In_ PVOID *Header,
	_In_ PULONG HeaderSize
)
{
BOOLEAN Ret = FALSE;

    if (SdkGetMachineType(PartitionEntry) == MACHINE_X86)
    {
        Red(L"MACHINE_X86 is not supported\n");
		Ret = FALSE;
    }
    else if (SdkGetMachineType(PartitionEntry) == MACHINE_AMD64)
    {
        *Header = DumpFillHeader64(PartitionEntry);
        *HeaderSize = sizeof(DUMP_HEADER64);
        if (*Header) Ret = TRUE;

    }

    return Ret;
}

PDUMP_HEADER64
DumpFillHeader64(
	_In_ ULONG64 PartitionEntry
)
{
    PHYSICAL_MEMORY_DESCRIPTOR64 MmPhysicalMemoryBlock64;
    EXCEPTION_RECORD64 Exception64;
    PDUMP_HEADER64 Header64 = NULL;
    SYSTEMTIME SystemTime;
    ULONG i;
    PUCHAR Buffer = NULL;
    BOOLEAN Ret;
	PCONTEXT pContext = NULL;

    Header64 = (PDUMP_HEADER64)malloc(sizeof(DUMP_HEADER64));
    if (Header64 == NULL) goto Exit;

    for (i = 0; i < sizeof(DUMP_HEADER64) / sizeof(ULONG); i += 1)
    {
        ((PULONG)Header64)[i] = DUMP_SIGNATURE;
    }

    //
    // Initialize header.
    //

    Header64->Signature = DUMP_SIGNATURE;
    Header64->ValidDump = DUMP_VALID_DUMP64;
    Header64->DumpType = DUMP_TYPE_FULL;
    Header64->MachineImageType = IMAGE_FILE_MACHINE_AMD64;

	ULONG64 NtBuildNumber = 0;
	SdkGetData(PartitionEntry, InfoNtBuildNumber, &NtBuildNumber);
	
    Header64->MinorVersion = (ULONG)(NtBuildNumber & 0xFFFF);
    Header64->MajorVersion = (ULONG) (NtBuildNumber >> 28);

	SdkGetData(PartitionEntry, InfoDirectoryTableBase, &Header64->DirectoryTableBase);
	SdkGetData(PartitionEntry, InfoMmPfnDatabase, &Header64->PfnDataBase);
	SdkGetData(PartitionEntry, InfoPsLoadedModuleList, &Header64->PsLoadedModuleList);
	SdkGetData(PartitionEntry, InfoPsActiveProcessHead, &Header64->PsActiveProcessHead);

	ULONG64 NumberOfCPU = 0;
	SdkGetData(PartitionEntry, InfoNumberOfCPU, &NumberOfCPU);

    Header64->NumberProcessors = (ULONG)NumberOfCPU;
	SdkGetData(PartitionEntry, InfoKdbgData, &Header64->KdDebuggerDataBlock);

    Header64->BugCheckCode = 'MATT';
    Header64->BugCheckParameter1 = 0x1;
    Header64->BugCheckParameter2 = 0x2;
    Header64->BugCheckParameter3 = 0x3;
    Header64->BugCheckParameter4 = 0x4;

    RtlZeroMemory(Header64->VersionUser, sizeof(Header64->VersionUser));

	ULONG64 MmMaximumPhysicalPage = 0;
	SdkGetData(PartitionEntry, InfoMmMaximumPhysicalPage, &MmMaximumPhysicalPage);

	ULONG64 NumberOfPages = 0;
	SdkGetData(PartitionEntry, InfoNumberOfPages, &NumberOfPages);

	ULONG64 NumberOfRuns = 0;
	SdkGetData(PartitionEntry, InfoNumberOfRuns, &NumberOfRuns);

	ULONG64 hRun = 0;
	SdkGetData(PartitionEntry, InfoRun, &hRun);

	MmPhysicalMemoryBlock64.NumberOfPages = MmMaximumPhysicalPage;
    MmPhysicalMemoryBlock64.NumberOfRuns = 1;
	MmPhysicalMemoryBlock64.Run[0].BasePage = 0;
	MmPhysicalMemoryBlock64.Run[0].PageCount = MmMaximumPhysicalPage;

    RtlCopyMemory(&Header64->PhysicalMemoryBlock,
                  &MmPhysicalMemoryBlock64,
                  sizeof(PHYSICAL_MEMORY_DESCRIPTOR64));

    //
    // Exception record.
    //

    Exception64.ExceptionCode = STATUS_BREAKPOINT;
    Exception64.ExceptionRecord = 0;
    Exception64.NumberParameters = 0;
    Exception64.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
    Exception64.ExceptionAddress = 0xDEADBABE;

    RtlCopyMemory(&Header64->ExceptionRecord,
                  &Exception64,
                  sizeof(EXCEPTION_RECORD64));

    GetSystemTime(&SystemTime);

    SystemTimeToFileTime(&SystemTime, &Header64->SystemTime);

    RtlZeroMemory(&Header64->RequiredDumpSpace, sizeof(LARGE_INTEGER));

	PULONG64 ContextVa = 0;
	ULONG64 KiProcessorBlock = 0;

	SdkGetData(PartitionEntry, InfoKdbgContext, &ContextVa);
	SdkGetData(PartitionEntry, InfoKiProcessorBlock, &KiProcessorBlock);

	wprintflvl1(L"KiExcaliburData.KiProcessorBlock 0x%llx \n", KiProcessorBlock);
    
	wprintflvl1(L"FunctionTable.ContextVa 0x%llx \n", ContextVa[0]); 

    Header64->RequiredDumpSpace.QuadPart = 
        (MmMaximumPhysicalPage * PAGE_SIZE) + sizeof(DUMP_HEADER64);

    RtlZeroMemory(Header64->ContextRecord, sizeof(Header64->ContextRecord));

    Buffer = malloc(sizeof(CONTEXT));
    if (Buffer == NULL) {
        printf("malloc (CONTEXT) failed\n");
        return FALSE;
    } 

    Ret = SdkReadVirtualMemory(PartitionEntry,
		ContextVa[0],
        Buffer,
		sizeof(CONTEXT));

    if (Ret == FALSE) {
        printf("Copy CONTEXT error\n");
        return FALSE;
    }

    RtlCopyMemory(Header64->ContextRecord,
        Buffer,
        sizeof(CONTEXT));

	pContext = (PCONTEXT) Header64->ContextRecord;

	if (pContext->SegCs != KGDT64_R0_CODE) pContext->SegCs = KGDT64_R0_CODE;
	if (pContext->SegDs != (KGDT64_R3_DATA | RPL_MASK)) pContext->SegDs = (KGDT64_R3_DATA | RPL_MASK);
	if (pContext->SegEs != (KGDT64_R3_DATA | RPL_MASK)) pContext->SegEs = (KGDT64_R3_DATA | RPL_MASK);
	if (pContext->SegFs != (KGDT64_R3_CMTEB | RPL_MASK)) pContext->SegFs = (KGDT64_R3_CMTEB | RPL_MASK);
	if (pContext->SegGs != 0) pContext->SegGs = 0;
	if (pContext->SegSs != KGDT64_R0_DATA) pContext->SegSs = KGDT64_R0_DATA;
 
    RtlZeroMemory(Header64->Comment, sizeof(Header64->Comment));
    strcpy_s(Header64->Comment, sizeof(Header64->Comment),
        DUMP_COMMENT_STRING);

Exit:
    free(Buffer);
    return Header64;
}