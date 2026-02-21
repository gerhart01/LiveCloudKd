This is description for Python API wrappers for hvlib.dll library
Main library file is hvlib.py (version 1.2.0). Written on Python 3.x (including Python 3.14 compatibility).
See Example.py for detailed information - [Example.py](https://github.com/gerhart01/LiveCloudKd/blob/master/LiveCloudKdPy/Example.py)

Also there is good example of Python API wrappers usage is Hyper-V memory manager plugin for [volatility plugin](https://github.com/gerhart01/Hyper-V-Tools/tree/main/Plugin_for_volatility)

## Table of Contents

- [Installation](#installation)
- [Enumerations](#enumerations)
- [CfgParameters Structure](#cfgparameters-structure)
- [1. GetPreferredSettings](#1-getpreferredsettings)
- [2. EnumPartitions](#2-enumpartitions)
- [3. GetData](#3-getdata)
- [4. SelectPartition](#4-selectpartition)
- [5. ReadPhysicalMemoryBlock](#5-readphysicalmemoryblock)
- [6. ReadVirtualMemoryBlock](#6-readvirtualmemoryblock)
- [7. WritePhysicalMemoryBlock](#7-writephysicalmemoryblock)
- [8. WriteVirtualMemoryBlock](#8-writevirtualmemoryblock)
- [9. PrintHex](#9-printhex)
- [10. Cleanup](#10-cleanup)

## Installation

Copy hvlib folder (hvlib.py, \_\_init\_\_.py), hvlib.dll and hvmm.sys from [latest](https://github.com/gerhart01/LiveCloudKd/releases) LiveCloudKd distributive to \<python_dir\>\Lib\site-packages (e.g. C:\Python312x64\Lib\site-packages).
If you use some python virtual environment plugins, you need to copy files inside it.
For example to "directory_name\venv\Lib\site-packages" for virtualenv.

Alternatively, place hvlib.dll and hvmm.sys in the same directory as hvlib.py (the wrapper resolves paths automatically).

## Enumerations

### ReadMemoryMethod

Memory reading method for driver.

```python
class ReadMemoryMethod(IntEnum):
    ReadInterfaceUnsupported = 0
    ReadInterfaceHvmmDrvInternal = 1
    ReadInterfaceWinHv = 2
    ReadInterfaceHvmmLocal = 3
    ReadInterfaceMax = 4
```

### WriteMemoryMethod

Memory writing method for driver.

```python
class WriteMemoryMethod(IntEnum):
    WriteInterfaceUnsupported = 0
    WriteInterfaceHvmmDrvInternal = 1
    WriteInterfaceWinHv = 2
    WriteInterfaceHvmmLocal = 3
    WriteInterfaceMax = 4
```

### SuspendResumeMethod

Method of suspend/resume VM.

```python
class SuspendResumeMethod(IntEnum):
    SuspendResumeUnsupported = 0
    SuspendResumePowershell = 1
    SuspendResumeWriteSpecRegister = 2
```

### VmStateAction

```python
class VmStateAction(IntEnum):
    SuspendVm = 0
    ResumeVm = 1
```

### HvmmInformationClass

Information classes for GetData. Only a subset of the full C SDK enum is exposed in the Python wrapper.

```python
class HvmmInformationClass(IntEnum):
    InfoPartitionFriendlyName = 1
    InfoPartitionId = 2
    InfoVmtypeString = 3
    InfoMmMaximumPhysicalPage = 6
    InfoKernelBase = 11
    InfoVmGuidString = 20
```

For the full list of information classes, see the C SDK header `HvlibEnumPublic.h` (enum `HVMM_INFORMATION_CLASS`).

## CfgParameters Structure

Configuration for VM operations. Used in GetPreferredSettings / EnumPartitions.

| Field                  | ctypes type      | Description                                                                 |
|------------------------|------------------|-----------------------------------------------------------------------------|
| ReadMethod             | c_int            | Memory reading method (ReadMemoryMethod)                                    |
| WriteMethod            | c_int            | Memory writing method (WriteMemoryMethod)                                   |
| PauseMethod            | c_int            | Method of suspend/resume VM (SuspendResumeMethod)                           |
| LogLevel               | c_int64          | Log level [0..4]                                                            |
| ForceFreezeCPU         | c_bool           | Freeze CPU using virtual VM registers when suspending VM                    |
| PausePartition         | c_bool           | VM is suspended when SdkSelectPartition is executed                         |
| ExdiConsoleHandle      | c_uint64         | EXDI console handle                                                         |
| ReloadDriver           | c_bool           | Reload driver when starting plugin. Needed when service is not deleted correctly |
| PFInjection            | c_bool           | Enable page fault injection                                                 |
| NestedScan             | c_bool           | Enable nested virtualization scan                                           |
| UseDebugApiStopProcess | c_bool           | Use Debug API to stop process                                               |
| SimpleMemory           | c_bool           | Used for Linux VM memory scanning                                           |

---

# 1. GetPreferredSettings

Description: Get default plugin configuration.

```python
CfgParameters GetPreferredSettings()
```

Example:

```python
vm_ops = objHvlib.GetPreferredSettings()
vm_ops.LogLevel = 1
```

result type: CfgParameters object, or False if error

# 2. EnumPartitions

Description: Hyper-V active partitions enumeration in root OS.

```python
EnumPartitions(vm_ops)
```

Parameters:

* **vm_ops** - CfgParameters structure, where you can modify one or more parameter

Example:

```python
from hvlib import *

objHvlib = hvlib("")

if objHvlib == None:
    exit

vm_ops = objHvlib.GetPreferredSettings()
vm_ops.LogLevel = 1

objHvlib.EnumPartitions(vm_ops)
```

After enumeration, partition data is stored internally and can be accessed via SelectPartition.

Prints partition list to console in format: `[index] FriendlyName. (PartitionId = N)`

# 3. GetData

Description: Get specific data from partition.

```python
GetData(vm_handle, info_class)
```

Parameters:

* **vm_handle** - handle of partition (returned by SelectPartition)
* **info_class** - HvmmInformationClass value

Available values of HvmmInformationClass:

```python
InfoPartitionFriendlyName = 1    # returns string
InfoPartitionId = 2              # returns integer
InfoVmtypeString = 3             # returns string
InfoMmMaximumPhysicalPage = 6    # returns integer
InfoKernelBase = 11              # returns integer
InfoVmGuidString = 20            # returns string
```

String-type classes (InfoPartitionFriendlyName, InfoVmtypeString, InfoVmGuidString) return `str`. All other classes return `int`.

Example:

```python
KernelBase = objHvlib.GetData(vm_handle, HvmmInformationClass.InfoKernelBase)
objHvlib.PrintHex(KernelBase)

VmName = objHvlib.GetData(vm_handle, HvmmInformationClass.InfoPartitionFriendlyName)
print(VmName)
```

result type: str or int (depending on information class)

# 4. SelectPartition

Description: Select one of partitions, which were gotten from EnumPartitions.

```python
SelectPartition(vm_id)
```

Parameters:

* **vm_id** - index of partition in the enumerated list (0-based)

Example:

```python
vm_handle = objHvlib.SelectPartition(vm_id)

if vm_handle == 0:
    print("SelectPartition failed")
    exit
```

result type: partition handle (integer), or 0 if failed

# 5. ReadPhysicalMemoryBlock

Description: Read memory block from specified physical address.

```python
ReadPhysicalMemoryBlock(vm_handle, address, block_size)
```

Parameters:
* **vm_handle** - handle of virtual machine
* **address** - physical memory address
* **block_size** - size of memory block for reading

Uses the ReadMethod from current CfgParameters configuration.

Example:

```python
page_size = 0x1000
phys_address = 0x10000

buffer = objHvlib.ReadPhysicalMemoryBlock(vm_handle, phys_address, page_size)

if buffer == 0:
    print("Read failed")
else:
    objHvlib.PrintHex(buffer)
```

result type: ctypes string buffer, or 0 if error

# 6. ReadVirtualMemoryBlock

Description: Read memory block from specified virtual address.

```python
ReadVirtualMemoryBlock(vm_handle, address, block_size)
```

Parameters:
* **vm_handle** - handle of virtual machine
* **address** - virtual memory address
* **block_size** - size of memory block for reading

Example:

```python
KernelBase = objHvlib.GetData(vm_handle, HvmmInformationClass.InfoKernelBase)
buffer = objHvlib.ReadVirtualMemoryBlock(vm_handle, KernelBase, page_size)

if buffer == 0:
    print("Read failed")
else:
    objHvlib.PrintHex(buffer)
```

result type: ctypes string buffer, or 0 if error

# 7. WritePhysicalMemoryBlock

Description: Write data to specified physical address in virtual machine.

```python
WritePhysicalMemoryBlock(vm_handle, address, buffer)
```

Parameters:
* **vm_handle** - handle of virtual machine
* **address** - physical memory address
* **buffer** - buffer with data to write (block_size is determined from buffer length)

Uses the WriteMethod from current CfgParameters configuration.

Example:

```python
bResult = objHvlib.WritePhysicalMemoryBlock(vm_handle, phys_address, buffer)
```

result type: bool

# 8. WriteVirtualMemoryBlock

Description: Write data to specified virtual address in virtual machine.

```python
WriteVirtualMemoryBlock(vm_handle, address, buffer)
```

Parameters:
* **vm_handle** - handle of virtual machine
* **address** - virtual memory address
* **buffer** - buffer with data to write (block_size is determined from buffer length)

Example:

```python
bResult = objHvlib.WriteVirtualMemoryBlock(vm_handle, KernelBase, buffer)
```

result type: bool

# 9. PrintHex

Description: Utility function to print data in hexadecimal format.

```python
PrintHex(buffer)
```

Parameters:
* **buffer** - integer value or bytes buffer

Example:

```python
# Print integer as hex
objHvlib.PrintHex(0xDEADBEEF)    # output: 0xdeadbeef

# Print buffer as hex
buffer = objHvlib.ReadPhysicalMemoryBlock(vm_handle, 0x1000, 16)
objHvlib.PrintHex(buffer)         # output: 4d:5a:90:00:03:00:...
```

# 10. Cleanup

Description: Close all partition handles and unload the driver. Called automatically via `atexit` when the Python process exits.

```python
cleanup()
```

Example:

```python
# Normally called automatically, but can be invoked manually:
objHvlib.cleanup()
```

---

## Low-level DLL access

The underlying ctypes DLL handle is available as `objHvlib.hvlib` for direct access to hvlib.dll functions not yet wrapped (e.g. `SdkControlVmState`, `SdkGetPhysicalAddress`, `SdkGetMachineType`, `SdkReadVpRegister`, `SdkWriteVpRegister`, symbol functions). See `HvlibHandle.h` for the full C API.

`SdkControlVmState` argtypes are already configured:

```python
# Suspend VM
objHvlib.hvlib.SdkControlVmState(vm_handle, VmStateAction.SuspendVm,
    SuspendResumeMethod.SuspendResumePowershell, False)

# Resume VM
objHvlib.hvlib.SdkControlVmState(vm_handle, VmStateAction.ResumeVm,
    SuspendResumeMethod.SuspendResumePowershell, False)
```
