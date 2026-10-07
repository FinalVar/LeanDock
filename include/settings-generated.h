#pragma once
struct NativeSetting { int component; const wchar_t* key; const wchar_t* value; };
static const NativeSetting NativeDefaults[] = {
    {1, L"clickThroughTaskbar", L"0"},
    {1, L"xamlDiagnosticsHandling", L"block"},
    {2, L"debugLayout", L"0"},
    {2, L"gridArrangement", L"columnFirstTopToBottom"},
    {2, L"mergeSystemIcons", L"1"},
    {2, L"notificationIconRows", L"2"},
    {2, L"notificationIconWidth", L"24"},
    {2, L"overflowIconWidth", L"40"},
    {2, L"overflowIconsPerRow", L"5"},
    {3, L"IconSize", L"32"},
    {3, L"IconSizeSmall", L"16"},
    {3, L"TaskbarButtonWidth", L"44"},
    {3, L"TaskbarButtonWidthSmall", L"32"},
    {3, L"TaskbarHeight", L"56"},
    {4, L"excludedPrograms[0]", L""},
    {4, L"foregroundWindowOnly", L"0"},
    {4, L"mode", L"intersected"},
    {4, L"oldTaskbarOnWin11", L"0"},
    {4, L"primaryMonitorOnly", L"0"},
    {5, L"mode", L"showAll"},
};
