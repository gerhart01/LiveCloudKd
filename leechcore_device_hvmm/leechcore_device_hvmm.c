// leechcore_device_hvmm.c : implementation for MemProcFS Hyper-V memory access using Hyper-V memory access library (hvlib.dll)
// Please refer to the hvmm and LiveCloudKdSdk folder for more information or its original repository:
// https://github.com/gerhart01/LiveCloudKd
//
// (c) Ulf Frisk, 2018-2024
// Author: Ulf Frisk, pcileech@frizk.net
//
// (c) Arthur Khudyaev, 2018-2024
// Author: Arthur Khudyaev, @gerhart_x
//
// (c) Matt Suiche, 2018-2024
// Author: Matt Suiche, www.msuiche.com
//


#include "HvlibHandle.h"
#include "leechcore_device.h"
#include "util.h"
#include "leechcore_device_hvmm.h"
#include "shlwapi.h"
#include "psapi.h"

DWORD g_cDeviceHVMM = 0;


//-----------------------------------------------------------------------------
// GENERAL FUNCTIONALITY BELOW:
//-----------------------------------------------------------------------------

extern READ_MEMORY_METHOD g_MemoryReadInterfaceType;
extern WRITE_MEMORY_METHOD g_MemoryWriteInterfaceType;


VOID DeviceHVMM_WriteScatter(_In_ PLC_CONTEXT ctxLC, _In_ DWORD cpMEMs, _Inout_ PPMEM_SCATTER ppMEMs)
{
    PDEVICE_CONTEXT_HVMM ctx = (PDEVICE_CONTEXT_HVMM)ctxLC->hDevice;
    PMEM_SCATTER pMEM;
    DWORD i;

    for (i = 0; i < cpMEMs; i++) {
        pMEM = ppMEMs[i];

        if (pMEM->f || MEM_SCATTER_ADDR_ISINVALID(pMEM)) { continue; }

        pMEM->f = HVMM_WriteFile(ctx->Partition, pMEM->qwA, pMEM->pb, pMEM->cb);

        if (pMEM->f) {
            if (ctxLC->fPrintf[LC_PRINTF_VVV]) {
                lcprintf_fn(
                    ctxLC,
                    "WRITE:\n        offset=%016llx req_len=%08x\n",
                    pMEM->qwA,
                    pMEM->cb
                );
                Util_PrintHexAscii(ctxLC, pMEM->pb, pMEM->cb, 0);
            }
        }
        else {
            lcprintfvvv_fn(ctxLC, "WRITE FAILED:\n        offset=%016llx req_len=%08x\n", pMEM->qwA, pMEM->cb);
        }
    }

    return;
}

VOID DeviceHVMM_ReadScatter(_In_ PLC_CONTEXT ctxLC, _In_ DWORD cpMEMs, _Inout_ PPMEM_SCATTER ppMEMs)
{
    PDEVICE_CONTEXT_HVMM ctx = (PDEVICE_CONTEXT_HVMM)ctxLC->hDevice;
    DWORD i;
    PMEM_SCATTER pMEM;

    for (i = 0; i < cpMEMs; i++) {
        pMEM = ppMEMs[i];
        if (pMEM->f || MEM_SCATTER_ADDR_ISINVALID(pMEM)) { continue; }

        pMEM->f = HVMM_ReadFile(ctx->Partition, pMEM->qwA, pMEM->pb, pMEM->cb);

        if (pMEM->f) {
            if (ctxLC->fPrintf[LC_PRINTF_VVV]) {
                lcprintf_fn(
                    ctxLC,
                    "READ:\n        offset=%016llx req_len=%08x\n",
                    pMEM->qwA,
                    pMEM->cb
                );
                Util_PrintHexAscii(ctxLC, pMEM->pb, pMEM->cb, 0);
            }
        }
        else {
            lcprintfvvv_fn(ctxLC, "READ FAILED:\n        offset=%016llx req_len=%08x\n", pMEM->qwA, pMEM->cb);
        }
    }
}

_Success_(return)
BOOL DeviceHVMM_GetOption(_In_ PLC_CONTEXT ctxLC, _In_ QWORD fOption, _Out_ PQWORD pqwValue)
{
    PDEVICE_CONTEXT_HVMM ctx = (PDEVICE_CONTEXT_HVMM)ctxLC->hDevice;

    if (fOption == LC_OPT_MEMORYINFO_VALID) {
        *pqwValue = 1;
        return TRUE;
    }

    switch (fOption) {
    case LC_OPT_MEMORYINFO_FLAG_32BIT:
        *pqwValue = 0; // only 64-bit supported currently
        return TRUE;
    case LC_OPT_MEMORYINFO_FLAG_PAE:
        *pqwValue = 0;
        return TRUE;
    case LC_OPT_MEMORYINFO_OS_VERSION_MINOR:
        *pqwValue = ctx->MemoryInfo.NtBuildNumber.HighPart;
        return TRUE;
    case LC_OPT_MEMORYINFO_OS_VERSION_MAJOR:
        *pqwValue = ctx->MemoryInfo.NtBuildNumber.LowPart;
        return TRUE;
    case LC_OPT_MEMORYINFO_OS_PFN:
        *pqwValue = ctx->MemoryInfo.PfnDataBase.QuadPart;
        return TRUE;
    case LC_OPT_MEMORYINFO_OS_PsLoadedModuleList:
        *pqwValue = ctx->MemoryInfo.PsLoadedModuleList.QuadPart;
        return TRUE;
    case LC_OPT_MEMORYINFO_OS_PsActiveProcessHead:
        *pqwValue = ctx->MemoryInfo.PsActiveProcessHead.QuadPart;
        return TRUE;
    case LC_OPT_MEMORYINFO_OS_MACHINE_IMAGE_TP:
        *pqwValue = IMAGE_FILE_MACHINE_AMD64; // only 64-bit supported currently
        return TRUE;
    case LC_OPT_MEMORYINFO_OS_KERNELBASE:
        *pqwValue = ctx->MemoryInfo.KernBase.QuadPart;
        return TRUE;
        // FUTURE:
    case LC_OPT_MEMORYINFO_OS_DTB:
        *pqwValue = ctx->MemoryInfo.CR3.QuadPart;
        return TRUE;
    case LC_OPT_MEMORYINFO_OS_KERNELHINT:
        *pqwValue = 0ULL;
        return FALSE;
    }

    *pqwValue = 0;
    return FALSE;
}

/*
* Unload the HVMM kernel driver and also delete the driver-loading service.
*/

VOID DeviceHVMM_SvcClose()
{
    SC_HANDLE hSCM, hSvcHvmm;
    SERVICE_STATUS SvcStatus;

    // 1: shut down and delete service.

    if ((g_MemoryReadInterfaceType == ReadInterfaceHvmmDrvInternal) || (g_MemoryReadInterfaceType == ReadInterfaceWinHv))
    {
        if ((hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE))) {
            hSvcHvmm = OpenServiceA(hSCM, DEVICEHVMM_SERVICENAME, SERVICE_ALL_ACCESS);

            if (hSvcHvmm) {
                ControlService(hSvcHvmm, SERVICE_CONTROL_STOP, &SvcStatus);
            }

            if (hSvcHvmm) { DeleteService(hSvcHvmm); }
            if (hSvcHvmm) { CloseServiceHandle(hSvcHvmm); }

            CloseServiceHandle(hSCM);
        }
    }
}

BOOL DeviceHVMM_CheckParams(_In_ PLC_CONTEXT ctxLC)
{
    ULONG64 uVmidLength = 0;
    CHAR szVmid[10] = { 0 };
    PCHAR pVmid = NULL;
    PCHAR pLogLevel = NULL;
    PCHAR pDelim = NULL;
    BOOLEAN bResult = FALSE;

    //_getch();

    ULONG64 id_size = sizeof(HVMM_ID_PARAM_NAME) - 1;
    PDEVICE_CONTEXT_HVMM ctx = (PDEVICE_CONTEXT_HVMM)ctxLC->hDevice;

    pVmid = StrStrIA(ctxLC->Config.szDevice, HVMM_ID_PARAM_NAME);

    if (pVmid)
    {
        ctx->Vmid = GetNumberFromParam(ctxLC, pLogLevel, HVMM_ID_PARAM_NAME);

        if (ctx->Vmid != -1)
            ctx->VmidPreselected = TRUE;

        bResult = TRUE;
    }

    pLogLevel = StrStrIA(ctxLC->Config.szDevice, HVMM_LOGLEVEL_PARAM_NAME);

    if (pLogLevel)
    {
        ctx->LogLevel = GetNumberFromParam(ctxLC, pLogLevel, HVMM_LOGLEVEL_PARAM_NAME);
        bResult = TRUE;
    }

    if (StrStrIA(ctxLC->Config.szDevice, HVMM_UNIX_PARAM_NAME))
    {
        ctx->SimpleMemory = TRUE;
        bResult = TRUE;
    }

    if (StrStrIA(ctxLC->Config.szDevice, HVMM_LISTVM_PARAM_NAME))
    {
        ctx->ListVm = TRUE;
        bResult = TRUE;
    }

    if (StrStrIA(ctxLC->Config.szDevice, HVMM_ENUM_GUEST_OS_BUILD_PARAM_NAME))
    {
        ctx->EnumGuestOsBuild = TRUE;
        bResult = TRUE;
    }

    return bResult;
}

_Success_(return)
BOOL DeviceHVMM_SvcStart(_In_ PLC_CONTEXT ctxLC)
{
    PDEVICE_CONTEXT_HVMM ctx = (PDEVICE_CONTEXT_HVMM)ctxLC->hDevice;
    DWORD dwWinErr;
    CHAR szDriverFile[MAX_PATH] = { 0 };
    FILE* pDriverFile = NULL;
    HMODULE hModuleLeechCore;


    SC_HANDLE hSCM = 0, hSvcHvmm = 0;

    ctx->hFile = GetHvmmHandle(ctxLC);

    if (ctx->hFile)
        return TRUE;

    // 1: verify that driver file exists.
    if ((g_MemoryReadInterfaceType == ReadInterfaceHvmmDrvInternal) || (g_MemoryReadInterfaceType == ReadInterfaceWinHv))
    {
        hModuleLeechCore = LoadLibraryA("leechcore.dll");
        // NB! defaults to locating 'hvmm.sys' relative to the loaded
        // 'leechcore.dll' - if unable to locate library (for whatever reason)
        // defaults will be to try to loade relative to executable (NULL).
        Util_GetPathLib(szDriverFile);
        if (hModuleLeechCore) { FreeLibrary(hModuleLeechCore); }
        strcat_s(szDriverFile, _countof(szDriverFile), DEVICEHVMM_DRIVERFILE);

        if (fopen_s(&pDriverFile, szDriverFile, "rb") || !pDriverFile) {
            lcprintf(ctxLC,
                "DEVICE_HVMM: ERROR: unable to locate driver file '%s'.\n",
                szDriverFile);
            return FALSE;
        }

        fclose(pDriverFile);

        // 2: create and start service to load driver into kernel.
        if (!(hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_CREATE_SERVICE))) {
            lcprintf(ctxLC, "DEVICE_HVMM: ERROR: unable to load driver - not running as elevated administrator?\n");
            return FALSE;
        }

        hSvcHvmm = CreateServiceA(
            hSCM,
            DEVICEHVMM_SERVICENAME,
            DEVICEHVMM_SERVICENAME,
            SERVICE_ALL_ACCESS,
            SERVICE_KERNEL_DRIVER,
            SERVICE_DEMAND_START,
            SERVICE_ERROR_NORMAL,
            szDriverFile,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL);

        if (!hSvcHvmm) {
            if ((dwWinErr = GetLastError()) == ERROR_SERVICE_EXISTS) {
                hSvcHvmm = OpenServiceA(hSCM, DEVICEHVMM_SERVICENAME, SERVICE_ALL_ACCESS);
            }
            else {
                lcprintf(ctxLC,
                    "DEVICE HVMM: ERROR: Unable create service required to load driver.\n");
                lcprintfv(ctxLC, "DEVICE HVMM: ERROR: LastError: 0x%08x\n", dwWinErr);
                CloseServiceHandle(hSCM);
                return FALSE;
            }
        }

        if (!StartServiceA(hSvcHvmm, 0, NULL) && ((dwWinErr = GetLastError()) != ERROR_SERVICE_ALREADY_RUNNING)) {
            lcprintf(ctxLC,
                "DEVICE_HVMM: ERROR: Unable to load driver into kernel.\n");
            lcprintfv(ctxLC, "DEVICE_HVMM: ERROR: LastError: 0x%08x\n", dwWinErr);
            CloseServiceHandle(hSvcHvmm);
            CloseServiceHandle(hSCM);
            DeviceHVMM_SvcClose();
            return FALSE;
        }

        CloseServiceHandle(hSvcHvmm);
        CloseServiceHandle(hSCM);

        // 3: open file handle

        ctx->hFile = CreateFileA(
            DEVICEHVMM_OBJECT,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

        if (!ctx->hFile) {
            DeviceHVMM_SvcClose();
            return FALSE;
        }
    }
    else
    {
        ctx->hFile = (HANDLE)1;
    }

    return TRUE;
}

/*
* Close the HVMM device and clean up both context and any kernel drivers.
*/
VOID DeviceHVMM_Close(_Inout_ PLC_CONTEXT ctxLC)
{
    PDEVICE_CONTEXT_HVMM ctx = (PDEVICE_CONTEXT_HVMM)ctxLC->hDevice;

    if (0 == --g_cDeviceHVMM) {
        DeviceHVMM_SvcClose();
    }
    
    if (ctx) {
        SdkClosePartition((ULONG64)ctx->Partition);

        if (ctx->hFile)
            CloseHandle(ctx->hFile);

        LocalFree(ctx);
    }

    DeviceHVMM_SvcClose();

    ctxLC->hDevice = 0;
}

_Success_(return)
BOOL DeviceHVMM_GetMemoryInformation(_Inout_ PLC_CONTEXT ctxLC)
{
    PDEVICE_CONTEXT_HVMM ctx = (PDEVICE_CONTEXT_HVMM)ctxLC->hDevice;
    DWORD i;

    // 1: retrieve information from hypervisor

    if (!HVMMStart(ctxLC))
        return FALSE;

    // 2: sanity checks

    if (ctx->MemoryInfo.NumberOfRuns.QuadPart > MAX_NUMBER_OF_RUNS) {

        lcprintf(ctxLC, "DEVICE_HVMM: ERROR: too few/many memory segments reported from winpmem driver. (%lli)\n", ctx->MemoryInfo.NumberOfRuns.QuadPart);
        return FALSE;
    }

    // 3: parse memory ranges

    for (i = 0; i < ctx->MemoryInfo.NumberOfRuns.QuadPart; i++) {
        if (!LcMemMap_AddRange(ctxLC, ctx->MemoryInfo.Run[i].start, ctx->MemoryInfo.Run[i].length, ctx->MemoryInfo.Run[i].start)) {
            lcprintf(ctxLC, "DEVICE_HVMM: FAIL: unable to add range to memory map. (%016llx %016llx %016llx)\n", ctx->MemoryInfo.Run[i].start, ctx->MemoryInfo.Run[i].length, ctx->MemoryInfo.Run[i].start);
            return FALSE;
        }
    }

    return TRUE;
}

_Success_(return)
BOOL DeviceHVMM_ListVM(_Inout_ PLC_CONTEXT ctxLC)
{
    if (!HVMMStart(ctxLC))
        return FALSE;

    return TRUE;
}

_Success_(return)
EXPORTED_FUNCTION BOOL LcPluginCreate(_Inout_ PLC_CONTEXT ctxLC, _Out_opt_ PPLC_CONFIG_ERRORINFO ppLcConfidErrorInfo)
{
    BOOL result;
    PDEVICE_CONTEXT_HVMM ctx;

    ctx = (PDEVICE_CONTEXT_HVMM)LocalAlloc(LMEM_ZEROINIT, sizeof(DEVICE_CONTEXT_HVMM));
    
    if (!ctx) 
        return FALSE;

    ctxLC->hDevice = (HANDLE)ctx;
    ctxLC->Config.fVolatile = TRUE;

    g_cDeviceHVMM++;

    ctxLC->pfnClose = DeviceHVMM_Close;
    ctxLC->pfnReadScatter = DeviceHVMM_ReadScatter;
    ctxLC->pfnWriteScatter = DeviceHVMM_WriteScatter;
    ctxLC->pfnGetOption = DeviceHVMM_GetOption;

    ctx->RemoteMode = IsRemoteMode();

    if (ctx->RemoteMode)
    {       
        BOOLEAN bParam = DeviceHVMM_CheckParams(ctxLC);

        if (!bParam || ctx->ListVm)
        {
            ctx->ListVm = TRUE;

            result = DeviceHVMM_SvcStart(ctxLC);
            if (!result) {
                lcprintf(ctxLC, "DEVICE_HVMM: FAILED: failed to initialize the driver.\n");
                goto fail;
            }

            LPWSTR szVmList = NULL;
            result = DeviceHVMM_ListVM(ctxLC);

            if (!result) {
                lcprintf(ctxLC, "DEVICE_HVMM: FAILED: failed list vm.\n");
                goto fail;
            }

            if (ctx->szVmNamesList)
                szVmList = ctx->szVmNamesList;

            DWORD cbStructErrorInfo = 0;
            LPWSTR szUserText = L"There is no launching VM\n";

            if (szVmList)
                cbStructErrorInfo = sizeof(LC_CONFIG_ERRORINFO) + (DWORD)(wcslen(szVmList) + 1) * sizeof(WCHAR);
            else
                cbStructErrorInfo = sizeof(LC_CONFIG_ERRORINFO) + (DWORD)(wcslen(szUserText) + 1) * sizeof(WCHAR);

            PLC_CONFIG_ERRORINFO pErrorInfo = LocalAlloc(LMEM_ZEROINIT, cbStructErrorInfo);

            if (pErrorInfo) {
                pErrorInfo->dwVersion = LC_CONFIG_ERRORINFO_VERSION;
                pErrorInfo->cbStruct = cbStructErrorInfo;

                if (szVmList)
                {
                    pErrorInfo->fUserInputRequest = TRUE;
                    wcsncpy_s(pErrorInfo->wszUserText, (pErrorInfo->cbStruct - sizeof(LC_CONFIG_ERRORINFO))/sizeof(WCHAR), szVmList, _TRUNCATE);
                }
                else
                {
                    pErrorInfo->fUserInputRequest = FALSE;
                    wcsncpy_s(pErrorInfo->wszUserText, (pErrorInfo->cbStruct - sizeof(LC_CONFIG_ERRORINFO))/sizeof(WCHAR), szUserText, _TRUNCATE);
                }

                if (ctx->ListVm && bParam)
                   pErrorInfo->fUserInputRequest = FALSE;

                pErrorInfo->cwszUserText = (DWORD)wcslen(pErrorInfo->wszUserText);

                if (ppLcConfidErrorInfo)
                    *ppLcConfidErrorInfo = pErrorInfo;

                if (szVmList)
                    free(szVmList);

                goto fail;
            }
        }  
    }
    else 
    {
        DeviceHVMM_CheckParams(ctxLC);

        result = DeviceHVMM_SvcStart(ctxLC);
        if (!result) {
            lcprintf(ctxLC, "DEVICE_HVMM: FAILED: Failed to initialize the driver.\n");
            goto fail;
        }
    }
   
    result = DeviceHVMM_GetMemoryInformation(ctxLC);
    if (!result) {
        lcprintf(ctxLC, "DEVICE_HVMM: FAILED: Unable to parse guest memory map.\n");
        goto fail;
    }

    lcprintf(ctxLC, "DEVICE_HVMM: Successfully loaded hvmm driver.\n");
    return TRUE;

fail:
    DeviceHVMM_Close(ctxLC);
    return FALSE;
}

