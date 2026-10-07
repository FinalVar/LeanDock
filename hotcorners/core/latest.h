#pragma once
#include <stdint.h>

namespace nhc {
struct Point { int x, y; };
struct Rect { int left, top, right, bottom; };
enum Action { None, TaskView, ShowDesktop, StartMenu };
struct Config {
    unsigned delay = 0;
    unsigned cooldown = 0;
    int pixels = 3;
    int rearm = 32;
    bool primaryOnly = false;
    bool fullscreen = true;
    Action actions[4] = { None, None, StartMenu, TaskView };
};
struct Target { int monitor = -1, corner = -1; };
inline bool Equal(Target a, Target b) { return a.monitor == b.monitor && a.corner == b.corner; }
inline bool Contains(Rect r, Point p) { return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom; }
inline bool InCorner(Rect r, Point p, int corner, int pixels) {
    if (!Contains(r, p)) return false;
    bool left = p.x - r.left < pixels, right = r.right - 1 - p.x < pixels;
    bool top = p.y - r.top < pixels, bottom = r.bottom - 1 - p.y < pixels;
    return corner == 0 ? left && top : corner == 1 ? right && top : corner == 2 ? left && bottom : right && bottom;
}
inline Target Hit(const Rect* monitors, int count, Point p, const Config& cfg) {
    for (int i = 0; i < count; ++i)
        for (int c = 0; c < 4; ++c)
            if (cfg.actions[c] != None && InCorner(monitors[i], p, c, cfg.pixels)) return { i, c };
    return {};
}
// Pure state machine: cancellation before dwell, one activation per entry, wide
// rearm zone after activation, and a global cooldown against corner jitter.
struct Engine {
    Target waiting, latched;
    uint64_t due = 0, lastFire = 0;
    bool firedOnce = false;
    void Reset() { waiting = {}; latched = {}; due = 0; }
    void Consume(Target target) { waiting = {}; due = 0; latched = target; }
    void Observe(const Rect* monitors, int count, Point p, const Config& cfg, uint64_t now, bool blocked) {
        if (latched.monitor >= 0 && latched.monitor < count &&
            InCorner(monitors[latched.monitor], p, latched.corner, cfg.rearm)) return;
        latched = {};
        Target hit = Hit(monitors, count, p, cfg);
        if (hit.monitor < 0) { waiting = {}; due = 0; return; }
        if (blocked || (firedOnce && now - lastFire < cfg.cooldown)) { Consume(hit); return; }
        if (!Equal(waiting, hit)) { waiting = hit; due = now + cfg.delay; }
    }
    Action Expire(const Rect* monitors, int count, Point p, const Config& cfg, uint64_t now, bool blocked) {
        Target target = waiting;
        if (target.monitor < 0) return None;
        if (target.monitor >= count || !InCorner(monitors[target.monitor], p, target.corner, cfg.pixels)) {
            waiting = {}; due = 0; return None;
        }
        if (blocked) { Consume(target); return None; }
        if (now < due) return None;
        Consume(target);
        lastFire = now;
        firedOnce = true;
        return cfg.actions[target.corner];
    }
};
}
