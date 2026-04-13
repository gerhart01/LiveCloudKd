//! LiveCloudKd Rust example — mirrors LiveCloudKdExample.c
//!
//! Demonstrates:
//!  1. Default SDK config + partition enumeration
//!  2. Partition selection
//!  3. Machine type and guest OS type
//!  4. Physical memory read (GPA 0)
//!  5. VA → PA translation
//!  6. VP register read/write (RIP on VP 0)
//!  7. Virtual memory read (KiProcessorBlock)
//!  8. Optional: full physical memory dump to a raw file
//!  9. Optional: crash dump (.dmp) for WinDbg

#![cfg(target_os = "windows")]

use livecloudkd::*;
use std::ffi::c_void;
use std::io::{self, Write};
use std::fs::File;

const DUMP_PAGE_SIZE: u64 = 0x1000;
const DUMP_BLOCK_SIZE: u64 = 1024 * 1024; // 1 MB

extern "C" {
    fn _getch() -> i32;
}

// ─────────────────────────────────────────────────────────────────────────────
// I/O helpers
// ─────────────────────────────────────────────────────────────────────────────

/// Read a single digit (0..max_exclusive) without requiring Enter.
fn prompt_digit(max_exclusive: usize) -> usize {
    loop {
        print!("   > ");
        io::stdout().flush().ok();
        let ch = unsafe { _getch() };
        if ch >= b'0' as i32 && ch <= b'9' as i32 {
            let n = (ch - b'0' as i32) as usize;
            if n < max_exclusive {
                println!("{}", n);
                return n;
            }
        }
    }
}

fn read_line_trimmed() -> String {
    let mut line = String::new();
    io::stdin().read_line(&mut line).unwrap_or_default();
    line.trim().to_string()
}

// ─────────────────────────────────────────────────────────────────────────────
// Dump full guest physical memory to a raw file (1 MB blocks)
// ─────────────────────────────────────────────────────────────────────────────

fn dump_vm(handle: u64, dest: &str, read_method: ReadMemoryMethod) {
    let page_count = sdk_get_max_physical_pages(handle);
    if page_count == 0 {
        eprintln!("   dump_vm: zero pages reported");
        return;
    }

    let mut file = match File::create(dest) {
        Ok(f) => f,
        Err(e) => {
            eprintln!("   dump_vm: cannot create '{}': {}", dest, e);
            return;
        }
    };

    let total_bytes = page_count * DUMP_PAGE_SIZE;
    println!("   PageCountTotal = {:#x}", page_count);
    println!("   Total size:     {} MB", total_bytes / (1024 * 1024));
    println!("   Starting dump...");

    let mut buf = vec![0u8; DUMP_BLOCK_SIZE as usize];

    let mut offset: u64 = 0;
    while offset < total_bytes {
        if offset % (50 * 1024 * 1024) == 0 {
            println!("   {} MB...", offset / (1024 * 1024));
        }

        let ok = sdk_read_physical_memory(handle, offset, &mut buf, read_method);
        if !ok {
            buf.iter_mut().for_each(|b| *b = 0);
        }

        if let Err(e) = file.write_all(&buf) {
            eprintln!("   dump_vm: write error: {}", e);
            return;
        }

        offset += DUMP_BLOCK_SIZE;
    }

    println!("   Done.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Crash dump (.dmp) — produces a file WinDbg can open directly
//
// Two header variants:
//   (a) dump_fill_header64()  — manual construction (DumpFillHeader64 port)
//   (b) sdk_get_dump_header() — pre-built header via InfoDumpHeaderPointer
//
// After the header the function writes physical memory in 1 MB blocks,
// patching CPU-context pages and the KDBG block inline (same logic as
// DumpCrashVirtualMachine in LiveCloudKd/src/dump.c).
// ─────────────────────────────────────────────────────────────────────────────

fn dump_crash_vm(handle: u64, dest: &str, read_method: ReadMemoryMethod) {
    // ── Build header ─────────────────────────────────────────────────────────
    println!("   Building header (DumpFillHeader64)...");
    let header = match dump_fill_header64(handle) {
        Some(h) => h,
        None => {
            eprintln!("   dump_fill_header64 failed");
            return;
        }
    };

    fn u32_to_ascii(v: u32) -> String {
        let b = v.to_le_bytes();
        b.iter().map(|&c| if c.is_ascii_graphic() { c as char } else { '.' }).collect()
    }
    println!("   Header: signature={} valid={} dump_type={} KDBG={:#x}",
             u32_to_ascii(header.signature),
             u32_to_ascii(header.valid_dump),
             header.dump_type,
             header.kd_debugger_data_block);

    // ── Create file ──────────────────────────────────────────────────────────
    let mut file = match File::create(dest) {
        Ok(f) => f,
        Err(e) => {
            eprintln!("   Cannot create '{}': {}", dest, e);
            return;
        }
    };

    // ── Write header ─────────────────────────────────────────────────────────
    if let Err(e) = file.write_all(dump_header_as_bytes(&header)) {
        eprintln!("   Header write error: {}", e);
        return;
    }
    println!("   Header written ({} bytes)", DUMP_HEADER64_SIZE);

    // ── Gather patching context ──────────────────────────────────────────────
    let ctx = gather_crash_dump_context(handle);
    println!("   CPUs with context PA: {}, KDBG PA: {:#x}, has_context: {}",
             ctx.context_pa.len(), ctx.kdbg_pa, ctx.has_context);

    // ── Write physical memory with patching ──────────────────────────────────
    let page_count = sdk_get_max_physical_pages(handle);
    let total_bytes = page_count * DUMP_PAGE_SIZE;
    let total_with_header = total_bytes + DUMP_HEADER64_SIZE as u64;
    println!("   Total dump size: {} MB (header + {} MB phys mem)",
             total_with_header / (1024 * 1024),
             total_bytes / (1024 * 1024));
    println!("   Starting...");

    let mut buf = vec![0u8; DUMP_BLOCK_SIZE as usize];
    let mut offset: u64 = 0;
    while offset < total_bytes {
        if offset % (50 * 1024 * 1024) == 0 {
            println!("   {} MB...", offset / (1024 * 1024));
        }

        let ok = sdk_read_physical_memory(handle, offset, &mut buf, read_method);
        if !ok {
            buf.fill(0);
        }

        // Patch context pages and KDBG block
        patch_crash_dump_block(&mut buf, offset, &ctx);

        if let Err(e) = file.write_all(&buf) {
            eprintln!("   Write error at offset {:#x}: {}", offset, e);
            return;
        }

        offset += DUMP_BLOCK_SIZE;
    }

    println!("   Done. Crash dump saved to: {}", dest);
}

// ─────────────────────────────────────────────────────────────────────────────
// Crash dump construction helpers (moved out of the library crate — these are
// example-specific wrappers around the raw SDK queries exposed by livecloudkd)
// ─────────────────────────────────────────────────────────────────────────────

fn write_u16_le(buf: &mut [u8], off: usize, v: u16) {
    buf[off..off + 2].copy_from_slice(&v.to_le_bytes());
}
fn read_u16_le(buf: &[u8], off: usize) -> u16 {
    u16::from_le_bytes([buf[off], buf[off + 1]])
}
fn read_u64_le(buf: &[u8], off: usize) -> u64 {
    let mut b = [0u8; 8];
    b.copy_from_slice(&buf[off..off + 8]);
    u64::from_le_bytes(b)
}

/// Fill the physical-memory-block area inside the header
/// (single run: BasePage=0, PageCount=max_pages).
fn fill_phys_mem_block(buf: &mut [u8; 700], max_pages: u64) {
    buf.fill(0);
    buf[0..4].copy_from_slice(&1u32.to_le_bytes());
    buf[8..16].copy_from_slice(&max_pages.to_le_bytes());
    buf[16..24].copy_from_slice(&0u64.to_le_bytes());
    buf[24..32].copy_from_slice(&max_pages.to_le_bytes());
}

/// Fix segment selectors in a context record to valid kernel-mode values.
pub fn fix_context_segments(ctx: &mut [u8]) {
    if ctx.len() < X64_CONTEXT_SIZE {
        return;
    }
    let seg_cs = read_u16_le(ctx, CTX_SEG_CS);
    if seg_cs != KGDT64_R0_CODE {
        write_u16_le(ctx, CTX_SEG_CS, KGDT64_R0_CODE);
    }
    let expected_ds = KGDT64_R3_DATA | RPL_MASK;
    if read_u16_le(ctx, CTX_SEG_DS) != expected_ds {
        write_u16_le(ctx, CTX_SEG_DS, expected_ds);
    }
    if read_u16_le(ctx, CTX_SEG_ES) != expected_ds {
        write_u16_le(ctx, CTX_SEG_ES, expected_ds);
    }
    let expected_fs = KGDT64_R3_CMTEB | RPL_MASK;
    if read_u16_le(ctx, CTX_SEG_FS) != expected_fs {
        write_u16_le(ctx, CTX_SEG_FS, expected_fs);
    }
    if read_u16_le(ctx, CTX_SEG_GS) != 0 {
        write_u16_le(ctx, CTX_SEG_GS, 0);
    }
    if read_u16_le(ctx, CTX_SEG_SS) != KGDT64_R0_DATA {
        write_u16_le(ctx, CTX_SEG_SS, KGDT64_R0_DATA);
    }
}

/// Windows FILETIME: 100-ns intervals since 1601-01-01.
fn now_as_filetime() -> u64 {
    let dur = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default();
    const EPOCH_DIFF: u64 = 116_444_736_000_000_000;
    let ticks = dur.as_nanos() as u64 / 100;
    ticks + EPOCH_DIFF
}

// ── Approach 1: manual header construction (DumpFillHeader64 translation) ────

/// Build a `DUMP_HEADER64` by querying the SDK for all required fields.
///
/// Direct Rust translation of `DumpFillHeader64()` from `LiveCloudKd/src/dump.c`.
pub fn dump_fill_header64(handle: u64) -> Option<Box<DumpHeader64>> {
    let mut header: Box<DumpHeader64> = unsafe {
        let mut h = Box::new(std::mem::zeroed::<DumpHeader64>());
        let words = std::slice::from_raw_parts_mut(
            &mut *h as *mut DumpHeader64 as *mut u32,
            DUMP_HEADER64_SIZE / 4,
        );
        for w in words.iter_mut() {
            *w = DUMP_SIGNATURE;
        }
        h
    };

    header.signature          = DUMP_SIGNATURE;
    header.valid_dump         = DUMP_VALID_DUMP64;
    header.dump_type          = DUMP_TYPE_FULL;
    header.machine_image_type = IMAGE_FILE_MACHINE_AMD64;

    let build = sdk_get_data_u64(handle, HvmmInformationClass::InfoNtBuildNumber);
    header.minor_version = (build & 0xFFFF) as u32;
    header.major_version = (build >> 28) as u32;

    header.directory_table_base   = sdk_get_data_u64(handle, HvmmInformationClass::InfoDirectoryTableBase);
    header.pfn_database           = sdk_get_data_u64(handle, HvmmInformationClass::InfoMmPfnDatabase);
    header.ps_loaded_module_list  = sdk_get_data_u64(handle, HvmmInformationClass::InfoPsLoadedModuleList);
    header.ps_active_process_head = sdk_get_data_u64(handle, HvmmInformationClass::InfoPsActiveProcessHead);
    header.number_processors      = sdk_get_data_u64(handle, HvmmInformationClass::InfoNumberOfCPU) as u32;
    header.kd_debugger_data_block = sdk_get_data_u64(handle, HvmmInformationClass::InfoKdbgData);

    header.bug_check_code       = 0x4D41_5454; // 'MATT'
    header.bug_check_parameter1 = 1;
    header.bug_check_parameter2 = 2;
    header.bug_check_parameter3 = 3;
    header.bug_check_parameter4 = 4;

    header.version_user = [0u8; 32];

    let max_pages = sdk_get_max_physical_pages(handle);
    fill_phys_mem_block(&mut header.physical_memory_block, max_pages);

    header.exception_record = ExceptionRecord64 {
        exception_code:    STATUS_BREAKPOINT,
        exception_flags:   EXCEPTION_NONCONTINUABLE,
        exception_record:  0,
        exception_address: 0xDEAD_BABE,
        number_parameters: 0,
        _unused_alignment: 0,
        exception_information: [0u64; 15],
    };

    header.system_time = now_as_filetime();

    header.required_dump_space =
        (max_pages * 0x1000) as i64 + DUMP_HEADER64_SIZE as i64;

    header.context_record = [0u8; 3000];
    let mut context_va_ptr: *const u64 = std::ptr::null();
    unsafe {
        SdkGetData(
            handle,
            HvmmInformationClass::InfoKdbgContext,
            &mut context_va_ptr as *mut _ as *mut c_void,
        );
    }
    if !context_va_ptr.is_null() {
        let ctx_va = unsafe { *context_va_ptr };
        if ctx_va != 0 {
            let mut ctx_buf = [0u8; X64_CONTEXT_SIZE];
            if sdk_read_virtual_memory(handle, ctx_va, &mut ctx_buf) {
                header.context_record[..X64_CONTEXT_SIZE].copy_from_slice(&ctx_buf);
                fix_context_segments(&mut header.context_record);
            }
        }
    }

    header.comment = [0u8; 128];
    let n = DUMP_COMMENT.len().min(127);
    header.comment[..n].copy_from_slice(&DUMP_COMMENT[..n]);

    Some(header)
}

// ── Approach 2: header from SDK via InfoDumpHeaderPointer ────────────────────

/// Retrieve the pre-built `DUMP_HEADER64` from the SDK.
///
/// Requires `VmOperationsConfig.full_crash_dump_emulation = 1` to be set
/// **before** calling `sdk_select_partition`.
pub fn sdk_get_dump_header(handle: u64) -> Option<Box<DumpHeader64>> {
    let mut ptr: *const u8 = std::ptr::null();
    let ok = unsafe {
        SdkGetData(
            handle,
            HvmmInformationClass::InfoDumpHeaderPointer,
            &mut ptr as *mut _ as *mut c_void,
        ) != 0
    };
    if !ok || ptr.is_null() {
        return None;
    }
    unsafe {
        let mut header = Box::new(std::mem::zeroed::<DumpHeader64>());
        std::ptr::copy_nonoverlapping(
            ptr,
            &mut *header as *mut DumpHeader64 as *mut u8,
            DUMP_HEADER64_SIZE,
        );
        Some(header)
    }
}

/// View a `DumpHeader64` as a raw byte slice.
pub fn dump_header_as_bytes(header: &DumpHeader64) -> &[u8] {
    unsafe {
        std::slice::from_raw_parts(
            header as *const DumpHeader64 as *const u8,
            DUMP_HEADER64_SIZE,
        )
    }
}

/// Context patching data needed during crash dump memory writes.
pub struct CrashDumpContext {
    pub context_pa: Vec<u64>,
    pub cpu_context: Vec<u8>,
    pub has_context: bool,
    pub kdbg_pa: u64,
    pub kdbg_data: Vec<u8>,
}

/// Gather context-patching metadata from the SDK.
pub fn gather_crash_dump_context(handle: u64) -> CrashDumpContext {
    let num_cpu = sdk_get_data_u64(handle, HvmmInformationClass::InfoNumberOfCPU) as usize;

    let mut ctx_va_ptr: *const u64 = std::ptr::null();
    unsafe {
        SdkGetData(handle, HvmmInformationClass::InfoKdbgContext,
                   &mut ctx_va_ptr as *mut _ as *mut c_void);
    }

    let mut cpu_ctx_va_ptr: *const u64 = std::ptr::null();
    unsafe {
        SdkGetData(handle, HvmmInformationClass::InfoCpuContextVa,
                   &mut cpu_ctx_va_ptr as *mut _ as *mut c_void);
    }

    let mut context_pa = Vec::with_capacity(num_cpu);
    if !ctx_va_ptr.is_null() {
        for i in 0..num_cpu {
            let va = unsafe { *ctx_va_ptr.add(i) };
            let pa = sdk_get_physical_address(handle, va, MemoryAccessType::VirtualMemory);
            context_pa.push(pa);
        }
    }

    let mut cpu_context = vec![0u8; X64_CONTEXT_SIZE];
    let mut has_context = false;
    if !cpu_ctx_va_ptr.is_null() {
        for i in 0..num_cpu {
            let va = unsafe { *cpu_ctx_va_ptr.add(i) };
            if sdk_read_virtual_memory(handle, va, &mut cpu_context) {
                let rip = read_u64_le(&cpu_context, CTX_RIP);
                if rip != 0 {
                    has_context = true;
                    break;
                }
            }
        }
    }

    let mut kdbg_block_ptr: *const u8 = std::ptr::null();
    unsafe {
        SdkGetData(handle, HvmmInformationClass::InfoKdbgDataBlockArea,
                   &mut kdbg_block_ptr as *mut _ as *mut c_void);
    }

    let kdbg_size = {
        let s = sdk_get_data2(handle, HvmmInformationClass::InfoSizeOfKdDebuggerData) as usize;
        if s == 0 { KD_DEBUGGER_BLOCK_SIZE } else { s }
    };

    let mut kdbg_data = vec![0u8; kdbg_size];
    if !kdbg_block_ptr.is_null() {
        unsafe {
            std::ptr::copy_nonoverlapping(kdbg_block_ptr, kdbg_data.as_mut_ptr(), kdbg_size);
        }
    }

    let kdbg_pa = sdk_get_data_u64(handle, HvmmInformationClass::InfoKDBGPa);

    CrashDumpContext { context_pa, cpu_context, has_context, kdbg_pa, kdbg_data }
}

/// Patch a physical-memory block in-place with CPU context and KDBG data.
pub fn patch_crash_dump_block(
    block: &mut [u8],
    block_offset: u64,
    ctx: &CrashDumpContext,
) {
    let block_end = block_offset + block.len() as u64;

    for &pa in &ctx.context_pa {
        if pa >= block_offset && pa < block_end {
            let off = (pa - block_offset) as usize;
            let copy_len = X64_CONTEXT_SIZE.min(block.len() - off);
            if ctx.has_context {
                block[off..off + copy_len]
                    .copy_from_slice(&ctx.cpu_context[..copy_len]);
            } else if copy_len >= X64_CONTEXT_SIZE {
                fix_context_segments(&mut block[off..off + X64_CONTEXT_SIZE]);
            }
        }
    }

    let kdbg_end = ctx.kdbg_pa + ctx.kdbg_data.len() as u64;
    if ctx.kdbg_pa >= block_offset && kdbg_end <= block_end {
        let off = (ctx.kdbg_pa - block_offset) as usize;
        block[off..off + ctx.kdbg_data.len()].copy_from_slice(&ctx.kdbg_data);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Main demo
// ─────────────────────────────────────────────────────────────────────────────

fn demo() {
    // ── 1. Default SDK config ─────────────────────────────────────────────────
    let mut cfg = VmOperationsConfig::default();
    sdk_get_default_config(&mut cfg);
    cfg.reload_driver = 1;

    // ── 2. Enumerate partitions ───────────────────────────────────────────────
    let handles = sdk_enum_partitions(&cfg);
    println!("\n   Virtual Machines:");

    if handles.is_empty() {
        println!("   --> No virtual machines running.");
        return;
    }

    for (i, &h) in handles.iter().enumerate() {
        let name  = sdk_get_friendly_name(h);
        let id    = sdk_get_partition_id(h);
        let vtype = sdk_get_vm_type_string(h);
        let guid  = sdk_get_vm_guid_string(h);

        if !guid.is_empty() {
            println!("    --> [{}] {} (PartitionId = {:#x}, {}, GUID: {})",
                     i, name, id, vtype, guid);
        } else {
            println!("    --> [{}] {} (PartitionId = {:#x}, {})",
                     i, name, id, vtype);
        }
    }

    // ── 3. User selects VM ────────────────────────────────────────────────────
    println!("\n   Please select the ID of the virtual machine:");
    let vm_idx = prompt_digit(handles.len());
    let handle = handles[vm_idx];
    println!("   Selected: {}", sdk_get_friendly_name(handle));

    // ── 4. Select action ──────────────────────────────────────────────────────
    println!("\n   Action List:");
    println!("    --> [0] Demo only (no file output)");
    println!("    --> [1] Linear physical memory dump (.raw)");
    println!("    --> [2] Crash dump (.dmp)");
    println!("\n   Please select the Action ID:");
    let action = prompt_digit(3);

    let dump_path = if action >= 1 {
        println!("\n   Destination path for dump:");
        print!("   > ");
        io::stdout().flush().ok();
        Some(read_line_trimmed())
    } else {
        None
    };

    // ── 5. Initialize the partition ───────────────────────────────────────────
    if !sdk_select_partition(handle) {
        eprintln!("ERROR: SdkSelectPartition failed.");
        sdk_close_all_partitions();
        return;
    }

    // Explicitly disable crash dump emulation (may persist from a previous run)
    unsafe {
        SdkSetData(handle, HvmmInformationClass::InfoSettingsCrashDumpEmulation, 0);
    }

    // ── 6. Machine type ───────────────────────────────────────────────────────
    match sdk_get_machine_type(handle) {
        MachineType::Amd64 => println!("   Machine type : AMD64"),
        MachineType::Arm64 => println!("   Machine type : ARM64"),
        _                  => {
            println!("   Machine type : unsupported");
            sdk_close_all_partitions();
            return;
        }
    }

    // ── 7. Guest OS type ──────────────────────────────────────────────────────
    let os_type = sdk_get_guest_os_type(handle);
    println!("   Guest OS type: {:?} (0=Unknown, 1=Standard, 2=NonKdbg, 3=HyperV)", os_type);

    // ── 8. Read physical memory at GPA 0 ─────────────────────────────────────
    let read_method = ReadMemoryMethod::ReadInterfaceHvmmDrvInternal;

    let mut buf4 = [0u8; 4];
    if sdk_read_physical_memory(handle, 0, &mut buf4, read_method) {
        let dword = u32::from_le_bytes(buf4);
        println!("   GPA 0x0 first DWORD: {:#x}", dword);
    } else {
        println!("   SdkReadPhysicalMemory failed");
    }

    // ── 9. KernelBase: VA → PA ────────────────────────────────────────────────
    let kernel_base = sdk_get_kernel_base(handle);
    if kernel_base != 0 {
        let kernel_base_pa =
            sdk_get_physical_address(handle, kernel_base, MemoryAccessType::VirtualMemory);
        println!("   KernelBase VA: {:#x}  PA: {:#x}", kernel_base, kernel_base_pa);
    }

    // ── 10. Read VP register: RIP on VP 0 ────────────────────────────────────
    if let Some(rip_val) = sdk_read_vp_register(handle, 0, VtlLevel::Vtl0, HvRegisterName::Rip) {
        let rip = unsafe { rip_val.reg64 };
        println!("   VP[0] RIP: {:#x}", rip);

        // Write back the same value (no-op demo).
        if rip != 0 {
            if sdk_write_vp_register(handle, 0, VtlLevel::Vtl0, HvRegisterName::Rip, &rip_val) {
                println!("   VP[0] RIP written back (no-op): {:#x}", rip);
            }
        }
    } else {
        println!("   SdkReadVpRegister failed");
    }

    // ── 11. Read virtual memory: KiProcessorBlock[0] ─────────────────────────
    let ki_va = sdk_get_data_u64(handle, HvmmInformationClass::InfoKiProcessorBlock);
    if ki_va != 0 {
        let mut buf8 = [0u8; 8];
        if sdk_read_virtual_memory(handle, ki_va, &mut buf8) {
            println!("   KiProcessorBlock[0] -> {:#x}", u64::from_le_bytes(buf8));
        } else {
            println!("   SdkReadVirtualMemory failed for KiProcessorBlock");
        }
    }

    // ── 12. Optional: dump ─────────────────────────────────────────────────
    if let Some(path) = dump_path.as_deref() {
        match action {
            1 => dump_vm(handle, path, read_method),
            2 => dump_crash_vm(handle, path, read_method),
            _ => {}
        }
    }

    // ── 13. Cleanup ──────────────────────────────────────────────────────────
    sdk_close_partition(handle);
    sdk_close_all_partitions();
}

fn main() {
    demo();

    println!("\nPress Enter to exit...");
    read_line_trimmed();
}
