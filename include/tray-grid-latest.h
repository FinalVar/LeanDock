#pragma once
// Layout shared by the retained native application and system icon containers.
// Parents and native input routes remain unchanged.
namespace DockGrid {
struct Group { int columns; int overlapColumns; };
constexpr Group PlaceGroup(int before, int count) {
    return count <= 0 ? Group{0, 0} : Group{(before % 2 + count + 1) / 2, before % 2};
}
constexpr int Column(int before, int index) { return (before % 2 + index) / 2; }
constexpr int Row(int before, int index) { return (before + index) % 2; }
constexpr int TotalColumns(int count) { return (count + 1) / 2; }
constexpr double RowOffset(int before, int index, int total) {
    return total % 2 && before + index == total - 1 ? 0.0 : Row(before, index) - 0.5;
}
}
