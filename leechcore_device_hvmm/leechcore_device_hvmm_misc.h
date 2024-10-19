#pragma once
#include "HvlibHandle.h"
#include "leechcore_device.h"
#include "util.h"
#include "leechcore_device_hvmm.h"
#include "shlwapi.h"
#include "psapi.h"

BOOL IsDigital(PLC_CONTEXT ctxLC, PCHAR str, ULONG64 len);
BOOL IsRemoteMode();
ULONG GetNumberFromParam(_In_ PLC_CONTEXT ctxLC, PCHAR pId, _In_ PCSTR pszSrch);
BOOLEAN GetHvmmPresent(_In_ PLC_CONTEXT ctxLC);
HANDLE GetHvmmHandle(_In_ PLC_CONTEXT ctxLC);