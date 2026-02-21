# hvlibdotnet — AI Code Generation Manifest

> **Goal:** Machine-readable context for AI tools working with the C# hvlib.dll wrapper
> **Principle:** Maximum information / minimum tokens

---

## 1. Integration

| Parameter | Value |
|-----------|-------|
| Language | C# (.NET 9.0, net9.0-windows7.0) |
| Binding | P/Invoke → hvlib.dll |
| Dependencies | hvlib.dll + hvmm.sys in the same directory |
| Privileges | **Administrator** |
| Namespace | `Hvlibdotnet` |
| Class | `Hvlib` (static methods) |
| File | `hvlibdotnet.cs` |

```csharp
using Hvlibdotnet;
// All methods are called as Hvlib.MethodName(...)
```

---

## 2. Enumerations (compact)

```
READ_MEMORY_METHOD:   0=Unsupported 1=HvmmDrvInternal 2=WinHv 3=HvmmLocal 4=Max
WRITE_MEMORY_METHOD:  0=Unsupported 1=HvmmDrvInternal 2=WinHv 3=HvmmLocal 4=Max
SUSPEND_RESUME_METHOD: 0=Unsupported 1=Powershell 2=WriteSpecRegister
VM_STATE_ACTION:      0=SuspendVm 1=ResumeVm
LOG_LEVEL:            0=er 1=v1 2=v2 3=v3 4=skip
GET_CR3_TYPE:         0=Process 0xFFFFFFD=Kernel 0xFFFFFFE=SecureKernel 0xFFFFFFF=Hypervisor
MEMORY_ACCESS_TYPE:   0=PhysicalMemory 1=VirtualMemory 2=RtCore64
MACHINE_TYPE:         0=Unknown 1=X86 2=AMD64 3=ARM64 4=Unsupported
VTL_LEVEL:            0..15=Vtl0..Vtl15 16=BadVtl
```

### HVDD_INFORMATION_CLASS (full list)

```
 0=KdbgData           1=FriendlyName      2=PartitionId       3=VmtypeString
 4=Structure           5=KiProcessorBlock  6=MmMaxPhysPage     7=KPCR
 8=NumberOfCPU         9=KDBGPa           10=NumberOfRuns      11=KernelBase
12=MmPfnDatabase      13=PsLoadedModuleList 14=PsActiveProcessHead 15=NtBuildNumber
16=NtBuildNumberVA    17=DirectoryTableBase 18=Run              19=KdbgDataBlockArea
20=VmGuidString       21=PartitionHandle   22=KdbgContext       23=KdVersionBlock
24=MmPhysicalMemBlock 25=NumberOfPages     26=IdleKernelStack   27=SizeOfKdDebuggerData
28=CpuContextVa       29=Size             30=MemoryBlockCount   31=SuspendedCores
32=SuspendedWorker    33=IsContainer       34=IsNeedVmwpSuspend 35=GuestOsType
36=CrashDumpEmulation 37=UseDecypheredKdbg 38=BuilLabBuffer     39=GetCr3byPid
40=GetProcessesIds    41=DumpHeaderPointer 42=UpdateCr3ForLocal 43=GetCr3Kernel
44=GetCr3Hv          45=GetCr3Securekernel
// Set-only:
HvddSetMemoryBlock, HvddEnlVmcsPointer
```

**String classes** (return IntPtr → Marshal.PtrToStringUni): FriendlyName, VmtypeString, VmGuidString, BuilLabBuffer.
**Do NOT free** returned pointers — they belong to the SDK.

---

## 3. Key Structures

### VM_OPERATIONS_CONFIG

```
Field                  | C# type                | Description
ReadMethod             | READ_MEMORY_METHOD     | Memory read method
WriteMethod            | WRITE_MEMORY_METHOD    | Memory write method
SuspendMethod          | SUSPEND_RESUME_METHOD  | Suspend/resume method
LogLevel               | LOG_LEVEL              | Log level [0..4]
ForceFreezeCPU         | bool                   | Freeze vCPU on read
PausePartition         | bool                   | Pause VM on SelectPartition
ExdiConsoleHandle      | IntPtr                 | EXDI handle (IntPtr.Zero for standalone use)
ReloadDriver           | bool                   | Reload hvmm.sys
PFInjection            | bool                   | Page-fault injection
NestedScan             | bool                   | Nested partitions
UseDebugApiStopProcess | bool                   | Debug API for process stop
SimpleMemory           | bool                   | For Linux VM
ReplaceDecypheredKDBG  | bool                   | Replace decyphered KDBG
FullCrashDumpEmulation | bool                   | Full crash dump emulation
EnumGuestOsBuild       | bool                   | Enumerate guest OS build
VSMScan                | bool                   | Virtual Secure Mode scan
```

### HV_REGISTER_VALUE

```csharp
[StructLayout(LayoutKind.Explicit)]
// Reg64 (offset 0) — GP registers. Low64+High64 — 128-bit (XMM).
```

### VmListBox

```csharp
public class VmListBox { UInt64 VmHandle; string? VMName; }
```

---

## 4. API — Signatures

### 4.1 Configuration

```csharp
static bool GetPreferredSettings(ref VM_OPERATIONS_CONFIG cfg)
static bool SetPreferredSettings(UInt64 PartitionHandle, ref VM_OPERATIONS_CONFIG cfg)
```

### 4.2 Enumeration and Partition Selection

```csharp
static List<VmListBox>? EnumPartitions(ref VM_OPERATIONS_CONFIG cfg)   // null = error
static List<VmListBox>? EnumAllPartitions()    // simplified variant with defaults
static bool SelectPartition(UInt64 PartitionHandle)
```

### 4.3 Data Query / Set

```csharp
static bool   GetPartitionData(UInt64 h, HVDD_INFORMATION_CLASS cls, out UIntPtr info)
static UInt64 GetPartitionData2(UInt64 h, HVDD_INFORMATION_CLASS cls)   // convenience version
static UInt64 SetPartitionData(UInt64 h, HVDD_INFORMATION_CLASS cls, UInt64 val)
```

### 4.4 Physical Memory (GPA)

```csharp
static bool ReadPhysicalMemory(UInt64 h, UInt64 addr, UInt64 size, IntPtr buf)
static bool ReadPhysicalMemory(UInt64 h, UInt64 addr, UInt64 size, byte[] buf)
static bool ReadPhysicalMemory(UInt64 h, UInt64 addr, UInt64 size, ulong[] buf)
static bool ReadPhysicalMemoryWithMethod(UInt64 h, UInt64 addr, UInt64 size, IntPtr buf, READ_MEMORY_METHOD method)
static bool WritePhysicalMemory(UInt64 h, UInt64 addr, UInt64 size, IntPtr buf)
```

**addr** = GPA byte address, NOT page number. Method is taken from current configuration (except WithMethod).

### 4.5 Virtual Memory (GVA)

```csharp
static bool ReadVirtualMemory(UInt64 h, UInt64 va, UInt64 size, IntPtr buf)
static bool WriteVirtualMemory(UInt64 h, UInt64 va, UInt64 size, IntPtr buf)
```

### 4.6 Address Translation and Architecture

```csharp
static UInt64 GetPhysicalAddress(UInt64 h, UInt64 va, MEMORY_ACCESS_TYPE = MmVirtualMemory)
// Returns GPA or 0 on error.

static MACHINE_TYPE GetMachineType(UInt64 h)

static UInt64 GetCr3FromPid(UInt64 h, UInt64 pid)
// Special PIDs: 0xFFFFFFD=Kernel, 0xFFFFFFE=SecureKernel, 0xFFFFFFF=Hypervisor
```

### 4.7 VM State Control

```csharp
static bool SuspendVm(UInt64 h, SUSPEND_RESUME_METHOD = Powershell, bool manageWorker = false)
static bool ResumeVm(UInt64 h, SUSPEND_RESUME_METHOD = Powershell, bool manageWorker = false)
static bool ControlVmState(UInt64 h, VM_STATE_ACTION action, SUSPEND_RESUME_METHOD method, bool manageWorker = false)
```

### 4.8 Virtual Processor Registers

```csharp
static bool ReadVpRegister(UInt64 h, UInt32 vpIndex, VTL_LEVEL vtl, UInt32 regCode, out HV_REGISTER_VALUE val)
static bool WriteVpRegister(UInt64 h, UInt32 vpIndex, VTL_LEVEL vtl, UInt32 regCode, HV_REGISTER_VALUE val)
```

`vpIndex` = 0-based. `regCode` = HV_REGISTER_NAME (HvX64RegisterRip etc.).

### 4.9 Symbols

```csharp
static UInt64 GetSymbolTableLength(UInt64 h, string driverName, MEMORY_ACCESS_TYPE = MmVirtualMemory)
static List<SYMBOL_INFO_PWSH>? GetAllSymbolsForModule(IntPtr h, string moduleName)
// Returns a list of symbols with values read from VM memory.
```

### 4.10 Cleanup

```csharp
static void ClosePartition(UInt64 h)
static void CloseAllPartitions()
```

---

## 5. Canonical Call Sequence

```csharp
// 1. Configuration
var cfg = new Hvlib.VM_OPERATIONS_CONFIG();
Hvlib.GetPreferredSettings(ref cfg);

// 2. Enumeration
var vms = Hvlib.EnumPartitions(ref cfg);      // or EnumAllPartitions()
if (vms == null) return;

// 3. Select
Hvlib.SelectPartition(vms[0].VmHandle);
UInt64 h = vms[0].VmHandle;

// 4. Use
var mt = Hvlib.GetMachineType(h);
UInt64 kb = Hvlib.GetPartitionData2(h, Hvlib.HVDD_INFORMATION_CLASS.HvddKernelBase);
byte[] buf = new byte[0x1000];
Hvlib.ReadPhysicalMemory(h, 0x1000, 0x1000, buf);

// 5. Cleanup
Hvlib.CloseAllPartitions();
```

---

## 6. Common Mistakes

| Mistake | Correct |
|---------|---------|
| Passing PFN instead of byte address to ReadPhysicalMemory | `addr = pfn << 12` (pfn × 0x1000) |
| Using ReadMethod for WritePhysicalMemory | Write operations use cfg.WriteMethod automatically |
| Calling API before SelectPartition | SelectPartition is required before operations on a partition |
| Freeing strings from GetPartitionData2 (FriendlyName etc.) | Pointers belong to the SDK — do NOT free |
| Using handle after ClosePartition | Handle is invalid after Close. Re-enumerate if needed |
| GetPartitionData vs GetPartitionData2 | GetPartitionData → bool + out param. GetPartitionData2 → UInt64 directly |
| EnumPartitions without prior GetPreferredSettings | Always initialize cfg via GetPreferredSettings |

---

## 7. P/Invoke — Parameter Order (pitfalls)

```
SdkReadVirtualMemory:  (handle, va, buffer, size)  ← buffer BEFORE size
SdkWriteVirtualMemory: (handle, va, buffer, size)  ← buffer BEFORE size
SdkReadPhysicalMemory: (handle, addr, size, buffer, method) ← size BEFORE buffer
SdkWritePhysicalMemory:(handle, addr, size, buffer, method) ← size BEFORE buffer
```

Public C# wrappers normalize the order. With direct DllImport — pay attention to the order.

---

## 8. Query Templates for This Project

```
[META] C# | .NET 9.0 | P/Invoke | hvlib.dll wrapper
[CODE] hvlibdotnet.cs, focus on [method/line]
[ISSUE] [problem description]
[TRIED] [what was tried]
```

**Examples of effective queries:**

```
Add wrapper for SdkInvokeHypercall
C API: BOOLEAN SdkInvokeHypercall(ULONG32 HvCallId, BOOLEAN IsFast, UINT32 RepStart, UINT32 Count, BOOLEAN IsNested, PVOID In, PVOID Out)
Pattern: DllImport private + public static wrapper
```

```
DEBUG: ReadVpRegister returns false
AT: hvlibdotnet.cs, ReadVpRegister
CONTEXT: VpIndex=0, Vtl0, regCode for RIP
TRIED: SelectPartition called, handle is valid
```
