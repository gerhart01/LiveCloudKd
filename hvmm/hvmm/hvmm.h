
//#define DBG 0
#include "Ntifs.h"
//include "ntddk.h"
#include "wdmsec.h"
#include "hv.h"
#include "mWin.h"
#include "mDbgPrint.h"

#define USER_BUFFER_LIMIT 0x1000000

//
//Windows builds specific offsets
//

//
// The fixed offsets in this file are the Windows 10 1803 layout and are kept as fallbacks
// only. vid.sys moves them with every Windows build. VidResolveLayout (vid.c) finds the live
// positions by scanning each structure for its signature ("Prtn", "Gpar", "Mb  ") and the
// read paths use what it found. A field marked "fixed" below is the same on every build
// checked so far; a field marked "moves" is one the scan resolves.
//
#define PARTITION_NAME_1803_OFFSET 0x78
#define PARTITION_ID_1803_OFFSET 0x278

//Vid.sys builded 05.2018
#define VID_PS_PROCESS_CHECK_01 0x123A5 
//#define VID_PS_PROCESS_CHECK_02 0x16847  
//Vid.sys builded 03.2019 18356
//#define VID_PS_PROCESS_CHECK_01 0x11fed
//#define VID_PS_PROCESS_CHECK_02 0x16847


//
// hvlib.dll reads the enumeration reply (VID_VM_INFO) with a 512-character name: PartitionId
// at +0x400 and VmType at +0x408, in a 0x610-byte buffer. With 256 here the driver wrote them
// at +0x200 and hvlib saw id 0 and an unknown type. vid.sys itself keeps the name in 0x200
// bytes (PartitionId follows at name+0x200), so only that much is copied; the rest is zero.
//

#define VID_PARTITION_FRIENDLY_NAME_MAX (512)
#define VID_PARTITION_NAME_BYTES_IN_CONTEXT (0x200)
#define VID_READ_WRITE_GPA_BUFFER_SIZE 0x10

//
//Registers count for registry read\write hypercalls
//

#define REGISTER_READ_WRITE_COUNT 1

//
//size of original nt!PsGetCurrentProcess
//

//#define SIZE_OF_PS_FUNCTION 0x11
#define SIZE_OF_ARCH_NEW_PS_FUNCTION 0x20 //for build 1803 may 2018

//
// Tag for vmmem scanning. VsmmProcessInitialize object tag
//

#define PRCS_TAG 'scrP' 

//
// SDDL string used when creating the device. This string
// limits access to this driver to system and admins only.
//

#define DEVICE_SDDL             L"D:P(A;;GA;;;SY)(A;;GA;;;BA)"


#define IOCTL_GET_FRIENDLY_PARTIION_NAME CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x820, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_ACTIVE_PARTITIONS CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x821, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_HV_READ_GPA CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x822, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_HV_WRITE_GPA CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x823, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_HV_READ_REG CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x824, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_HV_WRITE_REG CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x825, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_HV_TRANSLATE_VA CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x826, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_HV_PATCH_GETPROCESS CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x827, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_MBLOCK_FROM_GPA CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x828, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VID_QUERY_INFO CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x829, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VID_INTERNAL_READ_MEMORY CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x830, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_HV_RESTORE_GETPROCESS CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x831, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INJECT_VIDAUX_DLL CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x832, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISABLE_VMWP_MITIGATIONS CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x833, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ENABLE_VMWP_MITIGATIONS CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x834, METHOD_BUFFERED, FILE_ANY_ACCESS)
//
// Guest mapping IOCTLs. They start at 0x8A0, well above the existing codes, so they cannot
// collide with codes other hvmm.sys builds use. Clients send IOCTL_QUERY_MAPPING_SUPPORT first
// and nothing else unless it answers HVMM_MAPPING_SIGNATURE.
//

#define IOCTL_QUERY_MAPPING_SUPPORT CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x8A0, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MAP_GPA_RANGE CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x8A1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_UNMAP_GPA_RANGE CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x8A2, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_QUERY_PARTITION_LAYOUT CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x8A3, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HVMM_MAPPING_QUERY_MAGIC   (0x714D7648UL)   // "HvMq"
#define HVMM_MAPPING_SIGNATURE     (0x704D7648UL)   // "HvMp"
#define HVMM_MAPPING_VERSION       (1UL)

//
// One map call never spans more than 8 MB. The limit is the Size field of the MDL, a
// signed CSHORT: sizeof(MDL) plus 8 bytes per page must stay below 32768, which is about
// 4089 4 KB pages (16 MB). 8 MB leaves margin. The driver reports this value in the
// handshake and clients chunk by what it reports.
//

#define HVMM_MAP_GPA_MAX_LENGTH (0x800000ULL)

//
// Live mappings one handle may hold at once. A 64 GB VM in 8 MB chunks is 8192; the cap
// stops a caller from eating non-paged pool by mapping the same range forever.
//

#define HVMM_MAX_MAPPINGS_PER_HANDLE (16384UL)


typedef PVOID MB_HANDLE;
#define MAX_PATH          260

typedef enum _VID_INFORMATION_CLASS {
    VidMbBlockInfo,                  //0
} VID_INFORMATION_CLASS;

typedef enum _VM_TYPE {
	VidVmTypeUnknown = 0,
	VidVmTypeContainer = 0x200001e,
	VidVmTypeFullWin10VM = 0x200000e,
	VidVmTypeFullWinSrvVMSecure = 0x2000012,
	VidVmTypeFullWinSrvVM = 0x2000014,
	VidVmTypeDockerHyperVContainerUserName = 0x2000048,
	VidVmTypeDockerHyperVContainerGUID = 0x2000080,
	VidVmTypeLinuxContainer = 02000016
} VM_TYPE;

typedef enum _USR_VM_TYPE {
	UsrVidVmTypeUnknown = 0,
	UsrVidVmTypeContainer = 1,
	UsrVidVmTypeFullWin10VM = 2,
	UsrVidVmTypeFullWinSrvVMSecure = 3,
	UsrVidVmTypeFullWinSrvVM = 4,
	UsrVidVmTypeDockerHyperVContainerUserName = 5,
	UsrVidVmTypeDockerHyperVContainerGUID = 6,
	UsrVidVmTypeLinuxContainer = 7
} USR_VM_TYPE;

//
// Structures for usermode\kernelmode exchange
//

//
// The read and write requests (IOCTL_HV_READ_GPA, IOCTL_HV_WRITE_GPA,
// IOCTL_VID_INTERNAL_READ_MEMORY) as hvlib.dll sends them: handle first, then the
// partition id, the VM worker process id, two words hvlib leaves zero, then the byte
// position and the byte count. The older layout (id, position, count, handle) made
// the driver read handle 0 and position 2 from a real hvlib request, so every read
// through hvlib failed while a hand-built request worked.
//
typedef struct _GPA_INFO {
	HANDLE PartitionHandle;    // +0x00
	ULONG64 PartitionId;       // +0x08
	ULONG64 VmwpProcessId;     // +0x10, not used by the driver
	ULONG64 Reserved18;        // +0x18
	ULONG64 Reserved20;        // +0x20
	ULONG64 StartPage;         // +0x28, position in bytes
	ULONG64 BytesCount;        // +0x30, bytes
} GPA_INFO, *PGPA_INFO;


typedef struct _TRANSLATE_VA_INFO {
    ULONG64 PartitionId;
    HV_VP_INDEX VpIndex;
    HV_TRANSLATE_GVA_CONTROL_FLAGS ControlFlags;
    HV_GVA_PAGE_NUMBER GvaPage;
} TRANSLATE_VA_INFO, *PTRANSLATE_VA_INFO;

typedef struct _TRANSLATE_VA_RESULT {
    HV_GPA_PAGE_NUMBER GpaPage;
    HV_TRANSLATE_GVA_RESULT TranslationResult;
} TRANSLATE_VA_RESULT, *PTRANSLATE_VA_RESULT;

//
// The enumeration reply, laid out as hvlib.dll reads it (a 0x610-byte buffer):
// the friendly name, then at +0x400 the partition id, at +0x408 the type, at +0x40C the
// VM GUID as text ("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX", upper case, no braces).
// hvlib exposes that text as InfoVmGuidString; MemProcFS-based tools pick the VM by it.
// The window between the name and +0x400 carries the bytes that follow the name in the
// partition context (the id, then the binary GUID), the way the shipped driver fills it.
// The byte at +0x60D is 1 for a full VM in the shipped driver's reply; its meaning is not
// known, it is set the same way.
//
typedef struct _VID_VM_INFO {
    WCHAR FriendlyName[VID_PARTITION_FRIENDLY_NAME_MAX];   // +0x000, 0x400 bytes
    HV_PARTITION_ID PartitionId;                           // +0x400
	USR_VM_TYPE VmType;                                    // +0x408
	WCHAR VmGuidString[0x40];                              // +0x40C
	UCHAR Reserved[0x60D - 0x48C];                         // +0x48C
	UCHAR FullVmFlag;                                      // +0x60D
	UCHAR Reserved2[2];                                    // +0x60E
} VID_VM_INFO, *PVID_VM_INFO;                              // sizeof == 0x610

#define VID_VM_INFO_NAME_WINDOW_BYTES (0x400)

typedef struct _REGISTER_VP_INFO {
	ULONG64 PartitionId;
	HV_VP_INDEX VpIndex;
	HV_REGISTER_NAME RegisterCode;
	HV_REGISTER_VALUE RegisterValue;
} REGISTER_VP_INFO, * PREGISTER_VP_INFO;

typedef struct _GPAR_BLOCK_INFO {
	HANDLE PartitionHandle;
	UINT64 GPA;
	MB_HANDLE MbHandle;
	UINT64 MemoryBlockPageIndex;
	UINT64 Count; //Count in GPAR array
} GPAR_BLOCK_INFO, *PGPAR_BLOCK_INFO;

typedef struct _VID_INJECTION_INFO {
	WCHAR NtdllPath[MAX_PATH];
	PVOID NtdllImageBase;
	WCHAR VidAuxDllPath[MAX_PATH];
	ULONG64 VmwpPid;
} VID_INJECTION_INFO, *PVID_INJECTION_INFO;

//
// IOCTL_MAP_GPA_RANGE: map a guest physical range into the calling process,
// read-only, and keep it mapped. GpaStart and Length are byte counts and must be
// page aligned. The driver maps the longest host-backed prefix of the range and
// reports how many bytes it mapped in MappedBytes; the caller continues from there.
//

typedef struct _MAP_GPA_RANGE_INPUT {
	HANDLE PartitionHandle;
	UINT64 GpaStart;
	UINT64 Length;
} MAP_GPA_RANGE_INPUT, *PMAP_GPA_RANGE_INPUT;

typedef struct _MAP_GPA_RANGE_OUTPUT {
	PVOID UserVa;
	UINT64 MappedBytes;
} MAP_GPA_RANGE_OUTPUT, *PMAP_GPA_RANGE_OUTPUT;

//
// IOCTL_UNMAP_GPA_RANGE: unmap one range previously returned by IOCTL_MAP_GPA_RANGE.
// UserVa is the value the map call reported.
//

typedef struct _UNMAP_GPA_RANGE_INPUT {
	PVOID UserVa;
} UNMAP_GPA_RANGE_INPUT, *PUNMAP_GPA_RANGE_INPUT;

//
// IOCTL_QUERY_MAPPING_SUPPORT: the handshake a client sends before any map request.
// Input carries HVMM_MAPPING_QUERY_MAGIC; the driver answers with HVMM_MAPPING_SIGNATURE.
//

typedef struct _MAPPING_QUERY_INPUT {
	UINT32 Magic;
	UINT32 Reserved;
} MAPPING_QUERY_INPUT, *PMAPPING_QUERY_INPUT;

typedef struct _MAPPING_QUERY_OUTPUT {
	UINT32 Signature;
	UINT32 Version;
	UINT64 MaxMapLength;
} MAPPING_QUERY_OUTPUT, *PMAPPING_QUERY_OUTPUT;

//
// IOCTL_QUERY_PARTITION_LAYOUT: what the layout scan found for one partition. A support
// tool prints it when a VM lists with no id or type, so the failing step has a name.
//

#define VID_SCAN_NAME        0x01   // FriendlyName and PartitionId found
#define VID_SCAN_GPAR        0x02   // GPAR block handle found
#define VID_SCAN_OBJMBLOCK   0x04   // MEMORY_BLOCK pointer inside a GPAR found
#define VID_SCAN_GPA_ARRAY   0x08   // host-PFN array inside a MEMORY_BLOCK found
#define VID_SCAN_MBLOCKARRAY 0x10   // MEMORY_BLOCK_ARRAY pointer found (diagnostic paths only)
#define VID_SCAN_CACHED      0x20   // answered from the cached layout, no scan this time

//
// Per request-code counters, so a client can see which requests a caller sends and which
// ones fail. Diagnostic only.
//

#define HVMM_IOCTL_STAT_SLOTS 24

typedef struct _HVMM_IOCTL_STAT {
	UINT32 Code;
	UINT32 Calls;
	UINT32 Failures;             // completed with an error status, or with no bytes
	UINT32 LastStatus;
} HVMM_IOCTL_STAT, *PHVMM_IOCTL_STAT;

//
// Where the last failed IOCTL_VID_INTERNAL_READ_MEMORY gave up. Diagnostic only.
//

#define HVMM_READ_FAIL_HANDLE       1   // partition handle did not resolve
#define HVMM_READ_FAIL_NO_CONTEXT   2   // file object has no partition context
#define HVMM_READ_FAIL_NOT_FULL_VM  3   // layout scan says not a full VM and VmType is unknown
#define HVMM_READ_FAIL_LENGTH       4   // output length not page aligned
#define HVMM_READ_FAIL_NO_GPAR      5   // a page is outside every GPA range
#define HVMM_READ_FAIL_VMWP_RANGE   6   // a page sits in a vmwp.exe descriptor
#define HVMM_READ_FAIL_NO_MBLOCK    7   // objMBlock is NULL
#define HVMM_READ_FAIL_NO_ARRAY     8   // host-PFN array is NULL
#define HVMM_READ_FAIL_MDL          9   // IoAllocateMdl failed
#define HVMM_READ_FAIL_MAP          10  // MmMapLockedPagesSpecifyCache failed
#define HVMM_READ_FAIL_EXCEPTION    11  // the copy faulted
#define HVMM_READ_FAIL_ALL_UNBACKED 12  // no page of the block was host backed

typedef struct _HVMM_LAST_READ_FAIL {
	UINT64 Handle;
	UINT64 Gpa;          // bytes, as the caller asked
	UINT32 Length;       // output buffer length
	UINT32 Step;         // HVMM_READ_FAIL_*
	UINT64 FailPage;     // first page number the walk could not back
	UINT32 Backed;       // pages copied in that request
	UINT32 Unbacked;     // pages left zero in that request
	UINT8  Raw[64];      // first bytes of the request buffer, as the caller sent them
} HVMM_LAST_READ_FAIL, *PHVMM_LAST_READ_FAIL;

#define HVMM_CONTEXT_DUMP_BYTES 0x1000

typedef struct _PARTITION_LAYOUT_QUERY_INPUT {
	HANDLE PartitionHandle;
} PARTITION_LAYOUT_QUERY_INPUT, *PPARTITION_LAYOUT_QUERY_INPUT;

typedef struct _PARTITION_LAYOUT_QUERY_OUTPUT {
	UINT32 Signature;            // first 4 bytes of the partition context ("Prtn" expected)
	UINT32 ScanFlags;            // VID_SCAN_* bits
	UINT32 IsFullVm;
	UINT32 UsrVmType;
	UINT32 NameOffset;
	UINT32 PartitionIdOffset;
	UINT32 MblockArrayOffset;
	UINT32 GparHandleOffset;
	UINT32 GparCountOffset;
	UINT32 ObjMblockOffset;
	UINT32 GuestGpaArrayOffset;
	UINT32 Source;               // 0 = this handle, 1 = last enumeration (no handle given), 2 = handle refused (PartitionId holds the NTSTATUS)
	UINT64 PartitionId;          // read at PartitionIdOffset
	HVMM_IOCTL_STAT IoctlStats[HVMM_IOCTL_STAT_SLOTS]; // every request code seen since load, with call and failure counts
	HVMM_LAST_READ_FAIL LastReadFail;                  // where the last failed classic read gave up
	UINT32 ContextBytes;                               // how many bytes of the partition context follow
	UINT8  Context[HVMM_CONTEXT_DUMP_BYTES];           // the start of the partition context, for support
} PARTITION_LAYOUT_QUERY_OUTPUT, *PPARTITION_LAYOUT_QUERY_OUTPUT;

//
// One live guest mapping, tracked per open handle to \\.\hvmm.
//

typedef struct _HVMM_GPA_MAPPING {
	LIST_ENTRY Link;
	PMDL Mdl;
	PVOID UserVa;
	PEPROCESS Process;
	UINT64 MappedBytes;
} HVMM_GPA_MAPPING, *PHVMM_GPA_MAPPING;

//
// Per file object state, hung off FILE_OBJECT->FsContext2. IRP_MJ_CLEANUP unmaps
// everything still on the list, in the owning process context.
//

typedef struct _HVMM_FILE_CONTEXT {
	KSPIN_LOCK Lock;
	LIST_ENTRY MappingList;
	ULONG MappingCount;
} HVMM_FILE_CONTEXT, *PHVMM_FILE_CONTEXT;

//
// Internal vid.sys structures
//

typedef struct _PARTITION_INFO {
    HANDLE PartitionHandle;
    ULONG64 VmwpPid;
    ULONG64 ProcessPid; //process PID for get Handle (like kd.exe or livecloudkd.exe)
    VID_INFORMATION_CLASS VidInformationClass;
} PARTITION_INFO, *PPARTITION_INFO;

typedef struct _MEMORY_BLOCK {
	CHAR cMblockString[0x8]; // "Mb  " signature @ +0x0. fixed
	PVOID PartitionHandle; // @ +0x8. fixed
	CHAR Unknown01[0x8];
	ULONG MbHandle; // @ +0x18. fixed
	CHAR Unknown02[0x1C];
	ULONG64 BitMapSize01; // offset 0x38, size 0x8
	ULONG64 BitMapSize02; // offset 0x40, size 0x8
	CHAR Unknown03[0xA8];
	PULONG64 pGuestGPAArray; //offset 0xF0, size 0x8. moves: VM_LAYOUT.GuestGpaArrayOffset (entries at a 0x10 stride)
} MEMORY_BLOCK, *PMEMORY_BLOCK;

typedef struct _GPAR_OBJECT {
	CHAR cGparSignature[0x8]; // "Gpar" signature @ +0x0 (0x72617047) - eq GPA Range. fixed
	CHAR Unknown01[0xF8];
    UINT64 GpaIndexStart; //offset +0x100, size 0x8. fixed
    UINT64 GpaIndexEnd;  //offset +0x108, size 0x8. fixed
    UINT64 UnknowParam01;
    UINT64 UnknowParam02;
    UINT32 KernelMemoryBlockGpaRangeFlags; //offset +0x120, size 0x4. fixed
	CHAR Unknown02[0x4C];
	PMEMORY_BLOCK objMBlock; //offset +0x170, size 0x8. moves: VM_LAYOUT.ObjMblockOffset
	ULONG64 SomeGpaOffset; //offset +0x178, size 0x8. always objMBlock + 0x8
	ULONG64 VmmMemGpaOffset;//offset +0x180, size 0x8. always objMBlock + 0x10
} GPAR_OBJECT, *PGPAR_OBJECT;

typedef struct _GPAR_BLOCK_HANDLE {
    PVOID PartitionHandle; // @ +0x0. Points back to the "Prtn" partition. fixed
    PGPAR_OBJECT GparArray; // @ +0x8. fixed
    UINT32 Unknown01;
    UINT32 CountInGparArray; // @ +0x14 when the owner is a "Prtn" partition, +0x18 otherwise: VM_LAYOUT.GparCountOffset
} GPAR_BLOCK_HANDLE, *PGPAR_BLOCK_HANDLE;


typedef struct _MEMORY_BLOCK_ARRAY {
	ULONG64 Count; // @ +0x0. Element count; element 0 is the count, real blocks start at index 1 (see vid.c walk).
	PULONG64 ArrayStart; // @ +0x8. Start point to block of pointers to MBlock structures. fixed
} MEMORY_BLOCK_ARRAY, *PMEMORY_BLOCK_ARRAY;

//
// Signatures VidResolveLayout scans for. Each is the first 4 bytes of a vid.sys
// structure, little endian. They do NOT move between Windows builds (the game engine
// writes them verbatim), so they are the stable anchor the moving offsets are found by.
//   "Prtn" = partition context, "Gpar" = one GPA range, "Mb  " = one memory block.
//
#define VID_SIG_PRTN 0x6e747250UL
#define VID_SIG_GPAR 0x72617047UL
#define VID_SIG_MB   0x2020624dUL

//
// Where every moving field of one partition sits, discovered at run time.
//
// vid.sys is recompiled by Windows updates and every offset below moves with it, so we do
// NOT read them at the fixed 2018 positions the struct comments give (those are the fallback
// only). Instead we scan each structure for its signature
// (the vid.sys structures carry them). The offsets are the
// same for every partition on one host (they are vid.sys-build constants), so they are found
// once and cached; IsFullVm is re-checked per partition because it depends on that partition.
//
// Displacements are bytes: Name/PartitionId/MblockArray/GparHandle are into the "Prtn"
// context; ObjMblock/SomeGpa/VmmMemGpa are into a "Gpar" object; GuestGpaArray is into a
// "Mb  " block; GparCount is into the GPAR_BLOCK_HANDLE.
//
typedef struct _VM_LAYOUT {
	PVOID   Context;             // partition context these offsets were validated against (cache key)
	BOOLEAN Resolved;            // context is a "Prtn" partition and all required offsets are known
	BOOLEAN IsFullVm;            // shape is a full, host-page-backed VM (not a container / exo partition)
	ULONG   ScanFlags;           // VID_SCAN_* bits: which scans found their field
	ULONG   UsrVmType;           // USR_VM_TYPE reported to user mode (see enum), derived from the shape
	ULONG   NameOffset;          // FriendlyName (inline UTF-16) inside the context
	ULONG   PartitionIdOffset;   // HV_PARTITION_ID inside the context (= NameOffset + 0x200)
	ULONG   MblockArrayOffset;   // MEMORY_BLOCK_ARRAY pointer inside the context
	ULONG   GparHandleOffset;    // GPAR_BLOCK_HANDLE pointer inside the context
	ULONG   GparCountOffset;     // CountInGparArray inside the GPAR_BLOCK_HANDLE (0x14 for "Prtn", else 0x18)
	ULONG   ObjMblockOffset;     // MEMORY_BLOCK pointer inside a GPAR_OBJECT
	ULONG   SomeGpaOffset;       // ULONG64 inside a GPAR_OBJECT (= ObjMblockOffset + 0x8), used by containers
	ULONG   VmmMemGpaOffset;     // ULONG64 inside a GPAR_OBJECT (= ObjMblockOffset + 0x10), used by containers
	ULONG   GuestGpaArrayOffset; // host-PFN array pointer inside a MEMORY_BLOCK
} VM_LAYOUT, *PVM_LAYOUT;

// The partition context is reached as (FILE_OBJECT->FsContext - 1). Every field below except
// the signature moves between Windows builds; VidResolveLayout finds the live position.
typedef struct _VM_PROCESS_CONTEXT {
	CHAR cPrtnSignature[0x8];//"Prtn" signature @ +0x0 (0x6e747250). fixed
	CHAR Unknown01[0xF];
	CHAR IsSecurePartition;//offset +0x18, size 0x1. moves
    CHAR Unknown02[0x4F];
	DWORD VmType; //offset +0x68 - size 0x4. moves; full VMs are recognised by shape instead (VM_LAYOUT.IsFullVm)
	CHAR Unknown03[0xC];
    WCHAR FriendlyName[0x100]; //offset +0x78, size 0x200. moves: VM_LAYOUT.NameOffset
    HV_PARTITION_ID PartitionId; // offset +0x278, size 0x8. always FriendlyName + 0x200
    CHAR Unknown04[0xF98];
	PMEMORY_BLOCK_ARRAY ArrayOfMblocks;//offset +0x1218, size 0x8. moves: VM_LAYOUT.MblockArrayOffset
    CHAR Unknown05[0x300];
    PGPAR_BLOCK_HANDLE pGparBlockHandle; // offset +0x1520, size 0x8. moves: VM_LAYOUT.GparHandleOffset
	CHAR Unknown06[0x1570];
	PMEMORY_BLOCK_ARRAY ArrayOfMblocks20H1;//offset +0x2A98, size 0x8. the 20H1 position of ArrayOfMblocks; the scan covers it
	CHAR Unknown07[0x300];
	PGPAR_BLOCK_HANDLE pGparBlockHandle20H1; // offset +0x2DA0, size 0x8. the 20H1 position of pGparBlockHandle; the scan covers it
	CHAR Unknown08[0xA8];
} VM_PROCESS_CONTEXT, *PVM_PROCESS_CONTEXT;

typedef struct _EPROCESS_INTERNALS {
	ULONG VmwpMitigationsOriginal;
	ULONG VmwpSignaturesOriginal;
	ULONG EprocessSignaturesOffset;
	ULONG EprocessMitigationsOffset;
} EPROCESS_INTERNALS, *PEPROCESS_INTERNALS;

//
// vid.c
//
BOOLEAN VidDisableProcessProtection(PEPROCESS eProcess);
BOOLEAN VidEnableProcessProtection(PEPROCESS eProcess);
BOOLEAN VidIOCTLDisableProcessProtection(PCHAR pBuffer, ULONG len);
BOOLEAN VidIOCTLEnableProcessProtection(PCHAR pBuffer, ULONG len);
BOOLEAN VidQueryInformation(PCHAR pBuffer, ULONG len);
BOOLEAN VidGetFriendlyPartitionName(PCHAR pBuffer, ULONG len);
BOOLEAN VidHvMapGpaPages(PCHAR pBuffer, ULONG len);
BOOLEAN VidHvReadGpa(PCHAR pBuffer, ULONG len);
BOOLEAN VidHvWriteGpa(PCHAR pBuffer, ULONG len);
BOOLEAN VidReadVpRegisters(PCHAR pBuffer, ULONG len);
BOOLEAN VidWriteVpRegisters(PCHAR pBuffer, ULONG len);
BOOLEAN VidTranslateGvatoGpa(PCHAR pBuffer, ULONG len);
BOOLEAN VidPatchPsGetCurrentProcess(PCHAR pBuffer, ULONG len);
BOOLEAN VidRestorePsGetCurrentProcess();
BOOLEAN VidGetGparBlockInfoFromGPA(PCHAR pBuffer, ULONG len);
PVOID VidPsProcessCheckWorker(PVOID pCurrentProcess, PVOID pRetAddress);
BOOLEAN VidGetMBlockInfo(PCHAR pBuffer, ULONG len);
BOOLEAN VidInternalReadMemory(PCHAR pBuffer, ULONG len);
BOOLEAN VidQueryMappingSupport(PCHAR pBuffer, ULONG inLen, ULONG outLen, PULONG pBytesReturned);
BOOLEAN VidQueryPartitionLayout(PCHAR pBuffer, ULONG inLen, ULONG outLen, PULONG pBytesReturned);
VOID HvmmNoteIoctl(ULONG Code, NTSTATUS Status, ULONG Bytes);
VOID HvmmCopyIoctlStats(PHVMM_IOCTL_STAT Stats, ULONG Slots);
BOOLEAN VidMapGpaRange(PFILE_OBJECT FileObject, PCHAR pBuffer, ULONG inLen, ULONG outLen, PULONG pBytesReturned);
BOOLEAN VidUnmapGpaRange(PFILE_OBJECT FileObject, PCHAR pBuffer, ULONG inLen);
VOID VidCleanupGpaMappings(PFILE_OBJECT FileObject);
BOOLEAN VidResolveLayout(PVM_PROCESS_CONTEXT pPartitionHandle, PVM_LAYOUT pLayout);
PGPAR_OBJECT VidGetGparObjectForGpa(PVM_PROCESS_CONTEXT pPartitionHandle, PVM_LAYOUT pLayout, UINT64 GPA);
BOOLEAN VidGetContainerMemoryBlock(PVM_PROCESS_CONTEXT pPartitionHandle, PVM_LAYOUT pLayout, PCHAR pBuffer, ULONG len, ULONG64 GPA);

//
// process.c
//

BOOLEAN VidInjectDllToVmwp(PCHAR pBuffer, ULONG len);

//
// hvmm.c
//

PVOID HvmmPoolAlloc(SIZE_T size);
PVOID HvmmPoolFree(PVOID p);

//AMD64.asm
PVOID ArchPsGetCurrentProcess(); // modified version of PsGetCurrentProcess
VOID ArchmReplacePsGetCurrentProcess(PVOID, PVOID);
VOID ArchInt3();
VOID ArchNewPsGetCurrentProcess();
PVOID ArchNewPsGetCurrentProcess02();

NTSTATUS KeReadProcessMemory(PEPROCESS Process, PVOID SourceAddress, PVOID TargetAddress, SIZE_T Size);
PVOID KernelGetProcAddress(PVOID ModuleBase, PCHAR pFunctionName);
PVOID FindDrvBaseAddress(PCHAR pModuleName);