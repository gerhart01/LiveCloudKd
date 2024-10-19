// leechcore_device_hvmm.h : definitions related the Hyper-V live memory diving.
//
// (c) Ulf Frisk, 2018-2024
// Author: Ulf Frisk, pcileech@frizk.net
//
// (c) Arthur Khudyaev, 2020-2024
// Author: Arthur Khudyaev, @gerhart_x
//

#ifndef __DEVICE_HVMM_H__
#define __DEVICE_HVMM_H__
#include "HvlibHandle.h"
#include <stdio.h>
#include "leechcore_device.h"
#include "conio.h"
#include "shlwapi.h"
#include "leechcore_device_hvmm_misc.h"

//
// The number of runs may vary if Dynamic Memory is enabled inside the Hyper-V virtual machine.
//

#define MAX_NUMBER_OF_RUNS MAX_NUMBER_OF_RUNS_BYTES / sizeof(PHYSICAL_MEMORY_RANGE) //correlation with MAX_NUMBER_OF_RUNS_BYTES from LiveCloudKdSdkMisc.h

#define HVMM_PROBE_MAXPAGES 0x1

//
// hvmm driver definitions
//

#define DEVICEHVMM_SERVICENAME                  "hvmm"
#define DEVICEHVMM_OBJECT                       "\\\\.\\hvmm"
#define DEVICEHVMM_DRIVERFILE                   "hvmm.sys"

//
// MemProcFs param values
//

#define HVMM_ID_PARAM_NAME                      "id="
#define HVMM_PARAM_NAME                         "hvmm://"
#define HVMM_PARAM_DELIMITER                    ","

#define HVMM_LISTVM_PARAM_NAME                  "listvm"
#define HVMM_UNIX_PARAM_NAME                    "unix"
#define HVMM_LOGLEVEL_PARAM_NAME                "loglevel"
#define HVMM_ENUM_GUEST_OS_BUILD_PARAM_NAME     "enumguestosbuild"

typedef struct pmem_info_runs {
    __int64 start;
    __int64 length;
} PHYSICAL_MEMORY_RANGE;

struct PmemMemoryInfo {
    LARGE_INTEGER CR3;
    LARGE_INTEGER NtBuildNumber;
    LARGE_INTEGER KernBase;
    LARGE_INTEGER KDBG;
    LARGE_INTEGER KPCR[MAX_PROCESSORS];
    LARGE_INTEGER PfnDataBase;
    LARGE_INTEGER PsLoadedModuleList;
    LARGE_INTEGER PsActiveProcessHead;
    LARGE_INTEGER NtBuildNumberAddr;
    LARGE_INTEGER Padding[0xfe];
    LARGE_INTEGER NumberOfRuns;
    PHYSICAL_MEMORY_RANGE Run[MAX_NUMBER_OF_RUNS];
};

typedef struct tdDEVICE_CONTEXT_HVMM {
    HANDLE hFile;
	ULONG64 Partition;
    QWORD paMax;
    ULONG Vmid;
    BOOLEAN VmidPreselected;
    BOOLEAN ListVm;
    BOOLEAN SimpleMemory;
    ULONG LogLevel;
    BOOLEAN EnumGuestOsBuild;
    BOOLEAN RemoteMode;
    LPWSTR szVmNamesList;
    struct PmemMemoryInfo MemoryInfo;
} DEVICE_CONTEXT_HVMM, *PDEVICE_CONTEXT_HVMM;

/*
* Open a "connection" to the Hyper-V partition.
* -- result
*/

BOOL HVMMStart(_Inout_ PLC_CONTEXT ctxLC);
BOOLEAN HVMM_ReadFile(
	ULONG64 PartitionHandle,
    UINT64 StartPosition, 
    PVOID lpBuffer, 
    UINT64 nNumberOfBytesToRead
);

BOOLEAN HVMM_WriteFile(
	ULONG64 PartitionHandle,
	UINT64 StartPosition,
	PVOID lpBuffer,
	UINT64 nNumberOfBytesToRead
);

//BOOL IsDigital(PLC_CONTEXT ctxLC, PCHAR str, ULONG64 len);

#endif