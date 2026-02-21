#pragma once

#ifndef __SDKHANDLE_H__
#define __SDKHANDLE_H__

#ifdef __cplusplus
extern "C" {
#endif 

//#ifdef LIVECLOUDKDSDK_EXPORTS
//#define FUNCTION_TYPE DLLEXPORT
//#else
//#define FUNCTION_TYPE DECLSPEC_IMPORT
//#endif

#include "HvlibDllExport.h"
#include <windows.h> 
#include "HvlibEnumPublic.h"
#include "HyperV\vid.h"
#include <dbghelp.h>
 

//
// Next functions are wrappers for non-C languages
//

//
// Get preferred memory operations for different OS
//

FUNCTION_TYPE
BOOLEAN
SdkInvokeHypercall(
	_In_ ULONG32 HvCallId,
	_In_ BOOLEAN IsFast,
	_In_ UINT32 RepStartIndex,
	_In_ UINT32 CountOfElements,
	_In_ BOOLEAN IsNested,
	_In_ PVOID InputBuffer,
	_In_ PVOID OutputBuffer
);

//
// Get preferred memory operations for different OS.
//

FUNCTION_TYPE
BOOLEAN
SdkGetDefaultConfig(_Inout_ PVM_OPERATIONS_CONFIG VmOperationsConfig);

//
// Set config for hvlib.dll
//

FUNCTION_TYPE
BOOLEAN
SdkSetPartitionConfig(_In_ ULONG64 PartitionHandle, _In_ PVM_OPERATIONS_CONFIG VmOperationsConfig);

//
// Get config for hvlib.dll for special partition
//

FUNCTION_TYPE
PVM_OPERATIONS_CONFIG
SdkGetPartitionConfig(_In_ ULONG64 PartitionHandle);

//
// Enumerate active Hyper-V partitions, table of handles is returned
//

FUNCTION_TYPE PULONG64
SdkEnumPartitions(_Inout_ PULONG64 PartitionTableCount, _In_ PVM_OPERATIONS_CONFIG VmOpsConfig);

//
// Get information about partition object, using handle
//

FUNCTION_TYPE BOOLEAN SdkGetData(_In_ ULONG64 PartitionHandle, _In_ HVMM_INFORMATION_CLASS HvmmInformationClass, _Inout_ PVOID InfoInformation);
FUNCTION_TYPE ULONG64 SdkGetData2(_In_ ULONG64 PartitionHandle, _In_ HVMM_INFORMATION_CLASS HvmmInformationClass);
FUNCTION_TYPE ULONG64 SdkSetData(_In_ ULONG64 PartitionHandle, _In_ HVMM_INFORMATION_CLASS HvmmInformationClass, _In_ ULONG64 InfoInformation);

//
// Select current partition object, fill it with additional guest os info.
//

FUNCTION_TYPE BOOLEAN SdkSelectPartition(_In_ ULONG64 PartitionHandle);

//
// Read bytes from guest physical address (GPA) to (ClientBuffer)
//

FUNCTION_TYPE BOOLEAN SdkReadPhysicalMemory(_In_ ULONG64 PartitionHandle, _In_ UINT64 StartPosition, _In_ UINT64 ReadByteCount, _Inout_ PVOID ClientBuffer, _In_ READ_MEMORY_METHOD Method);

//
// Write bytes from buffer (ClientBuffer) to guest OS physical address space (GPA) 
//

FUNCTION_TYPE BOOLEAN SdkWritePhysicalMemory(_In_ ULONG64 PartitionHandle, _In_ UINT64 StartPosition, _In_ UINT64 WriteBytesCount, _In_ PVOID ClientBuffer, _In_ WRITE_MEMORY_METHOD Method);

//
// Read bytes from guest virtual address (GVA) to (Buffer)
//

FUNCTION_TYPE BOOLEAN SdkReadVirtualMemory(_In_ ULONG64 PartitionHandle, _In_ ULONG64 Va, _Out_ PVOID Buffer, _In_ ULONG Size);

//
// Write bytes from buffer (ClientBuffer) to guest OS virtual address space (GVA) 
//

FUNCTION_TYPE BOOLEAN SdkWriteVirtualMemory(_In_ ULONG64 PartitionHandle, _In_ ULONG64 Va, _In_ PVOID Buffer, _In_ ULONG Size);


//
// Resume or suspending VM (Action is SuspendVm or ResumeVm)
// ActionMethod is 	SuspendResumePowershell or SuspendResumeWriteSpecRegister
//

FUNCTION_TYPE BOOLEAN SdkControlVmState(_In_ ULONG64 PartitionHandle, _In_ VM_STATE_ACTION Action, _In_ SUSPEND_RESUME_METHOD ActionMethod, _In_ BOOLEAN ManageWorkerProcess);

//
// Get physical address for specifying GVA
//

FUNCTION_TYPE ULONG64 SdkGetPhysicalAddress(_In_ ULONG64 PartitionHandle, _In_ ULONG64 Va, _In_ MEMORY_ACCESS_TYPE MmAccess);

//
// Get architecture of guest VM (x86 or AMD64). Plugin supports only AMD64 guest VMs
//

FUNCTION_TYPE MACHINE_TYPE SdkGetMachineType(_In_ ULONG64 PartitionHandle);

//
// Read guest OS registers. 
//

FUNCTION_TYPE BOOLEAN SdkWriteVpRegister(_In_ ULONG64 PartitionHandle, _In_ HV_VP_INDEX VpIndex, _In_ VTL_LEVEL InputVtl, _In_ HV_REGISTER_NAME RegisterCode, _In_ PHV_REGISTER_VALUE RegisterValue);

//
// Write guest OS registers.  
//

FUNCTION_TYPE BOOLEAN SdkReadVpRegister(_In_ ULONG64 PartitionHandle, _In_ HV_VP_INDEX VpIndex, _In_ VTL_LEVEL InputVtl, _In_ HV_REGISTER_NAME RegisterCode, _Inout_ PHV_REGISTER_VALUE RegisterValue);

//
// Close and free all objects, when program is closing
//

FUNCTION_TYPE BOOLEAN SdkCloseAllPartitions();

//
// Close one partition object. Can be used if multiple instances are opened
//

FUNCTION_TYPE VOID SdkClosePartition(ULONG64 Handle);

//
// Read bytes from guest physical address (GPA) to (ClientBuffer)
//

FUNCTION_TYPE BOOLEAN SdkNumberToString(_In_ WCHAR* Str1, _In_ WCHAR* Str2, _In_ ULONG Num1, _In_ ULONG Num2);

//
// Symbols exported function
//

FUNCTION_TYPE ULONG64 SdkSymGetSymbolAddress(_In_ ULONG64 PartitionHandle, _In_ ULONG64 ImageBase, _In_ ULONG ImageSize, _In_ const char* symbolName, _In_ MEMORY_ACCESS_TYPE MmAccess);
FUNCTION_TYPE BOOLEAN SdkSymEnumAllSymbols(_In_ ULONG64 PartitionHandle, _In_ PWCHAR DriverName, _Inout_opt_ PSYMBOL_INFO_PACKAGE pSymbolTable, _In_ MEMORY_ACCESS_TYPE MmAccess);
FUNCTION_TYPE ULONG64 SdkSymEnumAllSymbolsGetTableLength(_In_ ULONG64 PartitionHandle, _In_ PWCHAR DriverName, _In_ MEMORY_ACCESS_TYPE MmAccess);

#ifdef __cplusplus
};
#endif

#endif //#ifndef __SDKPUBLIC_H__