/*++
    Copyright (C) 2010 Matthieu Suiche http://www.msuiche.net 
    Copyright (C) 2010 MoonSols. <http://www.moonsols.com>
    All rights reserved.

Module Name:

    - dmp.h

Abstract:

    - 


Environment:

    - User mode

Revision History:

    - Matthieu Suiche (Jan-2010)

--*/

#define DUMP_SIGNATURE ('EGAP')
#define DUMP_VALID_DUMP ('PMUD')
#define DUMP_VALID_DUMP64 ('46UD')

#define DUMP_TYPE_FULL 1

#define X86_KPRCB_OFFSET              0x120
#define X86_KPROCESSOR_STATE_OFFSET   0x01C
#define X86_CONTEXT_OFFSET            0x000
#define X86_KSPECIAL_REGISTERS_OFFSET 0x2CC
//
// If Win7 and above.
//
#define X86_NT61_KPROCESSOR_STATE_OFFSET 0x18

#define X64_KPRCB_OFFSET                0x180
#define X64_KPROCESSOR_STATE_OFFSET     0x100 //Windows 10 1803 HARDCODED
#define X64_KSPECIAL_REGISTERS_OFFSET   0x000

extern FUNCTION_TABLE FunctionTable;

typedef struct _PHYSICAL_MEMORY_RUN32 {
    ULONG BasePage;
    ULONG PageCount;
} PHYSICAL_MEMORY_RUN32, *PPHYSICAL_MEMORY_RUN32;

typedef struct _PHYSICAL_MEMORY_DESCRIPTOR32 {
    ULONG NumberOfRuns;
    ULONG NumberOfPages;
    PHYSICAL_MEMORY_RUN32 Run[1]; // NumberOfRuns is the total entries.
} PHYSICAL_MEMORY_DESCRIPTOR32, *PPHYSICAL_MEMORY_DESCRIPTOR32;

typedef struct _PHYSICAL_MEMORY_RUN64 {
    ULONG64 BasePage;
    ULONG64 PageCount;
} PHYSICAL_MEMORY_RUN64, *PPHYSICAL_MEMORY_RUN64;

typedef struct _PHYSICAL_MEMORY_DESCRIPTOR64 {
    ULONG NumberOfRuns;
    ULONG64 NumberOfPages;
    PHYSICAL_MEMORY_RUN64 Run[0x20];
} PHYSICAL_MEMORY_DESCRIPTOR64, *PPHYSICAL_MEMORY_DESCRIPTOR64;

typedef struct _DUMP_HEADER32 {
    ULONG Signature;
    ULONG ValidDump;
    ULONG MajorVersion;
    ULONG MinorVersion;
    ULONG DirectoryTableBase;
    ULONG PfnDataBase;
    ULONG PsLoadedModuleList;
    ULONG PsActiveProcessHead;
    ULONG MachineImageType;
    ULONG NumberProcessors;
    ULONG BugCheckCode;
    ULONG BugCheckParameter1;
    ULONG BugCheckParameter2;
    ULONG BugCheckParameter3;
    ULONG BugCheckParameter4;
    CHAR VersionUser[32];
    CHAR PaeEnabled;
    CHAR KdSecondaryVersion;
    CHAR spare[2];
    ULONG KdDebuggerDataBlock;
    union {
        PHYSICAL_MEMORY_DESCRIPTOR32 PhysicalMemoryBlock;
        UCHAR PhysicalMemoryBlockBuffer[700];
    };
    union {
        UCHAR ContextRecord[1200];
    };
    EXCEPTION_RECORD32 ExceptionRecord;
    CHAR Comment[128];
    UCHAR reserved0[1768];
    ULONG DumpType;
    ULONG MiniDumpFields;
    ULONG SecondaryDataState;
    ULONG ProductType;
    ULONG SuiteMask;
    ULONG WriterStatus;
    LARGE_INTEGER RequiredDumpSpace;
    UCHAR reserved2[16];
    FILETIME SystemUpTime;
    FILETIME SystemTime;
    UCHAR reserved3[56];
} DUMP_HEADER32, *PDUMP_HEADER32;

typedef struct _DUMP_HEADER64 {
    ULONG Signature;
    ULONG ValidDump;
    ULONG MajorVersion;
    ULONG MinorVersion;
    ULONG64 DirectoryTableBase;
    ULONG64 PfnDataBase;
    ULONG64 PsLoadedModuleList;
    ULONG64 PsActiveProcessHead;
    ULONG MachineImageType;
    ULONG NumberProcessors;
    ULONG BugCheckCode;
    ULONG64 BugCheckParameter1;
    ULONG64 BugCheckParameter2;
    ULONG64 BugCheckParameter3;
    ULONG64 BugCheckParameter4;
    CHAR VersionUser[32];
    PKDDEBUGGER_DATA64 KdDebuggerDataBlock;
    union {
        PHYSICAL_MEMORY_DESCRIPTOR64 PhysicalMemoryBlock;
        UCHAR PhysicalMemoryBlockBuffer [700];
    };
    UCHAR ContextRecord[3000];
    EXCEPTION_RECORD64 ExceptionRecord;
    ULONG DumpType;
    LARGE_INTEGER RequiredDumpSpace;
    FILETIME SystemTime;
    CHAR Comment[0x80]; // May not be present.
    FILETIME SystemUpTime;
    ULONG MiniDumpFields;
    ULONG SecondaryDataState;
    ULONG ProductType;
    ULONG SuiteMask;
    ULONG WriterStatus;
    UCHAR Unused1;
    UCHAR KdSecondaryVersion; // Present only for W2K3 SP1 and better
    UCHAR Unused[2];
    UCHAR _reserved0[4016];
} DUMP_HEADER64, *PDUMP_HEADER64;

typedef struct _X86_CONTEXT {
    /*0x000*/     ULONG32      ContextFlags;
    /*0x004*/     ULONG32      Dr0;
    /*0x008*/     ULONG32      Dr1;
    /*0x00C*/     ULONG32      Dr2;
    /*0x010*/     ULONG32      Dr3;
    /*0x014*/     ULONG32      Dr6;
    /*0x018*/     ULONG32      Dr7;
    /*0x01C*/     UCHAR        FloatSave[0x70];
    /*0x08C*/     ULONG32      SegGs;
    /*0x090*/     ULONG32      SegFs;
    /*0x094*/     ULONG32      SegEs;
    /*0x098*/     ULONG32      SegDs;
    /*0x09C*/     ULONG32      Edi;
    /*0x0A0*/     ULONG32      Esi;
    /*0x0A4*/     ULONG32      Ebx;
    /*0x0A8*/     ULONG32      Edx;
    /*0x0AC*/     ULONG32      Ecx;
    /*0x0B0*/     ULONG32      Eax;
    /*0x0B4*/     ULONG32      Ebp;
    /*0x0B8*/     ULONG32      Eip;
    /*0x0BC*/     ULONG32      SegCs;
    /*0x0C0*/     ULONG32      EFlags;
    /*0x0C4*/     ULONG32      Esp;
    /*0x0C8*/     ULONG32      SegSs;
    /*0x0CC*/     UINT8        ExtendedRegisters[512];
} X86_CONTEXT, *PX86_CONTEXT;

#define RPL_MASK            0x0003
#define MODE_MASK           0x0001

#define KGDT_NULL           (0x00)
#define KGDT_R0_CODE        (0x8)
#define KGDT_R0_DATA        (0x10)
#define KGDT_R3_CODE        (0x18)
#define KGDT_R3_DATA        (0x20)
#define KGDT_TSS            (0x28)
#define KGDT_R0_PCR         (0x30)
#define KGDT_R3_TEB         (0x38)
#define KGDT_LDT            (0x48)
#define KGDT_DF_TSS         (0x50)
#define KGDT_NMI_TSS        (0x58)

typedef struct _X64_CONTEXT {
/*0x000*/     UINT64       P1Home;
/*0x008*/     UINT64       P2Home;
/*0x010*/     UINT64       P3Home;
/*0x018*/     UINT64       P4Home;
/*0x020*/     UINT64       P5Home;
/*0x028*/     UINT64       P6Home;
/*0x030*/     ULONG32      ContextFlags;
/*0x034*/     ULONG32      MxCsr;
/*0x038*/     UINT16       SegCs;
/*0x03A*/     UINT16       SegDs;
/*0x03C*/     UINT16       SegEs;
/*0x03E*/     UINT16       SegFs;
/*0x040*/     UINT16       SegGs;
/*0x042*/     UINT16       SegSs;
/*0x044*/     ULONG32      EFlags;
/*0x048*/     UINT64       Dr0;
/*0x050*/     UINT64       Dr1;
/*0x058*/     UINT64       Dr2;
/*0x060*/     UINT64       Dr3;
/*0x068*/     UINT64       Dr6;
/*0x070*/     UINT64       Dr7;
/*0x078*/     UINT64       Rax;
/*0x080*/     UINT64       Rcx;
/*0x088*/     UINT64       Rdx;
/*0x090*/     UINT64       Rbx;
/*0x098*/     UINT64       Rsp;
/*0x0A0*/     UINT64       Rbp;
/*0x0A8*/     UINT64       Rsi;
/*0x0B0*/     UINT64       Rdi;
/*0x0B8*/     UINT64       R8;
/*0x0C0*/     UINT64       R9;
/*0x0C8*/     UINT64       R10;
/*0x0D0*/     UINT64       R11;
/*0x0D8*/     UINT64       R12;
/*0x0E0*/     UINT64       R13;
/*0x0E8*/     UINT64       R14;
/*0x0F0*/     UINT64       R15;
/*0x0F8*/     UINT64       Rip;
    struct
    {
        /*0x100*/ struct _M128A Header[2];
        /*0x120*/ struct _M128A Legacy[8];
        /*0x1A0*/ struct _M128A Xmm0;
        /*0x1B0*/ struct _M128A Xmm1;
        /*0x1C0*/ struct _M128A Xmm2;
        /*0x1D0*/ struct _M128A Xmm3;
        /*0x1E0*/ struct _M128A Xmm4;
        /*0x1F0*/ struct _M128A Xmm5;
        /*0x200*/ struct _M128A Xmm6;
        /*0x210*/ struct _M128A Xmm7;
        /*0x220*/ struct _M128A Xmm8;
        /*0x230*/ struct _M128A Xmm9;
        /*0x240*/ struct _M128A Xmm10;
        /*0x250*/ struct _M128A Xmm11;
        /*0x260*/ struct _M128A Xmm12;
        /*0x270*/ struct _M128A Xmm13;
        /*0x280*/ struct _M128A Xmm14;
        /*0x290*/ struct _M128A Xmm15;
        /*0x2A0*/ UINT8 _PADDING0_[0x60];
    };
/*0x300*/     struct       _M128A VectorRegister[26];
/*0x4A0*/     UINT64       VectorControl;
/*0x4A8*/     UINT64       DebugControl;
/*0x4B0*/     UINT64       LastBranchToRip;
/*0x4B8*/     UINT64       LastBranchFromRip;
/*0x4C0*/     UINT64       LastExceptionToRip;
/*0x4C8*/     UINT64       LastExceptionFromRip;
} X64_CONTEXT, *PX64_CONTEXT;

#define KGDT64_NULL         (0 * 16)
#define KGDT64_R0_CODE      (1 * 16)
#define KGDT64_R0_DATA      ((1 * 16) + 8)
#define KGDT64_R3_CMCODE    (2 * 16)
#define KGDT64_R3_DATA      ((2 * 16) + 8)
#define KGDT64_R3_CODE      (3 * 16)
#define KGDT64_SYS_TSS      (4 * 16)
#define KGDT64_R3_CMTEB     (5 * 16)
#define KGDT64_LAST         (6 * 16)