#pragma once

#include "imgui.h"
#include "plc_emulator/components/component_definition.h"

namespace plc_emulator {
using plc::ComponentPortDef;
struct PartDrawing {
  ImDrawList* list;
  ImVec2 origin;
  float zoom;

  ImVec2 Point(float x, float y) const {
    return {origin.x + x * zoom, origin.y + y * zoom};
  }
  void Rect(float x, float y, float width, float height, int grey) const {
    list->AddRectFilled(Point(x, y), Point(x + width, y + height),
                        IM_COL32(grey, grey, grey, 255));
    list->AddRect(Point(x, y), Point(x + width, y + height),
                  IM_COL32(40, 40, 40, 255), 0, 0, zoom);
  }
  void Circle(float x, float y, float radius, ImU32 color) const {
    const int segments = radius > 20 ? 48 : 16;
    list->AddCircleFilled(Point(x, y), radius * zoom, color, segments);
    list->AddCircle(Point(x, y), radius * zoom, IM_COL32(40, 40, 40, 255),
                    segments, zoom);
  }
  void Label(float x, float y, const char* label) const {
    if (zoom >= 0.5f) {
      list->AddText(ImGui::GetFont(), 8 * zoom, Point(x, y),
                    IM_COL32(40, 40, 40, 255), label);
    }
  }
  void Port(const ComponentPortDef& port) const {
    Circle(port.rel_pos.x, port.rel_pos.y, 4,
           ImGui::ColorConvertFloat4ToU32(
               ImVec4(port.color.r, port.color.g, port.color.b, port.color.a)));
  }
};
}  // namespace plc_emulator
