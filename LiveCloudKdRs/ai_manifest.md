# LiveCloudKdRs — AI Code Generation Manifest

> **Goal:** Machine-readable context for AI tools working with the Rust hvlib.dll bindings
> **Principle:** Maximum information / minimum tokens

---

## 1. Integration

| Parameter | Value |
|-----------|-------|
| Language | Rust 2021 edition (stable-x86_64-pc-windows-msvc - rustc 1.94.1) |
| Binding | `extern "C"` FFI → hvlib.dll (import lib: hvlib.lib) |
| Runtime deps | hvlib.dll + hvmm.sys next to the .exe |
| Privileges | **Administrator** (hvmm.sys load) |
| Platform | Windows only (`#![cfg(target_os = "windows")]`) |
| Crate | `livecloudkd` (path: `LiveCloudKdRs/`) |
| Example | `livecloudkd-example` (path: `LiveCloudKdExampleRs/`) |
| Import | `use livecloudkd::*;` |
| Workspace | `Cargo.toml` at repo root, members = both crates |
| Version | 0.1.0 |

`LiveCloudKdRs/build.rs` auto-links `hvlib.lib` from `../LiveCloudKdSdk/files/`.
No manual linker flags needed.

```rust
// Minimal start
use livecloudkd::*;

let mut cfg = VmOperationsConfig::default();
sdk_get_default_config(&mut cfg);
cfg.reload_driver = 1;

let handles = sdk_enum_partitions(&cfg);
if handles.is_empty() { return; }

let h = handles[0];
if !sdk_select_partition(h) { return; }

// IMPORTANT: disable stale crash dump emulation before reading real memory
unsafe { SdkSetData(h, HvmmInformationClass::InfoSettingsCrashDumpEmulation, 0); }

let mut buf = [0u8; 8];
sdk_read_virtual_memory(h, sdk_get_kernel_base(h), &mut buf);

sdk_close_partition(h);
sdk_close_all_partitions();
```

---

## 2. Enumerations

Enum variant names **match the C SDK** (`HvlibEnumPublic.h`) verbatim — do not
shorten them.

```rust
ReadMemoryMethod:    ReadInterfaceUnsupported=0, ReadInterfaceHvmmDrvInternal=1,
                     ReadInterfaceWinHv=2, ReadInterfaceHvmmLocal=3, ReadInterfaceMax=4
WriteMemoryMethod:   WriteInterfaceUnsupported=0, WriteInterfaceHvmmDrvInternal=1,
                     WriteInterfaceWinHv=2, WriteInterfaceHvmmLocal=3, WriteInterfaceMax=4
SuspendResumeMethod: SuspendResumeUnsupported=0, SuspendResumePowershell=1,
                     SuspendResumeWriteSpecRegister=2
MemoryAccessType:    PhysicalMemory=0, VirtualMemory=1, AccessRtCore64=2
VmStateAction:       SuspendVm=0, ResumeVm=1
MachineType:         Amd64, Arm64, Unknown (others → Unknown)
GuestType:           Unknown=0, Standard=1, NonKdbg=2, HyperV=3
VtlLevel:            Vtl0=0, Vtl1=1, Vtl2=2
HvmmInformationClass: 50+ discriminants — see lib.rs for the full list
```

**`HvRegisterName` is NOT a `repr` enum** — it's `#[repr(transparent)] u32` with
associated constants (`Rax`, `Rip`, `Cr3`, ...). This avoids exhaustive-match
pain on 200+ register codes.

---

## 3. VmOperationsConfig — MSVC x64 layout

`#[repr(C)]` with implicit padding matching C. Total: 56 bytes. Always
initialize via `sdk_get_default_config()`, then override individual fields.

```
Field                     | Rust type | Notes
read_method               | ReadMemoryMethod  (i32)
write_method              | WriteMemoryMethod (i32)
suspend_method            | SuspendResumeMethod (i32)
<4 bytes padding>         |                   | align log_level to 8
log_level                 | u64
force_freeze_cpu          | u8  (BOOLEAN)
pause_partition           | u8  (BOOLEAN)
<6 bytes padding>         |                   | align exdi_console_handle to 8
exdi_console_handle       | u64
reload_driver             | u8
pf_injection              | u8
nested_scan               | u8
use_debug_api_stop_process| u8
simple_memory             | u8  (for Linux guests)
<3 bytes tail padding>    |
```

---

## 4. API — Safe wrappers (snake_case)

Safe wrappers live in `lib.rs` as free functions. They handle `ULONG64` / `BOOL`
conversions, null checks, and unicode decoding.

### 4.1 Partition lifecycle

```rust
sdk_get_default_config(&mut VmOperationsConfig) -> bool
sdk_set_partition_config(handle: u64, &VmOperationsConfig) -> bool
sdk_enum_partitions(&VmOperationsConfig) -> Vec<u64>   // handles
sdk_select_partition(handle: u64) -> bool              // REQUIRED before reads
sdk_close_partition(handle: u64)
sdk_close_all_partitions() -> bool
```

### 4.2 Partition info

```rust
sdk_get_friendly_name(handle) -> String
sdk_get_vm_type_string(handle) -> String
sdk_get_vm_guid_string(handle) -> String
sdk_get_partition_id(handle) -> u64
sdk_get_kernel_base(handle) -> u64
sdk_get_max_physical_pages(handle) -> u64
sdk_get_guest_os_type(handle) -> GuestType
sdk_get_machine_type(handle) -> MachineType
```

### 4.3 Memory

```rust
sdk_read_physical_memory (handle, gpa, &mut [u8], ReadMemoryMethod)  -> bool
sdk_write_physical_memory(handle, gpa, &[u8],     WriteMemoryMethod) -> bool
sdk_read_virtual_memory  (handle, gva, &mut [u8])                    -> bool
sdk_write_virtual_memory (handle, gva, &[u8])                        -> bool
sdk_get_physical_address (handle, va, MemoryAccessType)              -> u64
```

Buffer length = `buf.len()`. GPAs/GVAs are **byte addresses**, never PFNs.

### 4.4 VP registers

```rust
sdk_read_vp_register (handle, vp_index, VtlLevel, HvRegisterName)
    -> Option<HvRegisterValue>
sdk_write_vp_register(handle, vp_index, VtlLevel, HvRegisterName,
                      &HvRegisterValue) -> bool
```

`HvRegisterValue` is a `#[repr(C)]` 16-byte union. Access `.reg64`, `.reg32`,
`.reg128`, etc. inside `unsafe`.

### 4.5 Generic data access

```rust
sdk_get_data_u64 (handle, HvmmInformationClass) -> u64
sdk_get_data2    (handle, HvmmInformationClass) -> u64   // read-only getter
sdk_get_data_wstr(handle, HvmmInformationClass) -> Option<String>
```

For pointer-returning classes (e.g. `InfoKdbgContext`, `InfoCpuContextVa`),
call the raw FFI `SdkGetData` inside `unsafe`:

```rust
let mut ptr: *const u64 = std::ptr::null();
unsafe {
    SdkGetData(handle, HvmmInformationClass::InfoKdbgContext,
               &mut ptr as *mut _ as *mut c_void);
}
```

### 4.6 Control

```rust
sdk_control_vm_state(handle, VmStateAction, SuspendResumeMethod,
                     pause_partition: bool) -> bool
```

---

## 5. Raw FFI (unsafe)

Declared in one `extern "C" { ... }` block. All symbols are `pub fn` so the
example crate can call them directly when a safe wrapper doesn't exist.

**Exported by hvlib.dll (25) — verified against
`LiveCloudKd.v3.2.0.20260304-release\hvlib.dll`:**

```
SdkCloseAllPartitions    SdkGetMachineType               SdkReadVpRegister
SdkClosePartition        SdkGetPartitionConfig           SdkSelectPartition
SdkControlVmState        SdkGetPhysicalAddress           SdkSetData
SdkEnumPartitions        SdkInvokeHypercall              SdkSetPartitionConfig
SdkGetData               SdkNumberToString               SdkSymEnumAllSymbols
SdkGetData2              SdkReadPhysicalMemory           SdkSymEnumAllSymbolsGetTableLength
SdkGetDefaultConfig      SdkReadVirtualMemory            SdkSymGetSymbolAddress
                         SdkWritePhysicalMemory          SdkSymGetSymbolAddress2
                         SdkWriteVirtualMemory
                         SdkWriteVpRegister
```

**Not currently bound in `lib.rs`** (add if you need them):
`SdkSymEnumAllSymbols`, `SdkSymEnumAllSymbolsGetTableLength`,
`SdkSymGetSymbolAddress`, `SdkSymGetSymbolAddress2`.

There is no `SdkGetFriendlyName` export — the friendly name is retrieved via
`SdkGetData` with `InfoPartitionFriendlyName` (see `sdk_get_friendly_name`).

---

## 6. Canonical call sequence

```rust
use livecloudkd::*;

let mut cfg = VmOperationsConfig::default();
sdk_get_default_config(&mut cfg);
cfg.reload_driver = 1;

let handles = sdk_enum_partitions(&cfg);
let h = *handles.first().expect("no VMs");

if !sdk_select_partition(h) { return; }

// ↓↓↓ ALWAYS do this right after SdkSelectPartition ↓↓↓
unsafe {
    SdkSetData(h, HvmmInformationClass::InfoSettingsCrashDumpEmulation, 0);
}

// ... reads, register queries, translations ...

sdk_close_partition(h);
sdk_close_all_partitions();
```

---

## 7. Crash dump construction (lives in the EXAMPLE, not the library)

The crate intentionally exposes only raw dump types and SDK queries. All crash-
dump *construction* helpers live in `LiveCloudKdExampleRs/src/main.rs`:

```
dump_fill_header64           — Rust port of DumpFillHeader64 (dump.c)
sdk_get_dump_header          — retrieves InfoDumpHeaderPointer (needs emulation on)
dump_header_as_bytes         — &DumpHeader64 → &[u8]
gather_crash_dump_context    — CPU context PAs + KDBG block
patch_crash_dump_block       — patches a 1 MB block before writing
fix_context_segments         — rewrites segment selectors
CrashDumpContext             — struct carrying all patching metadata
fill_phys_mem_block          — fills the single-run PhysicalMemoryBlock
now_as_filetime              — Unix time → Windows FILETIME
```

`lib.rs` exposes **only** the types/constants these helpers need:
`DumpHeader64` (size = 8192, asserted at compile time), `ExceptionRecord64`,
`DUMP_SIGNATURE`, `DUMP_VALID_DUMP64`, `DUMP_TYPE_FULL`, `IMAGE_FILE_MACHINE_AMD64`,
`STATUS_BREAKPOINT`, `EXCEPTION_NONCONTINUABLE`, `KGDT64_*`, `RPL_MASK`,
`X64_CONTEXT_SIZE` (= 1232), `CTX_SEG_CS/DS/ES/FS/GS/SS`, `CTX_RIP`, `CTX_RSP`,
`DUMP_COMMENT`, `KD_DEBUGGER_BLOCK_SIZE`, `DUMP_HEADER64_SIZE`.

**Rule for future additions:** anything that *uses* these types to build a dump
file belongs in the example. The library stays a thin binding layer.

---

## 8. Critical pitfalls

| Pitfall | Correct approach |
|---------|------------------|
| Stale crash dump emulation state in hvmm.sys across runs | Right after `sdk_select_partition`, call `SdkSetData(h, InfoSettingsCrashDumpEmulation, 0)` unconditionally. Symptoms of stale state: `GPA 0x0` returns `0x45474150` ("PAGE"), `KiProcessorBlock[0]` returns `0x14` instead of a kernel VA. |
| `InfoDumpHeaderPointer` fails unless emulation was on during `SdkSelectPartition` | Don't re-enable retroactively — use `dump_fill_header64` (manual port) instead. It produces an identical header. |
| Buffer length mismatch | Wrappers use `buf.len()`. Don't pass a size separately. |
| Binding `SdkGetPartitionConfig` / `SdkSym*` | Not exported by the current hvlib.dll — will fail at link time. |
| Forgetting `sdk_close_all_partitions()` | The driver holds state across runs. Always pair `enum` with `close_all`. |
| Using `std::ffi::CStr` for VM names | hvlib returns **UTF-16**. Use the provided `wchar_ptr_to_string()` helper. |
| Hyper-V VM name with spaces + `SuspendResumePowershell` | Known hvlib.dll bug — it constructs an unquoted PowerShell command. Avoid PowerShell suspend for such VMs, or use `SuspendResumeWriteSpecRegister`. |
| Treating `HvRegisterName` as a normal enum | It's `#[repr(transparent)] u32`. Use associated constants, not match. |
| Assuming `DumpHeader64` layout | Size is asserted at compile time: `const _: () = assert!(size_of::<DumpHeader64>() == 8192);` — do not change field order/size. |

---

## 9. Build & run

```powershell
cd C:\Projects\LiveCloudKd
cargo build --release                    # workspace build

# Deploy runtime files
copy LiveCloudKdSdk\files\hvlib.dll target\release\
copy LiveCloudKdSdk\files\hvmm.sys  target\release\

# Must run as Administrator
cd target\release
.\livecloudkd-example.exe
```

`build.rs` finds `hvlib.lib` via `../LiveCloudKdSdk/files/`. If the SDK is
relocated, update `build.rs` accordingly — no env vars.

---

## 10. Query templates for this project

```
[META] Rust 2021 | FFI to hvlib.dll | livecloudkd crate
[FILE] LiveCloudKdRs/src/lib.rs OR LiveCloudKdExampleRs/src/main.rs
[ISSUE] [description]
[TRIED] [what was tried]
```

**Effective queries:**

```
Add a safe wrapper for SdkSetPartitionConfig in lib.rs
FFI: BOOLEAN SdkSetPartitionConfig(ULONG64 Handle, VM_OPERATIONS_CONFIG* Config)
Pattern: follow sdk_set_partition_config / sdk_get_default_config style
```

```
Port a new helper from dump.c to main.rs (not lib.rs — it's example code)
Keep raw types (DumpHeader64 etc.) in lib.rs; put the logic in main.rs.
```

```
Translate a C struct from public/HyperV/hvgdk.h to #[repr(C)]
Verify MSVC x64 padding. Add a const _: () = assert!(size_of...) check.
```
