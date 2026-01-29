#include "plc_emulator/components/manifold_def.h"

#include "imgui.h"

#include "plc_emulator/lang/lang_manager.h"

namespace plc {

namespace {

ImU32 ToImU32(const Color& color) {
  return IM_COL32(static_cast<int>(color.r * 255.0f),
                  static_cast<int>(color.g * 255.0f),
                  static_cast<int>(color.b * 255.0f),
                  static_cast<int>(color.a * 255.0f));
}

const ComponentPortDef kPorts[] = {
    {0, {10.0f, 30.0f}, PortType::PNEUMATIC, true,
     {0.13f, 0.59f, 0.95f, 1.0f}, "PNEUMATIC_IN"},
    {1, {30.0f, 50.0f}, PortType::PNEUMATIC, false,
     {0.13f, 0.59f, 0.95f, 1.0f}, "PNEUMATIC_OUT"},
    {2, {50.0f, 50.0f}, PortType::PNEUMATIC, false,
     {0.13f, 0.59f, 0.95f, 1.0f}, "PNEUMATIC_OUT"},
    {3, {70.0f, 50.0f}, PortType::PNEUMATIC, false,
     {0.13f, 0.59f, 0.95f, 1.0f}, "PNEUMATIC_OUT"},
    {4, {90.0f, 50.0f}, PortType::PNEUMATIC, false,
     {0.13f, 0.59f, 0.95f, 1.0f}, "PNEUMATIC_OUT"},
};

void RenderManifold(ImDrawList* draw_list,
                    const PlacedComponent& comp,
                    ImVec2 screen_pos,
                    float zoom) {
  ImVec2 body_end = {screen_pos.x + comp.size.width * zoom,
                     screen_pos.y + comp.size.height * zoom};
  draw_list->AddRectFilled(screen_pos, body_end, IM_COL32(180, 180, 180, 255),
                           3.0f * zoom);
  draw_list->AddRect(screen_pos, body_end, IM_COL32(100, 100, 100, 255),
                     3.0f * zoom, 0, 2.0f);

  for (const auto& port : kPorts) {
    ImVec2 port_pos = {screen_pos.x + port.rel_pos.x * zoom,
                       screen_pos.y + port.rel_pos.y * zoom};
    draw_list->AddCircleFilled(port_pos, 5 * zoom, ToImU32(port.color));
  }

  if (zoom > 0.5f) {
    draw_list->AddText(
        ImVec2(screen_pos.x + 25 * zoom, screen_pos.y + 15 * zoom),
        IM_COL32(50, 50, 50, 255),
        TR("component.manifold.label", "MANIFOLD"));
  }
}

const ComponentDefinition kDefinition = {
    ComponentType::MANIFOLD,
    "component.manifold.name",
    "component.manifold.desc",
    {120.0f, 60.0f},
    kPorts,
    static_cast<int>(sizeof(kPorts) / sizeof(kPorts[0])),
    &RenderManifold,
    nullptr,
    ComponentCategory::AUTOMATION,
    "icon.manifold",
    "Manifold",
};

}  // namespace

const ComponentDefinition* GetManifoldDefinition() {
  return &kDefinition;
}

}  // namespace plc
