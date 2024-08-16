using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;

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

        public enum READ_MEMORY_METHOD
        {
            ReadInterfaceWinHv,
            ReadInterfaceHvmmDrvInternal,
            ReadInterfaceUnsupported
        }
        public enum WRITE_MEMORY_METHOD
        {
            WriteInterfaceWinHv,
            WriteInterfaceHvmmDrvInternal,
            WriteInterfaceUnsupported
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
            HvddHvddGetCr3byPid,
            HvddGetProcessesIds,
            //Special set values
            HvddSetMemoryBlock,
            HvddEnlVmcsPointer
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
            [MarshalAs(UnmanagedType.I1)] public bool ReloadDriver;
            [MarshalAs(UnmanagedType.I1)] public bool VSMScan;
            [MarshalAs(UnmanagedType.I1)] public bool UseDebugApiStopProcess;
            [MarshalAs(UnmanagedType.I1)] public bool ScanGuestOsImages;
            [MarshalAs(UnmanagedType.I1)] public bool BruteGuestReg;
            [MarshalAs(UnmanagedType.I1)] public bool ListWinHvrInfo;
            [MarshalAs(UnmanagedType.I1)] public bool SafeMode;
            [MarshalAs(UnmanagedType.I1)] public bool SimpleMemory;
            [MarshalAs(UnmanagedType.I1)] public bool DotNetNamedPipeLog;
        }

        [DllImport("hvlib.dll")]
        private static extern bool SdkGetDefaultConfig(ref VM_OPERATIONS_CONFIG cfg);

        [DllImport("hvlib.dll")]
        private static extern UIntPtr SdkEnumPartitions(ref UInt64 PartitionCount, ref VM_OPERATIONS_CONFIG cfg);

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
        private static extern bool SdkWritePhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 WriteByteCount, UIntPtr ClientBuffer, WRITE_MEMORY_METHOD Method);

        [DllImport("hvlib.dll")]
        private static extern bool SdkReadVirtualMemory(UInt64 PartitionHandle, UInt64 StartPosition, UIntPtr ClientBuffer, UInt64 ReadByteCount);

        [DllImport("hvlib.dll")]
        private static extern bool SdkWriteVirtualMemory(UInt64 PartitionHandle, UInt64 StartPosition, UIntPtr ClientBuffer, UInt64 WriteByteCount);

        [DllImport("hvlib.dll")]
        private static extern UInt64 SdkGetData2(UInt64 PartitionHandle, HVDD_INFORMATION_CLASS HvddInformationClass);

        [DllImport("hvlib.dll")]
        private static extern UInt64 SdkGetCr3FromPid(UInt64 PartitionHandle, UInt64 Pid, GET_CR3_TYPE Type);
        public static bool GetPreferredSettings(ref VM_OPERATIONS_CONFIG cfg)
        {

            Boolean bResult = SdkGetDefaultConfig(ref cfg);

            return bResult;
        }

        public static UInt64 GetSdkData(UInt64 PartitionHandle, HVDD_INFORMATION_CLASS HvddInformationClass)
        {
            return SdkGetData2(PartitionHandle, HvddInformationClass);
        }

        public static void TestHvLib()
        {
            Console.Write("Hvlib is loaded");
        }

        public static UInt64 VmHandle = 0x100000;

        private static VM_OPERATIONS_CONFIG cfg;

        public static List<VmListBox>? EnumAllPartitions()
        {
            Hvlib.VM_OPERATIONS_CONFIG cfg = new Hvlib.VM_OPERATIONS_CONFIG();

            bool bResult = Hvlib.GetPreferredSettings(ref cfg);
            cfg.DotNetNamedPipeLog = false;
            cfg.ReadMethod = Hvlib.READ_MEMORY_METHOD.ReadInterfaceHvmmDrvInternal;
            cfg.WriteMethod = Hvlib.WRITE_MEMORY_METHOD.WriteInterfaceWinHv;
            Hvlib.cfg = cfg;

            List<VmListBox>? res = EnumPartitions(ref cfg);
            return res;
        }
        public static List<VmListBox>? EnumPartitions(ref VM_OPERATIONS_CONFIG cfg)
        {
            UInt64 PartitionCount = 0;
            Int64[] arPartition;
            UIntPtr Partitions = SdkEnumPartitions(ref PartitionCount, ref cfg);

            List<VmListBox> ListObj = new List<VmListBox>();

            if (PartitionCount != 0)
            {
                arPartition = new Int64[PartitionCount];
                Marshal.Copy((nint)Partitions, arPartition, 0, (int)PartitionCount);
            }
            else
            {
                Console.Write("Partitions count is 0 \n");
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

        public static UInt64 GetData2(UInt64 VmHandle, HVDD_INFORMATION_CLASS InformationClass)
        {
            UInt64 uResult = Hvlib.SdkGetData2(VmHandle, InformationClass);
            return uResult;
        }

        public static bool GetData(UInt64 VmHandle, HVDD_INFORMATION_CLASS InformationClass, out UIntPtr HvddInformation)
        {
            bool bResult = Hvlib.SdkGetData(VmHandle, InformationClass, out HvddInformation);
            return bResult;
        }

        public static IntPtr[] GetProcessesList(UInt64 PartitionHandle)
        {

            IntPtr[] arPartition = new IntPtr[1];
            IntPtr aProcessList = (IntPtr)Hvlib.SdkGetData2(PartitionHandle, HVDD_INFORMATION_CLASS.HvddGetProcessesIds);
            Marshal.Copy(aProcessList, arPartition, 0, (int)1);
            IntPtr[] arProcess;

            UInt64 ProcessCount = (UInt64)arPartition[0];

            if (ProcessCount > 0)
            {
                arProcess = new IntPtr[ProcessCount + 1];
                Marshal.Copy(aProcessList, arProcess, 1, (int)ProcessCount);
            }
            else
            {
#pragma warning disable CS8603 // Possible null reference return.
                return null;
#pragma warning restore CS8603 // Possible null reference return.
            }

            return arProcess;
        }

        public static UInt64 GetCr3(UInt64 PartitionHandle, UInt64 Pid)
        {
            UInt64 Cr3 = 0;

            if (Pid == 0xFFFFFFFF)
                Cr3 = SdkGetCr3FromPid(PartitionHandle, Pid, GET_CR3_TYPE.Cr3Hypervisor);
            else if (Pid == 0xFFFFFFFE)
                Cr3 = SdkGetCr3FromPid(PartitionHandle, Pid, GET_CR3_TYPE.Cr3Kernel);
            else
                Cr3 = SdkGetCr3FromPid(PartitionHandle, Pid, GET_CR3_TYPE.Cr3Process);

            return Cr3;
        }
        public static bool SelectPartition(UInt64 Handle)
        {
            return SdkSelectPartition(Handle);
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

        public static bool WritePhysicalMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 WriteByteCount, UIntPtr ClientBuffer)
        {
            return SdkWritePhysicalMemory(PartitionHandle, StartPosition, WriteByteCount, ClientBuffer, Hvlib.cfg.WriteMethod);
        }

        public static bool ReadVirtualMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 ReadByteCount, UIntPtr ClientBuffer)
        {
            return SdkReadVirtualMemory(PartitionHandle, StartPosition, ClientBuffer, ReadByteCount);
        }

        public static bool WriteVirtualMemory(UInt64 PartitionHandle, UInt64 StartPosition, UInt64 WriteByteCount, UIntPtr ClientBuffer)
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
    }
}