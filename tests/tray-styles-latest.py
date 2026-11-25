"""Verify unified tray icon styling rules and normalization coverage across all tray stacks and content types."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

EXPECTED_TARGETS = [
    "SystemTray.NotifyIconView > Grid#ContainerGrid@ > Border#BackgroundBorder",
    "SystemTray.ChevronIconView > Grid#ContainerGrid@ > Border#BackgroundBorder",
    "SystemTray.Stack > * > SystemTray.IconView > Grid#ContainerGrid@ > Border#BackgroundBorder",
    "SystemTray.OmniButton#ControlCenterButton > Grid@CommonStates > Border#BackgroundBorder",
]

STATES = [
    "Normal",
    "PointerOver",
    "Pressed",
    "Checked",
    "CheckedNormal",
    "CheckedPointerOver",
    "CheckedPressed",
]

REQUIRED_BASE_RULES = {
    "Width=24",
    "Height=24",
    "HorizontalAlignment=Center",
    "VerticalAlignment=Center",
    "Margin=0",
    "CornerRadius=5",
    "BorderThickness=0",
    "BorderBrush=Transparent",
    "Background=Transparent",
}

ALL_ICON_CONTENTS = [
    "SystemTray.ImageIconContent",
    "SystemTray.TextIconContent",
    "SystemTray.LanguageTextIconContent",
    "SystemTray.BatteryIconContent",
    "SystemTray.BadgeIconContent",
    "SystemTray.LanguageImageIconContent",
    "SystemTray.DateTimeIconContent",
]


def extract_contiguous_styles(mapping: dict, index: int) -> list[str]:
    values = []
    for sub in range(100):
        key = f"controlStyles[{index}].styles[{sub}]"
        if key not in mapping:
            break
        values.append(mapping[key])
    assert values, f"controlStyles[{index}] has no styles"
    remaining = [k for k in mapping if k.startswith(f"controlStyles[{index}].styles[")]
    assert len(remaining) == len(values), f"Non-contiguous styles in controlStyles[{index}]: {remaining}"
    return values


def function_body(source: str, signature_prefix: str) -> str:
    start = source.index(signature_prefix)
    brace = source.index("{", start)
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start : pos + 1]
    raise AssertionError(f"Unclosed function for {signature_prefix}")


def wide_literal(text: str) -> str:
    escaped = text.replace("\\", "\\\\").replace('"', '\\"')
    return f'L"{escaped}"'


def main() -> int:
    checks = 0

    styles = json.loads((ROOT / "baseline" / "styles-latest.json").read_text(encoding="utf-8"))
    settings = json.loads((ROOT / "baseline" / "settings-latest.json").read_text(encoding="utf-8"))
    styler_settings = next(
        item["Settings"] for item in settings if item["Id"] == "windows-11-taskbar-styler"
    )

    rules_5 = extract_contiguous_styles(styles, 5)
    assert "Padding=8,0,10,0" in rules_5, f"Unexpected SystemTrayFrameGrid padding: {rules_5}"
    assert extract_contiguous_styles(styler_settings, 5) == rules_5
    checks += 2

    target_15 = styles["controlStyles[15].target"]
    actual_targets = [part.strip() for part in target_15.split(",")]
    assert actual_targets == EXPECTED_TARGETS, f"Unexpected target list: {actual_targets}"
    checks += 1

    rules_15 = extract_contiguous_styles(styles, 15)
    assert REQUIRED_BASE_RULES.issubset(rules_15), f"Missing base rules: {REQUIRED_BASE_RULES - set(rules_15)}"
    checks += 1

    for state in STATES:
        assert f"BorderBrush@{state}=Transparent" in rules_15, f"Missing BorderBrush@{state}=Transparent"
        checks += 1
        assert any(r.startswith(f"Background@{state}") for r in rules_15), f"Missing Background@{state}"
        checks += 1

    assert styles["controlStyles[16].target"] == (
        "SystemTray.OmniButton#ControlCenterButton > * > SystemTray.IconView > "
        "Grid#ContainerGrid > Border#BackgroundBorder"
    )
    assert extract_contiguous_styles(styles, 16) == ["Opacity=0"]
    checks += 1

    assert styler_settings["controlStyles[15].target"] == target_15
    assert extract_contiguous_styles(styler_settings, 15) == rules_15
    checks += 1

    styler_cpp = (ROOT / "src" / "styler-latest.cpp").read_text(encoding="utf-8")
    expected_call = (
        "AddElementCustomizationRules("
        + wide_literal(target_15)
        + ", {"
        + ", ".join(wide_literal(v) for v in rules_15)
        + "});"
    )
    assert expected_call in styler_cpp, "Generated src/styler-latest.cpp missing updated controlStyles[15] call"
    checks += 1

    tray_cpp = (ROOT / "src" / "tray-latest.cpp").read_text(encoding="utf-8")
    apply_style_fn = function_body(tray_cpp, "void ApplyNotifyIconViewStyle(")
    for content_type in ALL_ICON_CONTENTS[:-1]:  # DateTimeIconContent lives outside stack/notify icon views unless normalized
        assert content_type in apply_style_fn, f"ApplyNotifyIconViewStyle missing {content_type}"
        checks += 1

    normalize_fn = function_body(tray_cpp, "void DockNormalizeIcon(")
    for content_type in ALL_ICON_CONTENTS:
        assert content_type in normalize_fn, f"DockNormalizeIcon missing {content_type}"
        checks += 1

    hook_fn = function_body(tray_cpp, "void* WINAPI IconView_IconView_Hook(")
    for stack in ("MainStack", "NonActivatableStack", "NotifyIconStack", "SystemTray.ChevronIconView"):
        assert stack in hook_fn, f"IconView_IconView_Hook missing {stack}"
        checks += 1

    assert 'L"Padding=8,0,10,0"' in styler_cpp, "Generated src/styler-latest.cpp missing Padding=8,0,10,0"
    assert "pitch=cell+4" not in tray_cpp and "pitch=cell;" in tray_cpp and "pitch=cell,row=24;" in tray_cpp, (
        "Generated src/tray-latest.cpp missing tightened 24 DIP tray column pitch"
    )
    checks += 2

    print(f"{checks} tray style and normalization checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
