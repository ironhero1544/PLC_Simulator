#include "plc_emulator/components/workpiece_def.h"

#include "imgui.h"
#include "plc_emulator/components/state_keys.h"

#include <cmath>

namespace plc {
namespace {

void RenderWorkpiece(ImDrawList* draw_list,
                     const PlacedComponent& comp,
                     ImVec2 screen_pos,
                     float zoom) {
  float radius = 15.0f * zoom;
  ImVec2 center = {screen_pos.x + radius, screen_pos.y + radius};

  bool is_metal = comp.internalStates.count(state_keys::kIsMetal) &&
                  comp.internalStates.at(state_keys::kIsMetal) > 0.5f;
  bool is_processed =
      comp.internalStates.count(state_keys::kIsProcessed) &&
      comp.internalStates.at(state_keys::kIsProcessed) > 0.5f;

  ImU32 body_color = is_metal ? IM_COL32(192, 192, 192, 255)
                              : IM_COL32(139, 69, 19, 255);

  draw_list->AddCircleFilled(center, radius, body_color);
  draw_list->AddCircle(center, radius, IM_COL32(0, 0, 0, 255), 0, 2.0f * zoom);

  if (is_processed) {
    ImU32 hatch_color = IM_COL32(0, 0, 0, 150);
    float spacing = 5.0f * zoom;
    for (float d = -radius; d < radius; d += spacing) {
      float h = std::sqrt((radius * radius) - (d * d));
      if (h > 1.0f) {
        draw_list->AddLine(ImVec2(center.x + d, center.y - h + 1),
                           ImVec2(center.x + d, center.y + h - 1),
                           hatch_color, 2.0f * zoom);
      }
    }
  }
}

void InitMetalWorkpieceDefaults(PlacedComponent* comp) {
  if (!comp) {
    return;
  }
  comp->internalStates[state_keys::kIsMetal] = 1.0f;
  comp->internalStates[state_keys::kIsProcessed] = 0.0f;
  comp->internalStates[state_keys::kIsManualDrag] = 0.0f;
  comp->internalStates[state_keys::kIsContacted] = 0.0f;
  comp->internalStates[state_keys::kIsStuckBox] = 0.0f;
  comp->internalStates[state_keys::kVelX] = 0.0f;
  comp->internalStates[state_keys::kVelY] = 0.0f;
}

void InitNonmetalWorkpieceDefaults(PlacedComponent* comp) {
  if (!comp) {
    return;
  }
  comp->internalStates[state_keys::kIsMetal] = 0.0f;
  comp->internalStates[state_keys::kIsProcessed] = 0.0f;
  comp->internalStates[state_keys::kIsManualDrag] = 0.0f;
  comp->internalStates[state_keys::kIsContacted] = 0.0f;
  comp->internalStates[state_keys::kIsStuckBox] = 0.0f;
  comp->internalStates[state_keys::kVelX] = 0.0f;
  comp->internalStates[state_keys::kVelY] = 0.0f;
}

const ComponentDefinition kDefinitionMetal = {
    ComponentType::WORKPIECE_METAL,
    "component.workpiece_metal.name",
    "component.workpiece_metal.desc",
    {30.0f, 30.0f},
    nullptr,
    0,
    &RenderWorkpiece,
    &InitMetalWorkpieceDefaults,
    ComponentCategory::BOTH,
    "icon.workpiece_metal",
    "Metal",
};

const ComponentDefinition kDefinitionNonmetal = {
    ComponentType::WORKPIECE_NONMETAL,
    "component.workpiece_nonmetal.name",
    "component.workpiece_nonmetal.desc",
    {30.0f, 30.0f},
    nullptr,
    0,
    &RenderWorkpiece,
    &InitNonmetalWorkpieceDefaults,
    ComponentCategory::BOTH,
    "icon.workpiece_nonmetal",
    "Non-metal",
};

}  // namespace

const ComponentDefinition* GetWorkpieceMetalDefinition() {
  return &kDefinitionMetal;
}

const ComponentDefinition* GetWorkpieceNonmetalDefinition() {
  return &kDefinitionNonmetal;
}

}  // namespace plc
