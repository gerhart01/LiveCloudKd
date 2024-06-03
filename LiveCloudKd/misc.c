/*++
	Microsoft Hyper-V Virtual Machine Physical Memory Dumper
	Copyright (C) Matt Suiche. All rights reserved.

Module Name:

	- misc.c

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

//VIDDLL_FUNCTIONS g_VidDll = { 0 };
NTDDLL_FUNCTIONS g_NtDll = { 0 };

BOOLEAN
ImportGlobalNtFunctions(
    VOID
) {
    HMODULE hNtdll = LoadLibrary(L"ntdll.dll");
    if (!hNtdll) {
        Red(L"ERROR: Can't load NTDLL.dll.\n");
        return FALSE;
    }

    (FARPROC)g_NtDll.NtAllocateVirtualMemory = GetProcAddress(hNtdll, "NtAllocateVirtualMemory");
    (FARPROC)g_NtDll.NtDuplicateObject = GetProcAddress(hNtdll, "NtDuplicateObject");
    (FARPROC)g_NtDll.NtOpenProcess = GetProcAddress(hNtdll, "NtOpenProcess");
    (FARPROC)g_NtDll.NtQueryObject = GetProcAddress(hNtdll, "NtQueryObject");
    (FARPROC)g_NtDll.NtQuerySystemInformation = GetProcAddress(hNtdll, "NtQuerySystemInformation");

    if (!g_NtDll.NtAllocateVirtualMemory || !g_NtDll.NtDuplicateObject || !g_NtDll.NtOpenProcess ||
        !g_NtDll.NtQueryObject || !g_NtDll.NtQuerySystemInformation) {
        Red(L"ERROR: Can't import any of the NTDLL.dll functions.\n");
        return FALSE;
    }

    return TRUE;
}

BOOL
GetMmNonPagedPoolLimit(PULONG64 MmNonPagedPoolStart,
                       PULONG64 MmNonPagedPoolEnd)
{
SYSTEM_BASIC_INFORMATION SystemInfo;
ULONG64 PfnAllocation;
NTSTATUS NtStatus;
BOOL Ret;

    Ret = FALSE;

    RtlZeroMemory(&SystemInfo, sizeof(SYSTEM_BASIC_INFORMATION));
    NtStatus = g_NtDll.NtQuerySystemInformation(SystemBasicInformation, &SystemInfo, sizeof(SYSTEM_BASIC_INFORMATION), NULL);
    if (NtStatus != STATUS_SUCCESS) goto Exit;

    PfnAllocation = (SystemInfo.HighestPhysicalPageNumber + 1) * 0x30; // sizeof(MMPFN);
    PfnAllocation = ((PfnAllocation + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1));

    *MmNonPagedPoolStart = 0xFFFFFA8000000000ULL + PfnAllocation;
    *MmNonPagedPoolEnd = *MmNonPagedPoolStart + ((ULONG64)8 * 1024 * 1024 * 1024);

    Ret = TRUE;

Exit:
    return Ret;
}

USHORT
GetConsoleTextAttribute(HANDLE hConsole)
{
CONSOLE_SCREEN_BUFFER_INFO csbi;

    GetConsoleScreenBufferInfo(hConsole, &csbi);
    return(csbi.wAttributes);
}

VOID
White(LPCWSTR Format, ...)
{
HANDLE Handle;
USHORT Color;
va_list va;

    Handle = GetStdHandle(STD_OUTPUT_HANDLE);

    Color = GetConsoleTextAttribute(Handle);

    SetConsoleTextAttribute(Handle, 0xF);
    va_start(va, Format);
    vwprintf(Format, va);
    va_end(va); 

    SetConsoleTextAttribute(Handle, Color);
}

VOID
Red(LPCWSTR Format, ...)
{
HANDLE Handle;
USHORT Color;
va_list va;

    Handle = GetStdHandle(STD_OUTPUT_HANDLE);

    Color = GetConsoleTextAttribute(Handle);

    SetConsoleTextAttribute(Handle, FOREGROUND_RED | FOREGROUND_INTENSITY);
    va_start(va, Format);
    vwprintf(Format, va);
    va_end(va); 

    SetConsoleTextAttribute(Handle, Color);
}

VOID
Green(LPCWSTR Format, ...)
{
HANDLE Handle;
USHORT Color;
va_list va;

    Handle = GetStdHandle(STD_OUTPUT_HANDLE);

    Color = GetConsoleTextAttribute(Handle);

    SetConsoleTextAttribute(Handle, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    va_start(va, Format);
    vwprintf(Format, va);
    va_end(va); 

    SetConsoleTextAttribute(Handle, Color);
}