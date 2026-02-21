#pragma once

#ifndef __SDKENUMPUBLIC_H__
#define __SDKENUMPUBLIC_H__

#ifdef __cplusplus
extern "C" {
#endif 

#define PAGE_SIZE						0x1000
#define MAX_PROCESSORS					2048									// use for host and guest OS processor's count limitation
#define MAX_NUMBER_OF_RUNS_BYTES		0x4000									// also used in MAX_NUMBER_OF_RUNS calc in device_hvmm.c
#define KD_DEBUGGER_BLOCK_PAGE_SIZE		0x500									// correlation with KDDEBUGGER_DATA64 struct size
#define ROUND_PAGE(x)					((x + PAGE_SIZE) & ~(PAGE_SIZE - 1))
#define MZ_HEADER						0x905A4D
#define MAX_FILE_SOURCE_DMP_PATH		0x50

typedef enum _GET_CR3_TYPE {
	Cr3Process,
	Cr3Kernel		= 0xFFFFFFD,
	Cr3SecureKernel = 0xFFFFFFE,
	Cr3Hypervisor	= 0xFFFFFFF
} GET_CR3_TYPE;

typedef enum _GUEST_TYPE {
	MmUnknown,
	MmStandard,					// standard Windows with KDBG structure
	MmNonKdbgPartition,			// for hvix64\hvax64 memory area or securekernel
	MmHyperV					// Special for hvix64\hvax64 memory
} GUEST_TYPE;

typedef enum _MEMORY_ACCESS_TYPE {
	MmPhysicalMemory,
	MmVirtualMemory,
	MmAccessRtCore64,
} MEMORY_ACCESS_TYPE;

typedef enum _VTL_LEVEL {
	Vtl0	= 0,
	Vtl1	= 1,
	Vtl2	= 2,	//https://techcommunity.microsoft.com/blog/windowsosplatform/openhcl-evolving-azure%E2%80%99s-virtualization-model/4248345
	BadVtl	= 3
} VTL_LEVEL, *PVTL_LEVEL;

typedef enum _MACHINE_TYPE {
	MACHINE_UNKNOWN = 0,
	MACHINE_X86 = 1,
	MACHINE_AMD64 = 2,
	MACHINE_ARM64 = 3,
	MACHINE_UNSUPPORTED = 4
} MACHINE_TYPE, *PMACHINE_TYPE;

typedef enum _HVMM_INFORMATION_CLASS {
	InfoKdbgData,					
	InfoPartitionFriendlyName,		
	InfoPartitionId,				
	InfoVmtypeString,				
	InfoStructure,					
	InfoKiProcessorBlock,			
	InfoMmMaximumPhysicalPage,		
	InfoKPCR,						
	InfoNumberOfCPU,				
	InfoKDBGPa,						
	InfoNumberOfRuns,				
	InfoKernelBase,
	InfoMmPfnDatabase,
	InfoPsLoadedModuleList,
	InfoPsActiveProcessHead,
	InfoNtBuildNumber,
	InfoNtBuildNumberVA,
	InfoDirectoryTableBase,
	InfoRun,
	InfoKdbgDataBlockArea,
	InfoVmGuidString,
	InfoPartitionHandle,
	InfoKdbgContext,
	InfoKdVersionBlock,
	InfoMmPhysicalMemoryBlock,
	InfoNumberOfPages,
	InfoIdleKernelStack,
	InfoSizeOfKdDebuggerData,
	InfoCpuContextVa,
	InfoSize,
	InfoMemoryBlockCount,
	InfoSuspendedCores,
	InfoSuspendedWorker,
	InfoIsContainer,
	InfoIsNeedVmwpSuspend,
	InfoGuestOsType,
	InfoSettingsCrashDumpEmulation,
	InfoSettingsUseDecypheredKdbg,
	InfoBuilLabBuffer,
	InfoHvddGetCr3byPid,
	InfoGetProcessesIds,
	InfoDumpHeaderPointer,
	InfoUpdateCr3ForLocal,
	InfoHvddGetCr3Kernel,
	InfoHvddGetCr3Hv,
	InfoHvddGetCr3Securekernel,
	InfoIsVmSuspended,
	//Special set values
	InfoSetMemoryBlock,
	InfoEnlVmcsPointer
} HVMM_INFORMATION_CLASS;

typedef enum _VM_STATE_ACTION {
	SuspendVm,
	ResumeVm
} VM_STATE_ACTION;

typedef enum _SUSPEND_RESUME_METHOD {
	SuspendResumeUnsupported,
	SuspendResumePowershell,
	SuspendResumeWriteSpecRegister
} SUSPEND_RESUME_METHOD;

typedef enum _WRITE_MEMORY_METHOD {
	WriteInterfaceUnsupported,
	WriteInterfaceHvmmDrvInternal,
	WriteInterfaceWinHv,                 
	WriteInterfaceHvmmLocal,
	WriteInterfaceMax
} WRITE_MEMORY_METHOD;

typedef enum _READ_MEMORY_METHOD {
	ReadInterfaceUnsupported,
	ReadInterfaceHvmmDrvInternal,
	ReadInterfaceWinHv,                  
	ReadInterfaceHvmmLocal,
	ReadInterfaceMax
} READ_MEMORY_METHOD;

typedef struct _VM_OPERATIONS_CONFIG {
	READ_MEMORY_METHOD ReadMethod;
	WRITE_MEMORY_METHOD WriteMethod;
	SUSPEND_RESUME_METHOD SuspendMethod;
	ULONG64 LogLevel;
	BOOLEAN ForceFreezeCPU;
	BOOLEAN PausePartition;
	HANDLE ExdiConsoleHandle;
	BOOLEAN ReloadDriver;
	BOOLEAN PFInjection;
	BOOLEAN NestedScan;
	BOOLEAN UseDebugApiStopProcess;
	BOOLEAN SimpleMemory;
	BOOLEAN ReplaceDecypheredKDBG;
	BOOLEAN FullCrashDumpEmulation;
	BOOLEAN EnumGuestOsBuild;
	BOOLEAN VSMScan;
	BOOLEAN LogSilenceMode;
} VM_OPERATIONS_CONFIG, * PVM_OPERATIONS_CONFIG;

typedef enum {
	CONSOLE_RED = 1,
	CONSOLE_BLUE = 2,
	CONSOLE_GREEN = 3
} CONSOLE_COLOR;

#ifdef __cplusplus
};
#endif

#endif //#ifndef __SDKPUBLIC_H__