# LiveCloudKd SDK — AI Code Generation Manifest

> **Goal:** Machine-readable context for AI tools working with hvlib.dll / hvlib.lib
> **Principle:** Maximum information / minimum tokens

This file covers exact function signatures, parameter semantics, return values,
common call sequences, and known pitfalls.

**Related wrappers:** [C# (hvlibdotnet)](../hvlibdotnet/AI%20Code%20Generation%20Manifest.md) | [Python (LiveCloudKdPy)](../LiveCloudKdPy/AI%20Code%20Generation%20Manifest.md)

---

## 1. Integration Basics

| Item | Value |
|------|-------|
| Import library | `hvlib.lib` |
| Runtime DLL | `hvlib.dll` (+ `hvmm.sys` in same directory) |
| Public headers | `HvlibHandle.h`, `HvlibEnumPublic.h` |
| Language binding | C / C++ (ULONG64 handles; also usable from C#, Python via P/Invoke / ctypes) |
| Required privilege | Must run as **Administrator** |

Include order:

```c
#include <windows.h>
#include "HvlibEnumPublic.h"   // enums and VM_OPERATIONS_CONFIG
#include "HvlibHandle.h"       // function declarations
```

---

## 2. Core Type Reference

### ULONG64 — Partition Handle

All SDK functions that accept or return a partition handle use `ULONG64`.
**Never** dereference it as a pointer. Treat it as an opaque cookie.

### VM_OPERATIONS_CONFIG

```c
typedef struct _VM_OPERATIONS_CONFIG {
    READ_MEMORY_METHOD   ReadMethod;          // how to read guest RAM
    WRITE_MEMORY_METHOD  WriteMethod;         // how to write guest RAM
    SUSPEND_RESUME_METHOD SuspendMethod;      // how to suspend/resume
    ULONG64  LogLevel;          // 0=errors only, 1=verbose, 2=very verbose
    BOOLEAN  ForceFreezeCPU;    // freeze vCPUs on every memory read (slower, safer)
    BOOLEAN  PausePartition;    // pause the partition before enumeration
    HANDLE   ExdiConsoleHandle; // used by EXDI plugin; set NULL for standalone use
    BOOLEAN  ReloadDriver;      // force hvmm.sys reload
    BOOLEAN  PFInjection;       // enable page-fault injection
    BOOLEAN  NestedScan;        // scan nested Hyper-V partitions
    BOOLEAN  UseDebugApiStopProcess;
    BOOLEAN  SimpleMemory;
    BOOLEAN  ReplaceDecypheredKDBG;
    BOOLEAN  FullCrashDumpEmulation;
    BOOLEAN  EnumGuestOsBuild;
    BOOLEAN  VSMScan;
    BOOLEAN  LogSilenceMode;
} VM_OPERATIONS_CONFIG, *PVM_OPERATIONS_CONFIG;
```

Always initialize with `SdkGetDefaultConfig`, then override individual fields.

### READ_MEMORY_METHOD / WRITE_MEMORY_METHOD

```c
typedef enum _READ_MEMORY_METHOD {
    ReadInterfaceUnsupported     = 0,
    ReadInterfaceHvmmDrvInternal = 1,  // fastest; uses hvmm.sys kernel driver
    ReadInterfaceWinHv           = 2,  // slowest; uses HvReadGpa hypercall (~16 B/call)
    ReadInterfaceHvmmLocal       = 3,  // local OS physical memory
    ReadInterfaceMax             = 4
} READ_MEMORY_METHOD;

typedef enum _WRITE_MEMORY_METHOD {
    WriteInterfaceUnsupported     = 0,
    WriteInterfaceHvmmDrvInternal = 1,
    WriteInterfaceWinHv           = 2,
    WriteInterfaceHvmmLocal       = 3,
    WriteInterfaceMax             = 4
} WRITE_MEMORY_METHOD;
```

**Rule**: always pass the write-method enum to write functions and the read-method
enum to read functions. Do **not** pass `g_MemoryReadInterfaceType` to
`SdkWritePhysicalMemory`.

### HVMM_INFORMATION_CLASS (full list)

```c
 0  InfoKdbgData                    // KDBG structure pointer
 1  InfoPartitionFriendlyName       // WCHAR* — display name (do NOT free)
 2  InfoPartitionId                 // ULONG64 — Hyper-V partition ID
 3  InfoVmtypeString                // WCHAR* — VM generation/type string (do NOT free)
 4  InfoStructure                   // special: SdkSetData(0, InfoStructure, &obj) → new handle
 5  InfoKiProcessorBlock            // ULONG64 VA — KiProcessorBlock array
 6  InfoMmMaximumPhysicalPage       // ULONG64 — highest PFN
 7  InfoKPCR                        // ULONG64 VA — KPCR address
 8  InfoNumberOfCPU                 // ULONG64 — guest vCPU count
 9  InfoKDBGPa                      // ULONG64 — KDBG physical address
10  InfoNumberOfRuns                // ULONG64 — memory run count
11  InfoKernelBase                  // ULONG64 VA — ntoskrnl load address
12  InfoMmPfnDatabase               // ULONG64 VA — MmPfnDatabase
13  InfoPsLoadedModuleList          // ULONG64 VA — PsLoadedModuleList
14  InfoPsActiveProcessHead         // ULONG64 VA — PsActiveProcessHead
15  InfoNtBuildNumber               // ULONG64 — NT build number
16  InfoNtBuildNumberVA             // ULONG64 VA — NtBuildNumber address
17  InfoDirectoryTableBase          // ULONG64 — CR3 / DTB
18  InfoRun                         // memory run descriptor
19  InfoKdbgDataBlockArea           // KDBG data block area
20  InfoVmGuidString                // WCHAR* — VM GUID string (do NOT free)
21  InfoPartitionHandle             // ULONG64 — raw vid.dll partition handle
22  InfoKdbgContext                  // KDBG context
23  InfoKdVersionBlock              // ULONG64 VA — KdVersionBlock
24  InfoMmPhysicalMemoryBlock       // ULONG64 VA — MmPhysicalMemoryBlock
25  InfoNumberOfPages               // ULONG64 — total page count
26  InfoIdleKernelStack             // ULONG64 VA — idle thread kernel stack
27  InfoSizeOfKdDebuggerData        // ULONG64 — KDDEBUGGER_DATA64 size
28  InfoCpuContextVa                // ULONG64 VA — CPU context
29  InfoSize                        // ULONG64 — partition memory size
30  InfoMemoryBlockCount            // ULONG64 — memory block count
31  InfoSuspendedCores              // ULONG64 — suspended core count
32  InfoSuspendedWorker             // BOOLEAN — worker process suspended
33  InfoIsContainer                 // BOOLEAN — is container partition
34  InfoIsNeedVmwpSuspend           // BOOLEAN — need vmwp.exe suspend
35  InfoGuestOsType                 // GUEST_TYPE enum
36  InfoSettingsCrashDumpEmulation  // BOOLEAN — crash dump emulation on/off
37  InfoSettingsUseDecypheredKdbg   // BOOLEAN — use decyphered KDBG
38  InfoBuilLabBuffer               // WCHAR* — build lab string (do NOT free)
39  InfoHvddGetCr3byPid             // SdkGetData: pass &PID → returns CR3
40  InfoGetProcessesIds             // process ID enumeration
41  InfoDumpHeaderPointer           // dump header pointer
42  InfoUpdateCr3ForLocal           // update CR3 for local mode
43  InfoHvddGetCr3Kernel            // SdkGetData: out → kernel CR3
44  InfoHvddGetCr3Hv                // SdkGetData: out → hypervisor CR3
45  InfoHvddGetCr3Securekernel      // SdkGetData: out → secure kernel CR3
46  InfoIsVmSuspended               // BOOLEAN — is VM currently suspended
    // --- Special set-only values ---
    InfoSetMemoryBlock              // set memory block for partition
    InfoEnlVmcsPointer              // set enlightened VMCS pointer
```

**String classes** (WCHAR*, do NOT free): 1, 3, 20, 38.
**CR3 classes** (via SdkGetData with &variable): 39, 43, 44, 45.

### MACHINE_TYPE

```c
typedef enum _MACHINE_TYPE {
    MACHINE_UNKNOWN     = 0,
    MACHINE_X86         = 1,
    MACHINE_AMD64       = 2,
    MACHINE_ARM64       = 3,
    MACHINE_UNSUPPORTED = 4
} MACHINE_TYPE;
```

### GUEST_TYPE

```c
typedef enum _GUEST_TYPE {
    MmUnknown         = 0,
    MmStandard        = 1,  // Windows with KDBG
    MmNonKdbgPartition = 2, // hvix64/hvax64 or securekernel area
    MmHyperV          = 3   // hvix64/hvax64 full memory
} GUEST_TYPE;
```

### VTL_LEVEL

```c
typedef enum _VTL_LEVEL {
    Vtl0   = 0,  // normal world
    Vtl1   = 1,  // VBS / secure kernel
    Vtl2   = 2,  // OpenHCL / paravisor
    BadVtl = 3
} VTL_LEVEL;
```

### MEMORY_ACCESS_TYPE

```c
typedef enum _MEMORY_ACCESS_TYPE {
    MmPhysicalMemory = 0,
    MmVirtualMemory  = 1,
    MmAccessRtCore64 = 2
} MEMORY_ACCESS_TYPE;
```

---

## 3. Function Signatures and Semantics

### 3.1 Configuration

```c
// Fill VmOperationsConfig with OS-appropriate defaults.
// Always call before SdkEnumPartitions.
BOOLEAN SdkGetDefaultConfig(
    _Inout_ PVM_OPERATIONS_CONFIG VmOperationsConfig
);

// Apply a new config to an already-selected partition.
// (Exported in header; may not be present in older hvlib.lib builds.)
BOOLEAN SdkSetPartitionConfig(
    _In_ ULONG64 PartitionHandle,
    _In_ PVM_OPERATIONS_CONFIG VmOperationsConfig
);

// Read the active config of a selected partition.
// Returns pointer to internal buffer — do NOT free.
// (Exported in header; may not be present in older hvlib.lib builds.)
PVM_OPERATIONS_CONFIG SdkGetPartitionConfig(
    _In_ ULONG64 PartitionHandle
);
```

### 3.2 Partition Lifecycle

```c
// Returns heap-allocated array of PartitionHandle values.
// *PartitionTableCount is filled with the count.
// The caller does NOT free the returned array.
PULONG64 SdkEnumPartitions(
    _Inout_ PULONG64 PartitionTableCount,
    _In_    PVM_OPERATIONS_CONFIG VmOpsConfig
);

// Activate a partition: builds guest OS metadata (KDBG, memory runs, etc.).
// Must be called before any memory/register/state operations.
BOOLEAN SdkSelectPartition(
    _In_ ULONG64 PartitionHandle
);

// Close one partition. Call before opening a different one, or on exit.
VOID SdkClosePartition(ULONG64 Handle);

// Close all open partitions. Safe to call even if none are open.
BOOLEAN SdkCloseAllPartitions();
```

### 3.3 Data Query / Set

```c
// Query partition property into caller-supplied buffer.
// For pointer-typed results (WCHAR*, etc.) pass address of pointer.
// Do NOT free returned pointers — they belong to the SDK.
BOOLEAN SdkGetData(
    _In_    ULONG64 PartitionHandle,
    _In_    HVMM_INFORMATION_CLASS HvmmInformationClass,
    _Inout_ PVOID InfoInformation
);

// Convenience variant — returns value as ULONG64.
// For pointer results, cast the returned value to the appropriate pointer type.
ULONG64 SdkGetData2(
    _In_ ULONG64 PartitionHandle,
    _In_ HVMM_INFORMATION_CLASS HvmmInformationClass
);

// Set a partition property.
// Special case: SdkSetData(0, InfoStructure, (ULONG64)&InfoPartition)
//   creates a new partition object from a serialized structure and
//   RETURNS the new PartitionHandle (not a bool result).
// For all other classes, returns 0 on failure or non-zero on success.
ULONG64 SdkSetData(
    _In_ ULONG64 PartitionHandle,
    _In_ HVMM_INFORMATION_CLASS HvmmInformationClass,
    _In_ ULONG64 InfoInformation
);
```

### 3.4 Physical Memory (GPA)

```c
// Read bytes from guest physical address into ClientBuffer.
// StartPosition = byte address (GPA), NOT a page number.
// ReadByteCount should be PAGE_SIZE-aligned for best reliability.
BOOLEAN SdkReadPhysicalMemory(
    _In_    ULONG64 PartitionHandle,
    _In_    UINT64  StartPosition,      // GPA byte address
    _In_    UINT64  ReadByteCount,
    _Inout_ PVOID   ClientBuffer,
    _In_    READ_MEMORY_METHOD Method
);

// Write bytes from ClientBuffer to guest physical address.
// ClientBuffer must be a PVOID (pointer), never pass a value directly.
BOOLEAN SdkWritePhysicalMemory(
    _In_ ULONG64 PartitionHandle,
    _In_ UINT64  StartPosition,         // GPA byte address
    _In_ UINT64  WriteBytesCount,
    _In_ PVOID   ClientBuffer,          // pointer to data, e.g. &myVar
    _In_ WRITE_MEMORY_METHOD Method     // use write-method, not read-method
);
```

**Physical-memory dump loop pattern** (correct):

```c
ULONG64 lPageCountTotal = 0;
SdkGetData(h, InfoMmMaximumPhysicalPage, &lPageCountTotal);

for (UINT64 offset = 0;
     offset < lPageCountTotal * PAGE_SIZE;
     offset += BLOCK_SIZE)
{
    SdkReadPhysicalMemory(h, offset, BLOCK_SIZE, buf, readMethod);
}
```

### 3.5 Virtual Memory (GVA)

```c
// Read from guest virtual address.
BOOLEAN SdkReadVirtualMemory(
    _In_  ULONG64 PartitionHandle,
    _In_  ULONG64 Va,       // guest virtual address
    _Out_ PVOID   Buffer,
    _In_  ULONG   Size
);

// Write to guest virtual address.
// Buffer is a pointer (PVOID). Size comes last.
BOOLEAN SdkWriteVirtualMemory(
    _In_ ULONG64 PartitionHandle,
    _In_ ULONG64 Va,        // guest virtual address
    _In_ PVOID   Buffer,    // pointer to data, e.g. &myVar
    _In_ ULONG   Size       // byte count, LAST parameter
);
```

### 3.6 Address Translation

```c
// Translate guest virtual address to guest physical address.
// Returns 0 on failure.
ULONG64 SdkGetPhysicalAddress(
    _In_ ULONG64 PartitionHandle,
    _In_ ULONG64 Va,
    _In_ MEMORY_ACCESS_TYPE MmAccess    // MmVirtualMemory for GVA->GPA
);
```

### 3.7 Virtual Processor Registers

```c
// Read one VP register.
// VpIndex is 0-based. Use Vtl0 unless working with VBS/secure kernel.
BOOLEAN SdkReadVpRegister(
    _In_    ULONG64          PartitionHandle,
    _In_    HV_VP_INDEX      VpIndex,        // 0 = first vCPU
    _In_    VTL_LEVEL        InputVtl,       // Vtl0, Vtl1, or Vtl2
    _In_    HV_REGISTER_NAME RegisterCode,   // e.g. HvX64RegisterRip
    _Inout_ PHV_REGISTER_VALUE RegisterValue // out: Reg64 for GP registers
);

// Write one VP register.
BOOLEAN SdkWriteVpRegister(
    _In_ ULONG64          PartitionHandle,
    _In_ HV_VP_INDEX      VpIndex,
    _In_ VTL_LEVEL        InputVtl,
    _In_ HV_REGISTER_NAME RegisterCode,
    _In_ PHV_REGISTER_VALUE RegisterValue    // in: set Reg64 before calling
);
```

Common register names: `HvX64RegisterRip`, `HvX64RegisterRsp`, `HvX64RegisterRax`,
`HvX64RegisterRcx`, `HvX64RegisterRdx`, `HvX64RegisterRbx`, `HvX64RegisterCr3`,
`HvX64RegisterRflags`.

### 3.8 VM State Control

```c
// Suspend or resume the VM.
// ManageWorkerProcess: pass value of InfoIsNeedVmwpSuspend for correct behavior.
BOOLEAN SdkControlVmState(
    _In_ ULONG64 PartitionHandle,
    _In_ VM_STATE_ACTION    Action,       // SuspendVm | ResumeVm
    _In_ SUSPEND_RESUME_METHOD ActionMethod,
    _In_ BOOLEAN ManageWorkerProcess
);
```

### 3.9 Guest Architecture

```c
// Returns MACHINE_AMD64 for x64 VMs, MACHINE_ARM64 for ARM64.
// Check before any AMD64-specific operations.
MACHINE_TYPE SdkGetMachineType(
    _In_ ULONG64 PartitionHandle
);
```

### 3.10 Hypercall

```c
// Directly issue a Hyper-V hypercall.
// InputBuffer and OutputBuffer must be page-aligned (VirtualAlloc).
// For simple (non-rep) calls: RepStartIndex=0, CountOfElements=0.
BOOLEAN SdkInvokeHypercall(
    _In_ ULONG32 HvCallId,
    _In_ BOOLEAN IsFast,
    _In_ UINT32  RepStartIndex,
    _In_ UINT32  CountOfElements,
    _In_ BOOLEAN IsNested,
    _In_ PVOID   InputBuffer,
    _In_ PVOID   OutputBuffer
);
```

### 3.11 Symbol Resolution

```c
// Get virtual address of a symbol in the guest OS.
// ImageBase and ImageSize come from the loaded module list.
ULONG64 SdkSymGetSymbolAddress(
    _In_ ULONG64 PartitionHandle,
    _In_ ULONG64 ImageBase,
    _In_ ULONG   ImageSize,
    _In_ const char* symbolName,    // ANSI, null-terminated
    _In_ MEMORY_ACCESS_TYPE MmAccess
);

// Enumerate all symbols for a driver. Call with pSymbolTable=NULL first to
// get the count, allocate, then call again with the real buffer.
BOOLEAN SdkSymEnumAllSymbols(
    _In_         ULONG64 PartitionHandle,
    _In_         PWCHAR  DriverName,        // e.g. L"ntoskrnl.exe"
    _Inout_opt_  PSYMBOL_INFO_PACKAGE pSymbolTable,
    _In_         MEMORY_ACCESS_TYPE MmAccess
);

// Returns the number of symbols for a driver (use to size the buffer).
ULONG64 SdkSymEnumAllSymbolsGetTableLength(
    _In_ ULONG64 PartitionHandle,
    _In_ PWCHAR  DriverName,
    _In_ MEMORY_ACCESS_TYPE MmAccess
);
```

---

## 4. Canonical Call Sequence

```c
// Step 1 — configure
VM_OPERATIONS_CONFIG cfg = { 0 };
SdkGetDefaultConfig(&cfg);
// cfg.ReadMethod  = ReadInterfaceHvmmDrvInternal; // default, fastest
// cfg.WriteMethod = WriteInterfaceHvmmDrvInternal;

// Step 2 — enumerate
ULONG64 count = 0;
PULONG64 handles = SdkEnumPartitions(&count, &cfg);
if (!handles || count == 0) { /* error */ }

// Step 3 — inspect (before SdkSelectPartition)
WCHAR* name = NULL;
SdkGetData(handles[0], InfoPartitionFriendlyName, &name);

// Step 4 — activate one partition
if (!SdkSelectPartition(handles[0])) { /* error */ }
ULONG64 h = handles[0];

// Step 5 — use
MACHINE_TYPE mt = SdkGetMachineType(h);

ULONG64 kernBase = SdkGetData2(h, InfoKernelBase);

HV_REGISTER_VALUE rv = { 0 };
SdkReadVpRegister(h, 0, Vtl0, HvX64RegisterRip, &rv);

PVOID buf = malloc(PAGE_SIZE);
SdkReadPhysicalMemory(h, 0x1000, PAGE_SIZE, buf, cfg.ReadMethod);
free(buf);

// Step 6 — cleanup
SdkClosePartition(h);
SdkCloseAllPartitions();
```

---

## 5. Common Pitfalls

| Mistake | Correct practice |
|---------|-----------------|
| Pass page number as `StartPosition` to `SdkReadPhysicalMemory` | `StartPosition` is always a **byte address** (GPA). Multiply page index by `PAGE_SIZE` (0x1000). |
| Pass `g_MemoryReadInterfaceType` to `SdkWritePhysicalMemory` | Use `g_MemoryWriteInterfaceType` (or `cfg.WriteMethod`) for write functions. |
| Pass a value where `PVOID` is expected | `SdkWritePhysicalMemory` and `SdkWriteVirtualMemory` take `PVOID ClientBuffer` — always pass `&myVar`, not `myVar`. |
| Swap `Buffer` and `Size` in `SdkWriteVirtualMemory` | Signature is `(handle, Va, Buffer, Size)` — `Buffer` (PVOID) comes before `Size` (ULONG). |
| Free pointers returned by `SdkGetData` for name/string classes | `InfoPartitionFriendlyName`, `InfoVmtypeString`, `InfoVmGuidString` return pointers to **internal SDK buffers** — do **not** free. |
| Use a handle after `SdkClosePartition` | Handle is invalid after close. Re-enumerate and re-select if needed. |
| Call memory/register functions before `SdkSelectPartition` | `SdkSelectPartition` must succeed before any operation on that partition. |
| Assume `SdkSetData(0, InfoStructure, ...)` returns a bool | When `PartitionHandle=0` and `HvmmInformationClass=InfoStructure`, the return value is the **new partition handle**, not a success code. |
| Build against `hvlib.lib` without `hvmm.sys` present at runtime | `hvmm.sys` must be in the same directory as `hvlib.dll`. |

---

## 6. Cross-Process Handle Transfer Pattern

When a partition handle must be used in a child process (e.g., injected into kd.exe):

```c
// Parent: serialize partition state into InfoPartition structure
// (obtained via SdkGetData with InfoStructure), duplicate the raw handle,
// then in the child process:

ULONG64 NewHandle = SdkSetData(0, InfoStructure, (ULONG64)&InfoPartition);
SdkSetData(NewHandle, InfoPartitionHandle,           (ULONG64)DuplicatedHandle);
SdkSetData(NewHandle, InfoSettingsCrashDumpEmulation, TRUE);
SdkSetData(NewHandle, InfoSettingsUseDecypheredKdbg,  TRUE);

// NewHandle is now ready for SdkReadPhysicalMemory, etc.
```

---

## 7. Build Configuration (Visual Studio / MSBuild)

```xml
<!-- Additional include directories -->
<IncludePath>path\to\LiveCloudKdSdk\public;$(IncludePath)</IncludePath>

<!-- Additional library directories -->
<LibraryPath>path\to\LiveCloudKdSdk\files;$(LibraryPath)</LibraryPath>

<!-- Linker input -->
<AdditionalDependencies>hvlib.lib;%(AdditionalDependencies)</AdditionalDependencies>

<!-- Compile as C (not C++) for non-C++ projects -->
<CompileAs>CompileAsC</CompileAs>
```

Deploy `hvlib.dll` and `hvmm.sys` next to the built executable.

---

## 8. Available Exports in hvlib.lib (current build)

```
SdkCloseAllPartitions       SdkReadVpRegister
SdkClosePartition           SdkSelectPartition
SdkControlVmState           SdkSetData
SdkEnumPartitions           SdkWritePhysicalMemory
SdkGetData                  SdkWriteVirtualMemory
SdkGetData2                 SdkWriteVpRegister
SdkGetDefaultConfig         SdkInvokeHypercall
SdkGetMachineType           SdkNumberToString
SdkGetPhysicalAddress
SdkReadPhysicalMemory
SdkReadVirtualMemory
```

Note: `SdkGetPartitionConfig`, `SdkSetPartitionConfig`, and the `SdkSym*` family
are declared in the public header but are **not yet exported** by the current
`hvlib.lib`. Do not reference them until they appear in an updated library.

---

## 9. Query Templates for This Project

### 9.1 META-CODE-ISSUE-TRIED Format

```
[META] C | Win10+ | hvlib.dll SDK | function: [name]
[CODE] [file:lines] — [brief description]
[ISSUE] [problem description]
[TRIED] [what was tried]
```

### 9.2 Common Query Patterns (compact)

**Debugging:**
```
DEBUG: SdkReadPhysicalMemory returns FALSE
AT: my_code.c:47
CONTEXT: h=valid, addr=0x1000, size=PAGE_SIZE, method=ReadInterfaceHvmmDrvInternal
TRIED: SdkSelectPartition OK, handle != 0
```

**Code generation:**
```
CREATE: read all vCPU registers
API: SdkReadVpRegister(h, vpIndex, Vtl0, regCode, &rv)
REGISTERS: Rip, Rsp, Rax, Rcx, Rdx, Rbx, Cr3, Rflags
PATTERN: loop over register codes array
```

**Optimization:**
```
OPTIMIZE: physical memory dump
CURRENT: SdkReadPhysicalMemory per PAGE_SIZE (0x1000)
TARGET: batch per BLOCK_SIZE (0x100000)
CONSTRAINTS: PAGE_SIZE-aligned, check InfoMmMaximumPhysicalPage
```

### 9.3 Token-Efficiency Principles

- **addr** = byte address, **h** = partition handle — use abbreviations after establishing context
- Do not repeat SDK signatures — reference the section: "see §3.4"
- Send only the relevant code snippet, not the entire file
- For iterative debugging: diff instead of full code
- Structured format (key-value) instead of prose

---

## 10. Cross-References

| Wrapper | File | API Coverage | Documentation |
|---------|------|-------------|---------------|
| C# (.NET 9.0) | `hvlibdotnet/hvlibdotnet.cs` | Full (all functions + symbols) | [hvlibdotnet/AI Code Generation Manifest.md](../hvlibdotnet/AI%20Code%20Generation%20Manifest.md) |
| Python 3.x | `LiveCloudKdPy/hvlib/hvlib.py` | Partial (8 functions, no registers/symbols) | [LiveCloudKdPy/AI Code Generation Manifest.md](../LiveCloudKdPy/AI%20Code%20Generation%20Manifest.md) |
| C example | `LiveCloudKdExample/` | C API usage example | — |
