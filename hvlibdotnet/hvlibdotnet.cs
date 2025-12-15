using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;

namespace Hvlibdotnet
{
    public class VmListBox
    {
        public UInt64 VmHandle { get; set; }
        public string? VMName { get; set; }
    }
    public partial class Hvlib
    {
        public Hvlib()
        {
        }
        public enum HVDD_INFORMATION_CLASS
        {
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
            //Special set values
            HvddSetMemoryBlock,
            HvddEnlVmcsPointer
        }

        public enum READ_MEMORY_METHOD
        {
            ReadInterfaceUnsupported,
            ReadInterfaceHvmmDrvInternal,
            ReadInterfaceWinHv,
            ReadInterfaceHvmmMax
        }
        public enum WRITE_MEMORY_METHOD
        {
            WriteInterfaceUnsupported,
            WriteInterfaceHvmmDrvInternal,
            WriteInterfaceWinHv,
            WriteInterfaceHvmmMax
        }

        public enum SUSPEND_RESUME_METHOD
        {
            SuspendResumeUnsupported,
            SuspendResumePowershell,
            SuspendResumeWriteSpecRegister
        }

        public enum VM_STATE_ACTION
        {
            SuspendVm = 0,
            ResumeVm = 1
        }

        public enum GET_CR3_TYPE
        {
            Cr3Process = 0,
            Cr3Kernel = 1,
            Cr3SecureKenerl = 2,
            Cr3Hypervisor = 3
        }

        // NEW: Memory access type for address translation
        public enum MEMORY_ACCESS_TYPE
        {
            MmPhysicalMemory = 0,
            MmVirtualMemory = 1,
            MmAccessRtCore64 = 2
        }

        // NEW: Machine type enumeration
        public enum MACHINE_TYPE
        {
            MACHINE_UNKNOWN = 0,
            MACHINE_X86 = 1,
            MACHINE_AMD64 = 2,
            MACHINE_UNSUPPORTED = 3
        }

        // NEW: Virtual Trust Level enumeration
        public enum VTL_LEVEL
        {
            Vtl0 = 0,
            Vtl1 = 1,
            BadVtl = 2
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct VM_OPERATIONS_CONFIG
        {
            public READ_MEMORY_METHOD ReadMethod;
            public WRITE_MEMORY_METHOD WriteMethod;
            public SUSPEND_RESUME_METHOD SuspendMethod;
            public UInt64 LogLevel;
            [MarshalAs(UnmanagedType.I1)] public bool ForceFreezeCPU;
            [MarshalAs(UnmanagedType.I1)] public bool PausePartition;
            public IntPtr ExdiConsoleHandle;
            [MarshalAs(UnmanagedType.I1)] public bool ReloadDriver;
            [MarshalAs(UnmanagedType.I1)] public bool PFInjection;
            [MarshalAs(UnmanagedType.I1)] public bool NestedScan;
            [MarshalAs(UnmanagedType.I1)] public bool UseDebugApiStopProcess;
            [MarshalAs(UnmanagedType.I1)] public bool SimpleMemory;
            [MarshalAs(UnmanagedType.I1)] public bool ReplaceDecypheredKDBG;
            [MarshalAs(UnmanagedType.I1)] public bool FullCrashDumpEmulation;
            [MarshalAs(UnmanagedType.I1)] public bool EnumGuestOsBuild;
            [MarshalAs(UnmanagedType.I1)] public bool VSMScan;
        }

        public struct HV_X64_HYPERCALL_INPUT
        {
            public UInt32 CallCode;
            public UInt32 CountOfElements;
            public UInt32 RepStartIndex;
            [MarshalAs(UnmanagedType.I1)] public bool IsFast;
            [MarshalAs(UnmanagedType.I1)] public bool IsNested;
        }

        public struct HVCALL_DATA
        {
            public HV_X64_HYPERCALL_INPUT HvCallId;
            public IntPtr InputVA;
            public UInt64 InputBufferSize;
            public IntPtr OutputVA;
            public UInt64 OutputBufferSize;
            public UInt64 result;
        }

        // NEW: HV_REGISTER_VALUE structure for register operations
        [StructLayout(LayoutKind.Explicit)]
        public struct HV_REGISTER_VALUE
        {
            [FieldOffset(0)] public UInt64 Reg64;
            [FieldOffset(0)] public UInt32 Reg32;
            [FieldOffset(0)] public UInt16 Reg16;
            [FieldOffset(0)] public byte Reg8;

            // For 128-bit values (XMM registers, etc.)
            [FieldOffset(0)] public UInt64 Low64;
            [FieldOffset(8)] public UInt64 High64;
        }

        // ==============================================================================
        // DLL Import Declarations - Existing Functions
        // ==============================================================================

        [DllImport("hvlib.dll")]
        private static extern bool SdkGetDefaultConfig(ref VM_OPERATIONS_CONFIG cfg);

        [DllImport("hvlib.dll")]
        private static extern IntPtr SdkEnumPartitions(ref UInt64 PartitionCount, ref VM_OPERATIONS_CONFIG cfg);

        [DllImport("hvlib.dll")]
        private static extern void SdkCloseAllPartitions();

        [DllImport("hvlib.dll")]
        private static extern void SdkClosePartition(UInt64 PartitionHandle);

        [DllImport("hvlib.dll")]
        private static extern bool SdkSelectPartition(UInt64 PartitionHandle);

        [DllImport("hvlib.dll")]
        private static extern bool SdkGetData(UInt64 PartitionHandle, HVDD_INFORMATION_CLASS HvddInformationClass, out UIntPtr HvddInformation);

        [DllImport("hvlib.dll")]
        private static extern bool SdkReadPhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, IntPtr ClientBuffer, READ_MEMORY_METHOD Method);

        [DllImport("Hvlib.dll", EntryPoint = "SdkReadPhysicalMemory")]
        public static extern bool SdkReadPhysicalMemoryULong(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, ulong[] ClientBuffer, READ_MEMORY_METHOD Method);

        [DllImport("Hvlib.dll", EntryPoint = "SdkReadPhysicalMemory")]
        public static extern bool SdkReadPhysicalMemoryByte(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, byte[] ClientBuffer, READ_MEMORY_METHOD Method);

        [DllImport("hvlib.dll")]
        private static extern bool SdkWritePhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 WriteByteCount, IntPtr ClientBuffer, WRITE_MEMORY_METHOD Method);

        [DllImport("hvlib.dll")]
        private static extern bool SdkReadVirtualMemory(UInt64 PartitionHandle, UInt64 StartPosition, IntPtr ClientBuffer, UInt64 ReadByteCount);

        [DllImport("hvlib.dll")]
        private static extern bool SdkWriteVirtualMemory(UInt64 PartitionHandle, UInt64 StartPosition, IntPtr ClientBuffer, UInt64 WriteByteCount);

        [DllImport("hvlib.dll")]
        private static extern UInt64 SdkGetData2(UInt64 PartitionHandle, HVDD_INFORMATION_CLASS HvddInformationClass);

        [DllImport("hvlib.dll")]
        private static extern UInt64 SdkGetCr3FromPid(UInt64 PartitionHandle, UInt64 Pid, GET_CR3_TYPE Type);

        [DllImport("hvlib.dll")]
        private static extern UInt64 SdkInvokeHypercall(UInt32 HvCallId, bool IsFast, UInt32 RepStartIndex, UInt32 CountOfElements, bool IsNested, ref HVCALL_DATA HvCallData);

        // ==============================================================================
        // DLL Import Declarations - NEW FUNCTIONS
        // ==============================================================================

        /// <summary>
        /// Set data for partition
        /// </summary>
        [DllImport("hvlib.dll")]
        private static extern UInt64 SdkSetData(UInt64 PartitionHandle, HVDD_INFORMATION_CLASS HvmmInformationClass, UInt64 InfoInformation);

        /// <summary>
        /// Control VM state (suspend/resume)
        /// </summary>
        [DllImport("hvlib.dll")]
        private static extern bool SdkControlVmState(UInt64 PartitionHandle, VM_STATE_ACTION Action, SUSPEND_RESUME_METHOD ActionMethod, bool ManageWorkerProcess);

        /// <summary>
        /// Get physical address for virtual address
        /// </summary>
        [DllImport("hvlib.dll")]
        private static extern UInt64 SdkGetPhysicalAddress(UInt64 PartitionHandle, UInt64 Va, MEMORY_ACCESS_TYPE MmAccess);

        /// <summary>
        /// Get machine type (architecture)
        /// </summary>
        [DllImport("hvlib.dll")]
        private static extern MACHINE_TYPE SdkGetMachineType(UInt64 PartitionHandle);

        /// <summary>
        /// Get current VTL for virtual address
        /// </summary>
        [DllImport("hvlib.dll")]
        private static extern VTL_LEVEL SdkGetCurrentVtl(UInt64 PartitionHandle, UInt64 Va);

        /// <summary>
        /// Read virtual processor register
        /// </summary>
        [DllImport("hvlib.dll")]
        private static extern bool SdkReadVpRegister(UInt64 PartitionHandle, UInt32 VpIndex, VTL_LEVEL InputVtl, UInt32 RegisterCode, ref HV_REGISTER_VALUE RegisterValue);

        /// <summary>
        /// Write virtual processor register
        /// </summary>
        [DllImport("hvlib.dll")]
        private static extern bool SdkWriteVpRegister(UInt64 PartitionHandle, UInt32 VpIndex, VTL_LEVEL InputVtl, UInt32 RegisterCode, ref HV_REGISTER_VALUE RegisterValue);

        // ==============================================================================
        // Public API - Existing Functions
        // ==============================================================================

        public static bool GetPreferredSettings(ref VM_OPERATIONS_CONFIG cfg)
        {
            bool bResult = SdkGetDefaultConfig(ref cfg);
            return bResult;
        }

        //public static UInt64 GetSdkData2(UInt64 PartitionHandle, HVDD_INFORMATION_CLASS HvddInformationClass)
        //{
        //    return SdkGetData2(PartitionHandle, HvddInformationClass);
        //}

        //public static UInt64 GetSdkData(UInt64 PartitionHandle, HVDD_INFORMATION_CLASS HvddInformationClass, out UIntPtr HvddInformationData)
        //{
        //    return SdkGetData(PartitionHandle, HvddInformationClass, out HvddInformationData);
        //}

        public static void TestHvLib()
        {
            Console.Write("Hvlib is loaded");
        }

        public static UInt64 VmHandle = 0x100000;

        private static VM_OPERATIONS_CONFIG cfg;

        public static List<VmListBox>? EnumPartitions(ref VM_OPERATIONS_CONFIG cfg)
        {
            UInt64 PartitionCount = 0;
            Int64[] arPartition;
            IntPtr Partitions = SdkEnumPartitions(ref PartitionCount, ref cfg);

            if (Partitions == 0)
            {
                return null;
            }

            List<VmListBox> ListObj = new List<VmListBox>();

            if (PartitionCount != 0)
            {
                arPartition = new Int64[PartitionCount];
                // var bytes = new byte[0];
                // public static void Copy(IntPtr source, long[] destination, int startIndex, int length)
                Marshal.Copy(Partitions, arPartition, 0, (int)PartitionCount);
            }
            else
            {
                return null;
            }

            for (ulong i = 0; i < PartitionCount; i += 1)
            {
                VmListBox lbItem = new VmListBox();
                IntPtr VmName = (IntPtr)SdkGetData2((UInt64)arPartition[i], HVDD_INFORMATION_CLASS.HvddPartitionFriendlyName);
                string? VmNameStr = Marshal.PtrToStringUni(VmName);

                IntPtr VmGuid = (IntPtr)SdkGetData2((UInt64)arPartition[i], HVDD_INFORMATION_CLASS.HvddVmGuidString);
                string? VmGuidStr = Marshal.PtrToStringUni(VmGuid);

                IntPtr VmType = (IntPtr)SdkGetData2((UInt64)arPartition[i], HVDD_INFORMATION_CLASS.HvddVmtypeString);
                string? VmTypeStr = Marshal.PtrToStringUni(VmType);

                UInt64 PartitionId = SdkGetData2((UInt64)arPartition[i], HVDD_INFORMATION_CLASS.HvddPartitionId);

                Console.Write(VmNameStr + ", PartitionId = " + PartitionId + ", Guid: " + VmGuidStr + ", Type: " + VmTypeStr + "\n");
                lbItem.VmHandle = (UInt64)arPartition[i];
                lbItem.VMName = VmNameStr;

                ListObj.Add(lbItem);
            }

            return ListObj;
        }


        public static List<VmListBox>? EnumAllPartitions()
        {
            Hvlib.VM_OPERATIONS_CONFIG cfg = new Hvlib.VM_OPERATIONS_CONFIG();

            bool bResult = Hvlib.GetPreferredSettings(ref cfg);
            cfg.ReadMethod = Hvlib.READ_MEMORY_METHOD.ReadInterfaceHvmmDrvInternal;
            cfg.WriteMethod = Hvlib.WRITE_MEMORY_METHOD.WriteInterfaceWinHv;
            List<VmListBox>? res = EnumPartitions(ref cfg);

            if (res != null)
            {
                Hvlib.cfg = cfg;
            }

            return res;
        }

        public static bool SelectPartition(UInt64 PartitionHandle)
        {
            return SdkSelectPartition(PartitionHandle);
        }

        public static bool GetPartitionData(UInt64 PartitionHandle, HVDD_INFORMATION_CLASS HvddInformationClass, out UIntPtr HvddInformation)
        {
            return SdkGetData(PartitionHandle, HvddInformationClass, out HvddInformation);
        }

        public static UInt64 GetPartitionData2(UInt64 PartitionHandle, HVDD_INFORMATION_CLASS HvddInformationClass)
        {
            return SdkGetData2(PartitionHandle, HvddInformationClass);
        }
        public static UInt64 GetCr3FromPid(UInt64 PartitionHandle, UInt64 Pid)
        {
            UIntPtr ProcessId = new UIntPtr(Pid);

            bool bResult = SdkGetData(PartitionHandle, HVDD_INFORMATION_CLASS.HvddGetCr3byPid, out ProcessId);

            UInt64 cr3 = ProcessId.ToUInt64();
            return cr3;
        }

        public static bool ReadPhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, IntPtr ClientBuffer)
        {
            return SdkReadPhysicalMemory(PartitionHandle, StartPosition, ReadByteCount, ClientBuffer, Hvlib.cfg.ReadMethod);
        }

        public static bool ReadPhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, byte[] ClientBuffer)
        {
            return SdkReadPhysicalMemoryByte(PartitionHandle, StartPosition, ReadByteCount, ClientBuffer, Hvlib.cfg.ReadMethod);
        }

        public static bool ReadPhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, ulong[] ClientBuffer)
        {
            return SdkReadPhysicalMemoryULong(PartitionHandle, StartPosition, ReadByteCount, ClientBuffer, Hvlib.cfg.ReadMethod);
        }

        public static bool WritePhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 WriteByteCount, IntPtr ClientBuffer)
        {
            return SdkWritePhysicalMemory(PartitionHandle, StartPosition, WriteByteCount, ClientBuffer, Hvlib.cfg.WriteMethod);
        }

        public static bool ReadVirtualMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, IntPtr ClientBuffer)
        {
            return SdkReadVirtualMemory(PartitionHandle, StartPosition, ClientBuffer, ReadByteCount);
        }

        public static bool WriteVirtualMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 WriteByteCount, IntPtr ClientBuffer)
        {
            return SdkWriteVirtualMemory(PartitionHandle, StartPosition, ClientBuffer, WriteByteCount);
        }

        public static void CloseAllPartitions()
        {
            SdkCloseAllPartitions();
        }

        public static void ClosePartition(UInt64 PartitionHandle)
        {
            SdkClosePartition(PartitionHandle);
        }

        // ==============================================================================
        // Public API - NEW FUNCTIONS
        // ==============================================================================

        /// <summary>
        /// Set data for partition
        /// </summary>
        /// <param name="PartitionHandle">Handle to partition</param>
        /// <param name="InformationClass">Type of information to set</param>
        /// <param name="Information">Information value</param>
        /// <returns>Result value</returns>
        public static UInt64 SetPartitionData(UInt64 PartitionHandle, HVDD_INFORMATION_CLASS InformationClass, UInt64 Information)
        {
            return SdkSetData(PartitionHandle, InformationClass, Information);
        }

        /// <summary>
        /// Suspend virtual machine
        /// </summary>
        /// <param name="PartitionHandle">Handle to partition</param>
        /// <param name="Method">Suspend method (default: PowerShell)</param>
        /// <param name="ManageWorkerProcess">Manage worker process (default: false)</param>
        /// <returns>True if successful</returns>
        public static bool SuspendVm(UInt64 PartitionHandle, SUSPEND_RESUME_METHOD Method = SUSPEND_RESUME_METHOD.SuspendResumePowershell, bool ManageWorkerProcess = false)
        {
            return SdkControlVmState(PartitionHandle, VM_STATE_ACTION.SuspendVm, Method, ManageWorkerProcess);
        }

        /// <summary>
        /// Resume virtual machine
        /// </summary>
        /// <param name="PartitionHandle">Handle to partition</param>
        /// <param name="Method">Resume method (default: PowerShell)</param>
        /// <param name="ManageWorkerProcess">Manage worker process (default: false)</param>
        /// <returns>True if successful</returns>
        public static bool ResumeVm(UInt64 PartitionHandle, SUSPEND_RESUME_METHOD Method = SUSPEND_RESUME_METHOD.SuspendResumePowershell, bool ManageWorkerProcess = false)
        {
            return SdkControlVmState(PartitionHandle, VM_STATE_ACTION.ResumeVm, Method, ManageWorkerProcess);
        }

        /// <summary>
        /// Control VM state (suspend or resume)
        /// </summary>
        /// <param name="PartitionHandle">Handle to partition</param>
        /// <param name="Action">Action to perform (Suspend or Resume)</param>
        /// <param name="Method">Method to use</param>
        /// <param name="ManageWorkerProcess">Manage worker process</param>
        /// <returns>True if successful</returns>
        public static bool ControlVmState(UInt64 PartitionHandle, VM_STATE_ACTION Action, SUSPEND_RESUME_METHOD Method, bool ManageWorkerProcess = false)
        {
            return SdkControlVmState(PartitionHandle, Action, Method, ManageWorkerProcess);
        }

        /// <summary>
        /// Get physical address for virtual address (GVA to GPA translation)
        /// </summary>
        /// <param name="PartitionHandle">Handle to partition</param>
        /// <param name="VirtualAddress">Virtual address to translate</param>
        /// <param name="AccessType">Memory access type (default: Virtual)</param>
        /// <returns>Physical address (0 if translation failed)</returns>
        public static UInt64 GetPhysicalAddress(UInt64 PartitionHandle, UInt64 VirtualAddress, MEMORY_ACCESS_TYPE AccessType = MEMORY_ACCESS_TYPE.MmVirtualMemory)
        {
            return SdkGetPhysicalAddress(PartitionHandle, VirtualAddress, AccessType);
        }

        /// <summary>
        /// Get machine architecture type
        /// </summary>
        /// <param name="PartitionHandle">Handle to partition</param>
        /// <returns>Machine type (x86, AMD64, etc.)</returns>
        public static MACHINE_TYPE GetMachineType(UInt64 PartitionHandle)
        {
            return SdkGetMachineType(PartitionHandle);
        }

        /// <summary>
        /// Get current Virtual Trust Level for virtual address
        /// </summary>
        /// <param name="PartitionHandle">Handle to partition</param>
        /// <param name="VirtualAddress">Virtual address to check</param>
        /// <returns>VTL level (Vtl0, Vtl1, or BadVtl)</returns>
        public static VTL_LEVEL GetCurrentVtl(UInt64 PartitionHandle, UInt64 VirtualAddress)
        {
            return SdkGetCurrentVtl(PartitionHandle, VirtualAddress);
        }

        /// <summary>
        /// Read virtual processor register
        /// </summary>
        /// <param name="PartitionHandle">Handle to partition</param>
        /// <param name="VpIndex">Virtual processor index</param>
        /// <param name="Vtl">Virtual Trust Level</param>
        /// <param name="RegisterCode">Register code (e.g., HV_X64_REGISTER_RIP)</param>
        /// <param name="RegisterValue">Output register value</param>
        /// <returns>True if successful</returns>
        public static bool ReadVpRegister(UInt64 PartitionHandle, UInt32 VpIndex, VTL_LEVEL Vtl, UInt32 RegisterCode, out HV_REGISTER_VALUE RegisterValue)
        {
            RegisterValue = new HV_REGISTER_VALUE();
            return SdkReadVpRegister(PartitionHandle, VpIndex, Vtl, RegisterCode, ref RegisterValue);
        }

        /// <summary>
        /// Write virtual processor register
        /// </summary>
        /// <param name="PartitionHandle">Handle to partition</param>
        /// <param name="VpIndex">Virtual processor index</param>
        /// <param name="Vtl">Virtual Trust Level</param>
        /// <param name="RegisterCode">Register code (e.g., HV_X64_REGISTER_RIP)</param>
        /// <param name="RegisterValue">Register value to write</param>
        /// <returns>True if successful</returns>
        public static bool WriteVpRegister(UInt64 PartitionHandle, UInt32 VpIndex, VTL_LEVEL Vtl, UInt32 RegisterCode, HV_REGISTER_VALUE RegisterValue)
        {
            return SdkWriteVpRegister(PartitionHandle, VpIndex, Vtl, RegisterCode, ref RegisterValue);
        }
    }
}