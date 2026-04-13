//! Rust bindings for `hvlib.dll` — LiveCloudKd Hyper-V guest memory/register access SDK.
//!
//! # Example
//! ```no_run
//! use livecloudkd::*;
//!
//! let mut cfg = VmOperationsConfig::default();
//! sdk_get_default_config(&mut cfg);
//! cfg.reload_driver = 1;
//!
//! let handles = sdk_enum_partitions(&cfg);
//! if let Some(&handle) = handles.first() {
//!     sdk_select_partition(handle);
//!     let kernel_base = sdk_get_data2(handle, HvmmInformationClass::InfoKernelBase);
//!     println!("KernelBase = {:#x}", kernel_base);
//!     sdk_close_partition(handle);
//! }
//! sdk_close_all_partitions();
//! ```

#![cfg(target_os = "windows")]
#![allow(non_snake_case, non_camel_case_types, clippy::upper_case_acronyms)]

use std::ffi::c_void;

// ─────────────────────────────────────────────────────────────────────────────
// Primitive type aliases matching Windows SDK conventions
// ─────────────────────────────────────────────────────────────────────────────

pub type ULONG64  = u64;
pub type UINT64   = u64;
pub type ULONG    = u32;
pub type UINT32   = u32;
pub type UINT16   = u16;
pub type UINT8    = u8;
pub type BOOLEAN  = u8;   // UCHAR in Windows SDK
pub type HANDLE   = *mut c_void;
pub type PVOID    = *mut c_void;
pub type PWCHAR   = *mut u16;
pub type HV_VP_INDEX = u32;

// ─────────────────────────────────────────────────────────────────────────────
// Enumerations
// ─────────────────────────────────────────────────────────────────────────────

/// Memory read interface selection.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ReadMemoryMethod {
    ReadInterfaceUnsupported     = 0,
    ReadInterfaceHvmmDrvInternal = 1,
    ReadInterfaceWinHv           = 2,
    ReadInterfaceHvmmLocal       = 3,
    ReadInterfaceMax             = 4,
}

/// Memory write interface selection.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum WriteMemoryMethod {
    WriteInterfaceUnsupported     = 0,
    WriteInterfaceHvmmDrvInternal = 1,
    WriteInterfaceWinHv           = 2,
    WriteInterfaceHvmmLocal       = 3,
    WriteInterfaceMax             = 4,
}

/// Suspend/resume method for a Hyper-V partition.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SuspendResumeMethod {
    SuspendResumeUnsupported       = 0,
    SuspendResumePowershell        = 1,
    SuspendResumeWriteSpecRegister = 2,
}

/// Memory address space selector.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MemoryAccessType {
    PhysicalMemory  = 0,
    VirtualMemory   = 1,
    AccessRtCore64  = 2,
}

/// Guest VM action: suspend or resume.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum VmStateAction {
    SuspendVm = 0,
    ResumeVm  = 1,
}

/// Guest operating system type.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GuestType {
    Unknown          = 0,
    Standard         = 1,
    NonKdbgPartition = 2,
    HyperV           = 3,
}

/// Guest machine architecture.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MachineType {
    Unknown     = 0,
    X86         = 1,
    Amd64       = 2,
    Arm64       = 3,
    Unsupported = 4,
}

/// Virtual Trust Level (VTL 0–15).
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum VtlLevel {
    Vtl0  = 0,  Vtl1  = 1,  Vtl2  = 2,  Vtl3  = 3,
    Vtl4  = 4,  Vtl5  = 5,  Vtl6  = 6,  Vtl7  = 7,
    Vtl8  = 8,  Vtl9  = 9,  Vtl10 = 10, Vtl11 = 11,
    Vtl12 = 12, Vtl13 = 13, Vtl14 = 14, Vtl15 = 15,
    BadVtl = 16,
}

/// Information class selector for `SdkGetData` / `SdkGetData2` / `SdkSetData`.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum HvmmInformationClass {
    InfoKdbgData                    = 0,
    InfoPartitionFriendlyName       = 1,
    InfoPartitionId                 = 2,
    InfoVmtypeString                = 3,
    InfoStructure                   = 4,
    InfoKiProcessorBlock            = 5,
    InfoMmMaximumPhysicalPage       = 6,
    InfoKPCR                        = 7,
    InfoNumberOfCPU                 = 8,
    InfoKDBGPa                      = 9,
    InfoNumberOfRuns                = 10,
    InfoKernelBase                  = 11,
    InfoMmPfnDatabase               = 12,
    InfoPsLoadedModuleList          = 13,
    InfoPsActiveProcessHead         = 14,
    InfoNtBuildNumber               = 15,
    InfoNtBuildNumberVA             = 16,
    InfoDirectoryTableBase          = 17,
    InfoRun                         = 18,
    InfoKdbgDataBlockArea           = 19,
    InfoVmGuidString                = 20,
    InfoPartitionHandle             = 21,
    InfoKdbgContext                 = 22,
    InfoKdVersionBlock              = 23,
    InfoMmPhysicalMemoryBlock       = 24,
    InfoNumberOfPages               = 25,
    InfoIdleKernelStack             = 26,
    InfoSizeOfKdDebuggerData        = 27,
    InfoCpuContextVa                = 28,
    InfoSize                        = 29,
    InfoMemoryBlockCount            = 30,
    InfoSuspendedCores              = 31,
    InfoSuspendedWorker             = 32,
    InfoIsContainer                 = 33,
    InfoIsNeedVmwpSuspend           = 34,
    InfoGuestOsType                 = 35,
    InfoSettingsCrashDumpEmulation  = 36,
    InfoSettingsUseDecypheredKdbg   = 37,
    InfoBuilLabBuffer               = 38,
    InfoHvddGetCr3byPid             = 39,
    InfoGetProcessesIds             = 40,
    InfoDumpHeaderPointer           = 41,
    InfoUpdateCr3ForLocal           = 42,
    InfoHvddGetCr3Kernel            = 43,
    InfoHvddGetCr3Hv                = 44,
    InfoHvddGetCr3Securekernel      = 45,
    InfoIsVmSuspended               = 46,
    InfoSecureKernelBase            = 47,
    InfoSecureKernelSize            = 48,
    InfoSetMemoryBlock              = 49,
    InfoEnlVmcsPointer              = 50,
}

// ─────────────────────────────────────────────────────────────────────────────
// HV_REGISTER_NAME — newtype over u32 with named constants
// ─────────────────────────────────────────────────────────────────────────────

/// Hyper-V virtual-processor register selector.
///
/// Defined as a transparent `u32` newtype so that any raw numeric code can be
/// passed without requiring an exhaustive match. Named constants cover the most
/// common x64 registers.
#[repr(transparent)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct HvRegisterName(pub u32);

#[allow(non_upper_case_globals)]
impl HvRegisterName {
    // General-purpose registers
    pub const Rax:    Self = Self(0x0002_0000);
    pub const Rcx:    Self = Self(0x0002_0001);
    pub const Rdx:    Self = Self(0x0002_0002);
    pub const Rbx:    Self = Self(0x0002_0003);
    pub const Rsp:    Self = Self(0x0002_0004);
    pub const Rbp:    Self = Self(0x0002_0005);
    pub const Rsi:    Self = Self(0x0002_0006);
    pub const Rdi:    Self = Self(0x0002_0007);
    pub const R8:     Self = Self(0x0002_0008);
    pub const R9:     Self = Self(0x0002_0009);
    pub const R10:    Self = Self(0x0002_000A);
    pub const R11:    Self = Self(0x0002_000B);
    pub const R12:    Self = Self(0x0002_000C);
    pub const R13:    Self = Self(0x0002_000D);
    pub const R14:    Self = Self(0x0002_000E);
    pub const R15:    Self = Self(0x0002_000F);
    pub const Rip:    Self = Self(0x0002_0010);
    pub const Rflags: Self = Self(0x0002_0011);
    // Control registers
    pub const Cr0:    Self = Self(0x0004_0000);
    pub const Cr2:    Self = Self(0x0004_0001);
    pub const Cr3:    Self = Self(0x0004_0002);
    pub const Cr4:    Self = Self(0x0004_0003);
    pub const Cr8:    Self = Self(0x0004_0004);
    // Debug registers
    pub const Dr0:    Self = Self(0x0005_0000);
    pub const Dr1:    Self = Self(0x0005_0001);
    pub const Dr2:    Self = Self(0x0005_0002);
    pub const Dr3:    Self = Self(0x0005_0003);
    pub const Dr6:    Self = Self(0x0005_0004);
    pub const Dr7:    Self = Self(0x0005_0005);
    // Segment registers
    pub const Es:     Self = Self(0x0006_0000);
    pub const Cs:     Self = Self(0x0006_0001);
    pub const Ss:     Self = Self(0x0006_0002);
    pub const Ds:     Self = Self(0x0006_0003);
    pub const Fs:     Self = Self(0x0006_0004);
    pub const Gs:     Self = Self(0x0006_0005);
}

// ─────────────────────────────────────────────────────────────────────────────
// HV_REGISTER_VALUE — 128-bit union
// ─────────────────────────────────────────────────────────────────────────────

/// 128-bit register value union. Use the `reg64` field for most general-purpose
/// and control registers.
#[repr(C, align(16))]
#[derive(Clone, Copy)]
pub union HvRegisterValue {
    /// Full 128-bit value (low/high 64-bit words).
    pub reg128: [u64; 2],
    /// 64-bit view (most registers).
    pub reg64:  u64,
    /// 32-bit view.
    pub reg32:  u32,
    /// 16-bit view.
    pub reg16:  u16,
    /// 8-bit view.
    pub reg8:   u8,
}

impl Default for HvRegisterValue {
    fn default() -> Self {
        Self { reg128: [0, 0] }
    }
}

impl std::fmt::Debug for HvRegisterValue {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        // Safety: reg128 is always valid (all-zeros default).
        write!(f, "HvRegisterValue({:#018x})", unsafe { self.reg64 })
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// VM_OPERATIONS_CONFIG
// ─────────────────────────────────────────────────────────────────────────────

/// SDK configuration passed to `sdk_enum_partitions` and `sdk_set_partition_config`.
///
/// Field layout must exactly match the C `VM_OPERATIONS_CONFIG` struct.
/// Padding is inserted by `#[repr(C)]` to match MSVC alignment rules:
///   - 4 bytes after `suspend_method` (before the u64 `log_level`)
///   - 6 bytes after `pause_partition` (before the pointer `exdi_console_handle`)
///   - trailing padding to reach 56-byte total
#[repr(C)]
#[derive(Debug, Clone)]
pub struct VmOperationsConfig {
    pub read_method:                ReadMemoryMethod,
    pub write_method:               WriteMemoryMethod,
    pub suspend_method:             SuspendResumeMethod,
    // 4 bytes implicit padding here (MSVC aligns u64 to 8)
    pub log_level:                  u64,
    pub force_freeze_cpu:           BOOLEAN,
    pub pause_partition:            BOOLEAN,
    // 6 bytes implicit padding here (MSVC aligns HANDLE/pointer to 8)
    pub exdi_console_handle:        HANDLE,
    pub reload_driver:              BOOLEAN,
    pub pf_injection:               BOOLEAN,
    pub nested_scan:                BOOLEAN,
    pub use_debug_api_stop_process: BOOLEAN,
    pub simple_memory:              BOOLEAN,
    pub replace_decyphered_kdbg:    BOOLEAN,
    pub full_crash_dump_emulation:  BOOLEAN,
    pub enum_guest_os_build:        BOOLEAN,
    pub vsm_scan:                   BOOLEAN,
    pub log_silence_mode:           BOOLEAN,
}

impl Default for VmOperationsConfig {
    fn default() -> Self {
        // Safety: all-zero is a valid representation for this C struct.
        unsafe { std::mem::zeroed() }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Raw FFI declarations (unsafe, matches hvlib.dll exports)
// ─────────────────────────────────────────────────────────────────────────────

#[link(name = "hvlib")]
extern "C" {
    pub fn SdkGetDefaultConfig(config: *mut VmOperationsConfig) -> BOOLEAN;
    pub fn SdkSetPartitionConfig(handle: ULONG64, config: *const VmOperationsConfig) -> BOOLEAN;
    pub fn SdkGetPartitionConfig(handle: ULONG64) -> *mut VmOperationsConfig;

    pub fn SdkEnumPartitions(
        count: *mut ULONG64,
        config: *const VmOperationsConfig,
    ) -> *mut ULONG64;

    pub fn SdkSelectPartition(handle: ULONG64) -> BOOLEAN;

    pub fn SdkGetData(
        handle: ULONG64,
        info_class: HvmmInformationClass,
        out: PVOID,
    ) -> BOOLEAN;
    pub fn SdkGetData2(handle: ULONG64, info_class: HvmmInformationClass) -> ULONG64;
    pub fn SdkSetData(
        handle: ULONG64,
        info_class: HvmmInformationClass,
        value: ULONG64,
    ) -> ULONG64;

    pub fn SdkReadPhysicalMemory(
        handle: ULONG64,
        start: UINT64,
        count: UINT64,
        buf: PVOID,
        method: ReadMemoryMethod,
    ) -> BOOLEAN;
    pub fn SdkWritePhysicalMemory(
        handle: ULONG64,
        start: UINT64,
        count: UINT64,
        buf: PVOID,
        method: WriteMemoryMethod,
    ) -> BOOLEAN;

    pub fn SdkReadVirtualMemory(
        handle: ULONG64,
        va: ULONG64,
        buf: PVOID,
        size: ULONG,
    ) -> BOOLEAN;
    pub fn SdkWriteVirtualMemory(
        handle: ULONG64,
        va: ULONG64,
        buf: PVOID,
        size: ULONG,
    ) -> BOOLEAN;

    pub fn SdkGetPhysicalAddress(
        handle: ULONG64,
        va: ULONG64,
        access: MemoryAccessType,
    ) -> ULONG64;

    pub fn SdkGetMachineType(handle: ULONG64) -> MachineType;

    pub fn SdkReadVpRegister(
        handle: ULONG64,
        vp_index: HV_VP_INDEX,
        vtl: VtlLevel,
        reg: HvRegisterName,
        value: *mut HvRegisterValue,
    ) -> BOOLEAN;
    pub fn SdkWriteVpRegister(
        handle: ULONG64,
        vp_index: HV_VP_INDEX,
        vtl: VtlLevel,
        reg: HvRegisterName,
        value: *const HvRegisterValue,
    ) -> BOOLEAN;

    pub fn SdkControlVmState(
        handle: ULONG64,
        action: VmStateAction,
        method: SuspendResumeMethod,
        manage_worker: BOOLEAN,
    ) -> BOOLEAN;

    pub fn SdkClosePartition(handle: ULONG64);
    pub fn SdkCloseAllPartitions() -> BOOLEAN;

    pub fn SdkInvokeHypercall(
        hv_call_id: UINT32,
        is_fast: BOOLEAN,
        rep_start_index: UINT32,
        count_of_elements: UINT32,
        is_nested: BOOLEAN,
        input_buf: PVOID,
        output_buf: PVOID,
    ) -> BOOLEAN;

}

// ─────────────────────────────────────────────────────────────────────────────
// Safe public wrappers
// ─────────────────────────────────────────────────────────────────────────────

/// Fill `config` with SDK defaults (preferred read/write/suspend methods for
/// the current host OS).
pub fn sdk_get_default_config(config: &mut VmOperationsConfig) -> bool {
    unsafe { SdkGetDefaultConfig(config as *mut _) != 0 }
}

/// Apply `config` to a specific already-opened partition.
pub fn sdk_set_partition_config(handle: u64, config: &VmOperationsConfig) -> bool {
    unsafe { SdkSetPartitionConfig(handle, config as *const _) != 0 }
}

/// Enumerate running Hyper-V partitions. Returns a `Vec<u64>` of opaque handles.
///
/// The returned handles are valid until `sdk_close_all_partitions` is called.
/// `config.reload_driver = 1` is recommended on the first call.
pub fn sdk_enum_partitions(config: &VmOperationsConfig) -> Vec<u64> {
    let mut count: u64 = 0;
    let ptr = unsafe { SdkEnumPartitions(&mut count, config as *const _) };
    if ptr.is_null() || count == 0 {
        return Vec::new();
    }
    // Copy handles — the array is owned by the SDK.
    unsafe { std::slice::from_raw_parts(ptr, count as usize).to_vec() }
}

/// Initialize a partition handle (resolves kernel symbols, etc.).
/// Must be called before memory/register operations.
pub fn sdk_select_partition(handle: u64) -> bool {
    unsafe { SdkSelectPartition(handle) != 0 }
}

/// Retrieve a scalar u64 value for the given information class.
pub fn sdk_get_data2(handle: u64, info_class: HvmmInformationClass) -> u64 {
    unsafe { SdkGetData2(handle, info_class) }
}

/// Retrieve data by writing the result into a caller-supplied pointer.
///
/// # Safety
/// `out` must point to a location appropriate for the requested `info_class`.
pub unsafe fn sdk_get_data_raw(
    handle: u64,
    info_class: HvmmInformationClass,
    out: *mut c_void,
) -> bool {
    SdkGetData(handle, info_class, out) != 0
}

/// Retrieve a `*mut u16` (WCHAR pointer) field and convert it to a `String`.
///
/// Used for `InfoPartitionFriendlyName`, `InfoVmtypeString`, `InfoVmGuidString`.
pub fn sdk_get_data_wstr(handle: u64, info_class: HvmmInformationClass) -> Option<String> {
    let mut ptr: *mut u16 = std::ptr::null_mut();
    let ok = unsafe {
        SdkGetData(
            handle,
            info_class,
            &mut ptr as *mut *mut u16 as *mut c_void,
        ) != 0
    };
    if !ok || ptr.is_null() {
        return None;
    }
    Some(wchar_ptr_to_string(ptr))
}

/// Retrieve a u64 field (e.g. `InfoPartitionId`, `InfoKernelBase`).
pub fn sdk_get_data_u64(handle: u64, info_class: HvmmInformationClass) -> u64 {
    let mut value: u64 = 0;
    unsafe {
        SdkGetData(
            handle,
            info_class,
            &mut value as *mut u64 as *mut c_void,
        );
    }
    value
}

/// Read bytes from guest physical address space (GPA → host buffer).
pub fn sdk_read_physical_memory(
    handle: u64,
    gpa: u64,
    buf: &mut [u8],
    method: ReadMemoryMethod,
) -> bool {
    if buf.is_empty() {
        return false;
    }
    unsafe {
        SdkReadPhysicalMemory(
            handle,
            gpa,
            buf.len() as u64,
            buf.as_mut_ptr() as *mut c_void,
            method,
        ) != 0
    }
}

/// Write bytes from host buffer to guest physical address space.
pub fn sdk_write_physical_memory(
    handle: u64,
    gpa: u64,
    buf: &[u8],
    method: WriteMemoryMethod,
) -> bool {
    if buf.is_empty() {
        return false;
    }
    unsafe {
        SdkWritePhysicalMemory(
            handle,
            gpa,
            buf.len() as u64,
            buf.as_ptr() as *mut c_void,
            method,
        ) != 0
    }
}

/// Read bytes from guest virtual address space (GVA → host buffer).
pub fn sdk_read_virtual_memory(handle: u64, gva: u64, buf: &mut [u8]) -> bool {
    if buf.is_empty() {
        return false;
    }
    unsafe {
        SdkReadVirtualMemory(
            handle,
            gva,
            buf.as_mut_ptr() as *mut c_void,
            buf.len() as u32,
        ) != 0
    }
}

/// Write bytes from host buffer to guest virtual address space.
pub fn sdk_write_virtual_memory(handle: u64, gva: u64, buf: &[u8]) -> bool {
    if buf.is_empty() {
        return false;
    }
    unsafe {
        SdkWriteVirtualMemory(
            handle,
            gva,
            buf.as_ptr() as *mut c_void,
            buf.len() as u32,
        ) != 0
    }
}

/// Translate a guest virtual address to a guest physical address.
pub fn sdk_get_physical_address(
    handle: u64,
    gva: u64,
    access: MemoryAccessType,
) -> u64 {
    unsafe { SdkGetPhysicalAddress(handle, gva, access) }
}

/// Return the guest machine architecture.
pub fn sdk_get_machine_type(handle: u64) -> MachineType {
    unsafe { SdkGetMachineType(handle) }
}

/// Read a virtual-processor register. Returns `None` if the call fails.
pub fn sdk_read_vp_register(
    handle: u64,
    vp_index: u32,
    vtl: VtlLevel,
    reg: HvRegisterName,
) -> Option<HvRegisterValue> {
    let mut value = HvRegisterValue::default();
    let ok = unsafe { SdkReadVpRegister(handle, vp_index, vtl, reg, &mut value) != 0 };
    if ok { Some(value) } else { None }
}

/// Write a virtual-processor register. Returns `false` if the call fails.
pub fn sdk_write_vp_register(
    handle: u64,
    vp_index: u32,
    vtl: VtlLevel,
    reg: HvRegisterName,
    value: &HvRegisterValue,
) -> bool {
    unsafe { SdkWriteVpRegister(handle, vp_index, vtl, reg, value as *const _) != 0 }
}

/// Suspend or resume a Hyper-V partition.
pub fn sdk_control_vm_state(
    handle: u64,
    action: VmStateAction,
    method: SuspendResumeMethod,
    manage_worker: bool,
) -> bool {
    unsafe { SdkControlVmState(handle, action, method, manage_worker as u8) != 0 }
}

/// Close a single partition handle.
pub fn sdk_close_partition(handle: u64) {
    unsafe { SdkClosePartition(handle) }
}

/// Close all open partition handles and free SDK resources.
pub fn sdk_close_all_partitions() -> bool {
    unsafe { SdkCloseAllPartitions() != 0 }
}


// ─────────────────────────────────────────────────────────────────────────────
// Convenience helpers
// ─────────────────────────────────────────────────────────────────────────────

/// Convert a NUL-terminated UTF-16LE `*const u16` to a `String`.
/// Invalid surrogate pairs are replaced with U+FFFD.
pub fn wchar_ptr_to_string(ptr: *const u16) -> String {
    if ptr.is_null() {
        return String::new();
    }
    let mut len = 0usize;
    unsafe {
        while *ptr.add(len) != 0 {
            len += 1;
        }
        let slice = std::slice::from_raw_parts(ptr, len);
        String::from_utf16_lossy(slice).to_owned()
    }
}

/// Friendly name of the partition (e.g. "Win11-Guest").
pub fn sdk_get_friendly_name(handle: u64) -> String {
    sdk_get_data_wstr(handle, HvmmInformationClass::InfoPartitionFriendlyName)
        .unwrap_or_default()
}

/// VM type string (e.g. "Standard", "HyperV").
pub fn sdk_get_vm_type_string(handle: u64) -> String {
    sdk_get_data_wstr(handle, HvmmInformationClass::InfoVmtypeString)
        .unwrap_or_default()
}

/// VM GUID string.
pub fn sdk_get_vm_guid_string(handle: u64) -> String {
    sdk_get_data_wstr(handle, HvmmInformationClass::InfoVmGuidString)
        .unwrap_or_default()
}

/// Numeric partition ID.
pub fn sdk_get_partition_id(handle: u64) -> u64 {
    sdk_get_data_u64(handle, HvmmInformationClass::InfoPartitionId)
}

/// Guest kernel base virtual address.
pub fn sdk_get_kernel_base(handle: u64) -> u64 {
    sdk_get_data2(handle, HvmmInformationClass::InfoKernelBase)
}

/// Total physical pages in the guest.
pub fn sdk_get_max_physical_pages(handle: u64) -> u64 {
    let mut pages: u64 = 0;
    unsafe {
        SdkGetData(
            handle,
            HvmmInformationClass::InfoMmMaximumPhysicalPage,
            &mut pages as *mut u64 as *mut c_void,
        );
    }
    pages
}

/// Guest OS type.
pub fn sdk_get_guest_os_type(handle: u64) -> GuestType {
    match sdk_get_data2(handle, HvmmInformationClass::InfoGuestOsType) {
        1 => GuestType::Standard,
        2 => GuestType::NonKdbgPartition,
        3 => GuestType::HyperV,
        _ => GuestType::Unknown,
    }
}

/// `KiProcessorBlock` virtual address (pointer array of KPRCB pointers).
pub fn sdk_get_ki_processor_block(handle: u64) -> u64 {
    sdk_get_data_u64(handle, HvmmInformationClass::InfoKiProcessorBlock)
}

// ─────────────────────────────────────────────────────────────────────────────
// Crash dump (.dmp) support
// ─────────────────────────────────────────────────────────────────────────────

// Signatures
pub const DUMP_SIGNATURE: u32     = 0x4547_4150; // 'PAGE'
pub const DUMP_VALID_DUMP64: u32  = 0x3436_5544; // 'DU64'
pub const DUMP_TYPE_FULL: u32     = 1;
pub const IMAGE_FILE_MACHINE_AMD64: u32 = 0x8664;

// Exception constants
pub const STATUS_BREAKPOINT: u32       = 0x8000_0003;
pub const EXCEPTION_NONCONTINUABLE: u32 = 1;

// x64 GDT selectors
pub const KGDT64_R0_CODE:  u16 = 0x10;
pub const KGDT64_R0_DATA:  u16 = 0x18;
pub const KGDT64_R3_DATA:  u16 = 0x28;
pub const KGDT64_R3_CMTEB: u16 = 0x50;
pub const RPL_MASK: u16 = 3;

pub const DUMP_COMMENT: &[u8] =
    b"Hyper-V Memory Dump. (c) 2010 MoonSols SARL <http://www.moonsols.com>";

/// `sizeof(CONTEXT)` on x64 Windows.
pub const X64_CONTEXT_SIZE: usize = 1232;

/// Offsets of segment-selector fields inside `X64_CONTEXT`.
pub const CTX_SEG_CS: usize = 0x38;
pub const CTX_SEG_DS: usize = 0x3A;
pub const CTX_SEG_ES: usize = 0x3C;
pub const CTX_SEG_FS: usize = 0x3E;
pub const CTX_SEG_GS: usize = 0x40;
pub const CTX_SEG_SS: usize = 0x42;
pub const CTX_RIP:    usize = 0xF8;
pub const CTX_RSP:    usize = 0x98;

/// Size of `KDDEBUGGER_DATA64` (matches `KD_DEBUGGER_BLOCK_PAGE_SIZE`).
pub const KD_DEBUGGER_BLOCK_SIZE: usize = 0x500;

// ── Crash dump structs ───────────────────────────────────────────────────────

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ExceptionRecord64 {
    pub exception_code:        u32,
    pub exception_flags:       u32,
    pub exception_record:      u64,
    pub exception_address:     u64,
    pub number_parameters:     u32,
    pub _unused_alignment:     u32,
    pub exception_information: [u64; 15],
}

/// Windows 64-bit crash dump header (`DUMP_HEADER64`).
///
/// Exactly 8192 bytes (2 pages).  Written as the first 8 KiB of a `.dmp` file,
/// followed by raw guest physical memory in 1 MiB blocks.
#[repr(C)]
pub struct DumpHeader64 {
    pub signature:             u32,        // 0
    pub valid_dump:            u32,        // 4
    pub major_version:         u32,        // 8
    pub minor_version:         u32,        // 12
    pub directory_table_base:  u64,        // 16
    pub pfn_database:          u64,        // 24
    pub ps_loaded_module_list: u64,        // 32
    pub ps_active_process_head:u64,        // 40
    pub machine_image_type:    u32,        // 48
    pub number_processors:     u32,        // 52
    pub bug_check_code:        u32,        // 56
    pub _pad0:                 u32,        // 60
    pub bug_check_parameter1:  u64,        // 64
    pub bug_check_parameter2:  u64,        // 72
    pub bug_check_parameter3:  u64,        // 80
    pub bug_check_parameter4:  u64,        // 88
    pub version_user:          [u8; 32],   // 96
    pub kd_debugger_data_block:u64,        // 128
    pub physical_memory_block: [u8; 700],  // 136
    pub context_record:        [u8; 3000], // 836
    pub _pad1:                 [u8; 4],    // 3836
    pub exception_record:      ExceptionRecord64, // 3840
    pub dump_type:             u32,        // 3992
    pub _pad2:                 u32,        // 3996
    pub required_dump_space:   i64,        // 4000
    pub system_time:           u64,        // 4008
    pub comment:               [u8; 128],  // 4016
    pub system_up_time:        u64,        // 4144
    pub mini_dump_fields:      u32,        // 4152
    pub secondary_data_state:  u32,        // 4156
    pub product_type:          u32,        // 4160
    pub suite_mask:            u32,        // 4164
    pub writer_status:         u32,        // 4168
    pub unused1:               u8,         // 4172
    pub kd_secondary_version:  u8,         // 4173
    pub unused:                [u8; 2],    // 4174
    pub _reserved0:            [u8; 4016], // 4176
    // total: 8192
}

/// Compile-time size check.
const _: () = assert!(std::mem::size_of::<DumpHeader64>() == 8192);

/// Size of `DUMP_HEADER64` in bytes (2 pages).
pub const DUMP_HEADER64_SIZE: usize = std::mem::size_of::<DumpHeader64>();

// Crash dump construction helpers (dump_fill_header64, sdk_get_dump_header,
// dump_header_as_bytes, gather_crash_dump_context, patch_crash_dump_block,
// fix_context_segments, now_as_filetime, fill_phys_mem_block, CrashDumpContext)
// live in the example crate (LiveCloudKdExampleRs/src/main.rs) since they are
// example-specific wrappers around the raw SDK queries exposed by this crate.
