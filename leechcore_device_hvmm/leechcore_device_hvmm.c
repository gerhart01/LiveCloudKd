// leechcore_device_hvmm.c : implementation for MemProcFS Hyper-V memory access using Hyper-V memory access library (hvlib.dll)
// Please refer to the hvmm and LiveCloudKdSdk folder for more information or its original repository:
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


#include "HvlibHandle.h"
#include "leechcore_device.h"
#include "util.h"
#include "leechcore_device_hvmm.h"
#include "shlwapi.h"
#include "psapi.h"

DWORD g_cDeviceHVMM = 0;

/*
* The guest mapping of the device opened last, for the process that hosts this plugin: the
* bot reads whole pages straight from it (LcHvmmMappedPointer) instead of asking LeechCore for
* each one. One VM per process, so one mapping.
*/
PHVMM_MAPPED_MEMORY g_MappedForHost = NULL;


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

/*
* A read through the mapping failed. That happens when the VM went away and the
* pages are gone; drop the mapping so every later read takes the driver path,
* which reports the real state of the partition.
*/
static VOID DeviceHVMM_MappedLost(_In_ PLC_CONTEXT ctxLC)
{
    PDEVICE_CONTEXT_HVMM ctx = (PDEVICE_CONTEXT_HVMM)ctxLC->hDevice;

    if (!ctx->Mapped || HvmmMappedIsAlive(ctx->Mapped)) { return; }

    lcprintf(ctxLC, "DEVICE_HVMM: the guest mapping is gone (VM stopped?), reads go through the driver from now on.\n");
    if (g_MappedForHost == ctx->Mapped) { g_MappedForHost = NULL; }
    HvmmMappedClose(ctx->Mapped);
    ctx->Mapped = NULL;
}

/*
* Where guest physical address Gpa sits in this process, or NULL when it is not mapped; the
* bytes readable from there onward in *Available. The pointer reads live guest memory and
* stays valid while the device is open and the VM runs.
*/
EXPORTED_FUNCTION PVOID LcHvmmMappedPointer(_In_ UINT64 Gpa, _Out_opt_ PUINT64 Available)
{
    if (Available) { *Available = 0; }
    if (!g_MappedForHost) { return NULL; }
    return HvmmMappedGetPointer(g_MappedForHost, Gpa, Available);
}

VOID DeviceHVMM_ReadScatter(_In_ PLC_CONTEXT ctxLC, _In_ DWORD cpMEMs, _Inout_ PPMEM_SCATTER ppMEMs)
{
    PDEVICE_CONTEXT_HVMM ctx = (PDEVICE_CONTEXT_HVMM)ctxLC->hDevice;
    DWORD i;
    PMEM_SCATTER pMEM;

    for (i = 0; i < cpMEMs; i++) {
        pMEM = ppMEMs[i];
        if (pMEM->f || MEM_SCATTER_ADDR_ISINVALID(pMEM)) { continue; }

        if (ctx->Mapped) {
            pMEM->f = HvmmMappedRead(ctx->Mapped, pMEM->qwA, pMEM->pb, pMEM->cb);
            if (!pMEM->f) { DeviceHVMM_MappedLost(ctxLC); }
        }
        if (!pMEM->f) {
            pMEM->f = HVMM_ReadFile(ctx->Partition, pMEM->qwA, pMEM->pb, pMEM->cb);
        }

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
    PCHAR pMemoryType = NULL;
    PCHAR pDelim = NULL;
    BOOLEAN bResult = FALSE;

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
        ctx->bIsLogLevelSet = TRUE;
        bResult = TRUE;
    }

    pMemoryType = StrStrIA(ctxLC->Config.szDevice, HVMM_MEMORY_TYPE_PARAM_NAME);

    if (pMemoryType)
    {
        ULONG64 memType = GetNumberFromParam(ctxLC, pMemoryType, HVMM_MEMORY_TYPE_PARAM_NAME);

        switch (memType)
        {
            case 0: 
                ctx->MemoryType = ReadInterfaceWinHv;
                break;
            case 1:
                ctx->MemoryType = ReadInterfaceHvmmDrvInternal;
                break;
            case 2:
                ctx->MemoryType = ReadInterfaceHvmmLocal;
                break;
            default:
                break;
        }
        
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

    //
    // "mapped": serve reads from a persistent read-only mapping of guest memory
    // (IOCTL_MAP_GPA_RANGE in hvmm.sys) instead of one driver round trip per page.
    // Full VMs only; anything not mapped falls back to the driver. LC_HVMM_MAPPED=1
    // turns it on for programs whose device string cannot be changed.
    //

    if (StrStrIA(ctxLC->Config.szDevice, HVMM_MAPPED_PARAM_NAME))
    {
        ctx->MappedRequested = TRUE;
        bResult = TRUE;
    }
    else
    {
        CHAR szEnv[8] = { 0 };
        if (GetEnvironmentVariableA(HVMM_MAPPED_ENV_NAME, szEnv, sizeof(szEnv)) && szEnv[0] == '1')
        {
            ctx->MappedRequested = TRUE;
        }
    }

    //
    // "keepdriver": leave hvmm.sys loaded when this device closes. Several programs may
    // have the VM open at the same time, and stopping the service under them takes the
    // driver away from everyone. With this the driver is loaded on first use and stays
    // until the machine reboots. LC_HVMM_KEEP_DRIVER=1 turns it on for programs whose
    // device string cannot be changed.
    //

    if (StrStrIA(ctxLC->Config.szDevice, HVMM_KEEPDRIVER_PARAM_NAME))
    {
        ctx->KeepDriver = TRUE;
        bResult = TRUE;
    }
    else
    {
        CHAR szEnv[8] = { 0 };
        if (GetEnvironmentVariableA(HVMM_KEEPDRIVER_ENV_NAME, szEnv, sizeof(szEnv)) && szEnv[0] == '1')
        {
            ctx->KeepDriver = TRUE;
        }
    }

    return bResult;
}

/*
* The service manager stores the driver file with a "\??\" prefix on some systems and
* plain on others. Answer where the real path starts so two spellings of the same file
* compare equal.
*/
static LPCSTR DeviceHVMM_SvcPathBody(_In_ LPCSTR szPath)
{
    if ((szPath[0] == '\\') && (szPath[1] == '?') && (szPath[2] == '?') && (szPath[3] == '\\')) { return szPath + 4; }
    if ((szPath[0] == '\\') && (szPath[1] == '\\') && (szPath[2] == '?') && (szPath[3] == '\\')) { return szPath + 4; }
    return szPath;
}

/*
* An hvmm service is already there. A service keeps the driver file it was created with,
* so if it points at another hvmm.sys our build never loads and every read is answered by
* a stranger's driver. When that old service is stopped, throw it away and create it again
* with the file next to leechcore.dll. When it is running, say so and stop: taking the
* driver away from whoever is using it is not ours to do.
*/
static BOOL DeviceHVMM_SvcAdoptExisting(_In_ PLC_CONTEXT ctxLC, _In_ SC_HANDLE hSCM, _Inout_ SC_HANDLE *phSvcHvmm, _In_ LPSTR szDriverFile)
{
    BYTE pbConfig[8 * MAX_PATH] = { 0 };
    LPQUERY_SERVICE_CONFIGA pConfig = (LPQUERY_SERVICE_CONFIGA)pbConfig;
    SERVICE_STATUS_PROCESS SvcStatus = { 0 };
    DWORD cbNeeded = 0;
    LPCSTR szExisting;

    if (!QueryServiceConfigA(*phSvcHvmm, pConfig, sizeof(pbConfig), &cbNeeded) || !pConfig->lpBinaryPathName) {
        return TRUE;
    }

    szExisting = pConfig->lpBinaryPathName;

    if (!lstrcmpiA(DeviceHVMM_SvcPathBody(szExisting), DeviceHVMM_SvcPathBody(szDriverFile))) {
        return TRUE;
    }

    if (QueryServiceStatusEx(*phSvcHvmm, SC_STATUS_PROCESS_INFO, (LPBYTE)&SvcStatus, sizeof(SvcStatus), &cbNeeded) &&
        (SvcStatus.dwCurrentState != SERVICE_STOPPED)) {
        lcprintf(ctxLC,
            "DEVICE_HVMM: ERROR: another hvmm.sys is loaded from '%s'; stop it first (this one needs '%s').\n",
            szExisting,
            szDriverFile);
        return FALSE;
    }

    lcprintf(ctxLC,
        "DEVICE_HVMM: the stopped hvmm service points at '%s'; creating it again with '%s'.\n",
        szExisting,
        szDriverFile);

    if (!DeleteService(*phSvcHvmm)) {
        lcprintf(ctxLC, "DEVICE_HVMM: ERROR: unable to delete the old hvmm service. LastError: 0x%08x\n", GetLastError());
        return FALSE;
    }

    CloseServiceHandle(*phSvcHvmm);

    *phSvcHvmm = CreateServiceA(
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

    if (!*phSvcHvmm) {
        lcprintf(ctxLC, "DEVICE_HVMM: ERROR: unable to create the hvmm service with '%s'. LastError: 0x%08x\n", szDriverFile, GetLastError());
        return FALSE;
    }

    return TRUE;
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

                if (hSvcHvmm && !DeviceHVMM_SvcAdoptExisting(ctxLC, hSCM, &hSvcHvmm, szDriverFile)) {
                    if (hSvcHvmm) { CloseServiceHandle(hSvcHvmm); }
                    CloseServiceHandle(hSCM);
                    return FALSE;
                }
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
            if (!ctx->KeepDriver) { DeviceHVMM_SvcClose(); }
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
            if (!ctx->KeepDriver) { DeviceHVMM_SvcClose(); }
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
    BOOLEAN fKeepDriver = FALSE;

    if (g_cDeviceHVMM > 0)
        g_cDeviceHVMM -= 1;

    if (g_cDeviceHVMM)
        return;

    if (ctx)
    {
        fKeepDriver = ctx->KeepDriver;

        if (ctx->Mapped)
        {
            if (g_MappedForHost == ctx->Mapped) { g_MappedForHost = NULL; }
            HvmmMappedClose(ctx->Mapped);
            ctx->Mapped = NULL;
        }

        SdkClosePartition((ULONG64)ctx->Partition);

        if (ctx->hFile)
            CloseHandle(ctx->hFile);

        LocalFree(ctx);
    }

    //
    // "keepdriver": the driver is loaded on first use and stays until the machine
    // reboots. Other programs may have the VM open right now, and stopping the service
    // pulls hvmm.sys out from under them.
    //

    if (!fKeepDriver)
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
                lcprintf(ctxLC, "DEVICE_HVMM: FAILED: failed list VMs.\n");
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

