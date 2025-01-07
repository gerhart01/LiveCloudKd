// leechcore_device_hvmm_misc.c : implementation for Hyper-V memory access using Hyper-V memory access library
// Please, refer to the hvmm/ folder for more information or its original repository:
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
                bResult = TRUE;
            }
            else
            {
                lcprintf(ctxLC,
                    "DEVICE_HVMM: ERROR: param length is too big: %d\n",
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
                bResult = TRUE;
            }
            else
            {
                lcprintf(ctxLC,
                    "DEVICE_HVMM: ERROR: param length is too big: %d\n",
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

USHORT
GetConsoleTextAttribute(
    _In_ HANDLE hConsole
)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    GetConsoleScreenBufferInfo(hConsole, &csbi);
    return(csbi.wAttributes);
}

VOID
Green(LPCWSTR Format, ...)
{
    HANDLE Handle;
    USHORT Color;
    va_list va;

    Handle = GetStdHandle(STD_OUTPUT_HANDLE);

    Color = GetConsoleTextAttribute(Handle);

    SetConsoleTextAttribute(Handle, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    va_start(va, Format);
    vwprintf(Format, va);
    va_end(va);

    SetConsoleTextAttribute(Handle, Color);
}

BOOLEAN AsciiToUnicode(PCHAR Asciistring, PWCHAR Unistring, ULONG unistring_size)
{
    if (!Asciistring | !Unistring)
    {
        wprintf(L"hvlib:string param are NULL \n");
        return FALSE;
    }

    ULONG64 len_a = strlen(Asciistring);

    if (len_a > unistring_size)
        len_a = unistring_size;

    for (ULONG i = 0; i < len_a; i++)
        Unistring[i] = Asciistring[i];

    return TRUE;
}