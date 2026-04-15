/*
 * rtl_module_def.cpp
 *
 * Generic HDL block rendering.
 */

#include "plc_emulator/components/rtl_module_def.h"
#include "plc_emulator/lang/lang_manager.h"

#include "imgui.h"

#include <algorithm>
#include <cfloat>

namespace plc {
namespace {

ImU32 ToImU32(const Color& color) {
  return IM_COL32(static_cast<int>(color.r * 255.0f),
                  static_cast<int>(color.g * 255.0f),
                  static_cast<int>(color.b * 255.0f),
                  static_cast<int>(color.a * 255.0f));
}

std::string FitLabel(const std::string& label, size_t maxChars) {
  if (label.size() <= maxChars) {
    return label;
  }
  if (maxChars <= 3) {
    return label.substr(0, maxChars);
  }
  return label.substr(0, maxChars - 3) + "...";
}

float ComputeScaledFontSize(float baseSize, float zoom) {
  float text_scale = zoom / 1.4f;
  float font_size = baseSize * text_scale;
  const float max_font_size =
      ImGui::GetFontSize() * std::max(1.0f, zoom * 0.75f);
  if (font_size > max_font_size) {
    font_size = max_font_size;
  }
  return font_size;
}

void RenderRtlModule(ImDrawList* draw_list,
                     const PlacedComponent& comp,
                     ImVec2 pos,
                     float zoom) {
  if (!draw_list) {
    return;
  }

  ImVec2 size(comp.size.width * zoom, comp.size.height * zoom);
  ImVec2 max(pos.x + size.x, pos.y + size.y);
  const float bodyInset = 22.0f * zoom;
  ImVec2 bodyMin(pos.x + bodyInset, pos.y + 12.0f * zoom);
  ImVec2 bodyMax(max.x - bodyInset, max.y - 12.0f * zoom);
  draw_list->AddRectFilled(bodyMin, bodyMax, IM_COL32(46, 52, 64, 255),
                           8.0f * zoom);
  draw_list->AddRect(bodyMin, bodyMax, IM_COL32(20, 24, 32, 255), 8.0f * zoom,
                     0, 2.0f);

  ImVec2 notchCenter((bodyMin.x + bodyMax.x) * 0.5f, bodyMin.y + 6.0f * zoom);
  draw_list->AddCircleFilled(notchCenter, 8.0f * zoom, IM_COL32(24, 28, 36, 255),
                             24);

  ImFont* font = ImGui::GetFont();
  const float titleFontSize = ComputeScaledFontSize(16.0f, zoom);
  const float metaFontSize = ComputeScaledFontSize(11.0f, zoom);
  const float pinFontSize = ComputeScaledFontSize(10.0f, zoom);

  std::string label = comp.customLabel.empty()
                          ? TR("component.rtl_module.default_label", "RTL IC")
                          : comp.customLabel;
  std::string topLine = FitLabel(label, 20);
  ImVec2 topLineSize = font->CalcTextSizeA(titleFontSize, FLT_MAX, 0.0f,
                                           topLine.c_str());
  draw_list->AddText(font, titleFontSize,
                     ImVec2((bodyMin.x + bodyMax.x - topLineSize.x) * 0.5f,
                            bodyMin.y + 16.0f * zoom),
                     IM_COL32(242, 244, 247, 255), topLine.c_str());

  std::string subLabel =
      comp.rtlModuleId.empty()
          ? TR("component.rtl_module.default_sublabel", "verilog")
          : comp.rtlModuleId;
  subLabel = FitLabel(subLabel, 24);
  ImVec2 subLabelSize = font->CalcTextSizeA(metaFontSize, FLT_MAX, 0.0f,
                                            subLabel.c_str());
  draw_list->AddText(font, metaFontSize,
                     ImVec2((bodyMin.x + bodyMax.x - subLabelSize.x) * 0.5f,
                            bodyMin.y + 34.0f * zoom),
                     IM_COL32(170, 178, 191, 255), subLabel.c_str());

  for (const auto& port : comp.runtimePorts) {
    ImVec2 portPos(pos.x + port.relativePos.x * zoom,
                   pos.y + port.relativePos.y * zoom);
    ImVec2 pinInner = port.isInput ? ImVec2(bodyMin.x, portPos.y)
                                   : ImVec2(bodyMax.x, portPos.y);
    draw_list->AddLine(portPos, pinInner, IM_COL32(196, 161, 87, 255),
                       2.2f * zoom);
    draw_list->AddCircleFilled(portPos, 4.0f * zoom, ToImU32(port.color), 16);
    draw_list->AddCircle(portPos, 4.0f * zoom, IM_COL32(24, 28, 36, 255), 16,
                         1.2f * zoom);

    if (zoom > 0.45f) {
      std::string pinLabel = FitLabel(port.role, 10);
      ImVec2 textSize = font->CalcTextSizeA(pinFontSize, FLT_MAX, 0.0f,
                                            pinLabel.c_str());
      float textX = port.isInput ? (bodyMin.x + 8.0f * zoom)
                                 : (bodyMax.x - 8.0f * zoom - textSize.x);
      draw_list->AddText(
          font, pinFontSize,
          ImVec2(textX, portPos.y - textSize.y * 0.5f),
          IM_COL32(233, 236, 239, 255), pinLabel.c_str());
    }
  }
}

void InitDefaultState(PlacedComponent* comp) {
  if (!comp) {
    return;
  }
  if (comp->size.width <= 0.0f) {
    comp->size.width = 260.0f;
  }
  if (comp->size.height <= 0.0f) {
    comp->size.height = 180.0f;
  }
}

const ComponentDefinition kDefinition = {
    ComponentType::RTL_MODULE,
    "component.rtl_module.name",
    "component.rtl_module.desc",
    {260.0f, 180.0f},
    nullptr,
    0,
    RenderRtlModule,
    InitDefaultState,
    ComponentCategory::SEMICONDUCTOR,
    "rtl_module",
    "RTL",
};

}  // namespace

const ComponentDefinition* GetRtlModuleDefinition() {
  return &kDefinition;
}

}  // namespace plc
