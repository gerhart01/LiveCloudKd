This is description for Python API wrappers for hvlib.dll library  
Main library file is hvlib.py. Written on Python 3.x
See Example.py for detailed information - [Example.py](https://github.com/gerhart01/LiveCloudKd/blob/master/LiveCloudKdPy/Example.py)  

Also there is good example of Python API wrappers usage is Hyper-V memory manager plugin for [volatility plugin](https://github.com/gerhart01/Hyper-V-Tools/tree/main/Plugin_for_volatility)

- [1. GetPreferredSettings](#1-getpreferredsettings)
- [2. EnumPartitions](#2-enumpartitions)
- [3. GetData](#3-getdata)
- [4. SelectPartition](#4-selectpartition)
- [5. ReadPhysicalMemoryBlock](#5-readphysicalmemoryblock)
- [6. ReadVirtualMemoryBlock](#6-readvirtualmemoryblock)
- [7. WritePhysicalMemoryBlock](#7-writephysicalmemoryblock)
- [8. WriteVirtualMemoryBlock](#8-writevirtualmemoryblock)

Library installation:

Copy hvlib.py, hvlib.dll and hvmm.sys from [latest](https://github.com/gerhart01/LiveCloudKd/releases) LiveCloudKd distributive to <python_dir>\Lib\site-packages (f.e. C:\Python312x64\Lib\site-packages). 
If you use some python virtual environment plugins, you need to copy files inside it.
For example to "directory_name\venv\Lib\site-packages" for virtualenv

# 1. GetPreferredSettings 
   
Description: Get default plugin configuration.   

```python
CfgParameters GetPreferredSettings()
```   

Current configuration options:  

**"ReadMethod"** - memory reading method for driver. Class ReadMemoryMethod  
**"WriteMethod"** - memory writing method for driver. Class WriteMemoryMethod  
**"PauseMethod"** - method of suspend VM. Class SuspendResumeMethod  
**"LogLevel"** - log level. Integer [0..4]  
**"ForceFreezeCPU"** - boolean. FreezeCPU using virtual VM registers when suspend VM  
**"PausePartition"** - boolean. VM was suspended when SdkSelectPartition will be executed  
**"ReloadDriver"** - boolean. Reload driver when starting plugin. Need in some cases when service is not deleted correctly  
**"SimpleMemory"** - boolean. Uses for Linux VM memory scanning.  

Example:

```python
vm_ops = objHvlib.GetPreferredSettings()
vm_ops.LogLevel = 1
```

result type: CfgParameters object

# 2. EnumPartitions

Description: Hyper-V active partitions enumerations in root OS

```python
EnumPartitions(vm_ops)
```

parameters:  

* **vm_ops** - CfgParameters structure, where you can modify one or more parameter

 Example:
```python
objHvlib = hvlib("")

if objHvlib == None:
    exit

# Set logging level
vm_ops = objHvlib.vm_ops
vm_ops.LogLevel = 1

bResult = objHvlib.EnumPartitions(vm_ops)
```

result type: boolean


# 3. GetData 

Description: Get specific data from partition

```python
integer GetData(HvddInformationClass param1)
```

parameters:  

* **PartitionHandle** - handle of partition
* **HvddInformationClass** - PartitionClass

available values:

```python
HvddPartitionFriendlyName = 1
HvddPartitionId = 2			
HvddVmtypeString = 3
HvddMmMaximumPhysicalPage = 6
HvddKernelBase = 11	
HvddVmGuidString = 20
```

Example:

```python
KernelBase = objHvlib.GetData(vm_handle, HvddInformationClass.HvddKernelBase)
```

result type: integer

# 4. SelectPartition 

Description: select one of partitions, which were gotten from EnumPartitions

```python 
SelectPartition(CurrentPartition)
```

Parameters:  

* **CurrentPartition** - partition handle

Example:

```python 
vm_handle = objHvlib.SelectPartition(vm_id)
```

result type: handle of virtual machine or 0, if active virtual machines are not running on host

# 5. ReadPhysicalMemoryBlock

Description: Read memory block from specified physical address 
Parameters:   
* **vm_handle** - handle of virtual machine
* **address** - physical memory address 
* **block_size** - size of memory block for reading

```python
vm_handle = objHvlib.SelectPartition(vm_id)
ReadPhysicalMemoryBlock(vm_handle, phys_address, buffer1) 
```
  
Example:

```python 
buffer1 = objHvlib.ReadPhysicalMemoryBlock(vm_handle, phys_address, page_size)
```

result type: boolean

# 6. ReadVirtualMemoryBlock

Description: Read memory block from specified virtual address  

Parameters:    
* **vm_handle** - handle of virtual machine
* **address** - virtual memory address 
* **block_size** - size of memory block for reading
  
Example:

```python
KernelBase = objHvlib.GetData(vm_handle, HvddInformationClass.HvddKernelBase)
objHvlib.PrintHex(KernelBase)
buffer2 = objHvlib.ReadVirtualMemoryBlock(vm_handle, KernelBase, page_size)
```

result type: boolean

# 7. WritePhysicalMemoryBlock

Description: Write data to specified physical address in virtual machine  

```python   
WriteVirtualMemoryBlock(vm_handle, phys_address, buffer2)  
```

Parameters:    
* **vm_handle** - handle of virtual machine
* **address** - physical memory address 
* **block_size** - size of memory block for reading
  
Example:

```python 
bResult = objHvlib.WritePhysicalMemoryBlock(vm_handle,phys_address, buffer1)
```

result type: boolean

# 8. WriteVirtualMemoryBlock

Description: Write data to specified virtual address in virtual machine  

```python
WriteVirtualMemoryBlock(vm_handle, KernelBase, buffer2) 
```

Parameters:      
* **vm_handle** - handle of virtual machine  
* **address** - virtual memory address   
* **block_size** - size of memory block for reading  
  
Example:  

```python   
bResult = objHvlib.WriteVirtualMemoryBlock(vm_handle, KernelBase, buffer2)  
```

result type: boolean