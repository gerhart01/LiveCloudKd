#
#  hvlib.py examples
#  GPL3 License
#  version 1.0.0
#

from hvlib import *

def get_vm_id():
    print("Please select the ID of the virtual machine")
    vm_id = int(input('').split(" ")[0])
    return vm_id

objHvlib = hvlib("")

if objHvlib == None:
    exit

# Set logging level
vm_ops = objHvlib.GetPreferredSettings()
vm_ops.LogLevel = 1

bResult = objHvlib.EnumPartitions(vm_ops)

if bResult == False:
    exit

vm_id = get_vm_id()

page_size = 0x1000
phys_address = 0x10000

vm_handle = objHvlib.SelectPartition(vm_id)
buffer1 = objHvlib.ReadPhysicalMemoryBlock(vm_handle, phys_address, page_size)
KernelBase = objHvlib.GetData(vm_handle, HvddInformationClass.HvddKernelBase)
objHvlib.PrintHex(KernelBase)
buffer2 = objHvlib.ReadVirtualMemoryBlock(vm_handle, KernelBase, page_size)

bResult = objHvlib.WriteVirtualMemoryBlock(vm_handle, KernelBase, buffer2)
bResult = objHvlib.WritePhysicalMemoryBlock(vm_handle,phys_address, buffer1)