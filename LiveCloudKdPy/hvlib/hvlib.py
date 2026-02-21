#
#  hvlib.dll (Hyper-V memory manager library) python wrapper developed by Arthur Khudyaev (@gerhart_x)
#  GPL3 License
#  version 1.2.0
#
#  Patched: lazy argtypes initialization to work around Python 3.14 ctypes
#  access violation when all function signatures are set before first DLL call.
#

import ctypes
import os
from enum import IntEnum
import sys
import msvcrt
import atexit
import binascii

# problem with python 3 wprintf additional spaces between each letter
msvcrt.setmode(sys.stdout.fileno(), os.O_TEXT)

#
# https://gist.github.com/christoph2/9c390e5c094796903097 - python 3 enums fix
#

class StructureWithEnums(ctypes.Structure):
    """Add missing enum feature to ctypes Structures.
    """
    _map = {}

    def __getattribute__(self, name):
        _map = ctypes.Structure.__getattribute__(self, '_map')
        value = ctypes.Structure.__getattribute__(self, name)
        if name in _map:
            EnumClass = _map[name]
            if isinstance(value, ctypes.Array):
                return [EnumClass(x) for x in value]
            else:
                return EnumClass(value)
        else:
            return value

    def __str__(self):
        result = []
        result.append("struct {0} {{".format(self.__class__.__name__))
        for field in self._fields_:
            attr, attrType = field
            if attr in self._map:
                attrType = self._map[attr]
            value = getattr(self, attr)
            result.append("    {0} [{1}] = {2!r};".format(attr, attrType.__name__, value))
        result.append("};")
        return '\n'.join(result)


class CtypesEnum(IntEnum):
    """A ctypes-compatible IntEnum superclass."""

    @classmethod
    def from_param(cls, obj):
        return int(obj)


class ReadMemoryMethod(IntEnum):
    ReadInterfaceUnsupported = 0
    ReadInterfaceHvmmDrvInternal = 1
    ReadInterfaceWinHv = 2
    ReadInterfaceHvmmLocal = 3
    ReadInterfaceMax = 4

class WriteMemoryMethod(IntEnum):
    WriteInterfaceUnsupported = 0
    WriteInterfaceHvmmDrvInternal = 1
    WriteInterfaceWinHv = 2
    WriteInterfaceHvmmLocal = 3
    WriteInterfaceMax = 4

class SuspendResumeMethod(IntEnum):
    SuspendResumeUnsupported = 0
    SuspendResumePowershell = 1
    SuspendResumeWriteSpecRegister = 2


class HvmmInformationClass(IntEnum):
    InfoPartitionFriendlyName = 1
    InfoPartitionId = 2
    InfoVmtypeString = 3
    InfoMmMaximumPhysicalPage = 6
    InfoKernelBase = 11
    InfoVmGuidString = 20

class VmStateAction(IntEnum):
    SuspendVm = 0
    ResumeVm = 1


class CfgParameters(StructureWithEnums):
    _fields_ = [
        ("ReadMethod", ctypes.c_int),
        ("WriteMethod", ctypes.c_int),
        ("PauseMethod", ctypes.c_int),
        ("LogLevel", ctypes.c_int64),
        ("ForceFreezeCPU", ctypes.c_bool),
        ("PausePartition", ctypes.c_bool),
        ("ExdiConsoleHandle", ctypes.c_uint64),
        ("ReloadDriver", ctypes.c_bool),
        ("PFInjection", ctypes.c_bool),
        ("NestedScan", ctypes.c_bool),
        ("UseDebugApiStopProcess", ctypes.c_bool),
        ("SimpleMemory", ctypes.c_bool),
    ]
    _map = {
        "ReadMethod": ReadMemoryMethod,
        "WriteMethod": WriteMemoryMethod,
        "PauseMethod": SuspendResumeMethod,
        "LogLevel": ctypes.c_int64,
        "ForceFreezeCPU": ctypes.c_bool,
        "PausePartition": ctypes.c_bool,
        "ExdiConsoleHandle": ctypes.POINTER(ctypes.c_uint64),
        "ReloadDriver": ctypes.c_bool,
        "PFInjection": ctypes.c_bool,
        "NestedScan": ctypes.c_bool,
        "UseDebugApiStopProcess": ctypes.c_bool,
        "SimpleMemory": ctypes.c_bool,
    }


class hvlib:

    def __init__(self, dll_path):

        py_dir = os.path.dirname(os.path.abspath(__file__))

        if dll_path == "":
            dll_path = os.path.join(py_dir, "hvlib.dll")
            drv_path = os.path.join(py_dir, "hvmm.sys")
            py_module_path = os.path.join(py_dir, "hvlib.py")
        else:
            modules_dir = os.path.dirname(dll_path)
            drv_path = modules_dir + "\\hvmm.sys"
            py_module_path = modules_dir + "\\hvlib.py"

        if not os.path.exists(dll_path):
            raise FileNotFoundError(f"DLL file is not found: {dll_path}")

        if not os.path.exists(drv_path):
            raise FileNotFoundError(f"Driver file is not found: {drv_path}")

        if not os.path.exists(py_module_path):
            raise FileNotFoundError(f"Hvlib.py file is not found: {py_module_path}")

        print("dll_path: ", dll_path)
        print("drv_path: ", drv_path)
        print("py_module_path: ", py_module_path)

        self.hvlib = ctypes.CDLL(dll_path)
        self.PartitionArray = []
        self.CurrentPartition = 0
        self.PartitionCount = 0
        self.ArrayOfNames = []

        # ----------------------------------------------------------------
        # Phase 1: Set up ONLY the two functions needed for initialization.
        #
        # WORKAROUND for Python >= 3.14 ctypes bug:
        # Setting argtypes for ALL DLL functions before the first call
        # triggers a memory corruption / access violation inside ctypes.
        # Splitting into two phases avoids the issue.
        # ----------------------------------------------------------------

        # BOOLEAN SdkGetDefaultConfig(_Inout_ PVM_OPERATIONS_CONFIG VmOperationsConfig);
        self.hvlib.SdkGetDefaultConfig.restype = ctypes.c_bool
        self.hvlib.SdkGetDefaultConfig.argtypes = [ctypes.POINTER(CfgParameters)]

        # PULONG64 SdkEnumPartitions(_Inout_ PULONG64 PartitionTableCount, _In_ PVM_OPERATIONS_CONFIG VmOpsConfig);
        self.hvlib.SdkEnumPartitions.restype = ctypes.POINTER(ctypes.c_uint64)
        self.hvlib.SdkEnumPartitions.argtypes = [ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(CfgParameters)]

        # First DLL call — loads hvmm.sys driver
        atexit.register(self.cleanup)
        vm_ops = CfgParameters(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        self.hvlib.SdkGetDefaultConfig(ctypes.pointer(vm_ops))
        self.vm_ops = vm_ops

        # ----------------------------------------------------------------
        # Phase 2: Now that the driver is loaded, set up remaining sigs.
        # ----------------------------------------------------------------
        self._setup_remaining_argtypes()

    def _setup_remaining_argtypes(self):
        """Configure argtypes for all DLL functions except the two set in __init__ phase 1."""

        # BOOLEAN SdkGetData(_In_ ULONG64 PartitionIntHandle, _In_ HVMM_INFORMATION_CLASS HvmmInformationClass, _Inout_ PVOID HvddInformation);
        self.hvlib.SdkGetData.restype = ctypes.c_bool
        self.hvlib.SdkGetData.argtypes = [ctypes.c_uint64, ctypes.c_int32, ctypes.c_void_p]

        # BOOLEAN SdkSelectPartition(_In_ ULONG64 PartitionHandle);
        self.hvlib.SdkSelectPartition.restype = ctypes.c_bool
        self.hvlib.SdkSelectPartition.argtypes = [ctypes.c_uint64]

        # BOOLEAN SdkCloseAllPartitions(_In_ ULONG64 PartitionHandle);
        self.hvlib.SdkCloseAllPartitions.restype = ctypes.c_bool
        self.hvlib.SdkCloseAllPartitions.argtypes = []

        # BOOLEAN SdkReadPhysicalMemory(_In_ ULONG64 PartitionHandle, _In_ UINT64 StartPosition, _In_ UINT64 ReadByteCount, _Inout_ PVOID ClientBuffer, _In_ READ_MEMORY_METHOD Method);
        self.hvlib.SdkReadPhysicalMemory.restype = ctypes.c_bool
        self.hvlib.SdkReadPhysicalMemory.argtypes = [ctypes.c_uint64, ctypes.c_uint64, ctypes.c_uint64,
                                                                    ctypes.c_void_p, ctypes.c_uint32]

        # BOOLEAN SdkWritePhysicalMemory(_In_ ULONG64 PartitionEntryHandle, _In_ UINT64 StartPosition, _In_ UINT64 WriteBytesCount, _In_ PVOID ClientBuffer, _In_ WRITE_MEMORY_METHOD Method);
        self.hvlib.SdkWritePhysicalMemory.restype = ctypes.c_bool
        self.hvlib.SdkWritePhysicalMemory.argtypes = [ctypes.c_uint64, ctypes.c_uint64, ctypes.c_uint64,
                                                                     ctypes.c_void_p, ctypes.c_uint32]

        # BOOLEAN SdkReadVirtualMemory(_In_ ULONG64 PartitionHandle, _In_ ULONG64 Va, _Out_ PVOID Buffer, _In_ ULONG Size);
        self.hvlib.SdkReadVirtualMemory.restype = ctypes.c_bool
        self.hvlib.SdkReadVirtualMemory.argtypes = [ctypes.c_uint64, ctypes.c_uint64, ctypes.c_void_p,
                                                                    ctypes.c_uint32]

        # BOOLEAN SdkWriteVirtualMemory(_In_ ULONG64 PartitionHandle, _In_ ULONG64 Va, _In_ PVOID Buffer, _In_ ULONG Size);
        self.hvlib.SdkWriteVirtualMemory.restype = ctypes.c_bool
        self.hvlib.SdkWriteVirtualMemory.argtypes = [ctypes.c_uint64, ctypes.c_uint64, ctypes.c_void_p,
                                                                     ctypes.c_uint32]

        # BOOLEAN SdkControlVmState(_In_ ULONG64 PartitionHandle, _In_ VM_STATE_ACTION Action, _In_ SUSPEND_RESUME_METHOD ActionMethod, _In_ BOOLEAN ManageWorkerProcess);
        self.hvlib.SdkControlVmState.restype = ctypes.c_bool
        self.hvlib.SdkControlVmState.argtypes = [ctypes.c_uint64, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_bool]

    def PrintHex(self, buffer):

        if type(buffer) == int:
            print('0x'+'%08x' % buffer)
            return

        hex = str(binascii.hexlify(buffer), 'ascii')
        formatted_hex = ':'.join(hex[i:i+2] for i in range(0, len(hex), 2))
        print(formatted_hex)

    def GetPreferredSettings(self):

        VmOps = CfgParameters(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        bResult = self.hvlib.SdkGetDefaultConfig(ctypes.pointer(VmOps))
        if not bResult:
            print("Error in SdkGetDefaultConfig")
            return False
        return VmOps

    def EnumPartitions(self, vm_ops_param: CfgParameters):

        # vm_ops_param = GetPreferredSettings()

        PartitionCount = ctypes.c_uint64(0)

        Partitions = self.hvlib.SdkEnumPartitions(ctypes.pointer(PartitionCount),
                                                                 ctypes.pointer(vm_ops_param))
        PartitionArray = []
        PartitionArray = ctypes.cast(Partitions, ctypes.POINTER(ctypes.c_uint64 * PartitionCount.value)).contents
        FriendlyNameP = ctypes.c_wchar_p("N")
        PartitionId = ctypes.c_uint64(0)
        ArrayOfNames = []
        self.vm_ops = vm_ops_param
        self.PartitionCount = PartitionCount.value

        for x in range(0, PartitionCount.value):
            self.hvlib.SdkGetData(PartitionArray[x], HvmmInformationClass.InfoPartitionFriendlyName,
                                           ctypes.pointer(FriendlyNameP))
            ArrayOfNames.append(FriendlyNameP.value)
            self.hvlib.SdkGetData(PartitionArray[x], HvmmInformationClass.InfoPartitionId,
                                           ctypes.pointer(PartitionId))

            str_vm_info = " [" + str(x) + "] " + str(FriendlyNameP.value) + ". (PartitionId = " + str(PartitionId.value) + ")"
            # print("[",x,"]", FriendlyNameP.value, ". (PartitionId = ", c,")")
            print(str_vm_info)

            print("Active partitions count:", PartitionCount.value)

        self.PartitionArray = PartitionArray
        self.ArrayOfNames = ArrayOfNames

    def SelectPartition(self, vm_id):

        if vm_id >= self.PartitionCount:
            print("vm_id bigger, then count of partitions")

        CurrentPartition = self.PartitionArray[vm_id]
        print("You select", self.ArrayOfNames[vm_id])
        result = self.hvlib.SdkSelectPartition(CurrentPartition)

        if not result:
            print("SdkSelectPartition failed")
            return 0

        self.CurrentPartition = CurrentPartition
        return CurrentPartition

    def GetData(self, vm_handle, info_class: HvmmInformationClass):

        wchar_var = ctypes.c_wchar_p("N")
        int_var = ctypes.c_uint64(0)

        if (info_class == HvmmInformationClass.InfoPartitionFriendlyName) or (
                info_class == HvmmInformationClass.InfoVmtypeString) or (
                info_class == HvmmInformationClass.InfoVmGuidString):
            self.hvlib.SdkGetData(vm_handle, info_class,
                                           ctypes.pointer(wchar_var))
            return wchar_var.value
        else:
            self.hvlib.SdkGetData(vm_handle, info_class,
                                           ctypes.pointer(int_var))
            return int_var.value

    def ReadPhysicalMemoryBlock(self, vm_handle, address, block_size):

        buffer = ctypes.create_string_buffer(block_size)
        bResult = self.hvlib.SdkReadPhysicalMemory(vm_handle, address, block_size, buffer,
                                                                  self.vm_ops.ReadMethod)
        if not bResult:
            print(f"ReadPhysicalMemoryBlock error: vm_handle=0x{vm_handle:X}, address=0x{address:X}, block_size=0x{block_size:X}")
            return 0
        # print in hex
        # self.PrintHex(buffer)
        return buffer

    def ReadVirtualMemoryBlock(self, vm_handle, address, block_size):

        buffer = ctypes.create_string_buffer(block_size)
        bResult = self.hvlib.SdkReadVirtualMemory(vm_handle, address, buffer, block_size)
        if not bResult:
            print(f"ReadVirtualMemoryBlock error: vm_handle=0x{vm_handle:X}, address=0x{address:X}, block_size=0x{block_size:X}")
            return 0

        return buffer

    def WritePhysicalMemoryBlock(self, vm_handle, address, buffer):

        block_size = len(buffer)
        bResult = self.hvlib.SdkWritePhysicalMemory(vm_handle, address, block_size, buffer,
                                                                  self.vm_ops.WriteMethod)
        if not bResult:
            print(f"WritePhysicalMemoryBlock error: vm_handle=0x{vm_handle:X}, address=0x{address:X}")
            return False

        return True

    def WriteVirtualMemoryBlock(self, vm_handle, address, buffer):

        block_size = len(buffer)
        bResult = self.hvlib.SdkWriteVirtualMemory(vm_handle, address, buffer, block_size)
        if not bResult:
            print(f"WriteVirtualMemoryBlock error: vm_handle=0x{vm_handle:X}, address=0x{address:X}")
            return False

        return True

    def cleanup(self):
        self.hvlib.SdkCloseAllPartitions()
        print("hvlib.dll unloaded")
