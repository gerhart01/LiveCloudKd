typedef int (WINAPI *pMessageBox)(HWND, LPCWSTR, LPCWSTR, UINT);
typedef int (WINAPI *pMessageBoxA)(HWND, LPCSTR, LPCSTR, UINT);
typedef HMODULE (WINAPI *pLoadLibraryW)(LPCWSTR);
typedef FARPROC (WINAPI *pGetProcAddress)(HMODULE, LPCSTR);
typedef HANDLE (WINAPI *pCreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef BOOL (WINAPI *pVidReadMemoryBlockPageRange)(HANDLE, PVOID, UINT64, UINT64, PVOID, UINT64);
typedef BOOL (WINAPI *pVidWriteMemoryBlockPageRange)(HANDLE, PVOID, UINT64, UINT64, PVOID, UINT64);
typedef DWORD (WINAPI *pSetFilePointer)(HANDLE, LONG, PLONG, DWORD);
typedef LPVOID (WINAPI *pVirtualAlloc)(LPVOID, SIZE_T, DWORD, DWORD);
typedef BOOL (WINAPI *pVirtualFree)(LPVOID, SIZE_T, DWORD);
typedef BOOL (WINAPI *pVirtualProtect)(LPVOID, SIZE_T, DWORD, PDWORD);
typedef HANDLE (WINAPI *pCreateFileMappingA)(HANDLE, LPSECURITY_ATTRIBUTES, DWORD, DWORD, DWORD, LPCSTR);
typedef HANDLE (WINAPI *pCreateFileMappingW)(HANDLE, LPSECURITY_ATTRIBUTES, DWORD, DWORD, DWORD, LPCWSTR);
typedef LPVOID (WINAPI *pMapViewOfFile)(HANDLE, DWORD, DWORD, DWORD, SIZE_T);
typedef BOOL (WINAPI *pUnmapViewOfFile)(LPCVOID);
typedef DWORD (WINAPI *pGetFileSize)(HANDLE, LPDWORD);
typedef BOOL (WINAPI *pReadFile)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef VOID (WINAPI *pSetLastError)(DWORD);
typedef BOOLEAN(WINAPI *pSdkHvmmReadPhysicalMemoryHandle)(ULONG64, MB_PAGE_INDEX, UINT64, PVOID, READ_MEMORY_METHOD);
typedef PULONG64 (WINAPI *pSdkEnumPartitionsHandle)(PULONG64, PVM_OPERATIONS_CONFIG);
typedef BOOLEAN(WINAPI *pSdkSelectPartitionHandle)(ULONG64);
typedef ULONG64(WINAPI* pSdkSetData)(ULONG64, HVMM_INFORMATION_CLASS, ULONG64);

//
// Max file size of dmp file
//
#define MAX_HIGH_FILE_OFFSET 0xFF

typedef struct _MAP_FILE {
    PVOID Va;
    PHYSICAL_ADDRESS Pa;
    ULONG Size;
} MAP_FILE, *PMAP_FILE;

typedef struct _FUNCTION_TABLE {
    pLoadLibraryW _LoadLibrary;
    pGetProcAddress _GetProcAddress;
    pMessageBox _MessageBoxW;
    pMessageBoxA _MessageBoxA;
    pCreateFileW _CreateFileW;
    pSetFilePointer _SetFilePointer;
    pVirtualAlloc _VirtualAlloc;
    pVirtualFree _VirtualFree;
    pVirtualProtect _VirtualProtect;
    pVidReadMemoryBlockPageRange _VidReadMemoryBlockPageRange;
    pVidWriteMemoryBlockPageRange _VidWriteMemoryBlockPageRange;
	pSdkHvmmReadPhysicalMemoryHandle _SdkHvmmReadPhysicalMemoryHandle;
	pSdkEnumPartitionsHandle _SdkEnumPartitionsHandle;
	pSdkSelectPartitionHandle _SdkSelectPartitionHandle;
    pCreateFileMappingA _CreateFileMappingA;
    pCreateFileMappingW _CreateFileMappingW;
    pMapViewOfFile _MapViewOfFile;
	pSdkSetData _SdkSetData;
    pUnmapViewOfFile _UnmapViewOfFile;
    pGetFileSize _GetFileSize;
    pSetLastError _SetLastError;
    pReadFile _ReadFile;
    LARGE_INTEGER FileSize;
	ULONG64 VmId;
    HANDLE CrashDumpHandle;
    PUCHAR Header;
    ULONG HeaderSize;
    ULONG64 ContextPageIndex[MAX_PROCESSORS];
    ULONG ContextOffsetLow[MAX_PROCESSORS];
    HANDLE PartitionHandle;
    HANDLE PartitionHandleConst;
	ULONG64 CurrentPartitionHandle;
    MB_HANDLE MemoryHandle;
    HV_PARTITION_ID PartitionId;
    MACHINE_TYPE MachineType;
	VM_OPERATIONS_CONFIG VmOpsConfig;
    MAP_FILE MapFile[20];
    ULONG MapIndex;
    BOOL IsDllLoad;
	BOOL PartitionInit;
	CONTEXT Context;
    PHYSICAL_ADDRESS KdDebuggerDataBlockPa;
	ULONG64 KernelBase;
	ULONG64 NumberOfCPU;
	ULONG64 IdleKernelStack;
    CHAR KdDebuggerDataBlockBlock[KD_DEBUGGER_BLOCK_PAGE_SIZE];
	CHAR HvddPartition[0x7000];
} FUNCTION_TABLE, *PFUNCTION_TABLE;

#define TABLE_OFFSET (PVOID)(0xFFF000000) //random free address
#define CREATEFILE_OFFSET (PVOID)((PUCHAR)TABLE_OFFSET + sizeof(FUNCTION_TABLE))
#define CREATEFILEMAPPINGA_OFFSET (PVOID)((PUCHAR)CREATEFILE_OFFSET + 0x300)
#define CREATEFILEMAPPINGW_OFFSET (PVOID)((PUCHAR)CREATEFILEMAPPINGA_OFFSET + 0x100)
#define MAPVIEWOFFILE_OFFSET (PVOID)((PUCHAR)CREATEFILEMAPPINGW_OFFSET + 0x100)
#define UNMAPVIEWOFFILE_OFFSET (PVOID)((PUCHAR)MAPVIEWOFFILE_OFFSET + 0x1B00)
#define GETFILESIZE_OFFSET (PVOID)((PUCHAR)UNMAPVIEWOFFILE_OFFSET + 0x100)
#define READFILE_OFFSET (PVOID)((PUCHAR)GETFILESIZE_OFFSET + 0x100)
#define VIRTUALPROTECT_OFFSET (PVOID)((PUCHAR)READFILE_OFFSET + 0x100)

HANDLE WINAPI MyCreateFile(
  __in      LPCWSTR lpFileName,
  __in      DWORD dwDesiredAccess,
  __in      DWORD dwShareMode,
  __in_opt  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  __in      DWORD dwCreationDisposition,
  __in      DWORD dwFlagsAndAttributes,
  __in_opt  HANDLE hTemplateFile
);

LPVOID WINAPI MyMapViewOfFile(
  __in  HANDLE hFileMappingObject,
  __in  DWORD dwDesiredAccess,
  __in  DWORD dwFileOffsetHigh,
  __in  DWORD dwFileOffsetLow,
  __in  SIZE_T dwNumberOfBytesToMap
);

HANDLE WINAPI MyCreateFileMappingA(
  __in      HANDLE hFile,
  __in_opt  LPSECURITY_ATTRIBUTES lpAttributes,
  __in      DWORD flProtect,
  __in      DWORD dwMaximumSizeHigh,
  __in      DWORD dwMaximumSizeLow,
  __in_opt  LPCSTR lpName
);

HANDLE WINAPI MyCreateFileMappingW(
  __in      HANDLE hFile,
  __in_opt  LPSECURITY_ATTRIBUTES lpAttributes,
  __in      DWORD flProtect,
  __in      DWORD dwMaximumSizeHigh,
  __in      DWORD dwMaximumSizeLow,
  __in_opt  LPCWSTR lpName
);

BOOL WINAPI MyUnmapViewOfFile(
  __in  LPCVOID lpBaseAddress
);

DWORD WINAPI MyGetFileSize(
  __in       HANDLE hFile,
  __out_opt  LPDWORD lpFileSizeHigh
);

BOOL WINAPI MyReadFile(
  __in         HANDLE hFile,
  __out        LPVOID lpBuffer,
  __in         DWORD nNumberOfBytesToRead,
  __out_opt    LPDWORD lpNumberOfBytesRead,
  __inout_opt  LPOVERLAPPED lpOverlapped
);

BOOL WINAPI MyVirtualProtect(
  __in   LPVOID lpAddress,
  __in   SIZE_T dwSize,
  __in   DWORD flNewProtect,
  __out  PDWORD lpflOldProtect
);