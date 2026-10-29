#pragma once
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
BOOL startupEnabled(void);
BOOL repairStartup(void);
BOOL setStartup(BOOL enable);
HRESULT startupLastError(void);
#ifdef LITELAUNCH_STARTUP_TESTING
/* Accepted only for isolated QA task and registry namespaces. */
BOOL startupTestConfigure(LPCWSTR taskName, LPCWSTR legacyKey);
#endif
#ifdef __cplusplus
}
#endif
