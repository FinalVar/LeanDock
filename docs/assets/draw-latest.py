"""Draw the README illustrations (light and dark) as plain SVG.

Run: python docs/assets/draw-latest.py
Sizes and colors follow the shipped sources: aggregate/policy-latest.h,
src/cards-aggregate-xaml-latest.inc and docs/material-latest.md. The Dock order
(apps, time key, tray) matches the real screenshot in screenshot.png.
These are illustrations, not screenshots.
"""
from pathlib import Path

OUT = Path(__file__).resolve().parent
FONT = ("'Segoe UI Variable Text','Segoe UI',-apple-system,BlinkMacSystemFont,"
        "'PingFang SC','Microsoft YaHei','Noto Sans CJK SC',sans-serif")

THEMES = {
    "light": dict(
        base=("#E9EEF7", "#F5EEEA"),
        blobs=[(250, 560, 360, "#8FD3C8", .85), (1040, 170, 420, "#AFC0F5", .9),
               (760, 700, 300, "#F7C3A8", .8), (90, 90, 260, "#D6CCF7", .7)],
        ribbons=[("#FFFFFF", .55), ("#7FB8E6", .35), ("#F2A7B4", .30)],
        dock="rgba(200,209,222,0.62)", dock_edge="rgba(255,255,255,0.65)",
        panel="rgba(248,249,251,0.84)", panel_edge="rgba(255,255,255,0.7)",
        text="#15171B", sec="#5E6571", accent="#0A7470", hair="rgba(21,23,27,0.10)",
        tile="rgba(21,23,27,0.05)", track="rgba(30,62,52,0.15)",
        ring=("#368775", "#B88024", "#C45A51"),
        shadow=.22, dot="#7D838E", active="#005FB8", alert="#D93A3F",
        tray="#3B4048", term="#2B2F3A", gear="#7D8799", glow=.55),
    "dark": dict(
        base=("#0D1220", "#171A2C"),
        blobs=[(250, 560, 360, "#1B6A6E", .9), (1040, 170, 440, "#3B2F86", .9),
               (800, 720, 300, "#7A3A63", .75), (90, 90, 260, "#1F3F7A", .7)],
        ribbons=[("#9FE3DC", .22), ("#6E86FF", .28), ("#FF8FB1", .20)],
        dock="rgba(37,44,56,0.70)", dock_edge="rgba(255,255,255,0.10)",
        panel="rgba(34,36,42,0.86)", panel_edge="rgba(255,255,255,0.10)",
        text="#ECEEF2", sec="#9EA5B1", accent="#3CC2B7", hair="rgba(236,238,242,0.12)",
        tile="rgba(236,238,242,0.06)", track="rgba(208,233,221,0.16)",
        ring=("#89C7B3", "#E1B25C", "#ED8E86"),
        shadow=.45, dot="#A9AFB9", active="#60CDFF", alert="#FF6B6B",
        tray="#D9DCE2", term="#3A3F4C", gear="#9AA3B4", glow=.70),
}

# --- primitives -------------------------------------------------------------

def text(x, y, s, size, fill, weight=400, anchor="start", opacity=None):
    o = f' opacity="{opacity}"' if opacity is not None else ""
    s = s.replace("&", "&amp;").replace("<", "&lt;")
    return (f'<text x="{x:.1f}" y="{y:.1f}" font-size="{size}" font-weight="{weight}" '
            f'fill="{fill}" text-anchor="{anchor}"{o}>{s}</text>')


def defs_common(t, uid):
    return f"""
  <filter id="{uid}soft" x="-60%" y="-60%" width="220%" height="220%"><feGaussianBlur stdDeviation="80"/></filter>
  <filter id="{uid}ribbon" x="-10%" y="-50%" width="120%" height="200%"><feGaussianBlur stdDeviation="5"/></filter>
  <filter id="{uid}frost" x="-5%" y="-5%" width="110%" height="110%"><feGaussianBlur stdDeviation="16"/></filter>
  <filter id="{uid}shadow" x="-30%" y="-40%" width="160%" height="200%">
    <feDropShadow dx="0" dy="10" stdDeviation="16" flood-color="#060914" flood-opacity="{t['shadow']}"/></filter>
  <linearGradient id="{uid}base" x1="0" y1="0" x2="1" y2="1">
    <stop offset="0" stop-color="{t['base'][0]}"/><stop offset="1" stop-color="{t['base'][1]}"/></linearGradient>
  <linearGradient id="{uid}folder" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#FFD978"/><stop offset="1" stop-color="#F4B13C"/></linearGradient>
  <linearGradient id="{uid}web" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#36D1C4"/><stop offset="1" stop-color="#2F6BE3"/></linearGradient>
  <linearGradient id="{uid}book" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#4FD1C5"/><stop offset="1" stop-color="#16807C"/></linearGradient>
  <linearGradient id="{uid}music" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#FF7A8A"/><stop offset="1" stop-color="#E0336B"/></linearGradient>
  <linearGradient id="{uid}chat" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4ADE80"/><stop offset="1" stop-color="#16A34A"/></linearGradient>"""


def wallpaper(t, uid, w=1280, h=720):
    sx, sy = w / 1280, h / 720
    grads = "".join(f'<radialGradient id="{uid}blob{i}"><stop offset="0" stop-color="{c}" stop-opacity="{o}"/>'
                    f'<stop offset=".55" stop-color="{c}" stop-opacity="{o * .45:.3f}"/>'
                    f'<stop offset="1" stop-color="{c}" stop-opacity="0"/></radialGradient>'
                    for i, (x, y, r, c, o) in enumerate(t["blobs"]))
    blobs = "".join(f'<circle cx="{x}" cy="{y}" r="{r * 1.7:.0f}" fill="url(#{uid}blob{i})"/>'
                    for i, (x, y, r, c, o) in enumerate(t["blobs"]))
    (c1, o1), (c2, o2), (c3, o3) = t["ribbons"]
    ribbons = (
        f'<path d="M-40,610 C220,470 430,700 700,560 S1100,420 1330,520" stroke="{c1}" stroke-opacity="{o1}" stroke-width="46" fill="none"/>'
        f'<path d="M-40,680 C260,560 520,760 820,640 S1150,560 1330,640" stroke="{c2}" stroke-opacity="{o2}" stroke-width="70" fill="none"/>'
        f'<path d="M-40,520 C300,420 560,600 880,470 S1180,380 1330,430" stroke="{c3}" stroke-opacity="{o3}" stroke-width="30" fill="none"/>')
    return (f'<g transform="scale({sx:.4f},{sy:.4f})"><rect width="1280" height="720" fill="url(#{uid}base)"/>'
            f'<defs>{grads}</defs>{blobs}<g filter="url(#{uid}ribbon)">{ribbons}</g></g>')


def glass(uid, name, shape, tint, edge):
    """Shadow, frosted wallpaper clipped to shape, tint and a hairline edge."""
    return (f'<clipPath id="{uid}{name}"> {shape.format(fill="#000", extra="")}</clipPath>'
            f'<g filter="url(#{uid}shadow)">{shape.format(fill="#000", extra="")}</g>'
            f'<g clip-path="url(#{uid}{name})"><use href="#{uid}wall" filter="url(#{uid}frost)"/></g>'
            + shape.format(fill=tint, extra=f' stroke="{edge}" stroke-width="1"'))

# --- icons (drawn in a 24 or 40 unit box) -------------------------------------

GLYPH = {
    "wifi": '<path d="M2.8,10.8 A13,13 0 0 1 21.2,10.8 M5.6,13.6 A9,9 0 0 1 18.4,13.6 M8.5,16.5 A5,5 0 0 1 15.5,16.5" fill="none" stroke-linecap="round"/><circle cx="12" cy="19.6" r="1.5" stroke="none"/>',
    "bt": '<path d="M7,7.6 L17,16.4 L12,20.8 V3.2 L17,7.6 L7,16.4" fill="none" stroke-linejoin="round" stroke-linecap="round"/>',
    "moon": '<path d="M14.2,3.4 A8.8,8.8 0 1 0 20.8,16.4 A7,7 0 0 1 14.2,3.4 Z" fill="none" stroke-linejoin="round"/>',
    "hotspot": '<circle cx="12" cy="12" r="1.7" stroke="none"/><path d="M8.3,8.3 A5.2,5.2 0 0 0 8.3,15.7 M15.7,8.3 A5.2,5.2 0 0 1 15.7,15.7 M5.5,5.5 A9.2,9.2 0 0 0 5.5,18.5 M18.5,5.5 A9.2,9.2 0 0 1 18.5,18.5" fill="none" stroke-linecap="round"/>',
    "speaker": '<path d="M4,9.4 H7.6 L12,5.6 V18.4 L7.6,14.6 H4 Z" fill="none" stroke-linejoin="round"/><path d="M15,9.2 A4,4 0 0 1 15,14.8 M17.6,6.6 A7.6,7.6 0 0 1 17.6,17.4" fill="none" stroke-linecap="round"/>',
    "sun": '<circle cx="12" cy="12" r="3.6" fill="none"/>' + "".join(
        f'<path d="M{12+6.6*c:.2f},{12+6.6*s:.2f} L{12+8.9*c:.2f},{12+8.9*s:.2f}" stroke-linecap="round"/>'
        for c, s in [(1, 0), (.7071, .7071), (0, 1), (-.7071, .7071), (-1, 0), (-.7071, -.7071), (0, -1), (.7071, -.7071)]),
    "chev": '<path d="M10,7.5 L14.5,12 L10,16.5" fill="none" stroke-linecap="round" stroke-linejoin="round"/>',
    "lock": '<rect x="5" y="10.5" width="14" height="10" rx="2.4" fill="none"/><path d="M8.2,10.5 V7.8 A3.8,3.8 0 0 1 15.8,7.8 V10.5" fill="none"/><circle cx="12" cy="15.4" r="1.3" stroke="none"/>',
    "desktop": '<rect x="3" y="4.5" width="18" height="12" rx="2" fill="none"/><path d="M9,20 H15 M12,16.5 V20" stroke-linecap="round"/>',
    "switch": '<rect x="2.5" y="7" width="12" height="10" rx="2" fill="none"/><rect x="9.5" y="4" width="12" height="10" rx="2" fill="none"/>',
    "mic": '<rect x="9" y="3.5" width="6" height="11" rx="3" fill="none"/><path d="M5.8,11.5 A6.2,6.2 0 0 0 18.2,11.5 M12,17.7 V20.5" fill="none" stroke-linecap="round"/>',
    "pin": '<path d="M12,21 C12,21 5.5,14.6 5.5,9.8 A6.5,6.5 0 0 1 18.5,9.8 C18.5,14.6 12,21 12,21 Z" fill="none"/><circle cx="12" cy="9.8" r="2.2" fill="none"/>',
    "cloud": '<path d="M7,18.5 H17.5 A4,4 0 0 0 17.3,10.5 A5.6,5.6 0 0 0 6.6,11.6 A3.5,3.5 0 0 0 7,18.5 Z" fill="none" stroke-linejoin="round"/>',
    "shield": '<path d="M12,3.5 L19,6.2 V11.5 C19,16 15.8,19.2 12,20.6 C8.2,19.2 5,16 5,11.5 V6.2 Z" fill="none" stroke-linejoin="round"/>',
    "bubble": '<path d="M5,6.5 H19 A2,2 0 0 1 21,8.5 V15 A2,2 0 0 1 19,17 H10 L6,20.5 V17 H5 A2,2 0 0 1 3,15 V8.5 A2,2 0 0 1 5,6.5 Z" fill="none" stroke-linejoin="round"/>',
    "note": '<path d="M9.5,17.5 V5.5 L19,3.8 V15.5" fill="none" stroke-linejoin="round"/><circle cx="7.4" cy="17.5" r="2.3" fill="none"/><circle cx="16.9" cy="15.5" r="2.3" fill="none"/>',
    "keyboard": '<rect x="2.5" y="6.5" width="19" height="11" rx="2" fill="none"/><path d="M6,10 H6.2 M9.4,10 H9.6 M12.9,10 H13.1 M16.4,10 H16.6 M7.5,14 H16.5" stroke-linecap="round"/>',
    "drive": '<rect x="3" y="7.5" width="18" height="9" rx="2.2" fill="none"/><circle cx="16.8" cy="12" r="1.1" stroke="none"/>',
}


def glyph(name, x, y, size, color, width=1.6):
    k = size / 24
    return (f'<g transform="translate({x:.2f},{y:.2f}) scale({k:.4f})" stroke="{color}" fill="{color}" '
            f'stroke-width="{width / k:.2f}">{GLYPH[name]}</g>')


def app_icons(t, uid):
    return [
        # folder
        f'<path d="M4,12 A3,3 0 0 1 7,9 H15 L18,12 H33 A3,3 0 0 1 36,15 V17 H4 Z" fill="#E09A2C"/>'
        f'<rect x="4" y="15" width="32" height="19" rx="3" fill="url(#{uid}folder)"/>',
        # browser
        f'<circle cx="20" cy="20" r="16" fill="url(#{uid}web)"/>'
        f'<g fill="none" stroke="#fff" stroke-width="1.9" opacity=".95"><circle cx="20" cy="20" r="10.5"/>'
        f'<ellipse cx="20" cy="20" rx="4.6" ry="10.5"/><path d="M9.5,20 H30.5 M11.2,14.5 H28.8 M11.2,25.5 H28.8"/></g>',
        # terminal
        f'<rect x="4" y="6" width="32" height="28" rx="5" fill="{t["term"]}"/>'
        f'<rect x="4" y="6" width="32" height="6" rx="3" fill="#ffffff" opacity=".08"/>'
        f'<path d="M11,17 L16,21.5 L11,26 M19,26.5 H28" fill="none" stroke="#E6E9EF" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"/>',
        # reader (栖页)
        f'<rect x="8" y="4" width="25" height="32" rx="4" fill="url(#{uid}book)"/>'
        f'<rect x="8" y="4" width="5" height="32" rx="2.5" fill="#000" opacity=".16"/>'
        f'<path d="M17,13 H28 M17,18 H28 M17,23 H24" stroke="#fff" stroke-width="2" stroke-linecap="round" opacity=".92"/>',
        # chat
        f'<path d="M20,5 C29.4,5 36,10.9 36,18 C36,25.1 29.4,31 20,31 C18.3,31 16.7,30.8 15.2,30.4 L8.5,34.5 L9.6,27.8 C6.1,25.4 4,21.9 4,18 C4,10.9 10.6,5 20,5 Z" fill="url(#{uid}chat)"/>'
        f'<circle cx="13.5" cy="18" r="2" fill="#fff"/><circle cx="20" cy="18" r="2" fill="#fff"/><circle cx="26.5" cy="18" r="2" fill="#fff"/>',
        # music
        f'<circle cx="20" cy="20" r="16" fill="url(#{uid}music)"/>'
        f'<path d="M17,26.5 V12.5 L27,10.5 V23.5" fill="none" stroke="#fff" stroke-width="2.4" stroke-linejoin="round"/>'
        f'<circle cx="14.6" cy="26.5" r="3" fill="#fff"/><circle cx="24.6" cy="23.5" r="3" fill="#fff"/>',
        # settings
        f'<circle cx="20" cy="20" r="12.6" fill="none" stroke="{t["gear"]}" stroke-width="6" stroke-dasharray="4.95 4.95"/>'
        f'<circle cx="20" cy="20" r="11" fill="{t["gear"]}"/><circle cx="20" cy="20" r="4.6" fill="{t["term"]}" opacity=".85"/>',
    ]

# --- the Dock and the time key -------------------------------------------------

APP_DOTS = ["dot", "active", "dot", None, "alert", None, None]
TRAY = [["bubble", "cloud"], ["shield", "note"]]
KEY_X = 12 + 52 * len(APP_DOTS) + 4          # the time key sits right after the apps
TRAY_X = KEY_X + 78 + 2                      # 78 DIP reservation, then the native tray
DOCK_W, DOCK_H = TRAY_X + 24 * len(TRAY[0]) + 12, 60


def ring_path():
    # Same stadium as RingTrack: 68.8 x 42.8 inside 72 x 46, start at top centre, clockwise.
    return "M36,1.6 H49 A21.4,21.4 0 0 1 49,44.4 H23 A21.4,21.4 0 0 1 23,1.6 Z"


def time_key(t, percent, plugged=False, clock="21:47", date="10/30"):
    tone = 0 if plugged or percent >= 20 else (2 if percent < 10 else 1)
    parts = []
    parts.append(f'<path d="{ring_path()}" fill="none" stroke="{t["track"]}" stroke-width="1.4"/>')
    if percent > 0:
        dash = "" if percent >= 100 else f' stroke-dasharray="{percent} 100"'
        parts.append(f'<path d="{ring_path()}" pathLength="100" fill="none" stroke="{t["ring"][tone]}" '
                     f'stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"{dash}/>')
    parts.append(text(36, 22.2, clock, 18, t["text"], 600, "middle"))
    parts.append(text(36, 38.6, date, 11, t["text"], 400, "middle", .7))
    return "".join(parts)


def dock(t, uid, x, y, scale):
    s = scale
    shape = (f'<rect x="{x}" y="{y}" width="{DOCK_W * s}" height="{DOCK_H * s}" rx="{30 * s}" '
             'fill="{fill}"{extra}/>')
    out = [glass(uid, "dockclip", shape, t["dock"], t["dock_edge"])]
    out.append(f'<g transform="translate({x},{y}) scale({s})">')
    for i, (icon, dot) in enumerate(zip(app_icons(t, uid), APP_DOTS)):
        cx = 12 + 26 + 52 * i
        out.append(f'<g transform="translate({cx - 20},10)">{icon}</g>')
        if dot:
            out.append(f'<circle cx="{cx}" cy="55" r="2" fill="{t[dot]}"/>')
    for r, row in enumerate(TRAY):
        for j, g in enumerate(row):
            out.append(glyph(g, TRAY_X + 12 + 24 * j - 8, 18 + 24 * r - 8, 16, t["tray"], 1.4))
    out.append(f'<g transform="translate({KEY_X},7)">{time_key(t, 76)}</g>')
    out.append("</g>")
    return "".join(out)


# --- scenes --------------------------------------------------------------------

def svg(w, h, body, label):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}" '
            f'font-family="{FONT}" role="img" aria-label="{label}">\n{body}\n</svg>\n')


def keys(theme):
    t = THEMES[theme]
    W, H, S = 1200, 290, 3
    cases = [(76, False, "76%", "电量充足"), (15, True, "15%", "插电中，保持正常色"),
             (16, False, "16%", "低于 20%"), (7, False, "7%", "低于 10%")]
    gap = (W - 4 * 72 * S) / 5
    body = []
    for i, (p, plug, big, small) in enumerate(cases):
        x = gap + i * (72 * S + gap)
        y = 40
        body.append(f'<rect x="{x - 18}" y="{y - 21}" width="{72 * S + 36}" height="{46 * S + 42}" '
                    f'rx="{(46 * S + 42) / 2}" fill="{t["dock"]}" stroke="{t["dock_edge"]}"/>')
        body.append(f'<g transform="translate({x},{y}) scale({S})">{time_key(t, p, plugged=plug)}</g>')
        cx = x + 36 * S
        body.append(text(cx, 238, big, 26, t["text"], 600, "middle"))
        body.append(text(cx, 272, small, 19, t["sec"], 400, "middle"))
    return svg(W, H, "".join(body), "时间键的电量环：从顶部顺时针走一圈，低于 20% 变黄，低于 10% 变红，插电时保持正常色")


def corners(theme):
    t, uid = THEMES[theme], "c"
    W, H = 1200, 640
    sx, sy, sw, sh = 40, 20, 1120, 600
    S = 0.95
    dx, dy = sx + (sw - DOCK_W * S) / 2, sy + sh - 4 * S - DOCK_H * S
    clip = f'<clipPath id="{uid}screen"><rect x="{sx}" y="{sy}" width="{sw}" height="{sh}" rx="22"/></clipPath>'
    glow_defs = (f'<radialGradient id="{uid}glow"><stop offset="0" stop-color="{t["accent"]}" stop-opacity="{t["glow"]}"/>'
                 f'<stop offset="1" stop-color="{t["accent"]}" stop-opacity="0"/></radialGradient>')
    body = [f'<defs>{defs_common(t, uid)}{glow_defs}{clip}'
            f'<g id="{uid}wall"><g transform="translate({sx},{sy})">{wallpaper(t, uid, sw, sh)}</g></g></defs>',
            f'<g clip-path="url(#{uid}screen)"><use href="#{uid}wall"/>']
    spots = [(sx, sy, "lock", "锁屏", "停留 0.5 秒", "start", True),
             (sx, sy + sh, "desktop", "显示桌面", "再碰一次恢复", "start", True),
             (sx + sw, sy + sh, "switch", "切换窗口", "只有两个窗口时直接互换", "end", True),
             (sx + sw, sy, None, "右上角", "未使用", "end", False)]
    for cx, cy, *_rest, on in spots:
        if on:
            body.append(f'<circle cx="{cx}" cy="{cy}" r="210" fill="url(#{uid}glow)"/>')
            for r, o in ((30, .9), (54, .55), (78, .3)):
                body.append(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{t["accent"]}" '
                            f'stroke-opacity="{o}" stroke-width="2"/>')
    body.append(dock(t, uid, dx, dy, S))
    body.append("</g>")
    body.append(f'<rect x="{sx}" y="{sy}" width="{sw}" height="{sh}" rx="22" fill="none" '
                f'stroke="{t["hair"]}" stroke-width="2"/>')
    for i, (cx, cy, icon, title, sub, side, on) in enumerate(spots):
        bw, bh = 236, 64
        bx = cx + 34 if side == "start" else cx - 34 - bw
        by = cy + 34 if cy == sy else cy - 34 - bh - (0 if side == "end" else 0)
        if cy != sy:
            by = cy - 120 - bh
        shape = f'<rect x="{bx}" y="{by}" width="{bw}" height="{bh}" rx="16" fill="{{fill}}"{{extra}}/>'
        body.append(f'<g opacity="{1 if on else .55}">' + glass(uid, f"badge{i}", shape, t["panel"], t["panel_edge"]))
        if icon:
            body.append(f'<circle cx="{bx + 34}" cy="{by + bh / 2}" r="19" fill="{t["accent"]}" fill-opacity=".14"/>')
            body.append(glyph(icon, bx + 22, by + bh / 2 - 12, 24, t["accent"], 1.8))
            tx = bx + 66
        else:
            tx = bx + 22
        body.append(text(tx, by + 28, title, 17, t["text"], 600))
        body.append(text(tx, by + 49, sub, 13.5, t["sec"]))
        body.append("</g>")
    return svg(W, H, "".join(body), "四个触发角：左上锁屏，左下显示桌面，右下切换窗口，右上未使用")


BADGES = [("platform", "Windows 11", "#0A64C8"), ("C++", "23", "#0A7470"),
          ("version", "0.8.88", "#138A82"), ("license", "GPL-3.0", "#6A55C2")]


def badge(label, value, color):
    def width(s):
        return sum(3.9 if ch in " .,:;!1ilI-+" else 7.6 if ch.isupper() else 6.9 for ch in s) + 20
    lw, vw, h = width(label), width(value), 22
    body = (f'<clipPath id="b"><rect width="{lw + vw:.1f}" height="{h}" rx="11"/></clipPath>'
            f'<g clip-path="url(#b)"><rect width="{lw:.1f}" height="{h}" fill="#30353D"/>'
            f'<rect x="{lw:.1f}" width="{vw:.1f}" height="{h}" fill="{color}"/></g>'
            + text(lw / 2, 15.2, label, 12, "#E6E8EC", 400, "middle")
            + text(lw + vw / 2, 15.2, value, 12, "#FFFFFF", 600, "middle"))
    w = f"{lw + vw:.1f}"
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}" '
            f'font-family="{FONT}" role="img" aria-label="{label}: {value}">{body}</svg>\n')


if __name__ == "__main__":
    for theme in THEMES:
        for name, fn in (("keys", keys), ("corners", corners)):
            (OUT / f"{name}-{theme}.svg").write_text(fn(theme), encoding="utf-8")
    for label, value, color in BADGES:
        slug = label.lower().replace("+", "p")
        (OUT / f"badge-{slug}.svg").write_text(badge(label, value, color), encoding="utf-8")
    print("written to", OUT)
