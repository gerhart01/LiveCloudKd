
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

#define PARTITION_NAME_1803_OFFSET 0x78
#define PARTITION_ID_1803_OFFSET 0x278

//Vid.sys builded 05.2018
#define VID_PS_PROCESS_CHECK_01 0x123A5 
//#define VID_PS_PROCESS_CHECK_02 0x16847  
//Vid.sys builded 03.2019 18356
//#define VID_PS_PROCESS_CHECK_01 0x11fed
//#define VID_PS_PROCESS_CHECK_02 0x16847


#define VID_PARTITION_FRIENDLY_NAME_MAX (256)
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
// Mapped read mode (map.c). The codes start at 0x8A0, well above the existing ones. A client
// sends IOCTL_QUERY_MAPPING_SUPPORT first and nothing else unless it answers
// HVMM_MAPPING_SIGNATURE, so a map request never reaches a driver build that does not know it.
//

#define IOCTL_QUERY_MAPPING_SUPPORT CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x8A0, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MAP_GPA_RANGE CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x8A1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_UNMAP_GPA_RANGE CTL_CODE(\
	FILE_DEVICE_UNKNOWN, 0x8A2, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HVMM_MAPPING_QUERY_MAGIC   (0x714D7648UL)   // "HvMq"
#define HVMM_MAPPING_SIGNATURE     (0x704D7648UL)   // "HvMp"
#define HVMM_MAPPING_VERSION       (1UL)

//
// One map request never spans more than 8 MB. MDL.Size is a signed CSHORT, so
// sizeof(MDL) + 8 bytes per page must stay below 32768 (about 16 MB of 4 KB pages).
// The driver reports this value in the handshake and clients chunk by it.
//

#define HVMM_MAP_GPA_MAX_LENGTH (0x800000ULL)

//
// Live mappings one handle may hold. A 64 GB VM in 8 MB chunks is 8192.
//

#define HVMM_MAX_MAPPINGS_PER_HANDLE (8192UL)


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
typedef struct _GPA_INFO {
    ULONG64 PartitionId;
    ULONG64 StartPage;// Position in Bytes
    ULONG64 BytesCount; // now in bytes
	HANDLE PartitionHandle;
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

typedef struct _VID_VM_INFO {
    WCHAR FriendlyName[VID_PARTITION_FRIENDLY_NAME_MAX];
    HV_PARTITION_ID PartitionId;
	USR_VM_TYPE VmType;
} VID_VM_INFO, *PVID_VM_INFO;

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
// IOCTL_QUERY_MAPPING_SUPPORT: the handshake. Input carries HVMM_MAPPING_QUERY_MAGIC.
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
// IOCTL_MAP_GPA_RANGE: map a guest physical range into the calling process, read-only, and
// keep it mapped until IOCTL_UNMAP_GPA_RANGE or until the handle is closed. GpaStart and
// Length are bytes and page aligned. The driver maps the longest host-backed prefix of the
// range and reports it in MappedBytes; the caller continues from there.
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

typedef struct _UNMAP_GPA_RANGE_INPUT {
	PVOID UserVa;
} UNMAP_GPA_RANGE_INPUT, *PUNMAP_GPA_RANGE_INPUT;

//
// One live mapping. It holds a reference on the process it was mapped into and on the
// vid.sys partition file object it was resolved from, for as long as it lives.
//

typedef struct _HVMM_GPA_MAPPING {
	LIST_ENTRY Link;
	PMDL Mdl;
	PVOID UserVa;
	PEPROCESS Process;
	PFILE_OBJECT Partition;
	UINT64 MappedBytes;
} HVMM_GPA_MAPPING, *PHVMM_GPA_MAPPING;

//
// Per handle state, hung off FILE_OBJECT->FsContext2 by IRP_MJ_CREATE.
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
	CHAR cMblockString[0x8]; // "Mb  " signature
	PVOID PartitionHandle; // size 0x8
	CHAR Unknown01[0x8];
	ULONG MbHandle;
	CHAR Unknown02[0x1C];
	ULONG64 BitMapSize01; // offset 0x38, size 0x8
	ULONG64 BitMapSize02; // offset 0x40, size 0x8
	CHAR Unknown03[0xA8];
	PULONG64 pGuestGPAArray; //offset 0xF0, size 0x8
} MEMORY_BLOCK, *PMEMORY_BLOCK;

typedef struct _GPAR_OBJECT {
	CHAR cGparSignature[0x8]; // "GPAR" signature - eq GPA Range
	CHAR Unknown01[0xF8];
    UINT64 GpaIndexStart; //offset +0x100, size 0x8
    UINT64 GpaIndexEnd;  //offset +0x108, size 0x8
    UINT64 UnknowParam01;
    UINT64 UnknowParam02;
    UINT32 KernelMemoryBlockGpaRangeFlags; //offset +0x120, size 0x4
	CHAR Unknown02[0x4C];
	PMEMORY_BLOCK objMBlock; //offset +0x170, size 0x8 //in Windows 10 20H1 up to 0x8 bytes
	ULONG64 SomeGpaOffset; //offset +0x178, size 0x8
	ULONG64 VmmMemGpaOffset;//offset +0x180, size 0x8
} GPAR_OBJECT, *PGPAR_OBJECT;

typedef struct _GPAR_BLOCK_HANDLE {
    PVOID PartitionHandle;
    PGPAR_OBJECT GparArray;
    UINT32 Unknown01;
    UINT32 CountInGparArray;
} GPAR_BLOCK_HANDLE, *PGPAR_BLOCK_HANDLE;


typedef struct _MEMORY_BLOCK_ARRAY {
	ULONG64 Count;
	PULONG64 ArrayStart; // Start point to block of pointers to MBlock structures
} MEMORY_BLOCK_ARRAY, *PMEMORY_BLOCK_ARRAY;

typedef struct _VM_PROCESS_CONTEXT {
	CHAR cPrtnSignature[0x8];//Prtn signature
	CHAR Unknown01[0xF];
	CHAR IsSecurePartition;//offset +0x18, size 0x1
    CHAR Unknown02[0x4F];
	DWORD VmType; //offset +0x68 - size 0x4
	CHAR Unknown03[0xC];
    WCHAR FriendlyName[0x100]; //offset +0x78, size 0x200
    HV_PARTITION_ID PartitionId; // offset +0x278, size 0x8
    CHAR Unknown04[0xF98];
	PMEMORY_BLOCK_ARRAY ArrayOfMblocks;//offset +0x1218, size 0x8
    CHAR Unknown05[0x300];
    PGPAR_BLOCK_HANDLE pGparBlockHandle; // offset +0x1520, size 0x8
	CHAR Unknown06[0x1570];
	PMEMORY_BLOCK_ARRAY ArrayOfMblocks20H1;//offset +0x2A98, size 0x8
	CHAR Unknown07[0x300];
	PGPAR_BLOCK_HANDLE pGparBlockHandle20H1; // offset +0x2DA0, size 0x8
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
PGPAR_OBJECT VidGetGparObjectForGpa(PVM_PROCESS_CONTEXT pPartitionHandle, UINT64 GPA);

//
// map.c
//

NTSTATUS MapCreateFileContext(PFILE_OBJECT FileObject);
VOID MapCleanupFileContext(PFILE_OBJECT FileObject);
VOID MapFreeFileContext(PFILE_OBJECT FileObject);
NTSTATUS MapQuerySupport(PCHAR pBuffer, ULONG inLen, ULONG outLen, PULONG pBytesReturned);
NTSTATUS MapGpaRange(PFILE_OBJECT FileObject, PCHAR pBuffer, ULONG inLen, ULONG outLen, PULONG pBytesReturned);
NTSTATUS UnmapGpaRange(PFILE_OBJECT FileObject, PCHAR pBuffer, ULONG inLen);
BOOLEAN VidGetContainerMemoryBlock(PVM_PROCESS_CONTEXT pPartitionHandle, PCHAR pBuffer, ULONG len, ULONG64 GPA);

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