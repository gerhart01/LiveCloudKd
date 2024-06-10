This is description for C# API wrappers for hvlib.dll - Hyper-V memory manager plugin  
Main library file is hvlibdotnet.cs. It must be integrated in your C# project.  

Example of C# API wrappers usages in [Hyper Views](https://github.com/gerhart01/Hyper-V-Tools/tree/main/HyperViews)

- [1. GetPreferredSettings](#1-getpreferredsettings)
- [2. EnumPartitions](#2-enumpartitions)
- [3. GetData and GetData2](#3-getdata-and-getdata2)
- [4. SelectPartition](#4-selectpartition)
- [5. ReadPhysicalMemory](#5-readphysicalmemory)
- [6. ReadVirtualMemory](#6-readvirtualmemory)
- [7. WritePhysicalMemory](#7-writephysicalmemory)
- [8. WriteVirtualMemory](#8-writevirtualmemory)


# 1. GetPreferredSettings 
   
Description: Get default plugin configuration.   

```csharp
    bool bResult = Hvlib.GetPreferredSettings(ref cfg);
```   

Current configuration options:  

**"ReadMethod"**        - memory reading method for driver. Class ReadMemoryMethod  
**"WriteMethod"**       - memory writing method for driver. Class WriteMemoryMethod  
**"PauseMethod"**       - method of suspend VM. Class SuspendResumeMethod  
**"LogLevel"**          - log level. Integer [0..4]
**"ForceFreezeCPU"**    - boolean. FreezeCPU using virtual VM registers when suspend VM  
**"PausePartition"**    - boolean. VM was suspended when SdkSelectPartition will be executed  
**"ReloadDriver"**      - boolean. Reload driver when starting plugin. Need in some cases when service is not deleted correctly  
**"SimpleMemory"**      - boolean. Uses for Linux VM memory scanning.  

Example:

```csharp
    Hvlib.VM_OPERATIONS_CONFIG cfg = new Hvlib.VM_OPERATIONS_CONFIG();
    bool bResult = Hvlib.GetPreferredSettings(ref cfg);
```

result: CfgParameters object

# 2. EnumPartitions

Description: Hyper-V active partitions enumerations

```csharp
    IntPtr SdkEnumPartitions(ref UInt64 PartitionCount, ref VM_OPERATIONS_CONFIG cfg);
```

parameters:  

* **PartitionCount** - pointer to variable, which will be contains count of partitions
* **cfg** - CfgParameters structure, where you can modify one or more parameter

 Example:
```csharp
    UInt64 PartitionCount = 0;
    Hvlib.VM_OPERATIONS_CONFIG cfg = new Hvlib.VM_OPERATIONS_CONFIG();
    Int64[] arPartition;
    IntPtr Partitions = SdkEnumPartitions(ref PartitionCount, ref cfg);

    if (Partitions == null)
    {
        return null;
    }
```

result type: IntPtr


# 3. GetData and GetData2

Description: get specific data from partition. GetData and GetData2 have similar functionality, but with different parameters

```csharp
    bool GetData(UInt64 VmHandle, HVDD_INFORMATION_CLASS InformationClass, out UIntPtr HvddInformation)
    UInt64 GetData2(UInt64 PartitionHandle, HVDD_INFORMATION_CLASS HvddInformationClass)
```

parameters:  

* **PartitionHandle** - handle of partition
* **HvddInformationClass** - class of partition
* **HvddInformation** - pointer to variable with result

available values:

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
    HvddNtBuildNumber
```

Example:

```csharp
    VmListBox lbItem = new VmListBox();
    IntPtr VmName = (IntPtr)GetData2((UInt64)arPartition[i], HVDD_INFORMATION_CLASS.HvddPartitionFriendlyName);
    string? VmNameStr = Marshal.PtrToStringUni(VmName);

    IntPtr VmGuid = (IntPtr)GetData2((UInt64)arPartition[i], HVDD_INFORMATION_CLASS.HvddVmGuidString);
    string? VmGuidStr = Marshal.PtrToStringUni(VmGuid);

    IntPtr VmType = (IntPtr)GetData2((UInt64)arPartition[i], HVDD_INFORMATION_CLASS.HvddVmtypeString);
    string? VmTypeStr = Marshal.PtrToStringUni(VmType);

    UInt64 PartitionId = GetData2((UInt64)arPartition[i], HVDD_INFORMATION_CLASS.HvddPartitionId);
```

Example:

```csharp
    VmListBox lbItem = new VmListBox();
    string VmName;
    bool bResult = GetData2((UInt64)arPartition[i], HVDD_INFORMATION_CLASS.HvddPartitionFriendlyName, ref VmName);

    string VmGuid;
    bool bResult = GetData2((UInt64)arPartition[i], HVDD_INFORMATION_CLASS.HvddVmGuidString, ref VmGuid);

    string VmType;
    bool bResult = GetData2((UInt64)arPartition[i], HVDD_INFORMATION_CLASS.HvddVmtypeString, ref VmType);

    UInt64 PartitionId;
    bool bResult = GetData2(arPartition[i], HVDD_INFORMATION_CLASS.HvddPartitionId, PartitionId);
```

result type: UInt64 from GetData2 and boolean from GetData2 

# 4. SelectPartition 

Description: select one of partitions, which were gotten from EnumPartitions

```csharp 
    bool SelectPartition(UInt64 CurrentPartition)
```

Parameters:  

* **CurrentPartition** - partition handle

Example:

```csharp 
bResult = SelectPartition(Hvlib.VmHandle);

if (!bResult)
    return false;
```

result - BOOLEAN. True, if operation is success.

# 5. ReadPhysicalMemory

Description: Read memory block from specified physical address 

Parameters:   
* **PartitionHandle** - handle of virtual machine
* **StartPosition** - address of physical memory 
* **ReadByteCount** - size of memory block for reading
* **ClientBuffer** - size of memory block for reading

```csharp
    bool ReadPhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, IntPtr ClientBuffer)
```
  
Example:

```csharp 
    uint bufferLength = largePage ? 0x1000 * 512u : 0x1000;
    byte[] pageBuffer = new byte[bufferLength];

    pfn = pfn << 12;

    bool bResult = Hvlib.SdkReadPhysicalMemory(VmHandle, pfn, bufferLength, pageBuffer, Hvlib.READ_MEMORY_METHOD.ReadInterfaceHvmmDrvInternal)
```

result: boolean

# 6. ReadVirtualMemory

Description: Read memory block from specified virtual address  

Parameters:    
* **PartitionHandle** - handle of virtual machine
* **StartPosition**   - virtual memory address 
* **ReadByteCount**   - size of memory block for reading
* **ClientBuffer**    - buffer for data reading

```csharp
    bool ReadVirtualMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, IntPtr ClientBuffer)
```
  
Example:

```csharp
    uint bufferLength = largePage ? 0x1000 * 512u : 0x1000;
    byte[] pageBuffer = new byte[bufferLength];

    Address = 0xFFFFF78000000000;

    bool bResult = Hvlib.ReadVirtualMemory(VmHandle, Address, bufferLength, pageBuffer)
```

result: boolean

# 7. WritePhysicalMemory

Description: Write data to specified physical address 

```csharp   
    bool WritePhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 WriteByteCount, IntPtr ClientBuffer)
```

Parameters:    
* **PartitionHandle** - handle of virtual machine
* **StartPosition**   - physical memory address 
* **WriteByteCount**  - size of memory block for reading
* **ClientBuffer**    - buffer with data to write in memory
  
Example:

```csharp 

    uint bufferLength = sizeof(ulong);
    ulong pageBuffer = 0x12345678;

    ulong pfn = 0x800000;

    bool bResult = WritePhysicalMemory(PartitionHandle, pfn, bufferLength, ref pageBuffer);
```

result: boolean

# 8. WriteVirtualMemory

Description: Write data to specified virtual address in virtual machine  

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

    bool bResult = WriteVirtualMemory(PartitionHandle, Address, bufferLength, ref Value);

```

result: boolean