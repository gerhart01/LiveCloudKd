/*++
	Microsoft Hyper-V Virtual Machine Physical Memory Dumper
	Copyright (C) Matt Suiche. All rights reserved.

Module Name:

	- LiveCloudKd.c

Abstract:

	- This header file contains definition used by LiveCloudKd (2010) and open-sourced in December 2018 after
	collaborating with Arthur Khudyaev (@gerhart_x) to revive the project.

	More information can be found on the original repository: https://github.com/comaeio/LiveCloudKd

	Original 2010 blogpost: https://blogs.technet.microsoft.com/markrussinovich/2010/10/09/livekd-for-virtual-machine-debugging/

Environment:

	- User mode

Revision History:
	
	- Arthur Khudyaev (@gerhart_x) - Many fixes of bugs
	- Arthur Khudyaev (@gerhart_x) - 03-Feb-2020 - Add live debugging support for Windows Server 2019.
	- Arthur Khudyaev (@gerhart_x) - 15-Dec-2019 - Fixes for build memory blocking proc for large VM. Fix .dmp file creation proc.
	- Arthur Khudyaev (@gerhart_x) - 28-Nov-2019 - Add Hyper-V interception exception supporting (for PF injection). 
	- Arthur Khudyaev (@gerhart_x) - 28-Nov-2019 - Windows 19H2 is supported
	- Arthur Khudyaev (@gerhart_x) - 22-Nov-2019 - Add -f options for Windows Sandbox\WDAG. It freeze CPU on every read memory operation. Reliability of operation is increased meaningfully. (Suspending WindowsSandbox.exe is variant too)
	- Arthur Khudyaev (@gerhart_x) - 30-Oct-2019 - Add shielded VM support.
	- Arthur Khudyaev (@gerhart_x) - 20-Oct-2019 - Add logging level(/v key). 0 - errors. 1 - verbose logging, 2 - very verbose logging
	- Arthur Khudyaev (@gerhart_x) - 16-Oct-2019 - Add EXO partition supporting(/z key). Testing on QEMU qith -accel whpx only
	- Arthur Khudyaev (@gerhart_x) - 14-Oct-2019 - Add Windows 10x64 19H1 support
	- Arthur Khudyaev (@gerhart_x) - 14-Sep-2019 - Add guest memory writing capabilities for Windows Server 2012-2016 (in EXDi mode)
	- Arthur Khudyaev (@gerhart_x) - 20-August-2019 - Add native vid.dll support for Windows Server 2012\2012 R2
	- Arthur Khudyaev (@gerhart_x) - 01-August-2019 - Add native vid.dll support for Windows Server 2016
	- Arthur Khudyaev (@gerhart_x) - 08-July-2019 - Add injection of vidaux.dll to vmwp.exe
	- Arthur Khudyaev (@gerhart_x) - 01-June-2019 - Add EXDi (kd, WinDBG, WinDBGX) support
	- Arthur Khudyaev (@gerhart_x) - 18-Apr-2019 - Add additional methods (using Microsoft winhv.sys and own hvmm.sys driver) for reading guest memory
	- Arthur Khudyaev (@gerhart_x) - 20-Feb-2019 - Migrate part of code to LiveCloudKd plugin
	- Arthur Khudyaev (@gerhart_x) - 26-Jan-2019 - Migration to MemProcFS/LeechCore
	- Matthieu Suiche (@msuiche) 11-Dec-2018 - Open-sourced LiveCloudKd in December 2018 on GitHub
	- Arthur Khudyaev (@gerhart_x) - 28-Oct-2018 - Add partial Windows 10 support
	- Matthieu Suiche (@msuiche) 09-Dec-2010 - Initial version from LiveCloudKd and presented at BlueHat 2010

--*/

#include "hvdd.h"

BOOLEAN g_UseWinDbg = FALSE;
BOOLEAN g_UseWinDbgLive = FALSE;
BOOLEAN g_UseWinDbgX = FALSE;
BOOLEAN g_UseEXDi = FALSE;
BOOLEAN g_ExoPresent = FALSE;
ULONG64 g_LogLevel = 0;
BOOLEAN g_ForceFreezeCpu = FALSE;
BOOLEAN g_PausePartition = FALSE;
VM_OPERATIONS_CONFIG g_VmOperationsConfig = { 0 };

extern READ_MEMORY_METHOD g_MemoryReadInterfaceType;
extern WRITE_MEMORY_METHOD g_MemoryWriteInterfaceType;

LPCWSTR DestinationPath = NULL;
ULONG Action = -1;
ULONG g_VmId = -1;

CHAR Disclamer[] = ".Reverse Engineering of this program is prohibited. Please respect intellectual property. The code of this program and techniques involved belongs to MoonSols SARL and Matthieu Suiche.\n";


BOOLEAN
WriteEXDiPartitionId(ULONG VmId)
{
	HKEY hDrvKey, hkey;
	LSTATUS regStatus;
	BOOLEAN Ret = FALSE;
	//regStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\hvmm", 0, KEY_ALL_ACCESS, &hDrvKey);
	regStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\LiveCloudKd\\Parameters", 0, KEY_ALL_ACCESS, &hDrvKey);

	if (regStatus != ERROR_SUCCESS) {

		regStatus = RegCreateKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\LiveCloudKd\\Parameters", &hkey);
		if (regStatus != ERROR_SUCCESS) 
		{
			wprintf(L"Driver parameter key was not created\n");
			Ret = FALSE;
		}
	}

	regStatus = RegCreateKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\LiveCloudKd\\Parameters", &hkey);

	if (regStatus == ERROR_SUCCESS)
	{
		regStatus = RegSetValueEx(hkey, L"VmId", 0, REG_DWORD, (const BYTE*)&VmId, sizeof(VmId));
		if (regStatus == ERROR_SUCCESS)
		{
			Ret = TRUE;
		}
		else
		{
			wprintf(L"VmId key was not created\n");
			Ret = FALSE;
		}

	}
	RegCloseKey(hkey);
	RegCloseKey(hDrvKey);
	return Ret;
}

VOID
Help()
{
    Disclamer[0] = '!';
    wprintf(L"Usage: LiveCloudKd.exe [/w] [/x] [/a {0-5}] [/b {0-9}] [/m {0-3}] [/e] [/l] [-o path] [/v {0-2}] [/z] [/?]\n"
            L"      /a        Pre-selected action.\n"
            L"                   0 - Live kernel debugging\n"
            L"                   1 - Produce a linear physical memory dump\n"
            L"                   2 - Produce a Microsoft full memory crash dump\n"
			L"                   3 - Dump guest OS memory chunk\n"
			L"                   4 - Dump RAW guest OS memory (without KDBG scanning)\n"
			L"                   5 - Resume VM\n"
			L"      /b        Pre-selected VM.\n"
			L"      /e        LiveCloudKd works using EXDi plugin. See description on\n" 
			L"                https://github.com/gerhart01/LiveCloudKd/tree/master/ExdiKdSample for more detailes \n"
			L"      /f        Force freeze CPU on every read operations. It is actual for Windows Sandbox, because it constantly resume CPU.\n"
			L"      /m        Memory access type.\n"
			L"                   0 - Winhvr.sys interface	\n"
			L"                   1 - Raw memory interface (hvmm.sys) \n"
			L"                   2 - Injected vidaux.dll memory interface	\n"
			L"                   3 - Native vid.dll memory interface. Works in Windows Server  2012, 2012 R2 and 2016 Hyper-V\n"
			L"      /o        Destination path for the output file (Action 1 - 3).\n"
			L"      /p        Pause partition.\n"
			L"      /v        Verbose output.\n"
			L"      /w        Run Windbg instead of Kd (Kd is the default).\n"
			L"      /l        Run Windbg instead in EXDi Live mode.\n"
			L"      /x        Run Windbg Preview instead of Kd (Kd is the default).\n"
			L"      /z        Working with EXO partition.\n"
            L"      /?        Print this help.\n");
}

VOID
ParseArguments(
	_In_ ULONG argc,
	_In_ LPCWSTR *argv
)
{
ULONG Index;
UCHAR ChAction;
BOOLEAN bChoice = FALSE;

    for (Index = 1; Index < argc; Index += 1)
    {
        if ((argv[Index][0] != L'/') && (argv[Index][0] != L'-')) continue;

        switch (argv[Index][1])
        {
            case L'w':
                g_UseWinDbg = TRUE;
				g_UseEXDi = TRUE;
            break;
			case L'x':
				g_UseWinDbgX = TRUE;
				g_UseEXDi = TRUE;
				break;
			case L'l':
				g_UseWinDbgLive = TRUE;
				g_UseEXDi = TRUE;
				break;
			case L'f':
				g_ForceFreezeCpu = TRUE;
				break;
			case L'p':
				g_PausePartition = TRUE;
					break;
            case L'o':
                if ((Index + 1) < argc)
                {
                    DestinationPath = argv[Index + 1];
                    Index += 1;
                }
            break;
            case L'a':
                if ((Index + 1) < argc)
                {
                    ChAction = (UCHAR)argv[Index + 1][0];
                    if ((ChAction >= '0') && (ChAction <= '5'))
                    {
                        Action = ChAction - '0';
                    }
                    Index += 1;
                }
            break;
			case L'b':
				if ((Index + 1) < argc)
				{
					ChAction = (UCHAR)argv[Index + 1][0];
					g_VmId = ChAction - '0';
					Index += 1;
				}
				break;
            case L'?':
                Help();
                getchar();
                exit(1);
            break;
			case L'e':
				g_UseEXDi = TRUE;
			break;
			case L'v':
				if ((Index + 1) < argc)
				{
					ChAction = (UCHAR)argv[Index + 1][0];
					if ((ChAction >= '0') && (ChAction <= '2'))
					{
						g_LogLevel = ChAction - '0';
					}
					Index += 1;
				}
				break;
			case L'm':
				if ((Index + 1) < argc)
				{
					ChAction = (UCHAR)argv[Index + 1][0];
					if (ChAction == '0') 
					{
						g_MemoryReadInterfaceType = ReadInterfaceWinHv;
						g_MemoryWriteInterfaceType = WriteInterfaceWinHv;
						bChoice = TRUE;
					}
					if (ChAction == '1')
					{
						g_MemoryReadInterfaceType = ReadInterfaceHvmmDrvInternal;
						g_MemoryWriteInterfaceType = WriteInterfaceWinHv;
						bChoice = TRUE;
					}
					if (bChoice == FALSE) {
						Red(L"Unknown memory access type");
					}
					Index += 1;
				}
			break;
			case L'z':
				g_ExoPresent = TRUE;
				g_MemoryReadInterfaceType = ReadInterfaceWinHv;
				g_MemoryWriteInterfaceType = WriteInterfaceWinHv;
			break;
			default:	
				break;
        }
    }
}

wmain(
	_In_ int argc,
	_In_ LPCWSTR *argv
)
{
	PULONG64 Partitions;
	ULONG64 PartitionCount = 0;
	ULONG64 CurrentPartition;

	ULONG i;
	ULONG VmId, ActionId;

	WCHAR Destination[MAX_PATH + 1];
	HANDLE Handle;
	USHORT Color;
	BOOLEAN bResult = FALSE;
	WCHAR* FriendlyNameP = NULL;
	ULONG64 SuspendWorker = 0;

	wprintf(L"      LiveCloudKd - 2.0.0.20200308, beta\n"
	L"      Microsoft Hyper-V Virtual Machine  Physical Memory Dumper & Live Kernel Debugger\n"
	L"      Copyright (C) 2010-2020, Matthieu Suiche (@msuiche)\n"
	L"      Copyright (C) 2020, Comae Technologies DMCC <http://www.comae.com> <support@comae.io>\n"
	L"      All rights reserved.\n\n"

	L"      Contributor: Arthur Khudyaev (@gerhart_x)\n\n\n"
	L"");

    SetConsoleTitle(L"LiveCloudKd");
    if (!ImportGlobalNtFunctions()) goto Exit;
	
	ParseArguments(argc, argv);
	
	//
	// Set default g_MemoryInterfaceType (if -m option was not specified)
	//

	SdkGetDefaultConfig(&g_VmOperationsConfig);
		
	if (g_MemoryReadInterfaceType != ReadInterfaceUnsupported)
	{
		g_VmOperationsConfig.ReadMethod = g_MemoryReadInterfaceType;
		g_VmOperationsConfig.WriteMethod = g_MemoryWriteInterfaceType;
	}
	else {
		g_MemoryReadInterfaceType = g_VmOperationsConfig.ReadMethod;
		g_MemoryWriteInterfaceType = g_VmOperationsConfig.WriteMethod;
	}
	
	g_VmOperationsConfig.ForceFreezeCPU = g_ForceFreezeCpu;
	g_VmOperationsConfig.PausePartition = g_ForceFreezeCpu;

	g_VmOperationsConfig.LogLevel = g_LogLevel;
	g_VmOperationsConfig.ReloadDriver = FALSE;
	g_VmOperationsConfig.PFInjection = FALSE;

	Partitions = SdkEnumPartitions(&PartitionCount, &g_VmOperationsConfig);

	if (!Partitions)
	{
		wprintf(L"   Unable to get list of partitions\n");
		goto Exit;
	}

	wprintf(L"\n   Virtual Machines:\n");
    if (PartitionCount == 0)
    {
        Red(L"   --> No virtual machines running.\n");
        goto Exit;
    }

	for (i = 0; i < PartitionCount; i += 1)
	{		
		ULONG64 PartitionId = 0;
		WCHAR* VmTypeString = NULL;
		SdkGetData(Partitions[i], InfoPartitionFriendlyName, &FriendlyNameP);
		SdkGetData(Partitions[i], InfoPartitionId, &PartitionId);
		SdkGetData(Partitions[i], InfoVmtypeString, &VmTypeString);
		wprintf(L"    --> [%d] %s (PartitionId = 0x%I64X, %s)\n", i, FriendlyNameP, PartitionId, VmTypeString);
	}

	if (g_VmId == -1)
	{
		VmId = 0;
		while ((VmId < '0') || (VmId > '9'))
		{
			wprintf(L"\n"
				L"   Please select the ID of the virtual machine you want to play with\n"
				L"   > ");
			VmId = _getch();
		}
		VmId = VmId - 0x30;
	}
	else
	{
		VmId = g_VmId;
	}

	wprintf(L"%d\n", VmId);

	if ((VmId + 1) > PartitionCount)
	{
		Red(L"   The virtual machine you selected do not exist.\n");
		goto Exit;
	}

    wprintf(L"   You selected the following virtual machine: ");

	SdkGetData(Partitions[VmId], InfoPartitionFriendlyName, &FriendlyNameP);
    Green(L"%s\n", FriendlyNameP);

    wprintf(L"\n"
            L"   Action List:\n");
    wprintf(L"    --> [0] Live kernel debugger\n"
            L"    --> [1] Linear physical memory dump\n"
            L"    --> [2] Microsoft crash memory dump\n"
			L"    --> [3] memory chunk dump (start position, size)\n"
			L"    --> [4] RAW memory dump (start position, size)\n"
			L"    --> [5] Resume partition\n");

    if (Action == -1)
    {
        ActionId = 0;
		FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
		wprintf(L"\n"
			L"   Please select the Action ID\n"
			L"   > ");
        while ((ActionId < '0') || (ActionId > '5'))
        {
			ActionId = _getch();
        }

        ActionId = ActionId - 0x30;
    }
    else
    {
        ActionId = Action;
    }

    Green(L"%d\n", ActionId);

    if (ActionId != 0 && ActionId != 5)
    {
        wprintf(L"\n"
                L"   Destination path for the virtual machine physical memory dump (RAW dump)\n"
                L"   > ");

        if (DestinationPath == NULL)
        {
            Handle = GetStdHandle(STD_OUTPUT_HANDLE);
            Color = GetConsoleTextAttribute(Handle);
            SetConsoleTextAttribute(Handle, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                _getws_s(Destination, (_countof(Destination) - sizeof(Destination[0])));
            SetConsoleTextAttribute(Handle, Color);
            DestinationPath = Destination;
			FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
        }
        else
        {
            Green(L"%s\n", DestinationPath);
        }
    }

	ULONG64 BlockSize = 0;
	ULONG64 BlockAddress = 0;

	if ((ActionId == 3) | (ActionId == 4))
	{
		wprintf(L"\n"
			L"   Set block size (bytes):\n"
			L"   > ");

			Handle = GetStdHandle(STD_OUTPUT_HANDLE);
			Color = GetConsoleTextAttribute(Handle);
			SetConsoleTextAttribute(Handle, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
				wscanf_s(L"%llx", &BlockSize);
			SetConsoleTextAttribute(Handle, Color);

			Green(L"BlockSize 0x%llx%\n", BlockSize);

			wprintf(L"\n"
				L"   Set block's start physical address:\n"
				L"   > ");

			SetConsoleTextAttribute(Handle, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
				wscanf_s(L"%llx",&BlockAddress);
			SetConsoleTextAttribute(Handle, Color);

			Green(L"BlockAddress: 0x%llx%\n", BlockAddress);
	}

	//
	// if EXDi mode is enabled, we write VmId to registry
	//

	if (g_UseEXDi == TRUE) {
		WriteEXDiPartitionId(VmId);
	}

    //
    // Now, we successfully have the Partition Handle. Let's look for Memory Blocks.
    //

	if ((g_UseWinDbgLive == FALSE) & (ActionId != 4))
	{
		CurrentPartition = Partitions[VmId];

		if (!SdkSelectPartition(CurrentPartition))
		{
			wprintf(L"ERROR in SdkSelectPartitionHandle.\n");
			goto Exit;
		};
	}
	else
	{
		CurrentPartition = Partitions[VmId]; // only because KdVersionBlock is auto detected by EXDi plugin
	}

    switch (ActionId)
    {
        case 0:
			if (!DumpLiveVirtualMachine(CurrentPartition, VmId))
				Red(L"   Cannot initialize crash dump header.\n");
        break;

        case 1:
			if (!DumpVirtualMachine(CurrentPartition, DestinationPath))
				Red(L"   Cannot get internal VM structures.\n");
        break;

        case 2:
            if (!DumpCrashVirtualMachine(CurrentPartition, DestinationPath))
				Red(L"   Cannot initialize crash dump header.\n");
		break;
		case 3:
			if (!DumpMemoryBlock(CurrentPartition, DestinationPath, BlockAddress, BlockSize, MmStandard))
				Red(L"   Problem with memory chunk dumping\n");
        break;
		case 4:
			if (!DumpMemoryBlock(CurrentPartition, DestinationPath, BlockAddress, BlockSize, MmNonKdbgPartition))
				Red(L"   Problem with RAW memory dumping\n");
			break;
		case 5:
			
			SdkGetData(CurrentPartition, InfoIsNeedVmwpSuspend, &SuspendWorker);
			if (!SdkControlVmState(CurrentPartition, ResumeVm, g_VmOperationsConfig.SuspendMethod, (BOOLEAN)SuspendWorker))
				Red(L"   Problem with resuming selected VM\n");
		break;
    }

Exit:
	SdkCloseAllPartitions();
	wprintf(L"kd.exe was closed. Press enter for closing LiveCloudKd\n");

	int symbol = 0;
	do
	{
		symbol = getchar();
	} while (symbol != '\n' && symbol != EOF);

    return TRUE;
}