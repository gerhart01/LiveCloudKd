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
            HvddDumpHeaderPointer,
            HvddUpdateCr3ForLocal,
            HvddGetCr3Kernel,
            HvddGetCr3Hv,
            HvddGetCr3Securekernel,
            HvddIsVmSuspended,
            HvddSecureKernelBase,
            HvddSecureKernelSize,
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
            [MarshalAs(UnmanagedType.I1)] public bool LogSilenceMode;
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

        // Symbol information structure (matches native SYMBOL_INFO)
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct SYMBOL_INFO
        {
            public uint SizeOfStruct;
            public uint TypeIndex;
            public UInt64 Reserved1;
            public UInt64 Reserved2;
            public uint Index;
            public uint Size;
            public UInt64 ModBase;
            public uint Flags;
            public UInt64 Value;
            public UInt64 Address;
            public uint Register;
            public uint Scope;
            public uint Tag;
            public uint NameLen;
            public uint MaxNameLen;

            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 2008)]
            public string Name;
        }

        // Symbol information for PowerShell (string-formatted addresses)
        public struct SYMBOL_INFO_PWSH
        {
            public uint SizeOfStruct;
            public uint TypeIndex;
            public UInt64 Reserved1;
            public UInt64 Reserved2;
            public uint Index;
            public uint Size;
            public string ModBase;
            public uint Flags;
            public string Value;
            public string Address;
            public uint Register;
            public uint Scope;
            public uint Tag;
            public uint NameLen;
            public uint MaxNameLen;
            public string ModuleName;

            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 2008)]
            public string Name;
            public UInt64 ReadValue;
            public string ReadValueHex;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct SYMBOL_INFO_PACKAGE
        {
            public SYMBOL_INFO Symbol;

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2001)]
            public byte[] Name;
        }

        // ==============================================================================
        // DLL Import Declarations - Existing Functions
        // ==============================================================================

        [DllImport("hvlib.dll")]
        private static extern bool SdkGetDefaultConfig(ref VM_OPERATIONS_CONFIG cfg);

        [DllImport("hvlib.dll")]
        private static extern bool SdkSetPartitionConfig(UInt64 PartitionHandle, ref VM_OPERATIONS_CONFIG cfg);

        [DllImport("hvlib.dll")]
        private static extern IntPtr SdkGetPartitionConfig(UInt64 PartitionHandle);

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
        [return: MarshalAs(UnmanagedType.I1)]
        private static extern bool SdkInvokeHypercall(UInt32 HvCallId, [MarshalAs(UnmanagedType.I1)] bool IsFast, UInt32 RepStartIndex, UInt32 CountOfElements, [MarshalAs(UnmanagedType.I1)] bool IsNested, IntPtr InputBuffer, IntPtr OutputBuffer);

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
        // DLL Import Declarations - Symbol Operations
        // ==============================================================================

        [DllImport("hvlib.dll", CharSet = CharSet.Ansi)]
        private static extern UInt64 SdkSymGetSymbolAddress(
            UInt64 PartitionHandle, UInt64 ImageBase, UInt32 ImageSize,
            [MarshalAs(UnmanagedType.LPStr)] string symbolName,
            MEMORY_ACCESS_TYPE MmAccess);

        [DllImport("hvlib.dll", CharSet = CharSet.Ansi)]
        private static extern UInt64 SdkSymGetSymbolAddress2(
            UInt64 PartitionHandle,
            [MarshalAs(UnmanagedType.LPStr)] string moduleName,
            [MarshalAs(UnmanagedType.LPStr)] string symbolName,
            MEMORY_ACCESS_TYPE MmAccess);

        [DllImport("hvlib.dll", CharSet = CharSet.Unicode)]
        private static extern bool SdkSymEnumAllSymbols(
            UInt64 PartitionHandle,
            [MarshalAs(UnmanagedType.LPWStr)] string DriverName,
            [Out] SYMBOL_INFO[] SymbolTable,
            MEMORY_ACCESS_TYPE MmAccess);

        [DllImport("hvlib.dll", CharSet = CharSet.Unicode)]
        private static extern UInt64 SdkSymEnumAllSymbolsGetTableLength(
            UInt64 PartitionHandle,
            [MarshalAs(UnmanagedType.LPWStr)] string DriverName,
            MEMORY_ACCESS_TYPE MmAccess);

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

        // ==============================================================================
        // Public API - Partition Configuration
        // ==============================================================================

        public static bool SetPartitionConfig(UInt64 PartitionHandle, ref VM_OPERATIONS_CONFIG cfg)
        {
            return SdkSetPartitionConfig(PartitionHandle, ref cfg);
        }

        public static VM_OPERATIONS_CONFIG GetPartitionConfig(UInt64 PartitionHandle)
        {
            IntPtr ptr = SdkGetPartitionConfig(PartitionHandle);
            if (ptr == IntPtr.Zero)
                return new VM_OPERATIONS_CONFIG();
            return Marshal.PtrToStructure<VM_OPERATIONS_CONFIG>(ptr);
        }

        public static bool SetPreferredSettings(UInt64 PartitionHandle, ref VM_OPERATIONS_CONFIG cfg)
        {
            return SdkSetPartitionConfig(PartitionHandle, ref cfg);
        }

        /// <summary>
        /// Update the internal static cfg.WriteMethod field
        /// </summary>
        public static void SetWriteMethod(WRITE_MEMORY_METHOD Method)
        {
            Hvlib.cfg.WriteMethod = Method;
        }

        /// <summary>
        /// Get the current internal WriteMethod
        /// </summary>
        public static WRITE_MEMORY_METHOD GetWriteMethod()
        {
            return Hvlib.cfg.WriteMethod;
        }

        /// <summary>
        /// Write physical memory with explicit write method
        /// </summary>
        public static bool WritePhysicalMemoryEx(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 WriteByteCount, IntPtr ClientBuffer, WRITE_MEMORY_METHOD Method)
        {
            return SdkWritePhysicalMemory(PartitionHandle, StartPosition, WriteByteCount, ClientBuffer, Method);
        }

        /// <summary>
        /// Read physical memory with explicit read method
        /// </summary>
        public static bool ReadPhysicalMemoryWithMethod(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, IntPtr ClientBuffer, READ_MEMORY_METHOD Method)
        {
            return SdkReadPhysicalMemory(PartitionHandle, StartPosition, ReadByteCount, ClientBuffer, Method);
        }

        // ==============================================================================
        // Public API - Hypercall
        // ==============================================================================

        /// <summary>
        /// Result of a hypercall invocation via InvokeHypercallBytes.
        /// </summary>
        public struct HypercallResult
        {
            /// <summary>True if the SDK call succeeded.</summary>
            public bool Ok;
            /// <summary>Raw output page data (up to 4096 bytes).</summary>
            public byte[] OutputData;
        }

        /// <summary>
        /// High-level hypercall: accepts byte[] input, returns byte[] output.
        /// Handles all unmanaged buffer allocation/deallocation internally.
        ///
        /// This is the universal entry point — the caller serializes the
        /// hypercall-specific input structure into a byte array, and this
        /// method handles the rest (allocate pages, copy in, call SDK,
        /// copy out, free pages).
        ///
        /// Usage from PowerShell:
        ///   $r = [Hvlibdotnet.Hvlib]::InvokeHypercallBytes(0x0053, $inputBytes, 24)
        ///   $r.Ok          # bool
        ///   $r.OutputData  # byte[]
        /// </summary>
        /// <param name="callCode">Hypercall code (e.g. 0x0053 for HvCallReadGpa)</param>
        /// <param name="inputData">Serialized input structure (may be null)</param>
        /// <param name="outputSize">Expected output size in bytes (clamped to 4096)</param>
        /// <param name="isFast">Use fast (register-only) hypercall</param>
        /// <param name="countOfElements">Rep count for rep hypercalls</param>
        /// <param name="repStartIndex">Rep start index for rep hypercalls</param>
        /// <param name="isNested">Nested hypercall flag</param>
        /// <returns>HypercallResult with Ok and OutputData</returns>
        public static HypercallResult InvokeHypercallBytes(
            UInt32 callCode,
            byte[]? inputData,
            int outputSize,
            bool isFast = false,
            UInt32 countOfElements = 0,
            UInt32 repStartIndex = 0,
            bool isNested = false)
        {
            const int PageSize = 0x1000;
            int clampedOutput = Math.Min(Math.Max(outputSize, 0), PageSize);

            IntPtr inputBuf = Marshal.AllocHGlobal(PageSize);
            IntPtr outputBuf = Marshal.AllocHGlobal(PageSize);
            try
            {
                // Zero both pages
                byte[] zeroes = new byte[PageSize];
                Marshal.Copy(zeroes, 0, inputBuf, PageSize);
                Marshal.Copy(zeroes, 0, outputBuf, PageSize);

                // Copy input data
                if (inputData != null && inputData.Length > 0)
                {
                    Marshal.Copy(inputData, 0, inputBuf,
                                 Math.Min(inputData.Length, PageSize));
                }

                bool ok = SdkInvokeHypercall(
                    callCode, isFast, repStartIndex, countOfElements,
                    isNested, inputBuf, outputBuf);

                // Copy output
                byte[] output = new byte[clampedOutput];
                if (clampedOutput > 0)
                    Marshal.Copy(outputBuf, output, 0, clampedOutput);

                return new HypercallResult { Ok = ok, OutputData = output };
            }
            finally
            {
                Marshal.FreeHGlobal(inputBuf);
                Marshal.FreeHGlobal(outputBuf);
            }
        }

        /// <summary>
        /// Invoke a hypervisor hypercall. Parameters are extracted from HVCALL_DATA,
        /// InputVA/OutputVA are passed as raw pointers per SDK signature.
        /// Returns true on success; hypercall result is stored in HvCallData.result.
        /// </summary>
        public static bool InvokeHypercall(ref HVCALL_DATA HvCallData)
        {
            return SdkInvokeHypercall(
                HvCallData.HvCallId.CallCode,
                HvCallData.HvCallId.IsFast,
                HvCallData.HvCallId.RepStartIndex,
                HvCallData.HvCallId.CountOfElements,
                HvCallData.HvCallId.IsNested,
                HvCallData.InputVA,
                HvCallData.OutputVA);
        }

        /// <summary>
        /// Invoke a hypervisor hypercall with explicit parameters matching the SDK header:
        /// SdkInvokeHypercall(HvCallId, IsFast, RepStartIndex, CountOfElements, IsNested, InputBuffer, OutputBuffer)
        /// </summary>
        public static bool InvokeHypercall(UInt32 HvCallId, bool IsFast, UInt32 RepStartIndex, UInt32 CountOfElements, bool IsNested, IntPtr InputBuffer, IntPtr OutputBuffer)
        {
            return SdkInvokeHypercall(HvCallId, IsFast, RepStartIndex, CountOfElements, IsNested, InputBuffer, OutputBuffer);
        }

        /// <summary>
        /// Modify VTL protection mask for a physical page via HvModifyVtlProtectionMask hypercall (0x000C).
        /// </summary>
        public static UInt64 ModifyVtlProtection(UInt64 PartitionId, UInt64 PhysicalAddress)
        {
            byte[] inputStruct = new byte[24];
            BitConverter.GetBytes(PartitionId).CopyTo(inputStruct, 0);
            BitConverter.GetBytes((UInt32)0x05).CopyTo(inputStruct, 8);
            BitConverter.GetBytes((UInt32)0x11).CopyTo(inputStruct, 12);
            BitConverter.GetBytes(PhysicalAddress >> 12).CopyTo(inputStruct, 16);

            var result = InvokeHypercallBytes(0x000C, inputStruct, 0,
                countOfElements: 1);
            return result.Ok ? (UInt64)0 : (UInt64)1;
        }

        // ==============================================================================
        // Public API - Symbol Operations
        // ==============================================================================

        public static UInt64 GetSymbolTableLength(UInt64 PartitionHandle, string DriverName, MEMORY_ACCESS_TYPE AccessType = MEMORY_ACCESS_TYPE.MmVirtualMemory)
        {
            return SdkSymEnumAllSymbolsGetTableLength(PartitionHandle, DriverName, AccessType);
        }

        public static UInt64 GetSymbolAddress(UInt64 PartitionHandle, UInt64 ImageBase, UInt32 ImageSize, string SymbolName, MEMORY_ACCESS_TYPE AccessType = MEMORY_ACCESS_TYPE.MmVirtualMemory)
        {
            return SdkSymGetSymbolAddress(PartitionHandle, ImageBase, ImageSize, SymbolName, AccessType);
        }

        public static UInt64 GetSymbolAddress2(UInt64 PartitionHandle, string ModuleName, string SymbolName, MEMORY_ACCESS_TYPE AccessType = MEMORY_ACCESS_TYPE.MmVirtualMemory)
        {
            return SdkSymGetSymbolAddress2(PartitionHandle, ModuleName, SymbolName, AccessType);
        }

        public static List<SYMBOL_INFO_PWSH>? GetAllSymbolsForModule(IntPtr Handle, String ModuleName)
        {
            MEMORY_ACCESS_TYPE MmAccessType = MEMORY_ACCESS_TYPE.MmVirtualMemory;
            UInt64 SymTableCount = SdkSymEnumAllSymbolsGetTableLength((UInt64)Handle, ModuleName, MmAccessType);

            if (SymTableCount == 0)
                return null;

            SYMBOL_INFO[] SymbolTable = new SYMBOL_INFO[SymTableCount];
            Array.Clear(SymbolTable, 0, SymbolTable.Length);

            bool bResult = SdkSymEnumAllSymbols((UInt64)Handle, ModuleName, SymbolTable, MmAccessType);

            List<SYMBOL_INFO_PWSH>? ListObj = new List<SYMBOL_INFO_PWSH>();
            SYMBOL_INFO_PWSH SymInfoPwsh = new SYMBOL_INFO_PWSH();

            for (ulong i = 0; i < SymTableCount; i += 1)
            {
                SymInfoPwsh.SizeOfStruct = SymbolTable[i].SizeOfStruct;
                SymInfoPwsh.TypeIndex = SymbolTable[i].TypeIndex;
                SymInfoPwsh.Reserved1 = SymbolTable[i].Reserved1;
                SymInfoPwsh.Reserved2 = SymbolTable[i].Reserved2;
                SymInfoPwsh.Index = SymbolTable[i].Index;
                SymInfoPwsh.Size = SymbolTable[i].Size;
                SymInfoPwsh.ModBase = "0x" + SymbolTable[i].ModBase.ToString("X");
                SymInfoPwsh.Flags = SymbolTable[i].Flags;
                SymInfoPwsh.Value = "0x" + SymbolTable[i].Value.ToString("X");
                SymInfoPwsh.Address = "0x" + SymbolTable[i].Address.ToString("X");
                SymInfoPwsh.Register = SymbolTable[i].Register;
                SymInfoPwsh.Scope = SymbolTable[i].Scope;
                SymInfoPwsh.Tag = SymbolTable[i].Tag;
                SymInfoPwsh.NameLen = SymbolTable[i].NameLen;
                SymInfoPwsh.MaxNameLen = SymbolTable[i].MaxNameLen;
                SymInfoPwsh.Name = SymbolTable[i].Name;
                SymInfoPwsh.ModuleName = ModuleName;

                IntPtr unmanagedPointer = Marshal.AllocHGlobal(8);
                long value = 0;
                Marshal.WriteInt64(unmanagedPointer, value);
                ReadVirtualMemory((UInt64)Handle, SymbolTable[i].Address, 8, unmanagedPointer);
                long readValue = Marshal.ReadInt64(unmanagedPointer);
                SymInfoPwsh.ReadValue = (ulong)readValue;
                SymInfoPwsh.ReadValueHex = "0x" + readValue.ToString("X");
                Marshal.FreeHGlobal(unmanagedPointer);

                ListObj.Add(SymInfoPwsh);
            }

            return ListObj;
        }

        // ==============================================================================
        // Public API - Small Utilities
        // ==============================================================================
        //
        // x86-64 page-table walk (ResolvePageTableAddress) and the VTL1 read-with-
        // fallback helper (ReadVirtualMemoryWithFallback) used to live here. They
        // were moved back into PowerShell so scripts can inspect / tweak the walk
        // on the fly during VTL1 mapping debugging. The PFN-mask constants moved
        // with them (each script declares its own).

        /// <summary>
        /// Read a single 8-byte (UInt64) value from guest physical memory.
        /// Returns 0 on failure.
        /// </summary>
        public static ulong ReadPhysicalUInt64(ulong PartitionHandle, ulong PhysicalAddress)
        {
            ulong[] buf = new ulong[1];
            if (!ReadPhysicalMemory(PartitionHandle, PhysicalAddress, 8, buf))
                return 0UL;
            return buf[0];
        }

        /// <summary>
        /// Read virtual memory into a byte[] buffer. Convenience overload around the
        /// existing IntPtr-based ReadVirtualMemory.
        /// </summary>
        public static byte[]? ReadVirtualMemory(ulong PartitionHandle, ulong VirtualAddress, int Size)
        {
            if (Size <= 0) return null;
            IntPtr buf = Marshal.AllocHGlobal(Size);
            try
            {
                if (!ReadVirtualMemory(PartitionHandle, VirtualAddress, (ulong)Size, buf))
                    return null;
                byte[] data = new byte[Size];
                Marshal.Copy(buf, data, 0, Size);
                return data;
            }
            finally
            {
                Marshal.FreeHGlobal(buf);
            }
        }

        /// <summary>
        /// Format a byte buffer as a hex dump: address + hex pairs + ASCII gutter.
        /// Returns one string per line; the caller chooses how to render them.
        /// </summary>
        public static string[] FormatHexDump(byte[] Bytes, ulong BaseAddress = 0, int BytesPerLine = 16)
        {
            if (Bytes == null || Bytes.Length == 0) return new[] { "<empty>" };
            if (BytesPerLine <= 0) BytesPerLine = 16;

            var lines = new List<string>((Bytes.Length + BytesPerLine - 1) / BytesPerLine);
            var sb = new System.Text.StringBuilder(80);

            for (int i = 0; i < Bytes.Length; i += BytesPerLine)
            {
                sb.Clear();
                sb.AppendFormat("{0:X16}  ", BaseAddress + (ulong)i);

                // Hex pairs (with a wider gap between halves for readability)
                for (int j = 0; j < BytesPerLine; j++)
                {
                    int idx = i + j;
                    if (idx < Bytes.Length) sb.AppendFormat("{0:X2} ", Bytes[idx]);
                    else sb.Append("   ");
                    if (j == 7) sb.Append(' ');
                }
                sb.Append(' ');

                // ASCII gutter
                for (int j = 0; j < BytesPerLine; j++)
                {
                    int idx = i + j;
                    if (idx >= Bytes.Length) break;
                    byte b = Bytes[idx];
                    sb.Append((b >= 0x20 && b <= 0x7E) ? (char)b : '.');
                }

                lines.Add(sb.ToString());
            }
            return lines.ToArray();
        }

        /// <summary>
        /// Compare a Length-byte window in two arrays. Returns true if equal.
        /// Out-of-range offsets/lengths return false (no exception).
        /// </summary>
        public static bool BytesEqual(byte[] A, byte[] B, int OffsetA, int OffsetB, int Length)
        {
            if (A == null || B == null) return false;
            if (Length < 0) return false;
            if (OffsetA < 0 || OffsetB < 0) return false;
            if (OffsetA + Length > A.Length) return false;
            if (OffsetB + Length > B.Length) return false;

            for (int i = 0; i < Length; i++)
                if (A[OffsetA + i] != B[OffsetB + i]) return false;
            return true;
        }
    }
}