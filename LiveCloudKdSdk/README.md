- [Hvlib API (1.1.0)](#hvlib-api-110)
- [1. SdkGetDefaultConfig](#1-sdkgetdefaultconfig)
- [2. SdkSetPartitionConfig](#2-sdksetpartitionconfig)
- [3. SdkGetPartitionConfig](#3-sdkgetpartitionconfig)
- [4. SdkEnumPartitions](#4-sdkenumpartitions)
- [5. SdkSelectPartition](#5-sdkselectpartition)
- [6. SdkGetData (SdkGetData2, SdkSetData)](#6-sdkgetdata-sdkgetdata2-sdksetdata)
- [7. SdkReadPhysicalMemory](#7-sdkreadphysicalmemory)
- [8. SdkReadVirtualMemory](#8-sdkreadvirtualmemory)
- [9. SdkWritePhysicalMemory](#9-sdkwritephysicalmemory)
- [10. SdkWriteVirtualMemory](#10-sdkwritevirtualmemory)
- [11. SdkGetPhysicalAddress](#11-sdkgetphysicaladdress)
- [12. SdkReadVpRegister](#12-sdkreadvpregister)
- [13. SdkWriteVpRegister](#13-sdkwritevpregister)
- [14. SdkControlVmState](#14-sdkcontrolvmstate)
- [15. SdkGetMachineType](#15-sdkgetmachinetype)
- [16. SdkInvokeHypercall](#16-sdkinvokehypercall)
- [17. SdkCloseAllPartitions](#17-sdkcloseallpartitions)
- [18. SdkClosePartition](#18-sdkclosepartition)
- [19. SdkSymGetSymbolAddress](#19-sdksymgetsymboladdress)
- [20. SdkSymEnumAllSymbols](#20-sdksymenumallsymbols)
- [21. SdkSymEnumAllSymbolsGetTableLength](#21-sdksymenumallsymbolsgettablelength)

# Hvlib API (1.1.0)

Hvlib is plugin for working with MS Hyper-V virtual machines memory. It was developed, when
LiveCloudKd was rewritten for supporting new Windows versions, therefore it supports latest Windows 11 and Windows Server 2025 builds.
For working with old MS OS versions you can use old distributives or open issue with needed OS specifying

Part of code was taken from LiveCloudKd project https://github.com/msuiche/LiveCloudKd by Matt Suiche (www.msuiche.com).

So it was developed, because MS doesn't provide necessary API description and examples of vid.dll, vid.sys, winhv.sys, winhvr.sys, hvix64.exe, hvax64.exe, hvaa64.exe

Hvlib supports three memory access methods for Hyper-V memory

1. ReadInterfaceWinHv\WriteInterfaceWinHv - uses hypercalls HvReadGpa for memory reading and HvWriteGpa for writing. According Hyper-V TLFS HvReadGpa\HvWriteGpa can read\write only
maximum 0x10 bytes per call, therefore it slowest method (but robust - Sysinternals LiveKD uses it)

2. ReadInterfaceHvmmDrvInternal\WriteInterfaceHvmmDrvInternal - hvmm.sys directly read memory blocks from kernel mode. It uses internal vid.sys structures
and different methods for standard Hyper-V VM.

3. ReadInterfaceHvmmDrvInternal\WriteInterfaceHvmmDrvInternal - for local OS memory reading.

Of course, it can be broken during every patch of vid.sys, but now it works with new version of vid.sys without patching.

You can see examples of using in

```
LiveCloudKd project
MemProcsFS leechcore plugin project
EXDi plugin for WinDBG

and separate LiveCloudKdExample project
```

1. Fill VmOperationsConfig structure with options. See available options in VM_OPERATIONS_CONFIG structure
2. Get list of running Virtual Machines using SdkEnumPartitions function.

```
	SdkGetDefaultConfig(&VmOperationsConfig);
	PULONG64 Partitions = SdkEnumPartitions(&PartitionCount, &VmOperationsConfig);
```

3. Next use

	SdkSelectPartition(Partitions[i]) partition index in Partitions array

4. Use SDK functions, which you need (see HvlibEnumPublic.h for description). Use HvlibHandle.h for more simple type functions. See LiveCloudKdExample  for more details.
5. Call SdkCloseAllPartitions(), when your program is closing.

For using it you have to:
1. Include header file HvlibEnumPublic.h in your project. It contains all public functions definitions with small descriptions.
2. Include library hvlib.lib in Visual Studio:
```
   Linker->All Options->Additional Dependencies
```
3. Place hvlib.dll and hvmm.sys in one folder.

Functions description:

```c

Example:
See more examples in LiveCloudKdExample project. [Link](https://github.com/gerhart01/LiveCloudKd/tree/master/LiveCloudKdExample)

```

# 1. SdkGetDefaultConfig

Get preferred memory operation defaults for different OS. Fills VM_OPERATIONS_CONFIG structure with recommended settings

```c
BOOLEAN
SdkGetDefaultConfig(
	_Inout_ PVM_OPERATIONS_CONFIG VmOperationsConfig
);
```

Parameters:

* **VmOperationsConfig** - pointer to VM_OPERATIONS_CONFIG structure to be filled with default values

Example:

```c
VM_OPERATIONS_CONFIG VmOperationsConfig = { 0 };

SdkGetDefaultConfig(&VmOperationsConfig);
```

result: BOOLEAN

# 2. SdkSetPartitionConfig

Set config for hvlib.dll for specific partition

```c
BOOLEAN
SdkSetPartitionConfig(
	_In_ ULONG64 PartitionHandle,
	_In_ PVM_OPERATIONS_CONFIG VmOperationsConfig
);
```

Parameters:

* **PartitionHandle** - handle of partition
* **VmOperationsConfig** - pointer to VM_OPERATIONS_CONFIG structure with desired settings

Example:

```c
VM_OPERATIONS_CONFIG VmOperationsConfig = { 0 };

SdkGetDefaultConfig(&VmOperationsConfig);
VmOperationsConfig.ReadMethod = ReadInterfaceWinHv;
SdkSetPartitionConfig(g_CurrentPartitionIntHandle, &VmOperationsConfig);
```

result: BOOLEAN

# 3. SdkGetPartitionConfig

Get config for hvlib.dll for specific partition

```c
PVM_OPERATIONS_CONFIG
SdkGetPartitionConfig(
	_In_ ULONG64 PartitionHandle
);
```

Parameters:

* **PartitionHandle** - handle of partition

Example:

```c
PVM_OPERATIONS_CONFIG pConfig = SdkGetPartitionConfig(g_CurrentPartitionIntHandle);

if (pConfig)
	wprintf(L"Read method: %d\n", pConfig->ReadMethod);
```

result: PVM_OPERATIONS_CONFIG (pointer to config structure, or NULL on failure)

# 4. SdkEnumPartitions

Enumerate active Hyper-V partitions

```c
PULONG64
SdkEnumPartitions(
	_Inout_ PULONG64 PartitionTableCount,
	_In_ PVM_OPERATIONS_CONFIG VmOpsConfig
)
```

```c

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

```

result - PULONG64

# 5. SdkSelectPartition

Select current partition object and fill it with additional guest OS information

```c
BOOLEAN
SdkSelectPartition(
	_In_ ULONG64 PartitionHandle
);
```

Parameters:

* **PartitionHandle** - handle of partition obtained from SdkEnumPartitions

Example:

```c
PULONG64 Partitions = SdkEnumPartitions(&PartitionCount, &VmOperationsConfig);

if (Partitions && PartitionCount > 0)
	SdkSelectPartition(Partitions[0]);
```

result: BOOLEAN

# 6. SdkGetData (SdkGetData2, SdkSetData)

Description: get or set specific data for partition

```c
BOOLEAN
SdkGetData(
	_In_ ULONG64 PartitionHandle,
	_In_ HVMM_INFORMATION_CLASS HvmmInformationClass,
	_Inout_ PVOID InfoInformation
)
```

parameters:

* **PartitionHandle** - handle of partition
* **HvmmInformationClass** - information class selector
* **InfoInformation** - returned buffer

available values:

```c
InfoPartitionFriendlyName = 1
InfoPartitionId = 2
InfoVmtypeString = 3
InfoMmMaximumPhysicalPage = 6
InfoKernelBase = 11
InfoVmGuidString = 20
InfoGuestOsType = 35
```

Example:

```c
ULONG64 PartitionId = 0;
WCHAR* VmTypeString = NULL;
CHAR* VmmNameString = NULL;
WCHAR* VmGuidString = NULL;

SdkGetData(Partitions[i], InfoPartitionFriendlyName, &FriendlyNameP);
SdkGetData(Partitions[i], InfoPartitionId, &PartitionId);
SdkGetData(Partitions[i], InfoVmtypeString, &VmTypeString);
SdkGetData(Partitions[i], InfoVmGuidString, &VmGuidString);
```

result - BOOLEAN

SdkGetData2 same as SdkGetData, but with different parameters and more comfortable in some cases

```c
ULONG64
SdkGetData2(
	_In_ ULONG64 PartitionHandle,
	_In_ HVMM_INFORMATION_CLASS HvmmInformationClass
)
```

parameters:

* **PartitionHandle** - handle of partition
* **HvmmInformationClass** - information class selector

Example:

```c
	GUEST_TYPE GuestOsType = SdkGetData2(g_Partition, InfoGuestOsType);
```

return value ULONG64 - value of requested parameter (in some cases it can be address of buffer)

SdkSetData sets specific data for a partition

```c
ULONG64
SdkSetData(
	_In_ ULONG64 PartitionHandle,
	_In_ HVMM_INFORMATION_CLASS HvmmInformationClass,
	_In_ ULONG64 InfoInformation
)
```

parameters:

* **PartitionHandle** - handle of partition
* **HvmmInformationClass** - information class selector
* **InfoInformation** - value to set

Example:

```c
	// Primary use case: recreate a partition object in a new process from a serialized
	// InfoPartition structure. When PartitionHandle is 0 and InfoStructure is passed,
	// SdkSetData returns a new partition handle that can be used by the current process.
	ULONG64 NewPartitionHandle = SdkSetData(0, InfoStructure, (ULONG64)&InfoPartition);

	// Configure the new partition object before use
	SdkSetData(NewPartitionHandle, InfoPartitionHandle, (ULONG64)DuplicatedHandle);
	SdkSetData(NewPartitionHandle, InfoSettingsCrashDumpEmulation, TRUE);
	SdkSetData(NewPartitionHandle, InfoSettingsUseDecypheredKdbg, TRUE);
```

return value ULONG64 - new partition handle (when called with InfoStructure and PartitionHandle=0),
or result of the set operation for other information classes

# 7. SdkReadPhysicalMemory

Description: Read memory block from specified physical address

Parameters:
* **PartitionHandle** - handle of virtual machine
* **StartPosition** - physical memory address
* **ReadByteCount** - size of memory block for reading
* **ClientBuffer** - buffer for reading data
* **Method** - method for reading data

```python
BOOLEAN
SdkReadPhysicalMemory(
	_In_ ULONG64 PartitionHandle,
	_In_ UINT64 StartPosition,
	_In_ UINT64 ReadByteCount,
	_Inout_ PVOID ClientBuffer,
	_In_ READ_MEMORY_METHOD Method
```

Example:

```c
UINT64 Address = 0;
ULONG nNumberOfBytesToRead = 4;
PVOID lpBuffer = malloc(0x1000);

if (!lpBuffer)
	return FALSE;

Ret = SdkReadPhysicalMemory(g_CurrentPartitionIntHandle, Address, (ULONG)nNumberOfBytesToRead, lpBuffer, g_MemoryReadInterfaceType);

free(lpBuffer);
```

result: BOOLEAN

# 8. SdkReadVirtualMemory

Description: Read memory block from specified virtual address

Parameters:
* **PartitionHandle** - handle of virtual machine
* **Va** - virtual memory address
* **Buffer** - buffer for reading data
* **Size** - size of memory block for reading

```c
BOOLEAN
SdkReadVirtualMemory(
	_In_ ULONG64 PartitionHandle,
	_In_ ULONG64 Va,
	_Out_ PVOID Buffer,
	_In_ ULONG Size
)
```

Example:

```c
CONTEXT Context = { 0 };
ULONG64 VirtualAddress = 0xFFFFF80000000000;

SdkReadVirtualMemory(g_CurrentPartitionIntHandle, VirtualAddress, &Context, sizeof(CONTEXT));
```

result: BOOLEAN

# 9. SdkWritePhysicalMemory

Description: Write data to specified physical address in virtual machine

```c
BOOLEAN
SdkWritePhysicalMemory(
	_In_ ULONG64 PartitionHandle,
	_In_ UINT64 StartPosition,
	_In_ UINT64 WriteBytesCount,
	_In_ PVOID ClientBuffer,
	_In_ WRITE_MEMORY_METHOD Method
)
```

Parameters:
* **PartitionHandle** - handle of virtual machine
* **StartPosition** - physical memory address
* **WriteBytesCount** - size of memory block for writing
* **ClientBuffer** - buffer with data to write
* **Method** - method for writing data

Example:

```c
	UINT64 Address = 0x1000;
	ULONG64 buffer = 0x1234567812345678;
	BOOLEAN Ret;

	Ret = SdkWritePhysicalMemory(g_CurrentPartitionIntHandle, Address, sizeof(ULONG64), &buffer, g_MemoryWriteInterfaceType);
```

result: BOOLEAN

# 10. SdkWriteVirtualMemory

Description: Write data to specified virtual address in virtual machine

```c
BOOLEAN
SdkWriteVirtualMemory(
	_In_ ULONG64 PartitionHandle,
	_In_ ULONG64 Va,
	_In_ PVOID Buffer,
	_In_ ULONG Size
)
```

Parameters:
* **PartitionHandle** - handle of virtual machine
* **Va** - virtual memory address
* **Buffer** - buffer with data to write
* **Size** - size of memory block for writing

Example:

```c
	ULONG64 Va = 0xFFFFF80000000000;
	ULONG64 buffer = 0xFFFFFF8000000000;
	BOOLEAN Ret;

	Ret = SdkWriteVirtualMemory(g_CurrentPartitionIntHandle, Va, &buffer, (ULONG)sizeof(ULONG64));
```

result: BOOLEAN

# 11. SdkGetPhysicalAddress

Description: Get physical address (GPA) for specified guest virtual address (GVA)

```c
ULONG64
SdkGetPhysicalAddress(
	_In_ ULONG64 PartitionHandle,
	_In_ ULONG64 Va,
	_In_ MEMORY_ACCESS_TYPE MmAccess
)
```

Parameters:

* **PartitionHandle** - handle of virtual machine
* **Va** - guest virtual address to translate
* **MmAccess** - memory access type (MmPhysicalMemory or MmVirtualMemory)

MEMORY_ACCESS_TYPE:
```c
MmPhysicalMemory
MmVirtualMemory
MmAccessRtCore64
```

Example:

```c
	ULONG64 Va = 0xFFFFF80000000000;
	ULONG64 Pa = SdkGetPhysicalAddress(g_CurrentPartitionIntHandle, Va, MmVirtualMemory);

	if (Pa)
		wprintf(L"Physical address: 0x%I64X\n", Pa);
```

result: ULONG64 (physical address, or 0 on failure)

# 12. SdkReadVpRegister

Description: Read a register value from the specified virtual processor of guest OS

```c
BOOLEAN
SdkReadVpRegister(
	_In_ ULONG64 PartitionHandle,
	_In_ HV_VP_INDEX VpIndex,
	_In_ VTL_LEVEL InputVtl,
	_In_ HV_REGISTER_NAME RegisterCode,
	_Inout_ PHV_REGISTER_VALUE RegisterValue
)
```

Parameters:

* **PartitionHandle** - handle of virtual machine
* **VpIndex** - index of virtual processor (0-based)
* **InputVtl** - Virtual Trust Level (Vtl0, Vtl1, or Vtl2)
* **RegisterCode** - register identifier (HV_REGISTER_NAME)
* **RegisterValue** - pointer to buffer receiving the register value

VTL_LEVEL:
```c
Vtl0 = 0
Vtl1 = 1
Vtl2 = 2
```

Example:

```c
	HV_REGISTER_VALUE RegisterValue = { 0 };

	SdkReadVpRegister(g_CurrentPartitionIntHandle, 0, Vtl0, HvX64RegisterRip, &RegisterValue);
	wprintf(L"RIP = 0x%I64X\n", RegisterValue.Reg64);
```

result: BOOLEAN

# 13. SdkWriteVpRegister

Description: Write a register value to the specified virtual processor of guest OS

```c
BOOLEAN
SdkWriteVpRegister(
	_In_ ULONG64 PartitionHandle,
	_In_ HV_VP_INDEX VpIndex,
	_In_ VTL_LEVEL InputVtl,
	_In_ HV_REGISTER_NAME RegisterCode,
	_In_ PHV_REGISTER_VALUE RegisterValue
)
```

Parameters:

* **PartitionHandle** - handle of virtual machine
* **VpIndex** - index of virtual processor (0-based)
* **InputVtl** - Virtual Trust Level (Vtl0, Vtl1, or Vtl2)
* **RegisterCode** - register identifier (HV_REGISTER_NAME)
* **RegisterValue** - pointer to buffer containing the value to write

Example:

```c
	HV_REGISTER_VALUE RegisterValue = { 0 };
	RegisterValue.Reg64 = 0xFFFFF80000000000;

	SdkWriteVpRegister(g_CurrentPartitionIntHandle, 0, Vtl0, HvX64RegisterRip, &RegisterValue);
```

result: BOOLEAN

# 14. SdkControlVmState

Resume or suspending Hyper-V virtual machine

VM_STATE_ACTION:
```
	SuspendVm
	ResumeVm
```

SUSPEND_RESUME_METHOD:
```
	SuspendResumeUnsupported
	SuspendResumePowershell
	SuspendResumeWriteSpecRegister
```
 is

```c
BOOLEAN
SdkControlVmState(
	_In_ ULONG64 PartitionHandle,
	_In_ VM_STATE_ACTION Action,
	_In_ SUSPEND_RESUME_METHOD ActionMethod,
	_In_ BOOLEAN ManageWorkerProcess
);

Example:

	SdkControlVmState(g_CurrentPartitionIntHandle, SuspendVm, SuspendResumePowershell, FALSE);
	SdkControlVmState(g_CurrentPartitionIntHandle, ResumeVm, SuspendResumePowershell, FALSE);

	SdkControlVmState(g_CurrentPartitionIntHandle, SuspendVm, SuspendResumeWriteSpecRegister, FALSE);
	SdkControlVmState(g_CurrentPartitionIntHandle, ResumeVm, SuspendResumeWriteSpecRegister, FALSE);

```

result: BOOLEAN

# 15. SdkGetMachineType

Get architecture of guest VM. Plugin supports AMD64 and ARM64 guest virtual machines

```c
typedef enum _MACHINE_TYPE {
	MACHINE_UNKNOWN = 0,
	MACHINE_X86 = 1,
	MACHINE_AMD64 = 2,
	MACHINE_ARM64 = 3,
	MACHINE_UNSUPPORTED = 4
} MACHINE_TYPE, *PMACHINE_TYPE;
```

```c
MACHINE_TYPE
SdkGetMachineType(
	_In_ ULONG64 PartitionHandle
);


Example:

	if (SdkGetMachineType(PartitionEntry) == MACHINE_AMD64)
		return TRUE;

```

result: MACHINE_TYPE

# 16. SdkInvokeHypercall

Invoke a Hyper-V hypercall directly

```c
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
```

Parameters:

* **HvCallId** - hypercall identifier as defined in Hyper-V TLFS
* **IsFast** - TRUE for fast hypercall (registers only, no memory pages)
* **RepStartIndex** - start index for rep hypercalls, 0 for simple calls
* **CountOfElements** - number of elements for rep hypercalls, 0 for simple calls
* **IsNested** - TRUE if the hypercall is issued from a nested hypervisor context
* **InputBuffer** - pointer to input parameter buffer
* **OutputBuffer** - pointer to output parameter buffer

Example:

```c
	PVOID InputBuffer = VirtualAlloc(NULL, PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
	PVOID OutputBuffer = VirtualAlloc(NULL, PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);

	BOOLEAN Result = SdkInvokeHypercall(HvCallGetVpRegisters, FALSE, 0, 0, FALSE, InputBuffer, OutputBuffer);

	VirtualFree(InputBuffer, 0, MEM_RELEASE);
	VirtualFree(OutputBuffer, 0, MEM_RELEASE);
```

result: BOOLEAN

# 17. SdkCloseAllPartitions
Close and free all objects for all opened partition

```c
BOOLEAN
	SdkCloseAllPartitions();
```

Example:

```c
SdkCloseAllPartitions();
```

result: BOOLEAN

# 18. SdkClosePartition

Close and free all objects for specific partition. Execute this function, when you stop working with specific partition

```c
VOID
SdkClosePartition(
	ULONG64 Handle
	)
```

Example:
```c
SdkClosePartition(g_CurrentPartitionIntHandle);
```

result: VOID

# 19. SdkSymGetSymbolAddress

Get the virtual address of a symbol in the guest OS by driver image base and symbol name

```c
ULONG64
SdkSymGetSymbolAddress(
	_In_ ULONG64 PartitionHandle,
	_In_ ULONG64 ImageBase,
	_In_ ULONG ImageSize,
	_In_ const char* symbolName,
	_In_ MEMORY_ACCESS_TYPE MmAccess
);
```

Parameters:

* **PartitionHandle** - handle of virtual machine
* **ImageBase** - virtual address of the driver image base in guest OS
* **ImageSize** - size of the driver image in bytes
* **symbolName** - null-terminated ANSI string with the symbol name to look up
* **MmAccess** - memory access type (MmPhysicalMemory or MmVirtualMemory)

Example:

```c
	ULONG64 KernelBase = SdkGetData2(g_Partition, InfoKernelBase);
	ULONG KernelSize = 0; // obtained separately

	ULONG64 SymAddr = SdkSymGetSymbolAddress(g_CurrentPartitionIntHandle, KernelBase, KernelSize, "PsInitialSystemProcess", MmVirtualMemory);

	if (SymAddr)
		wprintf(L"PsInitialSystemProcess VA: 0x%I64X\n", SymAddr);
```

result: ULONG64 (virtual address of the symbol, or 0 on failure)

# 20. SdkSymEnumAllSymbols

Enumerate all symbols for the specified driver in guest OS. Fills an array of SYMBOL_INFO_PACKAGE structures

```c
BOOLEAN
SdkSymEnumAllSymbols(
	_In_ ULONG64 PartitionHandle,
	_In_ PWCHAR DriverName,
	_Inout_opt_ PSYMBOL_INFO_PACKAGE pSymbolTable,
	_In_ MEMORY_ACCESS_TYPE MmAccess
);
```

Parameters:

* **PartitionHandle** - handle of virtual machine
* **DriverName** - null-terminated wide-character string with the driver name (e.g. L"ntoskrnl.exe")
* **pSymbolTable** - pointer to a pre-allocated array of SYMBOL_INFO_PACKAGE structures to be filled, or NULL to query required count
* **MmAccess** - memory access type (MmPhysicalMemory or MmVirtualMemory)

Example:

```c
	ULONG64 TableLength = SdkSymEnumAllSymbolsGetTableLength(g_CurrentPartitionIntHandle, L"ntoskrnl.exe", MmVirtualMemory);

	PSYMBOL_INFO_PACKAGE pTable = (PSYMBOL_INFO_PACKAGE)malloc(TableLength * sizeof(SYMBOL_INFO_PACKAGE));

	if (pTable)
	{
		SdkSymEnumAllSymbols(g_CurrentPartitionIntHandle, L"ntoskrnl.exe", pTable, MmVirtualMemory);
		free(pTable);
	}
```

result: BOOLEAN

# 21. SdkSymEnumAllSymbolsGetTableLength

Get the number of symbols for the specified driver in guest OS. Use this function to allocate a correctly sized buffer before calling SdkSymEnumAllSymbols

```c
ULONG64
SdkSymEnumAllSymbolsGetTableLength(
	_In_ ULONG64 PartitionHandle,
	_In_ PWCHAR DriverName,
	_In_ MEMORY_ACCESS_TYPE MmAccess
);
```

Parameters:

* **PartitionHandle** - handle of virtual machine
* **DriverName** - null-terminated wide-character string with the driver name (e.g. L"ntoskrnl.exe")
* **MmAccess** - memory access type (MmPhysicalMemory or MmVirtualMemory)

Example:

```c
	ULONG64 Count = SdkSymEnumAllSymbolsGetTableLength(g_CurrentPartitionIntHandle, L"ntoskrnl.exe", MmVirtualMemory);
	wprintf(L"Symbol count: %I64u\n", Count);
```

result: ULONG64 (number of symbols, or 0 on failure)
