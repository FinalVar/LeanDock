// Ctrl+Alt+Tab visual adaptation. Uses the existing upstream style resolver,
// property ownership/restoration and XAML diagnostics lifetime; no new hooks.
// The selectors and RowHeight values were checked against the pinned system
// XBF resources. See docs/switcher-style-latest.md and NOTICE.md.
#include "verify-latest.h"
#include <cmath>

namespace LeanDockSwitcher {
constexpr PCWSTR kNamespace = L"WindowsInternal.ComposableShell.Experiences.Switcher.";
static std::atomic<unsigned> g_rulesThreads{}, g_itemsSeen{}, g_focusReadbacks{},
    g_panelsSeen{}, g_maxHeightReadbacks{}, g_canvasReadbacks{}, g_readErrors{};
static std::atomic<double> g_primary{-1}, g_secondary{-1}, g_maxPixels{-1}, g_maxRatio{-1};
static std::atomic<double> g_canvasAlpha{-1}, g_canvasBorder{-1};

bool Supported() noexcept {
    // A Windows update disables only this optional styling, never the native
    // switcher action or the already validated taskbar components.
    static const bool supported = []() noexcept {
        wchar_t windows[MAX_PATH]{}, hash[65]{};
        if (!GetWindowsDirectoryW(windows, MAX_PATH)) return false;
        try {
            auto base = std::wstring(windows);
            return NativeHashFile((base + L"\\ShellComponents\\WindowsInternal.ComposableShell.Experiences.Switcher.dll").c_str(), hash) &&
                !_wcsicmp(hash, L"A9A33AE9735F191895B39A2C62E2510993C3CA6E66D6723A5E16C770C70265F8") &&
                NativeHashFile((base + L"\\SystemResources\\ShellComponents.Switcher\\ShellComponents.Switcher.pri").c_str(), hash) &&
                !_wcsicmp(hash, L"928753406E1636E897847D07AC5203ACF12C12C6BBC1B17E9BB56BBC037E2141");
        } catch (...) { return false; }
    }();
    return supported;
}

void Publish() noexcept {
    wchar_t json[1152]{};
    _snwprintf_s(json, _TRUNCATE,
        L"{\"schema\":6,\"canvasPolicy\":\"native-material-unmodified\",\"supported\":%ls,\"ruleThreads\":%u,"
        L"\"itemsSeen\":%u,\"focusReadbacks\":%u,\"primaryThickness\":%.3f,\"secondaryThickness\":%.3f,"
        L"\"panelsSeen\":%u,\"layoutPolicy\":\"content-first-adaptive-row-height\",\"maxHeightReadbacks\":%u,"
        L"\"maxPixels\":%.3f,\"maxRatio\":%.3f,\"canvasReadbacks\":%u,\"canvasAlpha\":%.3f,\"canvasBorder\":%.3f,\"readErrors\":%u}",
        Supported() ? L"true" : L"false", g_rulesThreads.load(),
        g_itemsSeen.load(), g_focusReadbacks.load(), g_primary.load(), g_secondary.load(),
        g_panelsSeen.load(), g_maxHeightReadbacks.load(),
        g_maxPixels.load(), g_maxRatio.load(),g_canvasReadbacks.load(),g_canvasAlpha.load(),g_canvasBorder.load(),g_readErrors.load());
    // Reuses the controller's existing status request; no timer or file writer.
    Wh_SetStringValue(L"switcherStyleState", json);
}

bool InSwitcher(FrameworkElement element) {
    for (unsigned depth = 0; element && depth != 32; ++depth) {
        if (winrt::get_class_name(element) ==
            L"WindowsInternal.ComposableShell.Experiences.Switcher.AltTab") return true;
        element = Media::VisualTreeHelper::GetParent(element).try_as<FrameworkElement>();
    }
    return false;
}

double Number(winrt::Windows::Foundation::IInspectable const& value) {
    auto number = value.as<winrt::Windows::Foundation::IPropertyValue>();
    using Type = winrt::Windows::Foundation::PropertyType;
    switch (number.Type()) {
        case Type::Single: return number.GetSingle();
        case Type::Double: return number.GetDouble();
        case Type::Int32: return number.GetInt32();
        case Type::UInt32: return number.GetUInt32();
        default: return -1;
    }
}

void Record(FrameworkElement const& element) noexcept {
    try {
        auto name = winrt::get_class_name(element);
        bool item = name == L"WindowsInternal.ComposableShell.Experiences.Switcher.SwitchItemListViewItem";
        bool panel = name == L"WindowsInternal.ComposableShell.Experiences.Switcher.DynamicFlowPanel";
        bool canvas = name == L"Windows.UI.Xaml.Controls.Border" && element.Name()==L"BackgroundElement";
        if ((!item && !panel && !canvas) || !InSwitcher(element)) return;
        if(canvas) {
            auto border=element.as<Controls::Border>();
            auto brush=border.Background().try_as<Media::SolidColorBrush>();
            g_canvasBorder=border.BorderThickness().Left;
            g_canvasAlpha=brush ? double(brush.Color().A)*brush.Opacity()/255.0 : -1;
            // Non-solid native materials have no single alpha value; -1 is
            // unknown, not transparent. Count observations, not success.
            ++g_canvasReadbacks;
        } else if (item) {
            ++g_itemsSeen;
            auto control = element.as<Controls::Control>();
            auto primary = control.FocusVisualPrimaryThickness();
            auto secondary = control.FocusVisualSecondaryThickness();
            g_primary = primary.Left;
            g_secondary = secondary.Left;
            if (!control.UseSystemFocusVisuals() && primary.Left == 0 && primary.Top == 0 && primary.Right == 0 && primary.Bottom == 0 &&
                secondary.Left == 0 && secondary.Top == 0 && secondary.Right == 0 && secondary.Bottom == 0)
                ++g_focusReadbacks;
        } else {
            ++g_panelsSeen;
            // Resolve public dependency-property objects through the same XAML
            // metadata path as upstream, not hard-coded COM/vtable offsets.
            // Do not parse or write the private layout enum just to verify it.
            // The pinned native template already owns MinRow/FitToContent.
            // Diagnostics only inspect the height that we actually customize.
            auto probe = GetStyleFromXamlSetters(name,
                L"<Setter Property=\"MaxRowHeight\" Value=\"{x:Null}\" />");
            auto height = element.GetValue(probe.Setters().GetAt(0).as<Setter>().Property());
            // Optional detailed readback. Some host Application objects do not
            // expose a metadata provider; leave values unknown in that case.
            auto app = Application::Current();
            auto provider = app ? app.try_as<Markup::IXamlMetadataProvider>() : nullptr;
            if (height && provider) {
                auto type = provider.GetXamlType(winrt::get_class_name(height));
                if (type) {
                    auto pixels = type.GetMember(L"Pixels"), ratio = type.GetMember(L"Ratio");
                    if (pixels && ratio) {
                        auto p = Number(pixels.GetValue(height)), r = Number(ratio.GetValue(height));
                        g_maxPixels = p; g_maxRatio = r;
                        if (std::isfinite(p) && p >= 48 && r == 0) ++g_maxHeightReadbacks;
                    }
                }
            }
        }
        Publish();
    } catch (...) { ++g_readErrors; Publish(); }
}

void AddRules() {
    HIGHCONTRASTW contrast{sizeof(contrast)};
    if (!Supported() || !SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) ||
        (contrast.dwFlags & HCF_HIGHCONTRASTON)) { Publish(); return; }
    // ModalRootGrid occurs only in AltTab.xaml in the pinned resource set.
    // Its center cell is the available-area ceiling, not a fixed background.
    // The native centered Border and FitToContent shrink around the content.
    // Upstream clones definition collections per owner and restores originals.
    AddElementCustomizationRules(
        L"WindowsInternal.ComposableShell.Experiences.Switcher.AltTab > * > Grid#ModalRootGrid",
        {L"ColumnDefinitions:=<ColumnDefinitionCollection><ColumnDefinition Width=\"4*\"/><ColumnDefinition Width=\"92*\"/><ColumnDefinition Width=\"4*\"/></ColumnDefinitionCollection>",
         L"RowDefinitions:=<RowDefinitionCollection><RowDefinition Height=\"8*\"/><RowDefinition Height=\"84*\"/><RowDefinition Height=\"8*\"/></RowDefinitionCollection>"});
    // Keep the native canvas material and border. Only selection chrome is
    // hidden; canvas bounds still follow the accepted content-first layout.
    // The pinned SimpleVerticalScrollViewer template adds 44 DIP on all
    // sides, besides each item's native 12 DIP margin. Keep an 8 DIP inset
    // without changing item spacing, scroll behavior or the canvas material.
    AddElementCustomizationRules(
        L"WindowsInternal.ComposableShell.Experiences.Switcher.AltTab > * > Grid#ModalRootGrid > Border#BackgroundElement > WindowsInternal.ComposableShell.Experiences.Switcher.SwitchItemList#SwitchItemListControl > Border > ScrollViewer#ScrollViewer",
        {L"Padding=8"});
    AddElementCustomizationRules(
        L"WindowsInternal.ComposableShell.Experiences.Switcher.AltTab > * > WindowsInternal.ComposableShell.Experiences.Switcher.SwitchItemList#SwitchItemListControl > Border > ScrollViewer#ScrollViewer",
        {L"VerticalContentAlignment=Center", L"HorizontalContentAlignment=Center"});
    // HostControlName scopes these shared control classes to the Ctrl+Alt+Tab
    // surface; Task View and Snap Assist retain their own styles.
    AddElementCustomizationRules(
        L"WindowsInternal.ComposableShell.Experiences.Switcher.DynamicFlowPanel[HostControlName=AltTab] > WindowsInternal.ComposableShell.Experiences.Switcher.SwitchItemListViewItem",
        {L"UseSystemFocusVisuals=False", L"FocusVisualPrimaryThickness=0", L"FocusVisualSecondaryThickness=0", L"FocusVisualMargin=0", L"MinHeight=0", L"MinWidth=0",
         L"FocusVisualPrimaryBrush=Transparent", L"FocusVisualSecondaryBrush=Transparent"});
    // PanelLayoutMode and FitToContent remain native. Typed height objects are
    // scoped to each root by AdaptLayout and restored only while still owned.
    AddElementCustomizationRules(
        L"WindowsInternal.ComposableShell.Experiences.Switcher.DynamicFlowPanel[HostControlName=AltTab] > WindowsInternal.ComposableShell.Experiences.Switcher.SwitchItemListViewItem > Grid#Root > Border#BackgroundBorder",
        {L"BackgroundTransition:=<BrushTransition Duration=\"0:0:0.14\" />"});
    ++g_rulesThreads;
    Publish();
}
} // namespace LeanDockSwitcher
