#include "leechcore_device_hvmm_misc.h"

BOOL IsDigital(PLC_CONTEXT ctxLC, PCHAR str, ULONG64 len)
{
    for (ULONG i = 0; i < len; i++)
    {
        if ((str[i] < '0') || (str[i] > '9'))
        {
            lcprintf(ctxLC,
                "DEVICE_HVMM: ERROR: vmid is not integer: %s\n",
                str);
            return FALSE;
        }
    }

    return TRUE;
}

BOOL IsRemoteMode()
{
    WCHAR wszImageFileName[MAX_PATH] = { 0 };
    BOOL fIsAgent = (GetProcessImageFileNameW((HANDLE)-1, wszImageFileName, _countof(wszImageFileName)) >= 14) && !_wcsicmp(wszImageFileName + wcslen(wszImageFileName) - 14, L"LeechAgent.exe");
    return fIsAgent;
}

ULONG GetNumberFromParam(_In_ PLC_CONTEXT ctxLC, PCHAR pId, _In_ PCSTR pszSrch)
{
    PCHAR pDelim = NULL;
    BOOLEAN bResult = FALSE;
    ULONG64 uParamIdLength = 0;
    CHAR szParamId[10] = { 0 };
    ULONG64 sizeOfParam = strlen(pszSrch);
    ULONG szResult = 0;

    PDEVICE_CONTEXT_HVMM ctx = (PDEVICE_CONTEXT_HVMM)ctxLC->hDevice;

    pId = StrStrIA(ctxLC->Config.szDevice, pszSrch);

    if (pId)
    {
        pDelim = StrStrIA(pId, HVMM_PARAM_DELIMITER);

        if (pDelim)
        {
            uParamIdLength = pDelim - pId - sizeOfParam;
            if (uParamIdLength < 6)
            {
                memcpy(szParamId, pId + sizeOfParam, uParamIdLength);

                if (!IsDigital(ctxLC, szParamId, uParamIdLength))
                    return -1;

                szResult = atoi(szParamId);
                ctx->VmidPreselected = TRUE;
                bResult = TRUE;
            }
            else
            {
                lcprintf(ctxLC,
                    "DEVICE_HVMM: ERROR: vmid length is too big: %d\n",
                    uParamIdLength);
                return -1;
            }
        }
        else
        {
            uParamIdLength = strlen(ctxLC->Config.szDevice) - (pId - ctxLC->Config.szDevice) - sizeOfParam;

            strcpy_s(szParamId, _countof(szParamId), pId + sizeOfParam);

            if (!IsDigital(ctxLC, szParamId, uParamIdLength))
                return FALSE;

            if (uParamIdLength < 6)
            {
                szResult = atoi(szParamId);
                ctx->VmidPreselected = TRUE;
                bResult = TRUE;
            }
            else
            {
                lcprintf(ctxLC,
                    "DEVICE_HVMM: ERROR: vmid length is too big: %d\n",
                    uParamIdLength);
                return -1;
            }
        }
    }
    return szResult;
}

/*
* Create HVMM driver loader service and load the kernel driver
* into the kernel. Upon fail it's guaranteed that no lingering service exists.
*/

HANDLE GetHvmmHandle(_In_ PLC_CONTEXT ctxLC)
{
    HANDLE hDevice = INVALID_HANDLE_VALUE;

    hDevice = CreateFileA(DEVICEHVMM_OBJECT,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ |
        FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        0);

    if (hDevice == INVALID_HANDLE_VALUE)
        return NULL;

    lcprintf(ctxLC, "DEVICE_HVMM: driver is already loaded\n");

    return hDevice;
}

BOOLEAN GetHvmmPresent(_In_ PLC_CONTEXT ctxLC)
{
    HANDLE hDevice = GetHvmmHandle(ctxLC);
    BOOLEAN bResult = FALSE;

    if (hDevice)
    {
        CloseHandle(hDevice);
        return TRUE;
    }

    return FALSE;
}