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
            //Special set values
            HvddSetMemoryBlock,
            HvddEnlVmcsPointer
        }

        public enum READ_MEMORY_METHOD
        {
            ReadInterfaceUnsupported,
            ReadInterfaceHvmmDrvInternal,
            ReadInterfaceWinHv,
            ReadInterfaceHvmmLocal,
            ReadInterfaceHvmmMax
        }
        public enum WRITE_MEMORY_METHOD
        {
            WriteInterfaceUnsupported,
            WriteInterfaceHvmmDrvInternal,
            WriteInterfaceWinHv,
            WriteInterfaceHvmmLocal,
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

        public enum LOG_LEVEL
        {
            log_er = 0,
            log_v1 = 1,
            log_v2 = 2,
            log_v3 = 3,
            log_skip = 4
        }

        public enum GET_CR3_TYPE
        {
            Cr3Process = 0,
            Cr3Kernel = 0xFFFFFFD,
            Cr3SecureKernel = 0xFFFFFFE,
            Cr3Hypervisor = 0xFFFFFFF
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
            MACHINE_ARN64 = 3,
            MACHINE_UNSUPPORTED = 4
        }

        //
        // NEW: Virtual Trust Level enumeration
        // https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/tlfs/datatypes/hv_vtl
        // The HV_VTL is an 8-bit unsigned integer that identifies a Virtual Trust Level. Valid VTL values range from 0 to 15.
        //
        public enum VTL_LEVEL
        {
            Vtl0 = 0,
            Vtl1 = 1,
            Vtl2 = 2,
            Vtl3 = 3,
            Vtl4 = 4,
            Vtl5 = 5,
            Vtl6 = 6,
            Vtl7 = 7,
            Vtl8 = 8,
            Vtl9 = 9,
            Vtl10 = 10,
            Vtl11 = 11,
            Vtl12 = 12,
            Vtl13 = 13,
            Vtl14 = 14,
            Vtl15 = 15,
            BadVtl = 16
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct VM_OPERATIONS_CONFIG
        {
            public READ_MEMORY_METHOD ReadMethod;
            public WRITE_MEMORY_METHOD WriteMethod;
            public SUSPEND_RESUME_METHOD SuspendMethod;
            public LOG_LEVEL LogLevel;
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

        //// NEW: SYMBOL_INFO structure for symbol enumeration
        //[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        //public struct SYMBOL_INFO
        //{
        //    public UInt32 SizeOfStruct;
        //    public UInt32 TypeIndex;
        //    public UInt64 Reserved1;
        //    public UInt64 Reserved2;
        //    public UInt32 Index;
        //    public UInt32 Size;
        //    public UInt64 ModBase;
        //    public UInt32 Flags;
        //    public UInt64 Value;
        //    public UInt64 Address;
        //    public UInt32 Register;
        //    public UInt32 Scope;
        //    public UInt32 Tag;
        //    public UInt32 NameLen;
        //    public UInt32 MaxNameLen;
        //    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 1)]
        //    public string Name;
        //}

        //// NEW: SYMBOL_INFO_PACKAGE structure for symbol operations
        //[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        //public struct SYMBOL_INFO_PACKAGE
        //{
        //    public SYMBOL_INFO si;
        //    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 2001)]
        //    public string name;
        //}

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

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2001)] // Buffer for the symbol name
            public byte[] Name;
        }

        // ==============================================================================
        // DLL Import Declarations - Existing Functions
        // ==============================================================================

        [DllImport("hvlib.dll")]
        private static extern bool SdkGetDefaultConfig(ref VM_OPERATIONS_CONFIG cfg);

        [DllImport("hvlib.dll")]
        private static extern bool SdkSetDefaultConfig(UInt64 PartitionHandle, ref VM_OPERATIONS_CONFIG cfg);

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
        // DLL Import Declarations - NEW FUNCTIONS (VM Control & Introspection)
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
        // DLL Import Declarations - NEW FUNCTIONS (Symbol Operations)
        // ==============================================================================

        /// <summary>
        /// Get symbol address by name
        /// </summary>
        [DllImport("hvlib.dll", CharSet = CharSet.Ansi)]
        private static extern UInt64 SdkSymGetSymbolAddress(
            UInt64 PartitionHandle,
            UInt64 ImageBase,
            UInt32 ImageSize,
            [MarshalAs(UnmanagedType.LPStr)] string symbolName,
            MEMORY_ACCESS_TYPE MmAccess);

        /// <summary>
        /// Enumerate all symbols from a driver
        /// </summary>
        [DllImport("hvlib.dll", CharSet = CharSet.Unicode)]
        private static extern bool SdkSymEnumAllSymbols(UInt64 PartitionHandle, [MarshalAs(UnmanagedType.LPWStr)] string DriverName, [Out] SYMBOL_INFO[] SymbolTable, MEMORY_ACCESS_TYPE MmAccess);

        /// <summary>
        /// Get the length of symbol table for a driver
        /// </summary>
        [DllImport("hvlib.dll", CharSet = CharSet.Unicode)]
        private static extern UInt64 SdkSymEnumAllSymbolsGetTableLength(
            UInt64 PartitionHandle,
            [MarshalAs(UnmanagedType.LPWStr)] string DriverName,
            MEMORY_ACCESS_TYPE MmAccess);

        // ==============================================================================
        // Public API - Existing Functions
        // ==============================================================================

        /// <summary>
        /// Get default/preferred configuration settings for VM operations
        /// </summary>
        /// <param name="cfg">Configuration structure to populate</param>
        /// <returns>True if successful</returns>
        public static bool GetPreferredSettings(ref VM_OPERATIONS_CONFIG cfg)
        {
            bool bResult = SdkGetDefaultConfig(ref cfg);
            return bResult;
        }

        public static bool SetPreferredSettings(UInt64 PartitionHandle, ref VM_OPERATIONS_CONFIG cfg)
        {
            bool bResult = SdkSetDefaultConfig(PartitionHandle, ref cfg);
            return bResult;
        }

        /// <summary>
        /// Test function to verify Hvlib is loaded correctly
        /// </summary>
        public static void TestHvLib()
        {
            Console.Write("Hvlib is loaded");
        }

        public static UInt64 VmHandle = 0x100000;

        private static VM_OPERATIONS_CONFIG cfg;

        /// <summary>
        /// Enumerate all virtual machine partitions on the system
        /// </summary>
        /// <param name="cfg">VM operations configuration to use</param>
        /// <returns>List of VmListBox objects containing VM handles and names, or null if enumeration failed</returns>
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

        /// <summary>
        /// Enumerate all VM partitions using default configuration
        /// </summary>
        /// <returns>List of VmListBox objects containing VM handles and names, or null if enumeration failed</returns>
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

        /// <summary>
        /// Select a partition for subsequent operations
        /// </summary>
        /// <param name="PartitionHandle">Handle to the partition to select</param>
        /// <returns>True if successful</returns>
        public static bool SelectPartition(UInt64 PartitionHandle)
        {
            return SdkSelectPartition(PartitionHandle);
        }

        /// <summary>
        /// Get partition information data
        /// </summary>
        /// <param name="PartitionHandle">Partition handle</param>
        /// <param name="HvddInformationClass">Type of information to retrieve</param>
        /// <param name="HvddInformation">Output information value</param>
        /// <returns>True if successful</returns>
        public static bool GetPartitionData(UInt64 PartitionHandle, HVDD_INFORMATION_CLASS HvddInformationClass, out UIntPtr HvddInformation)
        {
            return SdkGetData(PartitionHandle, HvddInformationClass, out HvddInformation);
        }

        /// <summary>
        /// Get partition information data (alternative version returning UInt64)
        /// </summary>
        /// <param name="PartitionHandle">Partition handle</param>
        /// <param name="HvddInformationClass">Type of information to retrieve</param>
        /// <returns>Information value as UInt64</returns>
        public static UInt64 GetPartitionData2(UInt64 PartitionHandle, HVDD_INFORMATION_CLASS HvddInformationClass)
        {
            return SdkGetData2(PartitionHandle, HvddInformationClass);
        }

        /// <summary>
        /// Get CR3 (page table base) for a specific process ID
        /// </summary>
        /// <param name="PartitionHandle">Partition handle</param>
        /// <param name="Pid">Process ID</param>
        /// <returns>CR3 value (page table base address)</returns>
        public static UInt64 GetCr3FromPid(UInt64 PartitionHandle, UInt64 Pid)
        {
            UIntPtr ProcessId = new UIntPtr(Pid);

            bool bResult = false;

            switch (Pid)
            {
                case (ulong)GET_CR3_TYPE.Cr3Kernel:
                    bResult = SdkGetData(PartitionHandle, HVDD_INFORMATION_CLASS.HvddGetCr3Kernel, out ProcessId);
                    break;

                case (ulong)GET_CR3_TYPE.Cr3Hypervisor:
                    bResult = SdkGetData(PartitionHandle, HVDD_INFORMATION_CLASS.HvddGetCr3Hv, out ProcessId);
                    break;

                case (ulong)GET_CR3_TYPE.Cr3SecureKernel:
                    bResult = SdkGetData(PartitionHandle, HVDD_INFORMATION_CLASS.HvddGetCr3Securekernel, out ProcessId);
                    break;

                default:
                    bResult = SdkGetData(PartitionHandle, HVDD_INFORMATION_CLASS.HvddGetCr3byPid, out ProcessId);
                    break;
            }

            UInt64 cr3 = ProcessId.ToUInt64();
            return cr3;
        }

        /// <summary>
        /// Read physical memory from VM partition
        /// </summary>
        /// <param name="PartitionHandle">Partition handle</param>
        /// <param name="StartPosition">Physical address to start reading from</param>
        /// <param name="ReadByteCount">Number of bytes to read</param>
        /// <param name="ClientBuffer">Buffer to receive data (IntPtr)</param>
        /// <returns>True if successful</returns>
        public static bool ReadPhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, IntPtr ClientBuffer)
        {
            return SdkReadPhysicalMemory(PartitionHandle, StartPosition, ReadByteCount, ClientBuffer, Hvlib.cfg.ReadMethod);
        }

        /// <summary>
        /// Read physical memory from VM partition
        /// </summary>
        /// <param name="PartitionHandle">Partition handle</param>
        /// <param name="StartPosition">Physical address to start reading from</param>
        /// <param name="ReadByteCount">Number of bytes to read</param>
        /// <param name="ClientBuffer">Buffer to receive data (byte array)</param>
        /// <returns>True if successful</returns>
        public static bool ReadPhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, byte[] ClientBuffer)
        {
            return SdkReadPhysicalMemoryByte(PartitionHandle, StartPosition, ReadByteCount, ClientBuffer, Hvlib.cfg.ReadMethod);
        }

        /// <summary>
        /// Read physical memory from VM partition
        /// </summary>
        /// <param name="PartitionHandle">Partition handle</param>
        /// <param name="StartPosition">Physical address to start reading from</param>
        /// <param name="ReadByteCount">Number of bytes to read</param>
        /// <param name="ClientBuffer">Buffer to receive data (IntPtr)</param>
        /// <param name="Method">Buffer to receive data (IntPtr)</param>
        /// <returns>True if successful</returns>
        public static bool ReadPhysicalMemoryWithMethod(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, IntPtr ClientBuffer, READ_MEMORY_METHOD Method)
        {
            return SdkReadPhysicalMemory(PartitionHandle, StartPosition, ReadByteCount, ClientBuffer, Method);
        }

        /// <summary>
        /// Read physical memory from VM partition
        /// </summary>
        /// <param name="PartitionHandle">Partition handle</param>
        /// <param name="StartPosition">Physical address to start reading from</param>
        /// <param name="ReadByteCount">Number of bytes to read</param>
        /// <param name="ClientBuffer">Buffer to receive data (ulong array)</param>
        /// <returns>True if successful</returns>
        public static bool ReadPhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, ulong[] ClientBuffer)
        {
            return SdkReadPhysicalMemoryULong(PartitionHandle, StartPosition, ReadByteCount, ClientBuffer, Hvlib.cfg.ReadMethod);
        }

        /// <summary>
        /// Write to physical memory in VM partition
        /// </summary>
        /// <param name="PartitionHandle">Partition handle</param>
        /// <param name="StartPosition">Physical address to start writing to</param>
        /// <param name="WriteByteCount">Number of bytes to write</param>
        /// <param name="ClientBuffer">Buffer containing data to write</param>
        /// <returns>True if successful</returns>
        public static bool WritePhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 WriteByteCount, IntPtr ClientBuffer)
        {
            return SdkWritePhysicalMemory(PartitionHandle, StartPosition, WriteByteCount, ClientBuffer, Hvlib.cfg.WriteMethod);
        }

        /// <summary>
        /// Read virtual memory from VM partition
        /// </summary>
        /// <param name="PartitionHandle">Partition handle</param>
        /// <param name="StartPosition">Virtual address to start reading from</param>
        /// <param name="ReadByteCount">Number of bytes to read</param>
        /// <param name="ClientBuffer">Buffer to receive data</param>
        /// <returns>True if successful</returns>
        public static bool ReadVirtualMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, IntPtr ClientBuffer)
        {
            return SdkReadVirtualMemory(PartitionHandle, StartPosition, ClientBuffer, ReadByteCount);
        }

        /// <summary>
        /// Write to virtual memory in VM partition
        /// </summary>
        /// <param name="PartitionHandle">Partition handle</param>
        /// <param name="StartPosition">Virtual address to start writing to</param>
        /// <param name="WriteByteCount">Number of bytes to write</param>
        /// <param name="ClientBuffer">Buffer containing data to write</param>
        /// <returns>True if successful</returns>
        public static bool WriteVirtualMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 WriteByteCount, IntPtr ClientBuffer)
        {
            return SdkWriteVirtualMemory(PartitionHandle, StartPosition, ClientBuffer, WriteByteCount);
        }

        /// <summary>
        /// Close all open partition handles
        /// </summary>
        public static void CloseAllPartitions()
        {
            SdkCloseAllPartitions();
        }

        /// <summary>
        /// Close a specific partition handle
        /// </summary>
        /// <param name="PartitionHandle">Handle to the partition to close</param>
        public static void ClosePartition(UInt64 PartitionHandle)
        {
            SdkClosePartition(PartitionHandle);
        }

        // ==============================================================================
        // Public API - NEW FUNCTIONS (VM Control & Introspection)
        // ==============================================================================

        /// <summary>
        /// Set data for partition
        /// </summary>
        /// <param name="PartitionHandle">Partition handle</param>
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
        /// <param name="PartitionHandle">Partition handle</param>
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
        /// <param name="PartitionHandle">Partition handle</param>
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
        /// <param name="PartitionHandle">Partition handle</param>
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
        /// <param name="PartitionHandle">Partition handle</param>
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
        /// <param name="PartitionHandle">Partition handle</param>
        /// <returns>Machine type (x86, AMD64, etc.)</returns>
        public static MACHINE_TYPE GetMachineType(UInt64 PartitionHandle)
        {
            return SdkGetMachineType(PartitionHandle);
        }

        /// <summary>
        /// Read virtual processor register
        /// </summary>
        /// <param name="PartitionHandle">Partition handle</param>
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
        /// <param name="PartitionHandle">Partition handle</param>
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
        // Public API - NEW FUNCTIONS (Symbol Operations)
        // ==============================================================================

        /// <summary>
        /// Get the number of symbols in a driver's symbol table
        /// </summary>
        /// <param name="PartitionHandle">Partition handle</param>
        /// <param name="DriverName">Name of the driver (e.g., "ntoskrnl.exe")</param>
        /// <param name="AccessType">Memory access type (default: Virtual)</param>
        /// <returns>Number of symbols (0 if driver not found or no symbols)</returns>
        public static UInt64 GetSymbolTableLength(UInt64 PartitionHandle, string DriverName, MEMORY_ACCESS_TYPE AccessType = MEMORY_ACCESS_TYPE.MmVirtualMemory)
        {
            return SdkSymEnumAllSymbolsGetTableLength(PartitionHandle, DriverName, AccessType);
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
    }
}
