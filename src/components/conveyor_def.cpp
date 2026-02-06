#include "plc_emulator/components/conveyor_def.h"

#include "imgui.h"
#include "plc_emulator/components/state_keys.h"

#include <cmath>

namespace plc {
namespace {

ImU32 ToImU32(const Color& c) {
  return IM_COL32(c.r * 255, c.g * 255, c.b * 255, c.a * 255);
}

bool ClipLineToRect(const ImVec2& rect_min, const ImVec2& rect_max,
                    ImVec2* p0, ImVec2* p1) {
  if (!p0 || !p1) {
    return false;
  }
  float x0 = p0->x;
  float y0 = p0->y;
  float x1 = p1->x;
  float y1 = p1->y;
  float dx = x1 - x0;
  float dy = y1 - y0;
  float t0 = 0.0f;
  float t1 = 1.0f;
  auto clip = [&](float p, float q) -> bool {
    if (p == 0.0f) {
      return q >= 0.0f;
    }
    float r = q / p;
    if (p < 0.0f) {
      if (r > t1) {
        return false;
      }
      if (r > t0) {
        t0 = r;
      }
    } else {
      if (r < t0) {
        return false;
      }
      if (r < t1) {
        t1 = r;
      }
    }
    return true;
  };
  if (!clip(-dx, x0 - rect_min.x)) {
    return false;
  }
  if (!clip(dx, rect_max.x - x0)) {
    return false;
  }
  if (!clip(-dy, y0 - rect_min.y)) {
    return false;
  }
  if (!clip(dy, rect_max.y - y0)) {
    return false;
  }
  if (t1 < t0) {
    return false;
  }
  p0->x = x0 + t0 * dx;
  p0->y = y0 + t0 * dy;
  p1->x = x0 + t1 * dx;
  p1->y = y0 + t1 * dy;
  return true;
}

const ComponentPortDef kPorts[] = {
    {0, {372.0f, 70.0f}, PortType::ELECTRIC, true, {1, 0, 0, 1},
     "POWER_24V"},
    {1, {382.0f, 70.0f}, PortType::ELECTRIC, true, {0, 0, 0, 1},
     "POWER_0V"},
};

void RenderConveyor(ImDrawList* draw_list,
                    const PlacedComponent& comp,
                    ImVec2 pos,
                    float zoom) {
  ImVec2 size = {400.0f * zoom, 80.0f * zoom};
  float rounding = 10.0f * zoom;
  float base_y_offset = 60.0f * zoom;
  float belt_bottom = pos.y + base_y_offset;
  float belt_clip_bottom = belt_bottom - 1.5f * zoom;
  ImVec2 belt_min = {pos.x, pos.y};
  ImVec2 belt_max = {pos.x + size.x, belt_clip_bottom};
  ImU32 body_color = IM_COL32(255, 255, 255, 255);
  ImU32 frame_color = IM_COL32(0, 0, 0, 255);
  ImU32 base_fill_color = IM_COL32(200, 200, 200, 220);
  constexpr float kPi = 3.14159265f;

  draw_list->AddRectFilled(pos, {pos.x + size.x, pos.y + size.y}, body_color,
                           rounding, ImDrawFlags_RoundCornersTop);

  float offset = 0.0f;
  if (comp.internalStates.count(state_keys::kMotorActive) &&
      comp.internalStates.at(state_keys::kMotorActive) > 0.5f) {
    offset = std::fmod(ImGui::GetTime() * 50.0f, 30.0f);
  }
  for (float x = -size.y; x < size.x; x += 30.0f * zoom) {
    ImVec2 p0 = {pos.x + x + offset * zoom, pos.y};
    ImVec2 p1 = {pos.x + x + size.y + offset * zoom, belt_clip_bottom};
    if (ClipLineToRect(belt_min, belt_max, &p0, &p1)) {
      draw_list->AddLine(p0, p1, frame_color, 2.0f * zoom);
    }
  }

  draw_list->AddRect(pos, {pos.x + size.x, pos.y + size.y}, frame_color,
                     rounding, ImDrawFlags_RoundCornersTop, 2.0f * zoom);

  float base_inset = 2.0f * zoom;
  float base_rounding = rounding > base_inset ? (rounding - base_inset) : 0.0f;
  ImVec2 frame_min = {pos.x + base_inset, pos.y + base_y_offset};
  ImVec2 frame_max = {pos.x + size.x - base_inset, pos.y + size.y};
  draw_list->AddRectFilled(frame_min, frame_max, base_fill_color, base_rounding,
                           ImDrawFlags_RoundCornersTop);

  // Base top border only: arcs + top line (avoid wedge artifacts)
  float border_thickness = 2.0f * zoom;
  float top_y = frame_min.y;
  float left_arc_x = frame_min.x + base_rounding;
  float right_arc_x = frame_max.x - base_rounding;

  draw_list->PathClear();
  draw_list->PathArcTo({left_arc_x, top_y + base_rounding},
                       base_rounding, kPi, kPi * 1.5f);
  draw_list->PathStroke(frame_color, false, border_thickness);

  draw_list->AddLine({left_arc_x, top_y}, {right_arc_x, top_y},
                     frame_color, border_thickness);

  draw_list->PathClear();
  draw_list->PathArcTo({right_arc_x, top_y + base_rounding},
                       base_rounding, kPi * 1.5f, kPi * 2.0f);
  draw_list->PathStroke(frame_color, false, border_thickness);

  for (const auto& p : kPorts) {
    draw_list->AddCircleFilled(
        {pos.x + p.rel_pos.x * zoom, pos.y + p.rel_pos.y * zoom}, 4 * zoom,
        ToImU32(p.color));
  }
}

void InitConveyorDefaults(PlacedComponent* comp) {
  if (!comp) {
    return;
  }
  comp->internalStates[state_keys::kMotorActive] = 0.0f;
}

const ComponentDefinition kDefinition = {
    ComponentType::CONVEYOR,
    "component.conveyor.name",
    "component.conveyor.desc",
    {400.0f, 80.0f},
    kPorts,
    static_cast<int>(sizeof(kPorts) / sizeof(kPorts[0])),
    &RenderConveyor,
    &InitConveyorDefaults,
    ComponentCategory::BOTH,
    "icon.conveyor",
    "Conveyor",
};

}  // namespace

const ComponentDefinition* GetConveyorDefinition() {
  return &kDefinition;
}

}  // namespace plc
