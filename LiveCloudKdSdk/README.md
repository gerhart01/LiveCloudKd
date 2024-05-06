- [Hvlib API (1.0.0)](#hvlib-api-100)
- [1. SdkControlVmState](#1-sdkcontrolvmstate)
- [2. SdkGetMachineType](#2-sdkgetmachinetype)
- [3. SdkCloseAllPartitions](#3-sdkcloseallpartitions)
- [4. SdkClosePartition](#4-sdkclosepartition)
- [5. SdkEnumPartitions](#5-sdkenumpartitions)
- [6. SdkGetData (SdkGetData2)](#6-sdkgetdata-sdkgetdata2)
- [7. SdkReadPhysicalMemory](#7-sdkreadphysicalmemory)
- [8. SdkReadVirtualMemory](#8-sdkreadvirtualmemory)
- [9. SdkWritePhysicalMemory](#9-sdkwritephysicalmemory)
- [10. SdkWriteVirtualMemory](#10-sdkwritevirtualmemory)

# Hvlib API (1.0.0)

Hvlib is plugin for working with Microsoft Hyper-V virtual machines memory. It was developed, when 
LiveCloudKd was rewritten for supporting modern Windows versions, therefore it supports Windows 10 build 1803 and above, Windows Server 2019 and above
It also was test on Windows 8 x64 and Windows Server 2012, but not with latest patches.

Part of code was taken from LiveCloudKd project https://github.com/msuiche/LiveCloudKd by Matt Suiche (www.msuiche.com).

So it was developed, because Microsoft doesn't provide necessary API description and examples of vid.dll

Hvlib supports two memory access methods for Hyper-V memory

1. ReadInterfaceWinHv\WriteInterfaceWinHv - uses hypercalls HvReadGpa for memory reading and HvWriteGpa for writing. According Hyper-V TLFS HvReadGpa\HvWriteGpa can read\write only 
maximum 0x10 bytes per call, therefore it slowest method (but robust - Sysinternals LiveKD uses it)
                
2. ReadInterfaceHvmmDrvInternal\WriteInterfaceHvmmDrvInternal - hvmm.sys directly read memory blocks from kernel mode. It uses internal vid.sys structures 
and different methods for standard Hyper-V VM.

Of course, it can be broken during every patch of vid.sys, but now it works with new version of vid.sys without patching.

You can see examples of using in 

```
LiveCloudKd project 
MemProcsFS leechcore plugin project. 
Volatility Memory Manager plugin
EXDi plugin for WinDBG

and separate LiveCloudKdExample project
```

1. Fill VmOperationsConfig structure with options. See available options in VM_OPERATIONS_CONFIG structure
2. Get list of running Virtual Machines using SdkEnumPartitions function.

```c
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
See more examples in LiveCloudKdExample project.
https://github.com/gerhart01/LiveCloudKd/tree/master/LiveCloudKdExample
	
```

# 1. SdkControlVmState

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

# 2. SdkGetMachineType

Get architecture of guest VM (x86 or AMD64). Plugin support only AMD64 guest virtual machines

```c
typedef enum _MACHINE_TYPE {
	MACHINE_UNKNOWN = 0,
	MACHINE_X86 = 1,
	MACHINE_AMD64 = 2
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

# 3. SdkCloseAllPartitions
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

# 4. SdkClosePartition

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


# 5. SdkEnumPartitions

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

# 6. SdkGetData (SdkGetData2)

Description: get specific data from partition

```
BOOLEAN
SdkGetData(
	_In_ ULONG64 PartitionHandle,
	_In_ HVDD_INFORMATION_CLASS InfoInformationClass,
	_Inout_ PVOID InfoInformation
) 
```

parameters:  

* **PartitionHandle** - handle of partition
* **InfoInformationClass** - PartitionClass
* **InfoInformation** - returned buffer

available values:

```c
InfoPartitionFriendlyName = 1
InfoPartitionId = 2			
InfoVmtypeString = 3
InfoMmMaximumPhysicalPage = 6
InfoKernelBase = 11	
InfoVmGuidString = 20
```

Example:

```c
ULONG64 PartitionId = 0;
WCHAR* VmTypeString = NULL;
CHAR* VmmNameString = NULL;
WCHAR* VmGuidString = NULL;

SdkGetData(Partitions[i], InfoPartitionFriendlyName, &FriendlyNameP);
PartitionId = SdkGetData(Partitions[i], InfoPartitionId);
SdkGetData(Partitions[i], InfoVmtypeString, &VmTypeString);
SdkGetData(Partitions[i], InfoVmGuidString, &VmGuidString);
```

result - BOOLEAN

SdkGetData2 same as SdkGetData, but with different parameters and more comfortable in some cases

```
ULONG64
SdkGetData2(
	_In_ ULONG64 PartitionHandle,
	_In_ HVDD_INFORMATION_CLASS InfoInformationClass
)
```

parameters:  

* **PartitionHandle** - handle of partition
* **InfoInformationClass** - PartitionClass

Example:

```c
	GUEST_TYPE GuestOsType = SdkGetData2(g_Partition, InfoGuestOsType);
```

return value ULONG64 - value of requested parameter (in some cases it can be address of buffer)

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
* **Va** - physical memory address 
* **Buffer** - buffer for reading data
* **Size** - size of memory block for reading
  
Example:

```c
BOOLEAN
SdkReadVirtualMemory(
	_In_ ULONG64 PartitionHandle,
	_In_ ULONG64 Va,
	_Out_ PVOID Buffer,
	_In_ ULONG Size
)
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
* **ReadByteCount** - size of memory block for reading
* **ClientBuffer** - buffer for reading data
* **Method** - method for reading data
  
Example:

```c
	UINT64 Address = 0;
	ULONG64 buffer = 0x1234567812345678;

	if (!lpBuffer)
		return FALSE;

	Ret = SdkWritePhysicalMemory(g_CurrentPartitionIntHandle, Address, sizeof(ULONG64), buffer, g_MemoryReadInterfaceType);
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
* **Buffer** - buffer for reading data
* **Size** - size of memory block for reading
  
Example:  

```c   
	UINT64 Address = 0;
	ULONG64 buffer = 0xFFFFFF8000000000;

	if (!lpBuffer)
		return FALSE;

	Ret = SdkWriteVirtualMemory(g_CurrentPartitionIntHandle, Address, sizeof(ULONG64), buffer); 
```

result: BOOLEAN