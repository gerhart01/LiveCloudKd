/*++
	Microsoft Hyper-V Virtual Machine Physical Memory Dumper
	Copyright (C) Matt Suiche. All rights reserved.

Module Name:

	- hooker.c

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
	- Arthur Khudyaev (@gerhart_x) - 20-Feb-2019 - Migrate part of code to LiveCloudKdSdk plugin
	- Arthur Khudyaev (@gerhart_x) - 26-Jan-2019 - Migration to MemProcFS/LeechCore
	- Matthieu Suiche (@msuiche) 11-Dec-2018 - Open-sourced LiveCloudKd in December 2018 on GitHub
	- Arthur Khudyaev (@gerhart_x) - 28-Oct-2018 - Add partial Windows 10 support
	- Matthieu Suiche (@msuiche) 09-Dec-2010 - Initial version from LiveCloudKd and presented at BlueHat 2010

--*/

#include "hvdd.h"

FUNCTION_TABLE FunctionTable = { 0 };

#define PtrFromRva(Base, Rva) (((PBYTE) Base) + Rva )

BOOL
PatchIAT(PVOID ModuleBase,
         LPSTR ImporteModuleName,
         LPSTR FunctionName,
         ULONG64 Address)
{
    PIMAGE_DOS_HEADER DosHeader;
    PIMAGE_NT_HEADERS NtHeader;
    PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor;

    ULONG Index;

    DosHeader = (PIMAGE_DOS_HEADER)ModuleBase;

    NtHeader = (PIMAGE_NT_HEADERS)PtrFromRva(DosHeader, DosHeader->e_lfanew);

    if (NtHeader->Signature != IMAGE_NT_SIGNATURE) return FALSE;

    ImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)PtrFromRva(DosHeader,
                                                    NtHeader->OptionalHeader.DataDirectory
                                                    [IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

    for (Index = 0;
         ImportDescriptor[Index].Characteristics != 0;
         Index += 1)
    {
        PSTR dllName = (PSTR)PtrFromRva(DosHeader, ImportDescriptor[Index].Name);

        if (_strcmpi(dllName, ImporteModuleName) == 0)
        {
            PIMAGE_THUNK_DATA Thunk;
            PIMAGE_THUNK_DATA OrigThunk;
            PIMAGE_IMPORT_BY_NAME import;

            if (!ImportDescriptor[Index].FirstThunk || !ImportDescriptor[ Index ].OriginalFirstThunk)
            {
                return FALSE;
            }
			printflvl1("Successfully found module %s \n", ImporteModuleName);
            Thunk = (PIMAGE_THUNK_DATA )PtrFromRva(DosHeader, ImportDescriptor[Index].FirstThunk);
            OrigThunk = (PIMAGE_THUNK_DATA)PtrFromRva(DosHeader, ImportDescriptor[Index].OriginalFirstThunk);

            for (; OrigThunk->u1.Function != 0; OrigThunk++, Thunk++)
            {
                if (OrigThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG ) continue;
                import = (PIMAGE_IMPORT_BY_NAME)PtrFromRva( DosHeader, OrigThunk->u1.AddressOfData);
                if (strcmp(FunctionName, (PCHAR)import->Name) ==  0)
                {
                    *((PULONG64)Thunk) = Address;
					printflvl1("Successfully found function %s \n", FunctionName);
                    goto Exit;
                }
            }
        }
    }

Exit:
    return TRUE;
}

BOOL
HookKd(HANDLE ProcessHandle, ULONG ProcessId)
{
    HANDLE ModuleSnapshot;
    MODULEENTRY32 me32;

    BOOL Ret;

    Ret = FALSE;

    ModuleSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, ProcessId);
    if (ModuleSnapshot == INVALID_HANDLE_VALUE) goto Exit;

    me32.dwSize = sizeof(MODULEENTRY32);

    if (!Module32First(ModuleSnapshot, &me32))
    {
        CloseHandle(ModuleSnapshot);
        goto Exit;
    }

    while (Module32Next(ModuleSnapshot, &me32))
    {
        if(_wcsicmp(me32.szModule, L"dbgeng.dll") == 0)
        {
			SIZE_T size;

			PUCHAR Buffer, ModBase, Page;
			ULONG Rights;
			NTSTATUS NtStatus;
			BOOL Status;

			size = (WINDBG_FT_TABLE_PAGE_COUNT * PAGE_SIZE) + (ULONG64)FunctionTable.HeaderSize;// 4 before WINDBG_FT_TABLE_PAGE_COUNT
			Buffer = (PUCHAR)TABLE_OFFSET;
			NtStatus = g_NtDll.NtAllocateVirtualMemory(ProcessHandle, &Buffer, 0,
				&size, MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN,
				PAGE_EXECUTE_READWRITE);
            if (NtStatus != STATUS_SUCCESS) {
                Red(L"HookKd NtAllocateVirtualMemory failed");
                goto Exit;
            }

            Buffer = (PUCHAR)TABLE_OFFSET;
			Status = WriteProcessMemory(ProcessHandle, Buffer + WINDBG_FT_TABLE_PAGE_COUNT * PAGE_SIZE, FunctionTable.Header,
				FunctionTable.HeaderSize, &size);
			if (Status != TRUE) goto Exit;

			FunctionTable.Header = (PUCHAR)(Buffer + WINDBG_FT_TABLE_PAGE_COUNT * PAGE_SIZE);
			Status = WriteProcessMemory(ProcessHandle, TABLE_OFFSET, &FunctionTable, sizeof(FUNCTION_TABLE), &size);
			if (Status != TRUE) goto Exit;
			 //wprintf(L"CREATEFILE_OFFSET: %09llx-%09llx\n", (ULONG64)CREATEFILE_OFFSET, (ULONG64)CREATEFILE_OFFSET + 0x300);
			Status = WriteProcessMemory(ProcessHandle, CREATEFILE_OFFSET, &MyCreateFile, 0x300, &size);
			if (Status != TRUE) goto Exit;
			 //wprintf(L"CREATEFILEMAPPINGA_OFFSET: %08llx-%08llx\n", (ULONG64)CREATEFILEMAPPINGA_OFFSET, (ULONG64)CREATEFILEMAPPINGA_OFFSET + 0x100);
			Status = WriteProcessMemory(ProcessHandle, CREATEFILEMAPPINGA_OFFSET, &MyCreateFileMappingA, 0x100, &size);
			if (Status != TRUE) goto Exit;
			 //wprintf(L"CREATEFILEMAPPINGW_OFFSET: %08llx-%08llx\n", (ULONG64)CREATEFILEMAPPINGW_OFFSET, (ULONG64)CREATEFILEMAPPINGW_OFFSET + 0x100);
			Status = WriteProcessMemory(ProcessHandle, CREATEFILEMAPPINGW_OFFSET, &MyCreateFileMappingW, 0x100, &size);
			if (Status != TRUE) goto Exit;
			 //wprintf(L"MAPVIEWOFFILE_OFFSET: %08llx-%08llx\n", (ULONG64)MAPVIEWOFFILE_OFFSET, (ULONG64)MAPVIEWOFFILE_OFFSET + 0x1B00);
			Status = WriteProcessMemory(ProcessHandle, MAPVIEWOFFILE_OFFSET, &MyMapViewOfFile, 0x1B00, &size);
			if (Status != TRUE) goto Exit;
			 //wprintf(L"UNMAPVIEWOFFILE_OFFSET: %08llx-%08llx\n", (ULONG64)UNMAPVIEWOFFILE_OFFSET, (ULONG64)UNMAPVIEWOFFILE_OFFSET + 0x100);
			Status = WriteProcessMemory(ProcessHandle, UNMAPVIEWOFFILE_OFFSET, &MyUnmapViewOfFile, 0x100, &size);
			if (Status != TRUE) goto Exit;
			// wprintf(L"GETFILESIZE_OFFSET: %08llx-%08llx\n", (ULONG64)GETFILESIZE_OFFSET, (ULONG64)GETFILESIZE_OFFSET + 0x100);
			Status = WriteProcessMemory(ProcessHandle, GETFILESIZE_OFFSET, &MyGetFileSize, 0x100, &size);
			if (Status != TRUE) goto Exit;
			 //wprintf(L"VIRTUALPROTECT_OFFSET: %08llx-%08llx\n", (ULONG64)VIRTUALPROTECT_OFFSET, (ULONG64)VIRTUALPROTECT_OFFSET + 0x900);
			Status = WriteProcessMemory(ProcessHandle, VIRTUALPROTECT_OFFSET, &MyVirtualProtect, 0xD00, &size);
			// Status = WriteProcessMemory(ProcessHandle, VIRTUALPROTECT_OFFSET, "\xCC", 1, &size);
			if (Status != TRUE) goto Exit;

            Page = malloc(me32.modBaseSize);
            if (Page == NULL) goto Exit;

            ModBase = (PUCHAR)me32.modBaseAddr;
            Status = ReadProcessMemory(ProcessHandle, ModBase, Page, me32.modBaseSize, &size);
            if (Status != TRUE)
            {
                free(Page);
                goto Exit;
            }

            if (!PatchIAT(Page, "api-ms-win-core-file-l1-1-0.dll", "CreateFileW", (ULONG64)CREATEFILE_OFFSET)) //+
				PatchIAT(Page, "kernel32.dll", "CreateFileW", (ULONG64)CREATEFILE_OFFSET);

			if (!PatchIAT(Page, "api-ms-win-core-memory-l1-1-0.dll", "CreateFileMappingW", (ULONG64)CREATEFILEMAPPINGW_OFFSET))
				PatchIAT(Page, "kernel32.dll", "CreateFileMappingW", (ULONG64)CREATEFILEMAPPINGW_OFFSET);
			
			if (!PatchIAT(Page, "api-ms-win-core-memory-l1-1-0.dll", "MapViewOfFile", (ULONG64)MAPVIEWOFFILE_OFFSET))
				PatchIAT(Page, "kernel32.dll", "MapViewOfFile", (ULONG64)MAPVIEWOFFILE_OFFSET);

			if (!PatchIAT(Page, "api-ms-win-core-memory-l1-1-0.dll", "UnmapViewOfFile", (ULONG64)UNMAPVIEWOFFILE_OFFSET))
				PatchIAT(Page, "kernel32.dll", "UnmapViewOfFile", (ULONG64)UNMAPVIEWOFFILE_OFFSET);

            if (!PatchIAT(Page, "api-ms-win-core-file-l1-1-0.dll", "GetFileSize", (ULONG64)GETFILESIZE_OFFSET))
				PatchIAT(Page, "kernel32.dll", "GetFileSize", (ULONG64)GETFILESIZE_OFFSET);

			if (!PatchIAT(Page, "api-ms-win-core-memory-l1-1-0.dll", "VirtualProtect", (ULONG64)VIRTUALPROTECT_OFFSET))
				PatchIAT(Page, "kernel32.dll", "VirtualProtect", (ULONG64)VIRTUALPROTECT_OFFSET);


            size = 0;
            Status = VirtualProtectEx(ProcessHandle, ModBase, me32.modBaseSize, PAGE_EXECUTE_READWRITE, &Rights);
            if (Status != TRUE)
            {
                free(Page);
                goto Exit;
            }

            Status = WriteProcessMemory(ProcessHandle, ModBase, Page, (SIZE_T)me32.modBaseSize, &size);
            if (Status != TRUE)
            {
                free(Page);
                goto Exit;
            }
#if 0
            Status = ReadProcessMemory(ProcessHandle, NULL, Page, 2 * PAGE_SIZE, &size);
            CreateDestinationFile(L"I:\\Code\\vs2010\\hvdd\\x64\\Release\\LOOL", &FileHandle);
            WriteFileSynchronous(FileHandle, Page, PAGE_SIZE);
            CloseHandle(FileHandle);
#endif
            free (Page);
            Ret = TRUE;
        }
    }

Exit:
    return Ret;
}

HANDLE WINAPI MyCreateFile(
  __in      LPCWSTR lpFileName,
  __in      DWORD dwDesiredAccess,
  __in      DWORD dwShareMode,
  __in_opt  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  __in      DWORD dwCreationDisposition,
  __in      DWORD dwFlagsAndAttributes,
  __in_opt  HANDLE hTemplateFile
)
{
    PFUNCTION_TABLE FT;
    #if 0
    WCHAR DllName[11];
    #endif
    ULONG Index;

    HANDLE FileHandle;

	FT = (PFUNCTION_TABLE)TABLE_OFFSET;

    for (Index = 0; lpFileName[Index] != L'\0'; Index += 1);

    FileHandle = FT->_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                                  dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

    if ((lpFileName[Index - 1] == L'p') &&
        (lpFileName[Index - 2] == L'm') &&
        (lpFileName[Index - 3] == L'd') &&
        (lpFileName[Index - 4] == L'.') &&
        (lpFileName[Index - 5] == L'd') &&
        (lpFileName[Index - 6] == L'd') &&
        (lpFileName[Index - 7] == L'v') &&
        (lpFileName[Index - 8] == L'h'))
    {
#if 0
        DllName[0] = L'u';
        DllName[1] = L's';
        DllName[2] = L'e';
        DllName[3] = L'r';
        DllName[4] = L'3';
        DllName[5] = L'2';
        DllName[6] = L'.';
        DllName[7] = L'd';
        DllName[8] = L'l';
        DllName[9] = L'l';
        DllName[10] = L'\0';

        FT->_LoadLibrary(DllName);
        FT->_MessageBoxW(0, lpFileName, lpFileName, 0);
#endif

        FT->CrashDumpHandle = FileHandle;
    }

    return FileHandle;
}

LPVOID WINAPI MyMapViewOfFile(
  __in  HANDLE hFileMappingObject,
  __in  DWORD dwDesiredAccess,
  __in  DWORD dwFileOffsetHigh,
  __in  DWORD dwFileOffsetLow,
  __in  SIZE_T dwNumberOfBytesToMap
)
{
PFUNCTION_TABLE FT;

WCHAR DllName[19];

PUCHAR Buffer;
LARGE_INTEGER Offset;
PHYSICAL_ADDRESS KDBGPa;
ULONG64 Index;
ULONG64 i, j;
ULONG64 MbHandle = 0;
HMODULE Module;
ULONG64 Position;
ULONG64 NumberOfPages, SizeToMap;

#if 1
WCHAR Str1[9];
WCHAR Str2[9];
WCHAR Alpha[0x11];
ULONG lol;
#endif

PUCHAR C;
PX64_CONTEXT Context64;
BOOL Res;

FT = (PFUNCTION_TABLE)TABLE_OFFSET;

#if 0
    Alpha[0] = L'0';
    Alpha[1] = L'1';
    Alpha[2] = L'2';
    Alpha[3] = L'3';
    Alpha[4] = L'4';
    Alpha[5] = L'5';
    Alpha[6] = L'6';
    Alpha[7] = L'7';
    Alpha[8] = L'8';
    Alpha[9] = L'9';
    Alpha[0xA] = L'a';
    Alpha[0xB] = L'b';
    Alpha[0xC] = L'c';
    Alpha[0xD] = L'd';
    Alpha[0xE] = L'e';
    Alpha[0xF] = L'f';
    Alpha[0x10] = L'\0';
#endif

    if (hFileMappingObject == (HANDLE)0x1337)
    {
		if ((dwNumberOfBytesToMap % PAGE_SIZE) != 0) // check memory location granularity
        {
            return NULL;
        }

        Buffer = (PUCHAR)FT->_VirtualAlloc(NULL, dwNumberOfBytesToMap, MEM_COMMIT, PAGE_READWRITE);
        if (Buffer == NULL) {
            //FT->_MessageBoxW(0, (LPCWSTR)L"2", (LPCWSTR)L"2", 0);
            return NULL;
        }  

        Offset.LowPart = dwFileOffsetLow;
        Offset.HighPart = dwFileOffsetHigh;
        Index = 0;

        if ((Offset.QuadPart == 0ULL) && (dwNumberOfBytesToMap >= FT->HeaderSize)) //FT->HeaderSize = sizeof(DUMP_HEADER64)
        {
            for (i = Index; i < FT->HeaderSize; i += 1) 
				Buffer[i] = FT->Header[i];
            Index += FT->HeaderSize;
        }

        if (FT->IsDllLoad == FALSE) {
            DllName[0] = L'v';
            DllName[1] = L'i';
            DllName[2] = L'd';
            DllName[3] = L'.';
            DllName[4] = L'd';
            DllName[5] = L'l';
            DllName[6] = L'l';
            DllName[7] = L'\0';

            Module = FT->_LoadLibrary(DllName);

            DllName[0] = L'l';
            DllName[1] = L'i';
            DllName[2] = L'v';
            DllName[3] = L'e';
            DllName[4] = L'c';
            DllName[5] = L'l';
            DllName[6] = L'o';
            DllName[7] = L'u';
            DllName[8] = L'd';
            DllName[9] = L'k';
            DllName[0xA] = L'd';
            DllName[0xB] = L's';
            DllName[0xC] = L'd';
            DllName[0xD] = L'k';
            DllName[0xE] = L'.';
            DllName[0xF] = L'd';
            DllName[0x10] = L'l';
            DllName[0x11] = L'l';
            DllName[0x12] = L'\0';

            Module = FT->_LoadLibrary(DllName);

            FT->IsDllLoad = TRUE;
        }
        
        Position = (Offset.QuadPart + Index - FT->HeaderSize);
        SizeToMap = dwNumberOfBytesToMap - Index;

        NumberOfPages = SizeToMap / PAGE_SIZE;
        Position /= PAGE_SIZE;

        Res = FALSE;
#if 0
        lol = (ULONG)dwNumberOfBytesToMap;
        //lol = (ULONG)dwFileOffsetLow;
        Str1[7] = Alpha[(UCHAR)(lol & 0xF)];
        Str1[6] = Alpha[(UCHAR)((lol >> 4) & 0xF)];
        Str1[5] = Alpha[(UCHAR)((lol >> 8) & 0xF)];
        Str1[4] = Alpha[(UCHAR)((lol >> 12) & 0xF)];
        Str1[3] = Alpha[(UCHAR)((lol >> 16) & 0xF)];
        Str1[2] = Alpha[(UCHAR)((lol >> 20) & 0xF)];
        Str1[1] = Alpha[(UCHAR)((lol >> 24) & 0xF)];
        Str1[0] = Alpha[(UCHAR)((lol >> 28) & 0xF)];
        Str1[8] = L'\0';

        lol = (ULONG)dwFileOffsetLow;
        Str2[7] = Alpha[(UCHAR)(lol & 0xF)];
        Str2[6] = Alpha[(UCHAR)((lol >> 4) & 0xF)];
        Str2[5] = Alpha[(UCHAR)((lol >> 8) & 0xF)];
        Str2[4] = Alpha[(UCHAR)((lol >> 12) & 0xF)];
        Str2[3] = Alpha[(UCHAR)((lol >> 16) & 0xF)];
        Str2[2] = Alpha[(UCHAR)((lol >> 20) & 0xF)];
        Str2[1] = Alpha[(UCHAR)((lol >> 24) & 0xF)];
        Str2[0] = Alpha[(UCHAR)((lol >> 28) & 0xF)];
        Str2[8] = L'\0';

        FT->_MessageBoxW(0, Str1, Str2, 0);
#endif

		if (FT->PartitionInit == FALSE)
		{
			ULONG64 HvddData = (ULONG64)&FT->HvddPartition;
			FT->CurrentPartitionHandle = FT->_SdkSetData(0, InfoStructure, HvddData);
			FT->_SdkSetData(FT->CurrentPartitionHandle, InfoPartitionHandle, (ULONG64)FT->PartitionHandle); 
			FT->PartitionInit = TRUE;
		}

		Res = FT->_SdkHvmmReadPhysicalMemoryHandle(FT->CurrentPartitionHandle, Position * PAGE_SIZE, SizeToMap, Buffer + Index, FT->VmOpsConfig.ReadMethod);

        if (Res == TRUE) 
        {
			for (i = 0; i < FT->NumberOfCPU; i++)
			{
				if ((FT->ContextPageIndex[i] >= Position) && (FT->ContextPageIndex[i] < (Position + NumberOfPages))) 
				{
					C = Buffer + Index + ((FT->ContextPageIndex[i] - Position) * PAGE_SIZE); 
					C += FT->ContextOffsetLow[i] & (PAGE_SIZE - 1);

					PCONTEXT ContextBlock = &(FT->Context);

					if ((ContextBlock->SegCs == 0) || (ContextBlock->Rip == 0) || (ContextBlock->Rsp == 0))
					{
						Context64 = (PX64_CONTEXT)C;
						Context64->SegCs = KGDT64_R0_CODE;
						Context64->SegDs = (KGDT64_R3_DATA | RPL_MASK);
						Context64->SegEs = (KGDT64_R3_DATA | RPL_MASK);
						Context64->SegFs = (KGDT64_R3_CMTEB | RPL_MASK);
						Context64->SegGs = 0;
						Context64->SegGs = (KGDT64_R3_DATA | RPL_MASK);
						Context64->Rsp = FT->IdleKernelStack;
					}
					else
					{
						PUCHAR CB = (PUCHAR)ContextBlock;
						for (i = 0; i < sizeof(CONTEXT); i++)
						{
							C[i] = CB[i];
						}
					}
				}
			}
        }// end if Res == TRUE
        if (Res == FALSE) {
                     lol = (ULONG)Position;
                     Str1[7] = Alpha[(UCHAR)(lol & 0xF)];
                     Str1[6] = Alpha[(UCHAR)((lol >> 4) & 0xF)];
                     Str1[5] = Alpha[(UCHAR)((lol >> 8) & 0xF)];
                     Str1[4] = Alpha[(UCHAR)((lol >> 12) & 0xF)];
                     Str1[3] = Alpha[(UCHAR)((lol >> 16) & 0xF)];
                     Str1[2] = Alpha[(UCHAR)((lol >> 20) & 0xF)];
                     Str1[1] = Alpha[(UCHAR)((lol >> 24) & 0xF)];
                     Str1[0] = Alpha[(UCHAR)((lol >> 28) & 0xF)];
                     Str1[8] = L'\0';

                     lol = (ULONG)(SizeToMap);
                     Str2[7] = Alpha[(UCHAR)(lol & 0xF)];
                     Str2[6] = Alpha[(UCHAR)((lol >> 4) & 0xF)];
                     Str2[5] = Alpha[(UCHAR)((lol >> 8) & 0xF)];
                     Str2[4] = Alpha[(UCHAR)((lol >> 12) & 0xF)];
                     Str2[3] = Alpha[(UCHAR)((lol >> 16) & 0xF)];
                     Str2[2] = Alpha[(UCHAR)((lol >> 20) & 0xF)];
                     Str2[1] = Alpha[(UCHAR)((lol >> 24) & 0xF)];
                     Str2[0] = Alpha[(UCHAR)((lol >> 28) & 0xF)];
                     Str2[8] = L'\0';

                     FT->_MessageBoxW(0, Str1, Str2, 0);
        }
        
		KDBGPa.QuadPart = (FT->KdDebuggerDataBlockPa.QuadPart+0x2000) & 0xFFFFFFFFFFFF0000ULL;

        j = FT->KdDebuggerDataBlockPa.LowPart & 0xFFFF; // rewrite for dwReadDataByte size
        if ((KDBGPa.LowPart == Offset.LowPart) && (KDBGPa.HighPart == Offset.HighPart))
        {		
			
			for (i = 0; i < sizeof(KDDEBUGGER_DATA64); i++)
            {
				if (j >= 0xE000) 
				{
					Buffer[i + j - 0xE000] = FT->KdDebuggerDataBlockBlock[i]; 
				}
				else 
				{
					Buffer[i + j + 0x2000] = FT->KdDebuggerDataBlockBlock[i];
				}
            }
        }

        for (j = 0; j < 20; j += 1)
        {
            if ((FT->MapFile[j].Va == 0) && (FT->MapFile[j].Pa.QuadPart == 0)) break;
        }

        if (j < 20)
        {
            FT->MapFile[j].Pa.QuadPart = Position * PAGE_SIZE - Index;
            FT->MapFile[j].Va = Buffer;
            FT->MapFile[j].Size = (ULONG)SizeToMap;
        }

        return Buffer;
    }
    else
    {
        return FT->_MapViewOfFile(hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap);
    }

}

HANDLE WINAPI MyCreateFileMappingA(
  __in      HANDLE hFile,
  __in_opt  LPSECURITY_ATTRIBUTES lpAttributes,
  __in      DWORD flProtect,
  __in      DWORD dwMaximumSizeHigh,
  __in      DWORD dwMaximumSizeLow,
  __in_opt  LPCSTR lpName
)
{
    PFUNCTION_TABLE FT;
    FT = (PFUNCTION_TABLE)TABLE_OFFSET;

    if (hFile == FT->CrashDumpHandle)
    {
        return (HANDLE)0x1337;
    }

    return FT->_CreateFileMappingA(hFile, lpAttributes, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
}

HANDLE WINAPI MyCreateFileMappingW(
  __in      HANDLE hFile,
  __in_opt  LPSECURITY_ATTRIBUTES lpAttributes,
  __in      DWORD flProtect,
  __in      DWORD dwMaximumSizeHigh,
  __in      DWORD dwMaximumSizeLow,
  __in_opt  LPCWSTR lpName
)
{
    PFUNCTION_TABLE FT;

    FT = (PFUNCTION_TABLE)TABLE_OFFSET;

    if (hFile == FT->CrashDumpHandle)
    {
        return (HANDLE)0x1337;
    }

    return FT->_CreateFileMappingW(hFile, lpAttributes, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
}

BOOL WINAPI MyUnmapViewOfFile(
  __in  LPCVOID lpBaseAddress
)
{
    PFUNCTION_TABLE FT;
    ULONG i;
    BOOL Status;
    FT = (PFUNCTION_TABLE)TABLE_OFFSET;

    for (i = 0; i < 20; i += 1)
    {
        if (FT->MapFile[i].Va == lpBaseAddress)
        {
            FT->MapFile[i].Va = NULL;
            FT->MapFile[i].Pa.QuadPart = 0;
            FT->MapFile[i].Size = 0;

            Status = FT->_VirtualFree((LPVOID )lpBaseAddress, 0, MEM_RELEASE);
        }
    }

    if (i == 20) Status = FT->_UnmapViewOfFile(lpBaseAddress);

    return Status;
}

DWORD WINAPI MyGetFileSize(
  __in       HANDLE hFile,
  __out_opt  LPDWORD lpFileSizeHigh
)
{
    PFUNCTION_TABLE FT;
    FT = (PFUNCTION_TABLE)TABLE_OFFSET;

    if (hFile == FT->CrashDumpHandle)
    {
        if (lpFileSizeHigh) *lpFileSizeHigh = FT->FileSize.HighPart;
        return FT->FileSize.LowPart;
    }

    return FT->_GetFileSize(hFile, lpFileSizeHigh);
}

BOOL WINAPI MyReadFile(
  __in         HANDLE hFile,
  __out        LPVOID lpBuffer,
  __in         DWORD nNumberOfBytesToRead,
  __out_opt    LPDWORD lpNumberOfBytesRead,
  __inout_opt  LPOVERLAPPED lpOverlapped
)
{
PFUNCTION_TABLE FT;
FT = (PFUNCTION_TABLE)TABLE_OFFSET;

    return FT->_ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
}

BOOL WINAPI MyVirtualProtect(
  __in   LPVOID lpAddress,
  __in   SIZE_T dwSize,
  __in   DWORD flNewProtect,
  __out  PDWORD lpflOldProtect
)
{
    PFUNCTION_TABLE FT;
    ULONG64 i, j;
    ULONG64 Offset;
    PUCHAR Page, Source;
    BOOL Status;

    #if 0
    ULONG64 uAddress;
    PUCHAR x;
    WCHAR Str1[9];
    WCHAR Str2[9];
    WCHAR Alpha[0x11];
    ULONG lol;
    #endif

    PULONGLONG RegEsp;
    WCHAR DllName[19];

    FT = (PFUNCTION_TABLE)TABLE_OFFSET;
    RegEsp = (PULONGLONG)((PUCHAR)&lpAddress - sizeof(PVOID) + 0x80);

    if (FT->IsDllLoad == FALSE) {
        DllName[0] = L'v';
        DllName[1] = L'i';
        DllName[2] = L'd';
        DllName[3] = L'.';
        DllName[4] = L'd';
        DllName[5] = L'l';
        DllName[6] = L'l';
        DllName[7] = L'\0';

        FT->_LoadLibrary(DllName);

        DllName[0] = L'u';
        DllName[1] = L's';
        DllName[2] = L'e';
        DllName[3] = L'r';
        DllName[4] = L'3';
        DllName[5] = L'2';
        DllName[6] = L'.';
        DllName[7] = L'd';
        DllName[8] = L'l';
        DllName[9] = L'l';
        DllName[10] = L'\0';

        FT->_LoadLibrary(DllName);

        DllName[0] = L'l';
        DllName[1] = L'i';
        DllName[2] = L'v';
        DllName[3] = L'e';
        DllName[4] = L'c';
        DllName[5] = L'l';
        DllName[6] = L'o';
        DllName[7] = L'u';
        DllName[8] = L'd';
        DllName[9] = L'k';
        DllName[0xA] = L'd';
        DllName[0xB] = L's';
        DllName[0xC] = L'd';
        DllName[0xD] = L'k';
        DllName[0xE] = L'.';
        DllName[0xF] = L'd';
        DllName[0x10] = L'l';
        DllName[0x11] = L'l';
        DllName[0x12] = L'\0';

        FT->_LoadLibrary(DllName);
        FT->IsDllLoad = TRUE;
    }

    Status = FALSE;
    Page = NULL;

    if (RegEsp[4] != dwSize)
    {
        return FT->_VirtualProtect(lpAddress, dwSize, flNewProtect, lpflOldProtect);
    }

    for (i = 0; i < 20; i += 1)
    {
        if ((lpAddress >= FT->MapFile[i].Va) &&
            ((PUCHAR)lpAddress < ((PUCHAR)FT->MapFile[i].Va + FT->MapFile[i].Size)) &&
            (((PUCHAR)lpAddress + dwSize) < ((PUCHAR)FT->MapFile[i].Va + FT->MapFile[i].Size)))
        {
            
#if 0
			//FT->_MessageBoxW(0, 0, 0, 0);

			Alpha[0] = L'0';
			Alpha[1] = L'1';
			Alpha[2] = L'2';
			Alpha[3] = L'3';
			Alpha[4] = L'4';
			Alpha[5] = L'5';
			Alpha[6] = L'6';
			Alpha[7] = L'7';
			Alpha[8] = L'8';
			Alpha[9] = L'9';
			Alpha[0xA] = L'a';
			Alpha[0xB] = L'b';
			Alpha[0xC] = L'c';
			Alpha[0xD] = L'd';
			Alpha[0xE] = L'e';
			Alpha[0xF] = L'f';
			Alpha[0x10] = L'\0';

			x = (PUCHAR)lpAddress;
			x[0x10] = 0x41;
			uAddress = *(PULONG64)lpAddress;

			lol = (ULONG)uAddress & 0xFFFFFF;
			Str1[7] = Alpha[(UCHAR)(lol & 0xF)];
			Str1[6] = Alpha[(UCHAR)((lol >> 4) & 0xF)];
			Str1[5] = Alpha[(UCHAR)((lol >> 8) & 0xF)];
			Str1[4] = Alpha[(UCHAR)((lol >> 12) & 0xF)];
			Str1[3] = Alpha[(UCHAR)((lol >> 16) & 0xF)];
			Str1[2] = Alpha[(UCHAR)((lol >> 20) & 0xF)];
			Str1[1] = Alpha[(UCHAR)((lol >> 24) & 0xF)];
			Str1[0] = Alpha[(UCHAR)((lol >> 28) & 0xF)];
			Str1[8] = L'\0';

			lol = (ULONG)(uAddress >> 32);
			Str2[7] = Alpha[(UCHAR)(lol & 0xF)];
			Str2[6] = Alpha[(UCHAR)((lol >> 4) & 0xF)];
			Str2[5] = Alpha[(UCHAR)((lol >> 8) & 0xF)];
			Str2[4] = Alpha[(UCHAR)((lol >> 12) & 0xF)];
			Str2[3] = Alpha[(UCHAR)((lol >> 16) & 0xF)];
			Str2[2] = Alpha[(UCHAR)((lol >> 20) & 0xF)];
			Str2[1] = Alpha[(UCHAR)((lol >> 24) & 0xF)];
			Str2[0] = Alpha[(UCHAR)((lol >> 28) & 0xF)];
			Str2[8] = L'\0';

			//s = RegEsp[3];
			//FT->_MessageBoxA(0, (PUCHAR)(RegEsp[3]), (PUCHAR)RegEsp[3], 0);
			FT->_MessageBoxW(0, Str1, Str2, 0);

#endif
			
			Offset = (ULONG64)((PUCHAR)lpAddress - (ULONG64)FT->MapFile[i].Va);

            Page = (PUCHAR)FT->_VirtualAlloc(0, ROUND_PAGE(dwSize), MEM_COMMIT, PAGE_READWRITE);

            if (FT->_VidReadMemoryBlockPageRange(FT->PartitionHandle,
													(MB_HANDLE)1,
                                                    (FT->MapFile[i].Pa.QuadPart + Offset) / PAGE_SIZE,
                                                    ROUND_PAGE(dwSize) / PAGE_SIZE,
                                                    Page,
                                                    ROUND_PAGE(dwSize)) == FALSE)
            {
                goto Exit;
            }

            Source = (PUCHAR)RegEsp[3];

            //
            // Refresh the cached page at the same time.
            //

            for (j = 0; j < dwSize; j += 1) Page[(Offset & (PAGE_SIZE - 1)) + j] = Source[j];
            (ULONG64)lpAddress &= ~(PAGE_SIZE - 1);
            for (j = 0; j < ROUND_PAGE(dwSize); j += 1) ((PUCHAR)lpAddress)[j] = Page[j];

			if (FT->_VidWriteMemoryBlockPageRange(FT->PartitionHandle,
			                                    //FT->MemoryHandle,
												(MB_HANDLE)1,
			                                    (FT->MapFile[i].Pa.QuadPart + Offset) / PAGE_SIZE,
			                                    ROUND_PAGE(dwSize) / PAGE_SIZE,
			                                    Page,
			                                    ROUND_PAGE(dwSize)) == FALSE)
            {
                goto Exit;
            }
            else FT->_MessageBoxW(0, 0, 0, 0);

            Status = TRUE;
        }
    }

Exit:
    if (Page) FT->_VirtualFree(Page, 0, MEM_RELEASE);
    return Status;
}