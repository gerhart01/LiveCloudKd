# LiveCloudKdPy — AI Code Generation Manifest

> **Goal:** Machine-readable context for AI tools working with the Python hvlib.dll wrapper
> **Principle:** Maximum information / minimum tokens

---

## 1. Integration

| Parameter | Value |
|-----------|-------|
| Language | Python 3.x (including 3.14) |
| Binding | ctypes → hvlib.dll |
| Dependencies | hvlib.dll + hvmm.sys in the same directory |
| Privileges | **Administrator** |
| Module | `hvlib/hvlib.py` (class `hvlib`) |
| Import | `from hvlib import *` |
| Version | 1.2.0 |

```python
# Minimal start
from hvlib import *
obj = hvlib("")              # DLL + driver are loaded automatically
vm_ops = obj.GetPreferredSettings()
obj.EnumPartitions(vm_ops)   # Prints the list of VMs
h = obj.SelectPartition(0)   # Returns handle or 0
```

---

## 2. Enumerations (compact)

```
ReadMemoryMethod:   0=Unsupported 1=HvmmDrvInternal 2=WinHv 3=HvmmLocal 4=Max
WriteMemoryMethod:  0=Unsupported 1=HvmmDrvInternal 2=WinHv 3=HvmmLocal 4=Max
SuspendResumeMethod: 0=Unsupported 1=Powershell 2=WriteSpecRegister
VmStateAction:      0=SuspendVm 1=ResumeVm
HvmmInformationClass: 1=FriendlyName 2=PartitionId 3=VmtypeString 6=MmMaxPhysPage 11=KernelBase 20=VmGuidString
```

**Return types from GetData by class:**
- Strings (`str`): InfoPartitionFriendlyName, InfoVmtypeString, InfoVmGuidString
- Integers (`int`): all others

---

## 3. CfgParameters — Configuration Structure

```
Field                  | ctypes       | Description
ReadMethod             | c_int        | Memory read method
WriteMethod            | c_int        | Memory write method
PauseMethod            | c_int        | Suspend/resume method
LogLevel               | c_int64      | Log level [0..4]
ForceFreezeCPU         | c_bool       | Freeze vCPU on read
PausePartition         | c_bool       | Pause VM on SelectPartition
ExdiConsoleHandle      | c_uint64     | EXDI handle (NULL for standalone use)
ReloadDriver           | c_bool       | Reload hvmm.sys
PFInjection            | c_bool       | Page-fault injection
NestedScan             | c_bool       | Nested partition scan
UseDebugApiStopProcess | c_bool       | Debug API for process stop
SimpleMemory           | c_bool       | For Linux VM
```

**Initialization:** always via `GetPreferredSettings()`, then override individual fields.

---

## 4. API — Signatures and Semantics

### 4.1 Configuration and Enumeration

```python
GetPreferredSettings() -> CfgParameters | False
# Returns a struct with defaults. Always call first.

EnumPartitions(vm_ops: CfgParameters) -> None
# Fills internal arrays: PartitionArray, ArrayOfNames, PartitionCount.
# Prints VM list to stdout. Does NOT return a value.

SelectPartition(vm_id: int) -> int
# vm_id = index (0-based) from EnumPartitions.
# Returns handle (>0) or 0 on error.
```

### 4.2 Reading Data

```python
GetData(vm_handle: int, info_class: HvmmInformationClass) -> str | int
# String classes (1,3,20) → str. Others → int.

ReadPhysicalMemoryBlock(vm_handle, address, block_size) -> ctypes.buffer | 0
# address = GPA byte address (NOT page number).
# Uses ReadMethod from the current configuration.

ReadVirtualMemoryBlock(vm_handle, address, block_size) -> ctypes.buffer | 0
# address = GVA. CR3 is taken from the currently selected partition.
```

### 4.3 Writing Data

```python
WritePhysicalMemoryBlock(vm_handle, address, buffer) -> bool
# block_size = len(buffer). Uses WriteMethod from configuration.

WriteVirtualMemoryBlock(vm_handle, address, buffer) -> bool
# block_size = len(buffer).
```

### 4.4 Utilities

```python
PrintHex(buffer) -> None
# int → "0x%08x". bytes → "aa:bb:cc:dd:..."

cleanup() -> None
# Calls SdkCloseAllPartitions(). Registered via atexit automatically.
```

---

## 5. Canonical Call Sequence

```python
from hvlib import *

obj = hvlib("")                              # Step 1: load DLL + driver
vm_ops = obj.GetPreferredSettings()          # Step 2: default configuration
vm_ops.LogLevel = 1
obj.EnumPartitions(vm_ops)                   # Step 3: enumerate VMs
h = obj.SelectPartition(0)                   # Step 4: select partition
if h == 0: sys.exit(1)

buf = obj.ReadPhysicalMemoryBlock(h, 0x1000, 0x1000)  # Step 5: work
kb = obj.GetData(h, HvmmInformationClass.InfoKernelBase)

# Step 6: cleanup is automatic via atexit
```

---

## 6. Direct DLL Access

Not all SDK functions are wrapped. For unwrapped ones — direct access via `obj.hvlib`:

```python
# SdkControlVmState (argtypes already configured)
obj.hvlib.SdkControlVmState(h, VmStateAction.SuspendVm,
    SuspendResumeMethod.SuspendResumePowershell, False)

# For other functions, configure argtypes/restype manually.
# Full list: see HvlibHandle.h in LiveCloudKdSdk/public/
```

**Unwrapped SDK functions:**
`SdkGetPhysicalAddress`, `SdkGetMachineType`, `SdkReadVpRegister`, `SdkWriteVpRegister`, `SdkSetData`, `SdkGetData2`, `SdkSymGetSymbolAddress`, `SdkSymEnumAllSymbols`, `SdkSymEnumAllSymbolsGetTableLength`, `SdkInvokeHypercall`

---

## 7. Common Mistakes

| Mistake | Correct |
|---------|---------|
| Using `EnumPartitions` as bool (`if bResult == False`) | `EnumPartitions` returns nothing. Check `obj.PartitionCount == 0` |
| `HvddInformationClass.HvddKernelBase` | Correct name: `HvmmInformationClass.InfoKernelBase` |
| Passing page number as `address` | `address` is a byte address. PFN × 0x1000 = address |
| `ReadPhysicalMemoryBlock` → checking `if not buf` | Returns `0` on error, NOT `None`. Check `buf == 0` |
| Calling DLL functions without `SelectPartition` | `SelectPartition` is required before any operations |
| Initializing CfgParameters manually | Always via `GetPreferredSettings()` |
| All argtypes in `__init__` before first DLL call | Python 3.14 bug: argtypes are configured in 2 phases (see `_setup_remaining_argtypes`) |

---

## 8. Query Templates for This Project

```
[META] Python | ctypes | hvlib.dll wrapper
[CODE] hvlib.py, focus on [function/line]
[ISSUE] [problem description]
[TRIED] [what was tried]
```

**Examples of effective queries:**

```
Add wrapper for SdkGetPhysicalAddress in hvlib.py
API: ULONG64 SdkGetPhysicalAddress(ULONG64 Handle, ULONG64 Va, MEMORY_ACCESS_TYPE MmAccess)
Pattern: similar to existing methods of the hvlib class
```

```
OPTIMIZE: ReadPhysicalMemoryBlock for batch reading
CURRENT: one call = one page
TARGET: read N pages per single Python call
```
