#pragma once

#include <imgui.h>

#include <algorithm>

namespace plc_emulator {

// Printed 5/2 symbol. Exhausts R/S are internal atmospheric outlets.
inline void DrawValveSymbol(ImDrawList* draw_list, ImVec2 origin, float zoom,
                            bool double_solenoid) {
  if (draw_list == nullptr)
    return;
  const ImU32 color = IM_COL32(40, 40, 40, 255);
  const float thickness = std::max(0.7f, zoom);
  const auto point = [origin, zoom](float x, float y) {
    return ImVec2(origin.x + x * zoom, origin.y + y * zoom);
  };
  const auto line = [&](float x1, float y1, float x2, float y2) {
    draw_list->AddLine(point(x1, y1), point(x2, y2), color, thickness);
  };
  const auto arrow = [&](float x1, float y1, float x2, float y2) {
    line(x1, y1, x2, y2);
    const float tail_y = y2 + (y2 < y1 ? 3.0f : -3.0f);
    line(x2 - 2, tail_y, x2, y2);
    line(x2 + 2, tail_y, x2, y2);
  };
  draw_list->AddRect(point(10, 0), point(50, 16), color, 0, 0, thickness);
  line(30, 0, 30, 16);
  // Left position: P -> B, A -> R. Right: P -> A, B -> S.
  arrow(20, 16, 26, 0);
  arrow(14, 0, 14, 16);
  line(26, 16, 26, 13);
  line(23, 13, 29, 13);
  arrow(40, 16, 34, 0);
  arrow(46, 0, 46, 16);
  line(34, 16, 34, 13);
  line(31, 13, 37, 13);
  draw_list->AddRect(point(3, 4), point(10, 12), color, 0, 0, thickness);
  line(3, 12, 10, 4);
  if (double_solenoid) {
    draw_list->AddRect(point(50, 4), point(57, 12), color, 0, 0, thickness);
    line(50, 12, 57, 4);
  } else {
    line(50, 8, 52, 8);
    line(52, 8, 53, 4);
    line(53, 4, 55, 12);
    line(55, 12, 57, 4);
    line(57, 4, 59, 12);
    line(59, 12, 60, 8);
  }
  line(34, -2, 34, 0);
  line(46, -2, 46, 0);
  line(34, 16, 34, 18);
  line(40, 16, 40, 18);
  line(46, 16, 46, 18);
}

}  // namespace plc_emulator
