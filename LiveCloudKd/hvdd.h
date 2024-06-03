/*++
    Microsoft Hyper-V Virtual Machine Physical Memory Dumper
    Copyright (C) 2010 MoonSols SARL. All rights reserved.

Module Name:

    - hvdd.h

Abstract:

    - 


Environment:

    - User mode

Revision History:

    - Matthieu Suiche

--*/
#define _AMD64_
#include <windows.h>  
#include "HvlibHandle.h"
#include <wdbgexts.h>
#include "NtKernel\ntdefs.h"
#include <stdio.h>
#include <tlhelp32.h>
#include <conio.h>
#include "hooker.h"
#include "misc.h"
#include "dmp.h"


#define BLOCK_SIZE (1024 * 1024)

#define WINDBG_FT_TABLE_PAGE_COUNT 0x400

#define wprintflvl1(format, ...)  {if (g_VmOperationsConfig.LogLevel >=1 ) { wprintf(format, ##__VA_ARGS__);} else {} } 
#define printflvl1(format, ...)  {if (g_VmOperationsConfig.LogLevel >=1 ) { printf(format, ##__VA_ARGS__);} else {} } 

#if defined(PRO_EDITION)
#define DEBUG_ENABLED FORCE_DEBUG_MODE
#elif defined(COMMUNITY_EDITION)
#define DEBUG_ENABLED 0
#else
#define DEBUG_ENABLED 1
#endif

#define DUMP_COMMENT_STRING "Hyper-V Memory Dump. (c) 2010 MoonSols SARL <http://www.moonsols.com>"

//
// dump.c
//

BOOLEAN
DumpMemoryBlock(
	_In_ ULONG64 PartitionEntry,
	_In_ LPCWSTR DestinationFile,
	_In_ ULONG64 Start,
	_In_ ULONG64 Size,
    _In_ GUEST_TYPE DumpMode
);

BOOLEAN
DumpVirtualMachine(
	_In_ ULONG64 PartitionEntry,
	_In_ LPCWSTR DestinationFile
);

BOOLEAN
DumpLiveVirtualMachine(
	_In_ ULONG64 PartitionEntry,
	_In_ ULONG64 VmId
);

BOOLEAN
DumpCrashVirtualMachine(
	_In_ ULONG64 PartitionEntry,
	_In_ LPCWSTR DestinationFile
);

//
// file.c
//

BOOL
CreateDestinationFile(
    LPCWSTR Filename,
    PHANDLE Handle
);

BOOL
WriteFileSynchronous(
    HANDLE Handle,
    PVOID Buffer,
    ULONG NbOfBytesToWrite
);

//
// kd.c
//


BOOL
LaunchKd(
	LPCWSTR DumpFile,
	ULONG64 PartitionEntry
);

BOOLEAN
LaunchWinDbg(
	ULONG64 PartitionEntry
);

BOOLEAN
LaunchWinDbgX(
	ULONG64 PartitionEntry
);

BOOLEAN
LaunchWinDbgLive(
    ULONG64 PartitionEntry
);

//
// misc.c
//

BOOL
GetMmNonPagedPoolLimit(
    PULONG64 MmNonPagedPoolStart,
    PULONG64 MmNonPagedPoolEnd
);

VOID
White(
    LPCWSTR Format,
    ...
);

VOID
Red(
    LPCWSTR Format,
    ...
);

VOID
Green(
    LPCWSTR Format,
    ...
);

USHORT
GetConsoleTextAttribute(
    HANDLE hConsole
);

//
// dump.c
//

PDUMP_HEADER64
DumpFillHeader64(
	_In_ ULONG64 PartitionEntry
);


BOOLEAN
DumpFillHeader(
	_In_ ULONG64 PartitionEntry,
	_In_ PVOID* Header,
	_In_ PULONG HeaderSize
);

//
// hooker.c
//

BOOL
HookKd(
    HANDLE ProcessHandle,
    ULONG ProcessId
);

extern BOOLEAN g_UseWinDbg;
extern BOOLEAN g_UseWinDbgLive;
extern BOOLEAN g_UseWinDbgX;
extern BOOLEAN g_UseEXDi;
extern VM_OPERATIONS_CONFIG g_VmOperationsConfig;