#pragma once
// Column-first ordering, shared by application icons, battery and input method.
// This is a layout policy, not a second notification host.
namespace DockGrid {
struct Group { int columns; int overlapColumns; };
constexpr Group PlaceGroup(int before, int count) {
    return count <= 0 ? Group{0, 0} : Group{(before % 2 + count + 1) / 2, before % 2};
}
constexpr int Column(int before, int index) { return (before % 2 + index) / 2; }
constexpr int Row(int before, int index) { return (before + index) % 2; }
constexpr int TotalColumns(int count) { return (count + 1) / 2; }
}
