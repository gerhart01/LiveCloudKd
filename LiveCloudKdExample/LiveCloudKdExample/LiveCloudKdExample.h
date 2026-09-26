#pragma once

#include <conio.h>
#include <stdio.h>

#define HANDLE_TYPE_EXAMPLE

#include "HvlibHandle.h"

#define DUMP_BLOCK_SIZE 1024*1024
#define DUMP_PAGE_SIZE 0x1000

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
// MappedMemoryDemo.c: verification and benchmark of the mapped guest memory path.
//

BOOLEAN MappedMemoryDemo(ULONG64 Partition);
VOID MappedMemoryLayoutReport(ULONG64 Partition);
VOID MappedMemoryEnumDump(ULONG64 Partition);

//
// Unattended run: "LiveCloudKdExample.exe <vm index> <action>" answers every prompt itself
// (no VM suspend, no lifecycle step) and exits without waiting for a key. Both stay -1 when
// the program is started without arguments.
//

extern int g_AutoVmId;
extern int g_AutoActionId;

//
// Read one key from the console, or take AutoAnswer on an unattended run.
//

int DemoReadKey(int AutoAnswer);
