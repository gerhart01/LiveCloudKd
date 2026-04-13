# livecloudkd

Rust bindings for `hvlib.dll` from the [LiveCloudKd SDK](https://github.com/gerhart01/LiveCloudKdSdk).

Provides safe wrappers around the C API for reading and writing Hyper-V guest memory, translating addresses, and accessing virtual-processor registers — all from user mode on the host.

## Requirements

- Windows 10/11 or Windows Server 2019/2022/2025 (host)
- Hyper-V enabled with at least one running VM
- `hvlib.dll` and `hvmm.sys` from `[LiveCloudKdSdk\files\](https://github.com/gerhart01/LiveCloudKd/releases)` (included in the SDK)
- The `hvlib.dll` must be in the same directory as your binary at runtime
- Rust toolchain `stable-x86_64-pc-windows-msvc`

## Building

```
cargo build --release
```

`build.rs` automatically passes the correct `/LIBPATH` to the linker pointing at
`LiveCloudKdSdk\files\hvlib.lib`.  No manual linker flags are needed.

## API overview

### Configuration and partition enumeration

```rust
use livecloudkd::*;

//
// Fill config with host-appropriate defaults
//

let mut cfg = VmOperationsConfig::default();
sdk_get_default_config(&mut cfg);
cfg.reload_driver = 1;                      // reload hvmm.sys on first call


//
// Returns a Vec<u64> of opaque partition handles
//

let handles = sdk_enum_partitions(&cfg);
for h in &handles {
    println!("{} — id {:#x}", sdk_get_friendly_name(*h), sdk_get_partition_id(*h));
}
```

### Selecting a partition

```rust
//
// Must be called before any memory / register operations
//

sdk_select_partition(handle);
```

### Memory access

```rust
//
// Read 4 bytes from guest physical address 0
//

let mut buf = [0u8; 4];
sdk_read_physical_memory(handle, 0, &mut buf, ReadMemoryMethod::HvmmDrvInternal);

//
// Read 8 bytes from a guest virtual address
//

let va = sdk_get_ki_processor_block(handle);
let mut buf8 = [0u8; 8];
sdk_read_virtual_memory(handle, va, &mut buf8);

//
// Translate VA → PA
//

let pa = sdk_get_physical_address(handle, va, MemoryAccessType::VirtualMemory);
```

### Virtual-processor registers

```rust
//
// Read RIP of virtual processor 0 at VTL 0
//

if let Some(val) = sdk_read_vp_register(handle, 0, VtlLevel::Vtl0, HvRegisterName::Rip) {
    println!("RIP = {:#x}", unsafe { val.reg64 });
}

//
// Write it back (no-op demo)
//

sdk_write_vp_register(handle, 0, VtlLevel::Vtl0, HvRegisterName::Rip, &val);
```

### Suspend / resume

```rust
sdk_control_vm_state(handle, VmStateAction::SuspendVm, SuspendResumeMethod::Powershell, false);
sdk_control_vm_state(handle, VmStateAction::ResumeVm,  SuspendResumeMethod::Powershell, false);
```

### Cleanup

```rust
sdk_close_partition(handle);
sdk_close_all_partitions();
```

## Key types

| Type | Description |
|------|-------------|
| `VmOperationsConfig` | SDK configuration (read/write method, log level, flags) |
| `HvRegisterName` | Register selector — newtype `u32` with named constants (`Rip`, `Rax`, `Cr3`, …) |
| `HvRegisterValue` | 128-bit register value union (`reg64`, `reg32`, `reg128`) |
| `ReadMemoryMethod` | `HvmmDrvInternal` / `WinHv` / `HvmmLocal` |
| `WriteMemoryMethod` | same variants as read |
| `GuestType` | `Standard` / `NonKdbgPartition` / `HyperV` |
| `VtlLevel` | `Vtl0` … `Vtl15` |
| `HvmmInformationClass` | Selects what `sdk_get_data*` returns |

## Convenience helpers

```rust
sdk_get_friendly_name(h)       // → String  VM display name
sdk_get_vm_guid_string(h)      // → String  VM GUID
sdk_get_partition_id(h)        // → u64     numeric partition id
sdk_get_kernel_base(h)         // → u64     guest ntoskrnl base VA
sdk_get_max_physical_pages(h)  // → u64     total physical pages
sdk_get_guest_os_type(h)       // → GuestType
sdk_get_machine_type(h)        // → MachineType
sdk_get_ki_processor_block(h)  // → u64     KiProcessorBlock VA
```

## Low-level / raw access

All `extern "C"` declarations are re-exported from the `livecloudkd` crate so
you can call them directly when the safe wrappers do not cover your use case:

```rust
unsafe { livecloudkd::SdkGetData(handle, HvmmInformationClass::InfoKPCR, ptr) };
```
