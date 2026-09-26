#include "LiveCloudKdExample.h"

ULONG64 g_CurrentPartitionIntHandle = 0;
READ_MEMORY_METHOD g_MemoryReadInterfaceType = ReadInterfaceWinHv;
WRITE_MEMORY_METHOD g_MemoryWriteInterfaceType = WriteInterfaceWinHv;

BOOLEAN AsciiToUnicode(PCHAR Asciistring, PWCHAR Unistring)
{
	ULONG64 len = strlen(Asciistring);
	if ((len > 0) & (len < 500))
	{
		for (ULONG i = 0; i < len; i++)
			Unistring[i] = Asciistring[i];

		return TRUE;
	}
	else
	{
		wprintf(L"ERROR: String length is too big %lld.\n", len);
	}

	return FALSE;
};

BOOL
CreateDestinationFile(LPCWSTR Filename,
	PHANDLE Handle)
{
	*Handle = CreateFile(Filename,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_FLAG_NO_BUFFERING,
		NULL);

	if (Handle == INVALID_HANDLE_VALUE) return FALSE;

	return TRUE;
}

BOOL
WriteFileSynchronous(
	HANDLE Handle,
	PVOID Buffer,
	ULONG NbOfBytesToWrite
)
{
	DWORD WrittenBytes;
	BOOL Ret;

	WrittenBytes = 0;
	Ret = FALSE;

	Ret = WriteFile(Handle, Buffer, NbOfBytesToWrite, &WrittenBytes, NULL);
	if ((Ret == FALSE) && (GetLastError() == ERROR_IO_PENDING))
	{
		do
		{
			Ret = WaitForSingleObjectEx(Handle, INFINITE, TRUE);
		} while (Ret == WAIT_IO_COMPLETION);
	}

	if (WrittenBytes == NbOfBytesToWrite)
	{
		Ret = TRUE;
	}

	return Ret;
}



BOOLEAN
DumpVirtualMachineHandle(
	_In_ ULONG64 PartitionIntHandle,
	_In_ LPCWSTR DestinationFile
)
{
	HANDLE FileHandle = INVALID_HANDLE_VALUE;

	PVOID Buffer = NULL;

	UINT64 Index;
	BOOLEAN Ret = FALSE;

	ULONG64 lPageCountTotal = 0;

	SdkGetData(PartitionIntHandle, InfoMmMaximumPhysicalPage, &lPageCountTotal);

	if (CreateDestinationFile(DestinationFile, &FileHandle) == FALSE) goto Exit;

	Buffer = malloc(DUMP_BLOCK_SIZE);
	if (Buffer == NULL) goto Exit;

	wprintf(L"PageCountTotal = 0x%llx\n", lPageCountTotal);
	wprintf(L"\n"
		L"   Total Size: %d MB\n", (ULONG)(((ULONG)lPageCountTotal * DUMP_PAGE_SIZE) / (1024 * 1024)));
	wprintf(L"   Starting... \n");

	for (Index = 0;
		Index < (ULONG)lPageCountTotal;
		Index += (DUMP_BLOCK_SIZE / DUMP_PAGE_SIZE))
	{
		if (Index % (10 * 1024) == 0) {
			wprintf(L"%I64d MBs... \n", (Index * DUMP_PAGE_SIZE) / (1024 * 1024));
		}

		Ret = SdkReadPhysicalMemory(PartitionIntHandle,
			Index,
			DUMP_BLOCK_SIZE,
			Buffer,
			g_MemoryReadInterfaceType
		);
		if (Ret == TRUE) {
			WriteFileSynchronous(FileHandle, Buffer, DUMP_BLOCK_SIZE);
		}
		else
		{
			RtlZeroMemory(Buffer, DUMP_BLOCK_SIZE);
			WriteFileSynchronous(FileHandle, Buffer, DUMP_BLOCK_SIZE);
		}
	}

	wprintf(L"Done.\n");

	Ret = TRUE;
Exit:
	if (FileHandle != INVALID_HANDLE_VALUE) CloseHandle(FileHandle);

	return Ret;
}


int g_AutoVmId = -1;
int g_AutoActionId = -1;

int DemoReadKey(int AutoAnswer)
{
	if (g_AutoActionId >= 0) {
		return AutoAnswer;
	}
	return _getch();
}

BOOLEAN Demo2()
{
	PULONG64 Partitions;
	ULONG64 PartitionCount = 0;
	ULONG i;
	ULONG VmId;
	BOOLEAN Ret = FALSE;
	LPCWSTR DestinationPath = NULL;
	WCHAR Destination[MAX_PATH + 1];
	ULONG ActionId = 0;
	ULONG Action = -1;
	ULONG64 PartitionId = 0;
	WCHAR* FriendlyNameP = NULL;
	VM_OPERATIONS_CONFIG VmOperationsConfig = { 0 };

	SdkGetDefaultConfig(&VmOperationsConfig);

	VmOperationsConfig.ReloadDriver = TRUE;

	Partitions = SdkEnumPartitions(&PartitionCount, &VmOperationsConfig);

	if (!Partitions)
	{
		wprintf(L"   Unable to get list of partitions\n");
		return FALSE;
	}

	wprintf(L"\n   Virtual Machines:\n");

	if (PartitionCount == 0)
	{
		wprintf(L"   --> No virtual machines running.\n");
		return FALSE;
	}

	for (i = 0; i < PartitionCount; i += 1)
	{
		ULONG64 PartitionId = 0;
		WCHAR* VmTypeString = NULL;
		CHAR* VmmNameString = NULL;
		WCHAR* VmGuidString = NULL;
		SdkGetData(Partitions[i], InfoPartitionFriendlyName, &FriendlyNameP);
		SdkGetData(Partitions[i], InfoPartitionId, &PartitionId);
		SdkGetData(Partitions[i], InfoVmtypeString, &VmTypeString);
		SdkGetData(Partitions[i], InfoVmGuidString, &VmGuidString);

		if ((wcslen(VmGuidString) > 0))
		{
			wprintf(L"    --> [%d] %s (PartitionId = 0x%I64X, %s, GUID: %s)\n", i, FriendlyNameP, PartitionId, VmTypeString, VmGuidString);
		}
		else
		{
			wprintf(L"    --> [%d] %s (PartitionId = 0x%I64X, %s)\n", i, FriendlyNameP, PartitionId, VmTypeString);
		}
	}

	VmId = 0;
	while ((VmId < '0') || (VmId > '9'))
	{
		wprintf(L"\n"
			L"   Please select the ID of the virtual machine you want to play with\n"
			L"   > ");
		VmId = DemoReadKey('0' + g_AutoVmId);
	}

	VmId = VmId - 0x30;
	wprintf(L"%d\n", VmId);

	if ((VmId + 1) > PartitionCount)
	{
		wprintf(L"ERROR:    The virtual machine you selected do not exist.\n");
		return FALSE;
	}

	wprintf(L"   You selected the following virtual machine: ");

	SdkGetData(Partitions[VmId], InfoPartitionFriendlyName, &FriendlyNameP);

	wprintf(L"%s\n", FriendlyNameP);

	wprintf(L"\n"
		L"   Action List (enter 1 for demo):\n");
	wprintf(L"    --> [1] Linear physical memory dump\n"); // Enter 1 for testing purposes. Save path looks like C:\Distr\Test\Example\test.raw
	wprintf(L"    --> [2] Mapped memory: verify and benchmark (needs hvmm.sys with IOCTL_MAP_GPA_RANGE)\n");
	wprintf(L"    --> [3] Dump the driver's enumeration reply and partition context (support)\n");

	if (g_AutoActionId >= 0)
	{
		Action = g_AutoActionId;
	}

	if (Action == -1)
	{
		ActionId = 0;
		FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
		wprintf(L"\n"
			L"   Please select the Action ID\n"
			L"   > ");
		while ((ActionId < '0') || (ActionId > '3'))
		{
			ActionId = _getch();
		}

		ActionId = ActionId - 0x30;
	}
	else
	{
		ActionId = Action;
	}

	wprintf(L"%d\n", ActionId);


	if (ActionId == 1)
	{
		wprintf(L"\n"
			L"   Destination path for the virtual machine physical memory dump\n"
			L"   > ");

		if (DestinationPath == NULL)
		{
			_getws_s(Destination, (_countof(Destination) - sizeof(Destination[0])));
			DestinationPath = Destination;
		}
		else
		{
			wprintf(L"%s\n", DestinationPath);
		}
	}

	g_CurrentPartitionIntHandle = Partitions[VmId];

	if (ActionId == 2)
	{
		//
		// Before anything that needs the partition to be readable: say what the driver's
		// layout scan found, so a VM that lists with no id or type explains itself.
		//
		MappedMemoryLayoutReport(g_CurrentPartitionIntHandle);
	}

	if (!SdkSelectPartition(g_CurrentPartitionIntHandle))
	{
		wprintf(L"ERROR:    Cannot initialize hvdd structure.\n");
		return FALSE;
	};

	if (ActionId == 2)
	{
		//
		// Mapped memory path only: no writes, no suspend/resume demo below.
		//
		Ret = MappedMemoryDemo(g_CurrentPartitionIntHandle);
		SdkClosePartition(g_CurrentPartitionIntHandle);
		SdkCloseAllPartitions();
		return Ret;
	}

	if (ActionId == 3)
	{
		MappedMemoryEnumDump(g_CurrentPartitionIntHandle);
		SdkClosePartition(g_CurrentPartitionIntHandle);
		SdkCloseAllPartitions();
		return TRUE;
	}

	//
	// Reading physical memory
	//

	UINT64 Address = 0;
	ULONG nNumberOfBytesToRead = 4;
	PVOID lpBuffer = malloc(0x1000);

	if (!lpBuffer)
		return FALSE;

	Ret = SdkReadPhysicalMemory(g_CurrentPartitionIntHandle, Address, (ULONG)nNumberOfBytesToRead, lpBuffer, g_MemoryReadInterfaceType);

	free(lpBuffer);

	//
	// Write physical memory
	//

	Address = 0;
	ULONG64 buffer = 0x1234567812345678;

	if (!lpBuffer)
		return FALSE;

	Ret = SdkWritePhysicalMemory(g_CurrentPartitionIntHandle, Address, sizeof(ULONG64), &buffer, g_MemoryReadInterfaceType);

	//
	// Write virtual memory
	//

	Address = 0xFFFFFF8000000000;
	buffer = 0;

	if (!lpBuffer)
		return FALSE;

	Ret = SdkWriteVirtualMemory(g_CurrentPartitionIntHandle, Address, &buffer, sizeof(ULONG64));

	//
	// Test many times VM connection
	//

	SdkClosePartition(g_CurrentPartitionIntHandle);
	SdkCloseAllPartitions();

	Partitions = SdkEnumPartitions(&PartitionCount, &VmOperationsConfig);

	if (!Partitions)
	{
		wprintf(L"   Unable to get list of partitions\n");
		return FALSE;
	}

	g_CurrentPartitionIntHandle = Partitions[0];

	if (!SdkSelectPartition(g_CurrentPartitionIntHandle))
	{
		wprintf(L"ERROR:    Cannot initialize hvdd structure.\n");
		return FALSE;
	};

	SdkClosePartition(g_CurrentPartitionIntHandle);
	SdkCloseAllPartitions();

	//
	// Reading virtual memory (KiProcessorBlock area)
	//
	ULONG64 Va = 0;
	SdkGetData(g_CurrentPartitionIntHandle, InfoKiProcessorBlock, &Va);
	//ULONG64 Va = 0xFFFFF90012345678ULL;

	lpBuffer = malloc(0x1000);

	if (!lpBuffer)
		return FALSE;

	if (SdkReadVirtualMemory(g_CurrentPartitionIntHandle, Va, lpBuffer, sizeof(ULONG64)) == FALSE) {
		wprintf(L"MmReadVirtualAddress failed\n");
		return FALSE;
	}

	printf("_KPROCESSOR_STATE[0]  0x%llx \n", *((PULONG64)lpBuffer));

	free(lpBuffer);

	//
	// Suspend virtual machine
	//

	SdkControlVmState(g_CurrentPartitionIntHandle, SuspendVm, SuspendResumePowershell, FALSE);

	//
	// Resume virtual machine
	//

	SdkControlVmState(g_CurrentPartitionIntHandle, ResumeVm, SuspendResumePowershell, FALSE);

	//
	// dump VM to file
	//

	if (DestinationPath == NULL)
		return FALSE;

	DumpVirtualMachineHandle(g_CurrentPartitionIntHandle, DestinationPath);

	return TRUE;
}



int main(int argc, char* argv[])
{
	//
	//Demo1 uses full PHVMM_PARTITION structure for working with partition. 
	//Demo2 using more simple HANDLE for using with non-C languages.
	//

	if (argc >= 3) {
		g_AutoVmId = atoi(argv[1]);
		g_AutoActionId = atoi(argv[2]);
	}

	//Demo1(); 
	Demo2(); 

	if (g_AutoActionId < 0) {
		getchar();
	}
	return 0;
}
