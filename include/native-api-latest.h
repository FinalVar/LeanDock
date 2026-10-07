#pragma once
#ifndef ND_COMPONENT
#define ND_COMPONENT 0
#endif
#define WH_MOD
#define WH_MOD_ID L"NativeTaskbarDock"
#define WH_MOD_VERSION L"0.1.0"
#include <windhawk_api.h>
#include <windhawk_utils.h>
#undef GetCurrentTime
extern "C" void Native_ReportFailure(const wchar_t* stage, HRESULT error);

