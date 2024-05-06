// base on https://github.com/wbenny/injdrv

#include "hvmm.h"
#include "injlib.h"

UCHAR InjpThunkX64[] = {              //
  0x48, 0x83, 0xec, 0x38,             // sub    rsp,0x38
  0x48, 0x89, 0xc8,                   // mov    rax,rcx
  0x66, 0x44, 0x89, 0x44, 0x24, 0x20, // mov    [rsp+0x20],r8w
  0x66, 0x44, 0x89, 0x44, 0x24, 0x22, // mov    [rsp+0x22],r8w
  0x4c, 0x8d, 0x4c, 0x24, 0x40,       // lea    r9,[rsp+0x40]
  0x48, 0x89, 0x54, 0x24, 0x28,       // mov    [rsp+0x28],rdx
  0x4c, 0x8d, 0x44, 0x24, 0x20,       // lea    r8,[rsp+0x20]
  //0xeb, 0xfe,                         // jmp +0
  0x31, 0xd2,                         // xor    edx,edx
  0x31, 0xc9,                         // xor    ecx,ecx
  0xff, 0xd0,                         // call   rax
  0x48, 0x83, 0xc4, 0x38,             // add    rsp,0x38
  0xc2, 0x00, 0x00,                   // ret    0x0
};

UCHAR InjpThunkARM64[] = {            //
  0xfe, 0x0f, 0x1f, 0xf8,             // str    lr,[sp,#-0x10]!
  0xff, 0x83, 0x00, 0xd1,             // sub    sp,sp,#0x20
  0xe9, 0x03, 0x00, 0xaa,             // mov    x9,x0
  0xe2, 0x13, 0x00, 0x79,             // strh   w2,[sp,#8]
  0x00, 0x00, 0x80, 0xd2,             // mov    x0,#0
  0xe2, 0x17, 0x00, 0x79,             // strh   w2,[sp,#0xA]
  0xe2, 0x23, 0x00, 0x91,             // add    x2,sp,#8
  0xe1, 0x0b, 0x00, 0xf9,             // str    x1,[sp,#0x10]
  0x01, 0x00, 0x80, 0xd2,             // mov    x1,#0
  0xe3, 0x03, 0x00, 0x91,             // mov    x3,sp
  0x20, 0x01, 0x3f, 0xd6,             // blr    x9
  0xff, 0x83, 0x00, 0x91,             // add    sp,sp,#0x20
  0xfe, 0x07, 0x41, 0xf8,             // ldr    lr,[sp],#0x10
  0xc0, 0x03, 0x5f, 0xd6,             // ret
};

LIST_ENTRY      InjInfoListHead;

INJ_METHOD      InjMethod;

UNICODE_STRING  InjDllPath[InjArchitectureMax];

PETHREAD pThread = NULL;

ANSI_STRING LdrLoadDllRoutineName = RTL_CONSTANT_STRING("LdrLoadDll");

INJ_THUNK       InjThunk[InjArchitectureMax] = {
  { InjpThunkX64,   sizeof(InjpThunkX64)   },
  { InjpThunkARM64, sizeof(InjpThunkARM64) },
};


PVOID
NTAPI
RtlxFindExportedRoutineByName(
	_In_ PVOID DllBase,
	_In_ PANSI_STRING ExportName
)
{
	//
	// RtlFindExportedRoutineByName is not exported by ntoskrnl until Win10.
	// Following code is borrowed from ReactOS.
	//

	PULONG NameTable;
	PUSHORT OrdinalTable;
	PIMAGE_EXPORT_DIRECTORY ExportDirectory;
	LONG Low = 0, Mid = 0, High, Ret;
	USHORT Ordinal;
	PVOID Function;
	ULONG ExportSize;
	PULONG ExportTable;

	//
	// Get the export directory.
	//

	ExportDirectory = RtlImageDirectoryEntryToData(DllBase,
		TRUE,
		IMAGE_DIRECTORY_ENTRY_EXPORT,
		&ExportSize);

	if (!ExportDirectory)
	{
		return NULL;
	}

	//
	// Setup name tables.
	//

	NameTable = (PULONG)((ULONG_PTR)DllBase + ExportDirectory->AddressOfNames);
	OrdinalTable = (PUSHORT)((ULONG_PTR)DllBase + ExportDirectory->AddressOfNameOrdinals);

	//
	// Do a binary search.
	//

	High = ExportDirectory->NumberOfNames - 1;
	while (High >= Low)
	{
		//
		// Get new middle value.
		//

		Mid = (Low + High) >> 1;

		//
		// Compare name.
		//

		Ret = strcmp(ExportName->Buffer, (PCHAR)DllBase + NameTable[Mid]);
		if (Ret < 0)
		{
			//
			// Update high.
			//
			High = Mid - 1;
		}
		else if (Ret > 0)
		{
			//
			// Update low.
			//
			Low = Mid + 1;
		}
		else
		{
			//
			// We got it.
			//
			break;
		}
	}

	//
	// Check if we couldn't find it.
	//

	if (High < Low)
	{
		return NULL;
	}

	//
	// Otherwise, this is the ordinal.
	//

	Ordinal = OrdinalTable[Mid];

	//
	// Validate the ordinal.
	//

	if (Ordinal >= ExportDirectory->NumberOfFunctions)
	{
		return NULL;
	}

	//
	// Resolve the address and write it.
	//

	ExportTable = (PULONG)((ULONG_PTR)DllBase + ExportDirectory->AddressOfFunctions);
	Function = (PVOID)((ULONG_PTR)DllBase + ExportTable[Ordinal]);

	//
	// We found it!
	//

	NT_ASSERT(
		(Function < (PVOID)ExportDirectory) ||
		(Function > (PVOID)((ULONG_PTR)ExportDirectory + ExportSize))
	);

	return Function;
}


NTSTATUS
NTAPI
InjpQueueApc(
	_In_ KPROCESSOR_MODE ApcMode,
	_In_ PKNORMAL_ROUTINE NormalRoutine,
	_In_ PVOID NormalContext,
	_In_ PVOID SystemArgument1,
	_In_ PVOID SystemArgument2
)
{
	//
	// Allocate memory for the KAPC structure.
	//

	PETHREAD eThread;

	PKAPC Apc = ExAllocatePoolWithTag(NonPagedPoolNx,
		sizeof(KAPC),
		INJ_MEMORY_TAG);

	if (!Apc)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	// Initialize and queue the APC.
	//

	eThread = PsGetCurrentThread();

	if (pThread != NULL) {
		eThread = pThread;
	}

	KeInitializeApc(Apc,                                  // Apc
		eThread,                 // Thread
		OriginalApcEnvironment,               // Environment
		&InjpInjectApcKernelRoutine,          // KernelRoutine
		NULL,                                 // RundownRoutine
		NormalRoutine,                        // NormalRoutine
		ApcMode,                              // ApcMode
		NormalContext);                       // NormalContext

	BOOLEAN Inserted = KeInsertQueueApc(Apc,              // Apc
		SystemArgument1,  // SystemArgument1
		SystemArgument2,  // SystemArgument2
		0);               // Increment

	if (!Inserted)
	{
		ExFreePoolWithTag(Apc, INJ_MEMORY_TAG);
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}

VOID
NTAPI
InjpInjectApcNormalRoutine(
	_In_ PVOID NormalContext,
	_In_ PVOID SystemArgument1,
	_In_ PVOID SystemArgument2
)
{
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);

	PINJ_INJECTION_INFO InjectionInfo = NormalContext;
	InjInject(InjectionInfo);
}

VOID
NTAPI
InjpInjectApcKernelRoutine(
	_In_ PKAPC Apc,
	_Inout_ PKNORMAL_ROUTINE* NormalRoutine,
	_Inout_ PVOID* NormalContext,
	_Inout_ PVOID* SystemArgument1,
	_Inout_ PVOID* SystemArgument2
)
{
	UNREFERENCED_PARAMETER(NormalRoutine);
	UNREFERENCED_PARAMETER(NormalContext);
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);

	//
	// Common kernel routine for both user-mode and
	// kernel-mode APCs queued by the InjpQueueApc
	// function.  Just release the memory of the APC
	// structure and return back.
	//

	ExFreePoolWithTag(Apc, INJ_MEMORY_TAG);
}

NTSTATUS
NTAPI
InjpInject(
	_In_ PINJ_INJECTION_INFO InjectionInfo,
	_In_ INJ_ARCHITECTURE Architecture,
	_In_ HANDLE SectionHandle,
	_In_ SIZE_T SectionSize
)
{
	NTSTATUS Status;

	//
	// First, map this section with read-write access.
	//

	PVOID SectionMemoryAddress = NULL;
	Status = ZwMapViewOfSection(SectionHandle,
		ZwCurrentProcess(),
		&SectionMemoryAddress,
		0,
		SectionSize,
		NULL,
		&SectionSize,
		ViewUnmap,
		0,
		PAGE_READWRITE);

	if (!NT_SUCCESS(Status))
	{
		goto Exit;
	}

	//
	// Code of the APC routine (ApcNormalRoutine defined in the
	// "shellcode" above) starts at the SectionMemoryAddress.
	// Copy the shellcode to the allocated memory.
	//

	PVOID ApcRoutineAddress = SectionMemoryAddress;
	RtlCopyMemory(ApcRoutineAddress,
		InjThunk[Architecture].Buffer,
		InjThunk[Architecture].Length);

	//
	// Fill the data of the ApcContext.
	//

	PWCHAR DllPath = (PWCHAR)((PUCHAR)SectionMemoryAddress + InjThunk[Architecture].Length);
	RtlCopyMemory(DllPath,
		InjDllPath[Architecture].Buffer,
		InjDllPath[Architecture].Length);

	//
	// Unmap the section and map it again, but now
	// with read-execute (no write) access.
	//

	ZwUnmapViewOfSection(ZwCurrentProcess(), SectionMemoryAddress);

	SectionMemoryAddress = NULL;
	Status = ZwMapViewOfSection(SectionHandle,
		ZwCurrentProcess(),
		&SectionMemoryAddress,
		0,
		PAGE_SIZE,
		NULL,
		&SectionSize,
		ViewUnmap,
		0,
		PAGE_EXECUTE_READ);

	if (!NT_SUCCESS(Status))
	{
		goto Exit;
	}

	//
	// Reassign remapped address.
	//

	ApcRoutineAddress = SectionMemoryAddress;
	DllPath = (PWCHAR)((PUCHAR)SectionMemoryAddress + InjThunk[Architecture].Length);

	PVOID ApcContext = (PVOID)InjectionInfo->LdrLoadDllRoutineAddress;
	PVOID ApcArgument1 = (PVOID)DllPath;
	PVOID ApcArgument2 = (PVOID)InjDllPath[Architecture].Length;

	PKNORMAL_ROUTINE ApcRoutine = (PKNORMAL_ROUTINE)(ULONG_PTR)ApcRoutineAddress;

	Status = InjpQueueApc(UserMode,
		ApcRoutine,
		ApcContext,
		ApcArgument1,
		ApcArgument2);

	if (!NT_SUCCESS(Status))
	{
		//
		// If injection failed for some reason, unmap the section.
		//

		ZwUnmapViewOfSection(ZwCurrentProcess(), SectionMemoryAddress);
	}

Exit:
	return Status;
}

NTSTATUS
NTAPI
InjpInjectX64NoThunk(
	_In_ PINJ_INJECTION_INFO InjectionInfo,
	_In_ INJ_ARCHITECTURE Architecture,
	_In_ HANDLE SectionHandle,
	_In_ SIZE_T SectionSize
)
{
	NT_ASSERT(InjectionInfo->LdrLoadDllRoutineAddress);
	NT_ASSERT(Architecture == InjArchitectureX64);

	UNREFERENCED_PARAMETER(Architecture);

	NTSTATUS Status;

	PVOID SectionMemoryAddress = NULL;
	Status = ZwMapViewOfSection(SectionHandle,
		ZwCurrentProcess(),
		&SectionMemoryAddress,
		0,
		PAGE_SIZE,
		NULL,
		&SectionSize,
		ViewUnmap,
		0,
		PAGE_READWRITE);

	if (!NT_SUCCESS(Status))
	{
		goto Exit;
	}

	//
	// Create the UNICODE_STRING structure and fill out the
	// full path of the DLL.
	//

	PUNICODE_STRING DllPath = (PUNICODE_STRING)(SectionMemoryAddress);
	PWCHAR DllPathBuffer = (PWCHAR)((PUCHAR)DllPath + sizeof(UNICODE_STRING));

	RtlCopyMemory(DllPathBuffer,
		InjDllPath[Architecture].Buffer,
		InjDllPath[Architecture].Length);

	RtlInitUnicodeString(DllPath, DllPathBuffer);

	Status = InjpQueueApc(UserMode,
		(PKNORMAL_ROUTINE)(ULONG_PTR)InjectionInfo->LdrLoadDllRoutineAddress,
		NULL,     // Translates to 1st param. of LdrLoadDll (SearchPath)
		NULL,     // Translates to 2nd param. of LdrLoadDll (DllCharacteristics)
		DllPath);  // Translates to 3rd param. of LdrLoadDll (DllName));

//
// 4th param. of LdrLoadDll (BaseAddress) is actually an output parameter.
//
// When control is transferred to the KiUserApcDispatcher routine of the
// 64-bit ntdll.dll, the RSP points to the CONTEXT structure which might
// be eventually provided to the ZwContinue function (in case this APC
// dispatch will be routed to the Wow64 subsystem).
//
// Also, the value of the RSP register is moved to the R9 register before
// calling the KiUserCallForwarder function.  The KiUserCallForwarder
// function actually passes this value of the R9 register down to the
// NormalRoutine as a "hidden 4th parameter".
//
// Because LdrLoadDll writes to the provided address, it'll actually
// result in overwrite of the CONTEXT.P1Home field (the first field of
// the CONTEXT structure).
//
// Luckily for us, this field is only used in the very early stage of
// the APC dispatch and can be overwritten without causing any troubles.
//
// For excellent explanation, see:
// https://www.sentinelone.com/blog/deep-hooks-monitoring-native-execution-wow64-applications-part-2
//

Exit:
	return Status;
}

VOID
NTAPI
InjDestroy(
	VOID
)
{
	//
	// Release memory of all injection-info entries.
	//

	PLIST_ENTRY NextEntry = InjInfoListHead.Flink;

	while (NextEntry != &InjInfoListHead)
	{
		PINJ_INJECTION_INFO InjectionInfo = CONTAINING_RECORD(NextEntry,
			INJ_INJECTION_INFO,
			ListEntry);
		NextEntry = NextEntry->Flink;

		ExFreePoolWithTag(InjectionInfo, INJ_MEMORY_TAG);
	}

	//
	// Release memory of all buffers.
	//

	for (ULONG Architecture = 0; Architecture < InjArchitectureMax; Architecture += 1)
	{
		RtlFreeUnicodeString(&InjDllPath[Architecture]);
	}
}

NTSTATUS
NTAPI
InjCreateInjectionInfo(
	_In_opt_ PINJ_INJECTION_INFO* InjectionInfo,
	_In_ HANDLE ProcessId
)
{
	PINJ_INJECTION_INFO CapturedInjectionInfo;

	if (InjectionInfo && *InjectionInfo)
	{
		CapturedInjectionInfo = *InjectionInfo;
	}
	else
	{
		CapturedInjectionInfo = HvmmPoolAlloc(sizeof(INJ_INJECTION_INFO));

		if (!CapturedInjectionInfo)
		{
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		if (InjectionInfo)
		{
			*InjectionInfo = CapturedInjectionInfo;
		}
	}

	RtlZeroMemory(CapturedInjectionInfo, sizeof(INJ_INJECTION_INFO));

	CapturedInjectionInfo->ProcessId = ProcessId;
	CapturedInjectionInfo->ForceUserApc = TRUE;
	CapturedInjectionInfo->Method = InjMethod;

	InsertTailList(&InjInfoListHead, &CapturedInjectionInfo->ListEntry);

	return STATUS_SUCCESS;
}

VOID
NTAPI
InjRemoveInjectionInfo(
	_In_ PINJ_INJECTION_INFO InjectionInfo,
	_In_ BOOLEAN FreeMemory
)
{
	RemoveEntryList(&InjectionInfo->ListEntry);

	if (FreeMemory)
	{
		ExFreePoolWithTag(InjectionInfo, INJ_MEMORY_TAG);
	}
}

VOID
NTAPI
InjRemoveInjectionInfoByProcessId(
	_In_ HANDLE ProcessId,
	_In_ BOOLEAN FreeMemory
)
{
	PINJ_INJECTION_INFO InjectionInfo = InjFindInjectionInfo(ProcessId);

	if (InjectionInfo)
	{
		InjRemoveInjectionInfo(InjectionInfo, FreeMemory);
	}
}

PINJ_INJECTION_INFO
NTAPI
InjFindInjectionInfo(
	_In_ HANDLE ProcessId
)
{
	PLIST_ENTRY NextEntry = InjInfoListHead.Flink;

	while (NextEntry != &InjInfoListHead)
	{
		PINJ_INJECTION_INFO InjectionInfo = CONTAINING_RECORD(NextEntry,
			INJ_INJECTION_INFO,
			ListEntry);

		if (InjectionInfo->ProcessId == ProcessId)
		{
			return InjectionInfo;
		}

		NextEntry = NextEntry->Flink;
	}

	return NULL;
}

BOOLEAN
NTAPI
InjCanInject(
	_In_ PINJ_INJECTION_INFO InjectionInfo
)
{
	//
	// DLLs that need to be loaded in the native process
	// (i.e.: x64 process on x64 Windows, x86 process on
	// x86 Windows) before we can safely load our DLL.
	//

	ULONG RequiredDlls = INJ_SYSTEM32_NTDLL_LOADED;

	return (InjectionInfo->LoadedDlls & RequiredDlls) == RequiredDlls;
}

NTSTATUS
NTAPI
InjInject(
	_In_ PINJ_INJECTION_INFO InjectionInfo
)
{
	NTSTATUS Status;

	//
	// Create memory space for injection-specific data,
	// such as path to the to-be-injected DLL.  Memory
	// of this section will be eventually mapped to the
	// injected process.
	//
	// Note that this memory is created using sections
	// instead of ZwAllocateVirtualMemory, mainly because
	// function ZwProtectVirtualMemory is not exported
	// by ntoskrnl.exe until Windows 8.1.  In case of
	// sections, the effect of memory protection change
	// is achieved by remaping the section with different
	// protection type.
	//

	//DbgBreakPoint();

	OBJECT_ATTRIBUTES ObjectAttributes;
	InitializeObjectAttributes(&ObjectAttributes,
		NULL,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	HANDLE SectionHandle;
	SIZE_T SectionSize = PAGE_SIZE;
	LARGE_INTEGER MaximumSize;
	MaximumSize.QuadPart = SectionSize;
	Status = ZwCreateSection(&SectionHandle,
		GENERIC_READ | GENERIC_WRITE,
		&ObjectAttributes,
		&MaximumSize,
		PAGE_EXECUTE_READWRITE,
		SEC_COMMIT,
		NULL);

	if (!NT_SUCCESS(Status))
	{
		return Status;
	}

	INJ_ARCHITECTURE Architecture = InjArchitectureMax;

	if (InjectionInfo->Method == InjMethodThunk ||
		InjectionInfo->Method == InjMethodWow64LogReparse)
	{

		Architecture = InjArchitectureX64;

		NT_ASSERT(Architecture != InjArchitectureMax);

		InjpInject(InjectionInfo,
			Architecture,
			SectionHandle,
			SectionSize);
	}
#if defined(_M_AMD64)
	else if (InjectionInfo->Method == InjMethodThunkless)
	{
		Architecture = InjArchitectureX64;

		InjpInjectX64NoThunk(InjectionInfo,
			Architecture,
			SectionHandle,
			SectionSize);
	}
#endif

	ZwClose(SectionHandle);

	if (NT_SUCCESS(Status) && InjectionInfo->ForceUserApc)
	{
		//
		// Sets CurrentThread->ApcState.UserApcPending to TRUE.
		// This causes the queued user APC to be triggered immediately
		// on next transition of this thread to the user-mode.
		//

		KeTestAlertThread(UserMode);
	}

	return Status;
}

BOOLEAN VidInjectDllToVmwp(PCHAR pBuffer, ULONG len)
{
	UNREFERENCED_PARAMETER(len);
	PVID_INJECTION_INFO pInjInfo = (PVID_INJECTION_INFO) pBuffer;
	HANDLE ProcessId;
	NTSTATUS Status;
	PEPROCESS ProcessHandle = NULL;

	//DbgBreakPoint();
	
	if (pInjInfo == NULL)
		return FALSE;

	ProcessId = (HANDLE) pInjInfo->VmwpPid;

	Status = PsLookupProcessByProcessId(ProcessId, &ProcessHandle);

	if (Status != STATUS_SUCCESS) {
		KDbgLog("Status of PsLookupProcessByProcessId", Status);
		return FALSE;
	}

	InitializeListHead(&InjInfoListHead); // InjDestroy() in Unload Routine

	//INJ_SETTINGS Settings;

	InjMethod = InjMethodThunkless;

	InjDbgPrint("[injlib]: InjMethod: '%s'\n",
		InjMethod == InjMethodThunk ? "InjMethodThunk" :
		InjMethod == InjMethodThunkless ? "InjMethodThunkLess" :
		InjMethod == InjMethodWow64LogReparse ? "InjMethodWow64LogReparse" :
		"UNKNOWN"
	);


	RtlInitUnicodeString(&InjDllPath[InjArchitectureX64], pInjInfo->VidAuxDllPath);

	InjCreateInjectionInfo(NULL, ProcessId);

	PINJ_INJECTION_INFO InjectionInfo = InjFindInjectionInfo(ProcessId);

	if (!InjectionInfo || InjectionInfo->IsInjected)
	{
		return FALSE;
	}

	if (PsIsProtectedProcess(ProcessHandle))
	{
		//
		// Protected processes throw code-integrity error when
		// they are injected.  Signing policy can be changed, but
		// it requires hacking with lots of internal and Windows-
		// version-specific structures.  Simly don't inject such
		// processes.
		//
		// See Blackbone project (https://github.com/DarthTon/Blackbone)
		// if you're interested how protection can be temporarily
		// disabled on such processes.  (Look for BBSetProtection).
		//

		InjDbgPrint("[injlib]: Ignoring protected process (PID: %u, Name: '%s')\n",
			(ULONG)(ULONG_PTR)ProcessId,
			PsGetProcessImageFileName(ProcessHandle));

		InjRemoveInjectionInfoByProcessId(ProcessId, TRUE);

		return FALSE;
	}

	ULONG DllFlag = INJ_SYSTEM32_NTDLL_LOADED;
	InjectionInfo->LoadedDlls |= DllFlag;

	InjDbgPrint("[injlib]: Injecting (PID: %u, Name: '%s')\n",
		(ULONG)(ULONG_PTR)ProcessId,
		PsGetProcessImageFileName(ProcessHandle));

	//
	// Find thread in vmwp.exe process
	//

	PSYSTEM_PROCESS_INFORMATION processInfo;
	PVOID processInformaionBuffer;
	NTSTATUS status;
	ULONG size = 0;
	PETHREAD eThread = NULL;

	status = ZwQuerySystemInformation(SystemProcessAndThreadInformation, NULL, 0, &size);
	if (status == STATUS_INFO_LENGTH_MISMATCH)
	{
		processInformaionBuffer = ExAllocatePool(NonPagedPool, size);
		status = ZwQuerySystemInformation(SystemProcessAndThreadInformation, processInformaionBuffer, size, NULL);
		if (processInformaionBuffer && NT_SUCCESS(status))
		{
			processInfo = (PSYSTEM_PROCESS_INFORMATION)processInformaionBuffer;

			while (processInfo->NextEntryOffset)
			{
				if (processInfo->UniqueProcessId == ProcessId)
				{
					
					ULONG ThreadCount = processInfo->NumberOfThreads;
					for (ULONG i = 0; i < ThreadCount; i++) {
						status = PsLookupThreadByThreadId(processInfo->Threads[i].ClientId.UniqueThread, &eThread);
						if (NT_SUCCESS(status)) {

							ULONG ThreadMiscFlag = *(PULONG)((PCHAR)eThread + 0x74);
							BOOLEAN bAlertable = ThreadMiscFlag & 0x10;

							if (bAlertable)
							{
								InjDbgPrint("Alertable thread has been found!: 0x%p, id: %llx\n", eThread, (ULONG64)PsGetThreadId(eThread));
								pThread = eThread;
								break;
							}
						}
					}

					VidDisableProcessProtection(ProcessHandle);
					//KeAttachProcess(ProcessHandle);

					//PVOID LdrLoadDllRoutineAddress = RtlxFindExportedRoutineByName(pInjInfo->NtdllImageBase,
					//	&LdrLoadDllRoutineName);

					//InjectionInfo->LdrLoadDllRoutineAddress = LdrLoadDllRoutineAddress;

					//InjpQueueApc(KernelMode,
					//	&InjpInjectApcNormalRoutine,
					//	InjectionInfo,
					//	NULL,
					//	NULL);

					//KeDetachProcess();

					pThread = NULL;

					break;
					
				}
				processInfo = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)processInfo + processInfo->NextEntryOffset);
			}
		}
		ExFreePool(processInformaionBuffer);
	}



	//
	// Mark that this process is injected.
	//

	//InjectionInfo->IsInjected = TRUE;

	return TRUE;
}