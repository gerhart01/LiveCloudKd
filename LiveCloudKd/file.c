/*++
	Microsoft Hyper-V Virtual Machine Physical Memory Dumper
	Copyright (C) Matt Suiche. All rights reserved.

Module Name:

	- file.c

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