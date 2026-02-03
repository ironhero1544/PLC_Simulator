#include "plc_emulator/components/box_def.h"

#include "imgui.h"

#include <cfloat>

namespace plc {
namespace {

void RenderBox(ImDrawList* draw_list,
               const PlacedComponent& comp,
               ImVec2 pos,
               float zoom) {
  (void)comp;
  ImVec2 size = {80.0f * zoom, 100.0f * zoom};

  draw_list->AddRectFilled(pos, {pos.x + size.x, pos.y + size.y},
                           IM_COL32(101, 67, 33, 255));

  draw_list->PushClipRect(pos, {pos.x + size.x, pos.y + size.y});
  for (float x = -size.y; x < size.x; x += 20.0f * zoom) {
    draw_list->AddLine({pos.x + x, pos.y},
                       {pos.x + x + size.y, pos.y + size.y},
                       IM_COL32(139, 69, 19, 255), 2.0f * zoom);
  }
  draw_list->PopClipRect();

  draw_list->AddRect(pos, {pos.x + size.x, pos.y + size.y},
                     IM_COL32(0, 0, 0, 255), 0, 0, 5.0f * zoom);

  const char* text = "BOX";
  float text_scale = zoom / 1.3f;
  float font_size = 16.0f * text_scale;
  ImVec2 txt_size =
      ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text);
  ImVec2 text_pos = {pos.x + (size.x - txt_size.x) * 0.5f,
                     pos.y + (size.y - txt_size.y) * 0.5f};
  draw_list->AddText(ImGui::GetFont(), font_size, text_pos,
                     IM_COL32(255, 255, 255, 255), text);
}

const ComponentDefinition kDefinition = {
    ComponentType::BOX,
    "component.box.name",
    "component.box.desc",
    {80.0f, 100.0f},
    nullptr,
    0,
    &RenderBox,
    nullptr,
    ComponentCategory::BOTH,
    "icon.box",
    "Box",
};

}  // namespace

const ComponentDefinition* GetBoxDefinition() {
  return &kDefinition;
}

}  // namespace plc
