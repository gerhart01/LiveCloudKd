#
#  hvlib.py examples
#  GPL3 License
#  version 1.2.0
#

import sys
from hvlib import *

def get_vm_id():
    print("Please select the ID of the virtual machine")
    vm_id = int(input('').split(" ")[0])
    return vm_id

objHvlib = hvlib("")

if objHvlib is None:
    sys.exit(1)

# Set logging level
vm_ops = objHvlib.GetPreferredSettings()

if not vm_ops:
    print("GetPreferredSettings failed")
    sys.exit(1)

vm_ops.LogLevel = 1

objHvlib.EnumPartitions(vm_ops)

if objHvlib.PartitionCount == 0:
    print("No active partitions found")
    sys.exit(1)

vm_id = get_vm_id()

page_size = 0x1000
phys_address = 0x10000

vm_handle = objHvlib.SelectPartition(vm_id)

if vm_handle == 0:
    print("SelectPartition failed")
    sys.exit(1)

# Read physical memory
buffer1 = objHvlib.ReadPhysicalMemoryBlock(vm_handle, phys_address, page_size)

if buffer1 == 0:
    print("ReadPhysicalMemoryBlock failed")
    sys.exit(1)

objHvlib.PrintHex(buffer1)

# Get kernel base address and read virtual memory
KernelBase = objHvlib.GetData(vm_handle, HvmmInformationClass.InfoKernelBase)
print("KernelBase:")
objHvlib.PrintHex(KernelBase)

buffer2 = objHvlib.ReadVirtualMemoryBlock(vm_handle, KernelBase, page_size)

if buffer2 == 0:
    print("ReadVirtualMemoryBlock failed")
    sys.exit(1)

objHvlib.PrintHex(buffer2)

# Write back (demonstrates write operations)
bResult = objHvlib.WriteVirtualMemoryBlock(vm_handle, KernelBase, buffer2)
print("WriteVirtualMemoryBlock:", bResult)

bResult = objHvlib.WritePhysicalMemoryBlock(vm_handle, phys_address, buffer1)
print("WritePhysicalMemoryBlock:", bResult)

# Get additional partition info
VmName = objHvlib.GetData(vm_handle, HvmmInformationClass.InfoPartitionFriendlyName)
print("VM Name:", VmName)

PartitionId = objHvlib.GetData(vm_handle, HvmmInformationClass.InfoPartitionId)
print("Partition ID:", PartitionId)

VmGuid = objHvlib.GetData(vm_handle, HvmmInformationClass.InfoVmGuidString)
print("VM GUID:", VmGuid)

MmMaxPhysPage = objHvlib.GetData(vm_handle, HvmmInformationClass.InfoMmMaximumPhysicalPage)
print("MmMaximumPhysicalPage:", hex(MmMaxPhysPage))
