This is description for C# API wrappers for hvlib.dll - Hyper-V memory manager plugin
Main library file is hvlibdotnet.cs. It must be integrated in your C# project.
Target framework: .NET 9.0 (net9.0-windows7.0)

Example of C# API wrappers usages in [Hyper Views](https://github.com/gerhart01/Hyper-V-Tools/tree/main/HyperViews)

## Table of Contents

- [Enumerations](#enumerations)
- [Structures](#structures)
- [1. GetPreferredSettings](#1-getpreferredsettings)
- [2. SetPreferredSettings](#2-setpreferredsettings)
- [3. EnumPartitions](#3-enumpartitions)
- [4. EnumAllPartitions](#4-enumallpartitions)
- [5. GetData and GetData2](#5-getdata-and-getdata2)
- [6. SetPartitionData](#6-setpartitiondata)
- [7. SelectPartition](#7-selectpartition)
- [8. ReadPhysicalMemory](#8-readphysicalmemory)
- [9. ReadVirtualMemory](#9-readvirtualmemory)
- [10. WritePhysicalMemory](#10-writephysicalmemory)
- [11. WriteVirtualMemory](#11-writevirtualmemory)
- [12. GetCr3FromPid](#12-getcr3frompid)
- [13. SuspendVm / ResumeVm / ControlVmState](#13-suspendvm--resumevm--controlvmstate)
- [14. GetPhysicalAddress](#14-getphysicaladdress)
- [15. GetMachineType](#15-getmachinetype)
- [16. ReadVpRegister / WriteVpRegister](#16-readvpregister--writevpregister)
- [17. Symbol Operations](#17-symbol-operations)
- [18. ClosePartition / CloseAllPartitions](#18-closepartition--closeallpartitions)

## Enumerations

### READ_MEMORY_METHOD

Memory reading method for driver.

```csharp
public enum READ_MEMORY_METHOD
{
    ReadInterfaceUnsupported,
    ReadInterfaceHvmmDrvInternal,
    ReadInterfaceWinHv,
    ReadInterfaceHvmmLocal,
    ReadInterfaceHvmmMax
}
```

### WRITE_MEMORY_METHOD

Memory writing method for driver.

```csharp
public enum WRITE_MEMORY_METHOD
{
    WriteInterfaceUnsupported,
    WriteInterfaceHvmmDrvInternal,
    WriteInterfaceWinHv,
    WriteInterfaceHvmmLocal,
    WriteInterfaceHvmmMax
}
```

### SUSPEND_RESUME_METHOD

Method of suspend/resume VM.

```csharp
public enum SUSPEND_RESUME_METHOD
{
    SuspendResumeUnsupported,
    SuspendResumePowershell,
    SuspendResumeWriteSpecRegister
}
```

### VM_STATE_ACTION

```csharp
public enum VM_STATE_ACTION
{
    SuspendVm = 0,
    ResumeVm = 1
}
```

### LOG_LEVEL

```csharp
public enum LOG_LEVEL
{
    log_er = 0,
    log_v1 = 1,
    log_v2 = 2,
    log_v3 = 3,
    log_skip = 4
}
```

### GET_CR3_TYPE

Special PID values for GetCr3FromPid to retrieve kernel/hypervisor/secure kernel CR3.

```csharp
public enum GET_CR3_TYPE
{
    Cr3Process = 0,
    Cr3Kernel = 0xFFFFFFD,
    Cr3SecureKernel = 0xFFFFFFE,
    Cr3Hypervisor = 0xFFFFFFF
}
```

### MEMORY_ACCESS_TYPE

Memory access type for address translation.

```csharp
public enum MEMORY_ACCESS_TYPE
{
    MmPhysicalMemory = 0,
    MmVirtualMemory = 1,
    MmAccessRtCore64 = 2
}
```

### MACHINE_TYPE

Machine architecture type.

```csharp
public enum MACHINE_TYPE
{
    MACHINE_UNKNOWN = 0,
    MACHINE_X86 = 1,
    MACHINE_AMD64 = 2,
    MACHINE_ARN64 = 3,
    MACHINE_UNSUPPORTED = 4
}
```

### VTL_LEVEL

Virtual Trust Level. See [HV_VTL](https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/datatypes/hv_vtl).

```csharp
public enum VTL_LEVEL
{
    Vtl0 = 0,
    Vtl1 = 1,
    Vtl2 = 2,
    ...
    Vtl15 = 15,
    BadVtl = 16
}
```

### HVDD_INFORMATION_CLASS

Information classes for GetData / GetData2 / SetPartitionData.

```csharp
    HvddKdbgData,
    HvddPartitionFriendlyName,
    HvddPartitionId,
    HvddVmtypeString,
    HvddStructure,
    HvddKiProcessorBlock,
    HvddMmMaximumPhysicalPage,
    HvddKPCR,
    HvddNumberOfCPU,
    HvddKDBGPa,
    HvddNumberOfRuns,
    HvddKernelBase,
    HvddMmPfnDatabase,
    HvddPsLoadedModuleList,
    HvddPsActiveProcessHead,
    HvddNtBuildNumber,
    HvddNtBuildNumberVA,
    HvddDirectoryTableBase,
    HvddRun,
    HvddKdbgDataBlockArea,
    HvddVmGuidString,
    HvddPartitionHandle,
    HvddKdbgContext,
    HvddKdVersionBlock,
    HvddMmPhysicalMemoryBlock,
    HvddNumberOfPages,
    HvddIdleKernelStack,
    HvddSizeOfKdDebuggerData,
    HvddCpuContextVa,
    HvddSize,
    HvddMemoryBlockCount,
    HvddSuspendedCores,
    HvddSuspendedWorker,
    HvddIsContainer,
    HvddIsNeedVmwpSuspend,
    HvddGuestOsType,
    HvddSettingsCrashDumpEmulation,
    HvddSettingsUseDecypheredKdbg,
    HvddBuilLabBuffer,
    HvddGetCr3byPid,
    HvddGetProcessesIds,
    HvddDumpHeaderPointer,
    HvddUpdateCr3ForLocal,
    HvddGetCr3Kernel,
    HvddGetCr3Hv,
    HvddGetCr3Securekernel,
    //Special set values
    HvddSetMemoryBlock,
    HvddEnlVmcsPointer
```

## Structures

### VM_OPERATIONS_CONFIG

Configuration for VM operations. Used in GetPreferredSettings / SetPreferredSettings / EnumPartitions.

| Field                    | Type                   | Description                                                                  |
|--------------------------|------------------------|------------------------------------------------------------------------------|
| ReadMethod               | READ_MEMORY_METHOD     | Memory reading method for driver                                             |
| WriteMethod              | WRITE_MEMORY_METHOD    | Memory writing method for driver                                             |
| SuspendMethod            | SUSPEND_RESUME_METHOD  | Method of suspend/resume VM                                                  |
| LogLevel                 | LOG_LEVEL              | Log level                                                                    |
| ForceFreezeCPU           | bool                   | Freeze CPU using virtual VM registers when suspending VM                     |
| PausePartition           | bool                   | VM is suspended when SdkSelectPartition is executed                          |
| ExdiConsoleHandle        | IntPtr                 | EXDI console handle                                                          |
| ReloadDriver             | bool                   | Reload driver when starting plugin. Needed when service is not deleted correctly |
| PFInjection              | bool                   | Enable page fault injection                                                  |
| NestedScan               | bool                   | Enable nested virtualization scan                                            |
| UseDebugApiStopProcess   | bool                   | Use Debug API to stop process                                                |
| SimpleMemory             | bool                   | Used for Linux VM memory scanning                                            |
| ReplaceDecypheredKDBG    | bool                   | Replace decyphered KDBG                                                      |
| FullCrashDumpEmulation   | bool                   | Full crash dump emulation                                                    |
| EnumGuestOsBuild         | bool                   | Enumerate guest OS build information                                         |
| VSMScan                  | bool                   | Enable Virtual Secure Mode scanning                                          |

### HV_REGISTER_VALUE

Union structure for virtual processor register values (used in ReadVpRegister / WriteVpRegister).

```csharp
[StructLayout(LayoutKind.Explicit)]
public struct HV_REGISTER_VALUE
{
    [FieldOffset(0)] public UInt64 Reg64;
    [FieldOffset(0)] public UInt32 Reg32;
    [FieldOffset(0)] public UInt16 Reg16;
    [FieldOffset(0)] public byte Reg8;
    [FieldOffset(0)] public UInt64 Low64;   // for 128-bit values (XMM registers, etc.)
    [FieldOffset(8)] public UInt64 High64;
}
```

---

# 1. GetPreferredSettings

Description: Get default plugin configuration.

```csharp
    bool GetPreferredSettings(ref VM_OPERATIONS_CONFIG cfg)
```

Example:

```csharp
    Hvlib.VM_OPERATIONS_CONFIG cfg = new Hvlib.VM_OPERATIONS_CONFIG();
    bool bResult = Hvlib.GetPreferredSettings(ref cfg);
```

result: bool. True, if operation is success.

# 2. SetPreferredSettings

Description: Set plugin configuration for a specific partition.

```csharp
    bool SetPreferredSettings(UInt64 PartitionHandle, ref VM_OPERATIONS_CONFIG cfg)
```

Parameters:

* **PartitionHandle** - handle of partition
* **cfg** - VM_OPERATIONS_CONFIG structure with settings to apply

Example:

```csharp
    Hvlib.VM_OPERATIONS_CONFIG cfg = new Hvlib.VM_OPERATIONS_CONFIG();
    Hvlib.GetPreferredSettings(ref cfg);
    cfg.ReadMethod = Hvlib.READ_MEMORY_METHOD.ReadInterfaceHvmmDrvInternal;
    cfg.WriteMethod = Hvlib.WRITE_MEMORY_METHOD.WriteInterfaceWinHv;
    bool bResult = Hvlib.SetPreferredSettings(PartitionHandle, ref cfg);
```

result: bool. True, if operation is success.

# 3. EnumPartitions

Description: Hyper-V active partitions enumeration.

```csharp
    List<VmListBox>? EnumPartitions(ref VM_OPERATIONS_CONFIG cfg)
```

Parameters:

* **cfg** - VM_OPERATIONS_CONFIG structure, where you can modify one or more parameter

Example:
```csharp
    Hvlib.VM_OPERATIONS_CONFIG cfg = new Hvlib.VM_OPERATIONS_CONFIG();
    Hvlib.GetPreferredSettings(ref cfg);
    cfg.ReadMethod = Hvlib.READ_MEMORY_METHOD.ReadInterfaceHvmmDrvInternal;

    List<Hvlib.VmListBox>? partitions = Hvlib.EnumPartitions(ref cfg);

    if (partitions == null)
    {
        Console.WriteLine("No partitions found");
        return;
    }

    foreach (var vm in partitions)
    {
        Console.WriteLine($"VM: {vm.VMName}, Handle: {vm.VmHandle}");
    }
```

result type: List\<VmListBox\>? (null if enumeration failed)

VmListBox structure:

```csharp
public class VmListBox
{
    public UInt64 VmHandle { get; set; }
    public string? VMName { get; set; }
}
```

# 4. EnumAllPartitions

Description: Enumerate all VM partitions using default configuration (ReadInterfaceHvmmDrvInternal / WriteInterfaceWinHv).

```csharp
    List<VmListBox>? EnumAllPartitions()
```

Example:
```csharp
    List<Hvlib.VmListBox>? partitions = Hvlib.EnumAllPartitions();
```

result type: List\<VmListBox\>? (null if enumeration failed)

# 5. GetData and GetData2

Description: Get specific data from partition. GetPartitionData and GetPartitionData2 have similar functionality, but with different return types.

```csharp
    bool GetPartitionData(UInt64 PartitionHandle, HVDD_INFORMATION_CLASS InformationClass, out UIntPtr HvddInformation)
    UInt64 GetPartitionData2(UInt64 PartitionHandle, HVDD_INFORMATION_CLASS InformationClass)
```

Parameters:

* **PartitionHandle** - handle of partition
* **InformationClass** - information class (see [HVDD_INFORMATION_CLASS](#hvdd_information_class))
* **HvddInformation** - pointer to variable with result (GetPartitionData only)

Example (GetPartitionData2):

```csharp
    IntPtr VmName = (IntPtr)Hvlib.GetPartitionData2((UInt64)vm.VmHandle, Hvlib.HVDD_INFORMATION_CLASS.HvddPartitionFriendlyName);
    string? VmNameStr = Marshal.PtrToStringUni(VmName);

    IntPtr VmGuid = (IntPtr)Hvlib.GetPartitionData2((UInt64)vm.VmHandle, Hvlib.HVDD_INFORMATION_CLASS.HvddVmGuidString);
    string? VmGuidStr = Marshal.PtrToStringUni(VmGuid);

    IntPtr VmType = (IntPtr)Hvlib.GetPartitionData2((UInt64)vm.VmHandle, Hvlib.HVDD_INFORMATION_CLASS.HvddVmtypeString);
    string? VmTypeStr = Marshal.PtrToStringUni(VmType);

    UInt64 PartitionId = Hvlib.GetPartitionData2((UInt64)vm.VmHandle, Hvlib.HVDD_INFORMATION_CLASS.HvddPartitionId);
```

Example (GetPartitionData):

```csharp
    UIntPtr info;
    bool bResult = Hvlib.GetPartitionData(vm.VmHandle, Hvlib.HVDD_INFORMATION_CLASS.HvddPartitionId, out info);
```

result type: UInt64 from GetPartitionData2, bool from GetPartitionData

# 6. SetPartitionData

Description: Set data for a partition.

```csharp
    UInt64 SetPartitionData(UInt64 PartitionHandle, HVDD_INFORMATION_CLASS InformationClass, UInt64 Information)
```

Parameters:

* **PartitionHandle** - handle of partition
* **InformationClass** - information class to set (see [HVDD_INFORMATION_CLASS](#hvdd_information_class))
* **Information** - value to set

Example:

```csharp
    UInt64 result = Hvlib.SetPartitionData(vm.VmHandle, Hvlib.HVDD_INFORMATION_CLASS.HvddSetMemoryBlock, value);
```

result type: UInt64

# 7. SelectPartition

Description: Select one of partitions, which were gotten from EnumPartitions.

```csharp
    bool SelectPartition(UInt64 PartitionHandle)
```

Parameters:

* **PartitionHandle** - partition handle

Example:

```csharp
    bool bResult = Hvlib.SelectPartition(vm.VmHandle);

    if (!bResult)
        return false;
```

result: bool. True, if operation is success.

# 8. ReadPhysicalMemory

Description: Read memory block from specified physical address.

Multiple overloads are available:

```csharp
    bool ReadPhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, IntPtr ClientBuffer)
    bool ReadPhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, byte[] ClientBuffer)
    bool ReadPhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, ulong[] ClientBuffer)
    bool ReadPhysicalMemoryWithMethod(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, IntPtr ClientBuffer, READ_MEMORY_METHOD Method)
```

Parameters:
* **PartitionHandle** - handle of virtual machine
* **StartPosition** - address of physical memory
* **ReadByteCount** - size of memory block for reading
* **ClientBuffer** - buffer for data reading
* **Method** - explicit read method (ReadPhysicalMemoryWithMethod only)

Example (byte array):

```csharp
    uint bufferLength = 0x1000;
    byte[] pageBuffer = new byte[bufferLength];

    ulong pfn = 0x1000;
    ulong physicalAddress = pfn << 12;

    bool bResult = Hvlib.ReadPhysicalMemory(vm.VmHandle, physicalAddress, bufferLength, pageBuffer);
```

result: bool

# 9. ReadVirtualMemory

Description: Read memory block from specified virtual address.

```csharp
    bool ReadVirtualMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, IntPtr ClientBuffer)
```

Parameters:
* **PartitionHandle** - handle of virtual machine
* **StartPosition**   - virtual memory address
* **ReadByteCount**   - size of memory block for reading
* **ClientBuffer**    - buffer for data reading

Example:

```csharp
    uint bufferLength = 0x1000;
    IntPtr buffer = Marshal.AllocHGlobal((int)bufferLength);

    UInt64 Address = 0xFFFFF78000000000;

    bool bResult = Hvlib.ReadVirtualMemory(vm.VmHandle, Address, bufferLength, buffer);

    Marshal.FreeHGlobal(buffer);
```

result: bool

# 10. WritePhysicalMemory

Description: Write data to specified physical address.

```csharp
    bool WritePhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 WriteByteCount, IntPtr ClientBuffer)
```

Parameters:
* **PartitionHandle** - handle of virtual machine
* **StartPosition**   - physical memory address
* **WriteByteCount**  - size of data to write
* **ClientBuffer**    - buffer with data to write in memory

Example:

```csharp
    uint bufferLength = sizeof(ulong);
    ulong pageBuffer = 0x12345678;

    ulong pfn = 0x800000;

    bool bResult = Hvlib.WritePhysicalMemory(vm.VmHandle, pfn, bufferLength, ref pageBuffer);
```

result: bool

# 11. WriteVirtualMemory

Description: Write data to specified virtual address in virtual machine.

```csharp
    bool WriteVirtualMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 WriteByteCount, IntPtr ClientBuffer)
```

Parameters:
* **PartitionHandle** - handle of virtual machine
* **StartPosition**   - virtual memory address
* **WriteByteCount**  - size of buffer with data for writing
* **ClientBuffer**    - buffer with data to write in memory

Example:

```csharp
    uint bufferLength = 0x8;

    UInt64 Address = 0xFFFFF78000000000;
    UInt64 Value = 0x15000;

    bool bResult = Hvlib.WriteVirtualMemory(vm.VmHandle, Address, bufferLength, ref Value);
```

result: bool

# 12. GetCr3FromPid

Description: Get CR3 (page table base address) for a specific process ID. Special PID values from GET_CR3_TYPE can be used to get kernel, hypervisor, or secure kernel CR3.

```csharp
    UInt64 GetCr3FromPid(UInt64 PartitionHandle, UInt64 Pid)
```

Parameters:

* **PartitionHandle** - handle of virtual machine
* **Pid** - process ID, or special value from GET_CR3_TYPE (Cr3Kernel = 0xFFFFFFD, Cr3SecureKernel = 0xFFFFFFE, Cr3Hypervisor = 0xFFFFFFF)

Example (process CR3):

```csharp
    UInt64 pid = 4; // System process
    UInt64 cr3 = Hvlib.GetCr3FromPid(vm.VmHandle, pid);
```

Example (kernel CR3):

```csharp
    UInt64 cr3 = Hvlib.GetCr3FromPid(vm.VmHandle, (ulong)Hvlib.GET_CR3_TYPE.Cr3Kernel);
```

result: UInt64 (CR3 value, 0 if failed)

# 13. SuspendVm / ResumeVm / ControlVmState

Description: Suspend or resume virtual machine.

```csharp
    bool SuspendVm(UInt64 PartitionHandle, SUSPEND_RESUME_METHOD Method = SUSPEND_RESUME_METHOD.SuspendResumePowershell, bool ManageWorkerProcess = false)
    bool ResumeVm(UInt64 PartitionHandle, SUSPEND_RESUME_METHOD Method = SUSPEND_RESUME_METHOD.SuspendResumePowershell, bool ManageWorkerProcess = false)
    bool ControlVmState(UInt64 PartitionHandle, VM_STATE_ACTION Action, SUSPEND_RESUME_METHOD Method, bool ManageWorkerProcess = false)
```

Parameters:

* **PartitionHandle** - handle of virtual machine
* **Method** - suspend/resume method (default: SuspendResumePowershell)
* **ManageWorkerProcess** - manage worker process (default: false)
* **Action** - SuspendVm or ResumeVm (ControlVmState only)

Example:

```csharp
    bool bResult = Hvlib.SuspendVm(vm.VmHandle);
    // ... do work ...
    bResult = Hvlib.ResumeVm(vm.VmHandle);
```

Example (with explicit method):

```csharp
    bool bResult = Hvlib.ControlVmState(vm.VmHandle, Hvlib.VM_STATE_ACTION.SuspendVm,
        Hvlib.SUSPEND_RESUME_METHOD.SuspendResumeWriteSpecRegister);
```

result: bool

# 14. GetPhysicalAddress

Description: Translate guest virtual address (GVA) to guest physical address (GPA).

```csharp
    UInt64 GetPhysicalAddress(UInt64 PartitionHandle, UInt64 VirtualAddress, MEMORY_ACCESS_TYPE AccessType = MEMORY_ACCESS_TYPE.MmVirtualMemory)
```

Parameters:

* **PartitionHandle** - handle of virtual machine
* **VirtualAddress** - virtual address to translate
* **AccessType** - memory access type (default: MmVirtualMemory)

Example:

```csharp
    UInt64 va = 0xFFFFF78000000000;
    UInt64 pa = Hvlib.GetPhysicalAddress(vm.VmHandle, va);
```

result: UInt64 (physical address, 0 if translation failed)

# 15. GetMachineType

Description: Get guest VM architecture (x86 or AMD64). Plugin supports only AMD64 guest VMs.

```csharp
    MACHINE_TYPE GetMachineType(UInt64 PartitionHandle)
```

Parameters:

* **PartitionHandle** - handle of virtual machine

Example:

```csharp
    Hvlib.MACHINE_TYPE machineType = Hvlib.GetMachineType(vm.VmHandle);
```

result: MACHINE_TYPE

# 16. ReadVpRegister / WriteVpRegister

Description: Read or write virtual processor registers.

```csharp
    bool ReadVpRegister(UInt64 PartitionHandle, UInt32 VpIndex, VTL_LEVEL Vtl, UInt32 RegisterCode, out HV_REGISTER_VALUE RegisterValue)
    bool WriteVpRegister(UInt64 PartitionHandle, UInt32 VpIndex, VTL_LEVEL Vtl, UInt32 RegisterCode, HV_REGISTER_VALUE RegisterValue)
```

Parameters:

* **PartitionHandle** - handle of virtual machine
* **VpIndex** - virtual processor index
* **Vtl** - Virtual Trust Level (see [VTL_LEVEL](#vtl_level))
* **RegisterCode** - register code (e.g., HV_X64_REGISTER_RIP)
* **RegisterValue** - register value (output for Read, input for Write)

Example (read register):

```csharp
    Hvlib.HV_REGISTER_VALUE regValue;
    bool bResult = Hvlib.ReadVpRegister(vm.VmHandle, 0, Hvlib.VTL_LEVEL.Vtl0, registerCode, out regValue);
    UInt64 value = regValue.Reg64;
```

Example (write register):

```csharp
    Hvlib.HV_REGISTER_VALUE regValue = new Hvlib.HV_REGISTER_VALUE();
    regValue.Reg64 = 0x12345678;
    bool bResult = Hvlib.WriteVpRegister(vm.VmHandle, 0, Hvlib.VTL_LEVEL.Vtl0, registerCode, regValue);
```

result: bool

# 17. Symbol Operations

### GetSymbolTableLength

Description: Get the number of symbols in a driver's symbol table.

```csharp
    UInt64 GetSymbolTableLength(UInt64 PartitionHandle, string DriverName, MEMORY_ACCESS_TYPE AccessType = MEMORY_ACCESS_TYPE.MmVirtualMemory)
```

Parameters:

* **PartitionHandle** - handle of virtual machine
* **DriverName** - name of the driver (e.g., "ntoskrnl.exe")
* **AccessType** - memory access type (default: MmVirtualMemory)

Example:

```csharp
    UInt64 count = Hvlib.GetSymbolTableLength(vm.VmHandle, "ntoskrnl.exe");
```

result: UInt64 (number of symbols, 0 if driver not found or no symbols)

### GetAllSymbolsForModule

Description: Enumerate all symbols from a driver and read their values from VM memory.

```csharp
    List<SYMBOL_INFO_PWSH>? GetAllSymbolsForModule(IntPtr Handle, String ModuleName)
```

Parameters:

* **Handle** - partition handle (as IntPtr)
* **ModuleName** - name of the module (e.g., "ntoskrnl.exe")

Example:

```csharp
    List<Hvlib.SYMBOL_INFO_PWSH>? symbols = Hvlib.GetAllSymbolsForModule((IntPtr)vm.VmHandle, "ntoskrnl.exe");

    if (symbols != null)
    {
        foreach (var sym in symbols)
        {
            Console.WriteLine($"{sym.Name}: {sym.Address} = {sym.ReadValueHex}");
        }
    }
```

result: List\<SYMBOL_INFO_PWSH\>? (null if module not found or no symbols)

SYMBOL_INFO_PWSH structure:

```csharp
public struct SYMBOL_INFO_PWSH
{
    public uint SizeOfStruct;
    public uint TypeIndex;
    public uint Index;
    public uint Size;
    public string ModBase;       // hex string
    public uint Flags;
    public string Value;         // hex string
    public string Address;       // hex string
    public uint Register;
    public uint Scope;
    public uint Tag;
    public uint NameLen;
    public uint MaxNameLen;
    public string ModuleName;
    public string Name;
    public UInt64 ReadValue;
    public string ReadValueHex;  // hex string of value read from VM memory at symbol address
}
```

# 18. ClosePartition / CloseAllPartitions

Description: Close partition handles and free resources.

```csharp
    void ClosePartition(UInt64 PartitionHandle)
    void CloseAllPartitions()
```

Example:

```csharp
    Hvlib.ClosePartition(vm.VmHandle);
    // or close all at once
    Hvlib.CloseAllPartitions();
```
