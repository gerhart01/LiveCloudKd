// leechcore_device_hvmm_internal.c : implementation for Hyper-V memory access using Hyper-V memory access library
// Please refer to the hvmm/ folder for more information or its original repository:
// https://github.com/gerhart01/LiveCloudKd
//
// (c) Ulf Frisk, 2018-2025
// Author: Ulf Frisk, pcileech@frizk.net
//
// (c) Arthur Khudyaev, 2018-2025
// Author: Arthur Khudyaev, @gerhart_x
//
// (c) Matt Suiche, 2018-2025
// Author: Matt Suiche, www.msuiche.com
//

#include "leechcore_device_hvmm.h"

READ_MEMORY_METHOD g_MemoryReadInterfaceType = ReadInterfaceHvmmDrvInternal;
WRITE_MEMORY_METHOD g_MemoryWriteInterfaceType = WriteInterfaceHvmmDrvInternal;
ULONG64 g_Partition = 0;

BOOL HVMMStart(_Inout_ PLC_CONTEXT ctxLC)
{
	PULONG64 Partitions;
	ULONG64 PartitionCount = 0;
	VM_OPERATIONS_CONFIG VmOperationsConfig = { 0 };
	WCHAR* FriendlyNameP = NULL;
	ULONG64 VmNameListLen = 0;
	LPWSTR szVmList = NULL;

	ULONG i;
	ULONG VmId;

	PDEVICE_CONTEXT_HVMM ctx = (PDEVICE_CONTEXT_HVMM)ctxLC->hDevice;

	wprintf(L"\n"
		L"   Microsoft Hyper-V Virtual Machine plugin 1.5.20241221 for MemProcFS (by Ulf Frisk).\n"
		L"    \n"
		L"   plugin parameters:\n"
		L"      hvmm://id=<VM id number>\n"
		L"      hvmm://id=<VM id number>,unix\n"
		L"      hvmm://listvm\n"
		L"      hvmm://enumguestosbuild\n"
		L"      hvmm://loglevel=<log level number>\n"
		L"      hvmm://m=<reading memory type>\n"
		L"          m=0, Winhvr.sys interface for Hyper-V\n"
		L"          m=1, Raw memory interface for Hyper-V\n"
		L"          m=2, Local OS\n"
		L"   Example: MemProcFS.exe -device hvmm://listvm\n"
		L"   Example: MemProcFS.exe -device hvmm://listvm,enumguestosbuild,loglevel=2,m=1\n"
		L"\n");

	//_getch();

	SdkGetDefaultConfig(&VmOperationsConfig);

	g_MemoryReadInterfaceType = VmOperationsConfig.ReadMethod;
	g_MemoryWriteInterfaceType = VmOperationsConfig.WriteMethod;

	if (ctx->MemoryType != ReadInterfaceUnsupported) {
		g_MemoryReadInterfaceType = ctx->MemoryType;

		switch (ctx->MemoryType)
		{
			case ReadInterfaceWinHv:
				g_MemoryWriteInterfaceType = WriteInterfaceWinHv;
				break;
			case ReadInterfaceHvmmDrvInternal:
				g_MemoryWriteInterfaceType = WriteInterfaceHvmmDrvInternal;
				break;
			case ReadInterfaceHvmmLocal:
				g_MemoryWriteInterfaceType = WriteInterfaceHvmmLocal;
				break;
			default:
				break;
		}

		VmOperationsConfig.ReadMethod = g_MemoryReadInterfaceType;
		VmOperationsConfig.WriteMethod = g_MemoryWriteInterfaceType;
	}

	if (ctx->SimpleMemory)
		VmOperationsConfig.SimpleMemory = TRUE;

	if (ctx->EnumGuestOsBuild)
		VmOperationsConfig.EnumGuestOsBuild = TRUE;

	if (ctx->bIsLogLevelSet)
		VmOperationsConfig.LogLevel = ctx->LogLevel;

	VmOperationsConfig.ReloadDriver = FALSE;

	Partitions = SdkEnumPartitions(&PartitionCount, &VmOperationsConfig); // driver already was loaded by leechcore

	if (!Partitions)
	{
		wprintf(L"   Unable to get list of partitions\n");
		return FALSE;
	}

	wprintf(L"   Virtual Machines:\n");

	if (PartitionCount == 0)
	{
		wprintf(L"   ERROR:    --> No virtual machines running.\n");
		return FALSE;
	}

	if (ctx->RemoteMode && ctx->ListVm)
	{
		LPWSTR wszUserText = L"Please, select ID of the virtual machine\n";

		VmNameListLen = PartitionCount * 0x200 + PartitionCount * 4 + 0x200 + sizeof(wszUserText); // vm name + \n	 
		ctx->szVmNamesList = malloc(VmNameListLen);
		szVmList = ctx->szVmNamesList;

		if (!szVmList)
		{
			wprintf(L"   ERROR:    --> Memory allocation for VM names are failed.\n");
			return FALSE;
		}

		RtlZeroMemory(szVmList, VmNameListLen);

		lstrcatW(szVmList, wszUserText);
	}

	//_getch();

	if (g_MemoryReadInterfaceType == ReadInterfaceHvmmLocal) 
	{
		g_Partition = Partitions[0];

		if (!SdkSelectPartition(g_Partition))
			return FALSE;

		char* NtBuildLab = NULL;
		SdkGetData(g_Partition, InfoBuilLabBuffer, &NtBuildLab);

		WCHAR unistring[0x50] = { 0 };
		if (AsciiToUnicode(NtBuildLab, unistring, 0x50))
			wprintf(L"    --> [id = 0] Local computer, OS build = %s)\n", unistring);
	}
	else 
	{
		for (i = 0; i < PartitionCount; i += 1)
		{
			ULONG64 PartitionId = 0;
			WCHAR* VmTypeString = NULL;
			char* NtBuildLab = NULL;

			SdkGetData(Partitions[i], InfoPartitionFriendlyName, &FriendlyNameP);
			SdkGetData(Partitions[i], InfoPartitionId, &PartitionId);
			SdkGetData(Partitions[i], InfoVmtypeString, &VmTypeString);

			if (ctx->EnumGuestOsBuild) {
				SdkSelectPartition(Partitions[i]);
				SdkGetData(Partitions[i], InfoBuilLabBuffer, &NtBuildLab);
			}

			if (PartitionId != 0)
			{
				if (ctx->EnumGuestOsBuild)
				{
					WCHAR unistring[0x50] = { 0 };
					if (AsciiToUnicode(NtBuildLab, unistring, 0x50))
						wprintf(L"    --> [id = %d] %s (PartitionId = 0x%I64X, %s, OS build = %s)\n", i, FriendlyNameP, PartitionId, VmTypeString, unistring);
				}
				else
				{
					wprintf(L"    --> [id = %d] %s (PartitionId = 0x%I64X, %s)\n", i, FriendlyNameP, PartitionId, VmTypeString);
				}

				if (ctx->RemoteMode && ctx->ListVm)
				{
					WCHAR AscciVmId[0x10] = { 0 };
					lstrcatW(szVmList, L"[id = ");
					wnsprintfW(AscciVmId, 0x10, L"%d", (int)i);
					lstrcatW(szVmList, AscciVmId);
					lstrcatW(szVmList, L"] ");
					lstrcatW(szVmList, FriendlyNameP);
					lstrcatW(szVmList, L"\n");
				}
			}
			else 
			{
				wprintf(L"    --> [id = %d] PartitionId is 0. Probably, it is container or not successfully deleted partition\n", i);
			}
		}

		if (ctx->ListVm)
		{
			wprintf(L"   ListVM command was executed\n");
			return TRUE;
		}

		VmId = 0;

		if (ctx->VmidPreselected == TRUE)
		{
			VmId = ctx->Vmid;
		}
		else
		{
			if (PartitionCount <= 9)
			{
				while ((VmId < '0') || (VmId > '9'))
				{
					wprintf(L"\n"
						L"   Please, select the ID of the virtual machine you want to play with\n"
						L"   > ");
					VmId = _getch();
				}
				VmId = VmId - 0x30;
			}
			else
			{
				wprintf(L"\n"
					L"   Please, select the ID of the virtual machine you want to play with and press Enter\n"
					L"   > ");

				CHAR cVmId[0x10] = { 0 };
				int a = scanf_s("%s", cVmId, 0x10);

				if (!IsDigital(ctxLC, cVmId, strlen(cVmId)))
					return FALSE;

				VmId = atoi(cVmId);
			}
			Green(L"   %d\n", VmId);
		}

		if (((ULONG64)VmId + 1) > PartitionCount)
		{
			wprintf(L"ERROR: The virtual machine you selected do not exist. Vmid = %d\n", VmId);
			return FALSE;
		}

		wprintf(L"   You selected the following virtual machine: ");

		SdkGetData(Partitions[VmId], InfoPartitionFriendlyName, &FriendlyNameP);
		Green(L"%s\n", FriendlyNameP);

		g_Partition = Partitions[VmId];

		if (!SdkSelectPartition(g_Partition))
		{
			wprintf(L"ERROR:    Cannot initialize hvdd structure.\n");
			return FALSE;
		};
	}

	ctx->Partition = g_Partition;

	SdkGetData(g_Partition, InfoMmMaximumPhysicalPage, &ctx->paMax);
	ctx->paMax *= PAGE_SIZE;

	ULONG64 NumberOfCPU = 0;

	SdkGetData(g_Partition, InfoNumberOfCPU, &NumberOfCPU);
	SdkGetData(g_Partition, InfoDirectoryTableBase, &ctx->MemoryInfo.CR3.QuadPart);

	GUEST_TYPE GuestOsType = SdkGetData2(g_Partition, InfoGuestOsType);

	if (GuestOsType == MmStandard) 
	{
		//
		// Get KPCR for every processor
		//

		PULONG64 KPCR = NULL;

		SdkGetData(g_Partition, InfoKPCR, &KPCR);

		for (size_t i = 0; i < NumberOfCPU; i++)
		{
			ctx->MemoryInfo.KPCR[i].QuadPart = KPCR[i];
		}

		SdkGetData(g_Partition, InfoKDBGPa, &ctx->MemoryInfo.KDBG.QuadPart);
		SdkGetData(g_Partition, InfoNumberOfRuns, &ctx->MemoryInfo.NumberOfRuns.QuadPart);
		SdkGetData(g_Partition, InfoKernelBase, &ctx->MemoryInfo.KernBase.QuadPart);

		ULONG64 MmPfnDatabase = 0;
		ULONG64 PsLoadedModuleList = 0;
		ULONG64 PsActiveProcessHead = 0;
		SdkGetData(g_Partition, InfoMmPfnDatabase, &MmPfnDatabase);
		SdkGetData(g_Partition, InfoPsLoadedModuleList, &PsLoadedModuleList);
		SdkGetData(g_Partition, InfoPsActiveProcessHead, &PsActiveProcessHead);

		ctx->MemoryInfo.PfnDataBase.QuadPart = SdkGetPhysicalAddress(g_Partition, MmPfnDatabase, MmVirtualMemory);
		ctx->MemoryInfo.PsLoadedModuleList.QuadPart = SdkGetPhysicalAddress(g_Partition, PsLoadedModuleList, MmVirtualMemory);
		ctx->MemoryInfo.PsActiveProcessHead.QuadPart = SdkGetPhysicalAddress(g_Partition, PsActiveProcessHead, MmVirtualMemory);

		SdkGetData(g_Partition, InfoNtBuildNumber, &ctx->MemoryInfo.NtBuildNumber.LowPart);
		SdkGetData(g_Partition, InfoNtBuildNumberVA, &ctx->MemoryInfo.NtBuildNumberAddr.QuadPart);

		PULONG64 pRun = NULL;
		SdkGetData(g_Partition, InfoRun, &pRun);

		RtlCopyMemory(&ctx->MemoryInfo.Run, pRun, ctx->MemoryInfo.NumberOfRuns.QuadPart * sizeof(ULONG64) * 3 + sizeof(ULONG64) * 3);

		for (i = 0; i < ctx->MemoryInfo.NumberOfRuns.QuadPart; i++)
		{
			ctx->MemoryInfo.Run[i].start = ctx->MemoryInfo.Run[i].start * PAGE_SIZE;
			ctx->MemoryInfo.Run[i].length = ctx->MemoryInfo.Run[i].length * PAGE_SIZE;
		}
	}
	else 
	{
		ctx->MemoryInfo.Run[0].start = 0;
		ctx->MemoryInfo.Run[0].length = ctx->paMax;
		ctx->MemoryInfo.NumberOfRuns.QuadPart = 1;
	}

	return TRUE;
}


BOOLEAN HVMM_ReadFile(ULONG64 PartitionHandle, UINT64 StartPosition, PVOID lpBuffer, UINT64 nNumberOfBytesToRead)
{
	return SdkReadPhysicalMemory(PartitionHandle, StartPosition, (ULONG) nNumberOfBytesToRead, lpBuffer, g_MemoryReadInterfaceType);
}

BOOLEAN HVMM_WriteFile(ULONG64 PartitionHandle, UINT64 StartPosition, PVOID lpBuffer, UINT64 nNumberOfBytesToWrite)
{
	return SdkWritePhysicalMemory(PartitionHandle, StartPosition, nNumberOfBytesToWrite, lpBuffer, g_MemoryWriteInterfaceType);
}