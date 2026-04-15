#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_COMPONENT_BEHAVIOR_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_COMPONENT_BEHAVIOR_H_

#include "plc_emulator/core/data_types.h"

#include <vector>

struct ImGuiIO;
struct ImVec2;

namespace plc {

enum class ComponentInteractionCommandType {
  None,
  SetButtonPressed,
  ReleaseButtonMomentary,
  ToggleButtonLatch,
  ToggleEmergencyStop,
  ToggleLimitSwitchManualOverride,
  SetAirPressure,
  SetFlowSetting
};

struct ComponentInteractionCommand {
  ComponentInteractionCommandType type =
      ComponentInteractionCommandType::None;
  int button_index = -1;
  bool pressed = false;
  float scalar = 0.0f;
};

struct ComponentInteractionResult {
  bool handled = false;
  std::vector<ComponentInteractionCommand> commands;
};

struct ComponentBehavior {
  ComponentInteractionResult (*OnMouseDown)(const PlacedComponent& comp,
                                            ImVec2 world_pos,
                                            int button);
  ComponentInteractionResult (*OnMouseUp)(const PlacedComponent& comp,
                                          ImVec2 world_pos,
                                          int button);
  ComponentInteractionResult (*OnDoubleClick)(const PlacedComponent& comp,
                                              ImVec2 world_pos,
                                              int button);
  ComponentInteractionResult (*OnMouseWheel)(const PlacedComponent& comp,
                                             ImVec2 world_pos,
                                             const ImGuiIO& io);
  bool (*BuildTooltip)(const PlacedComponent& comp, ImVec2 world_pos);
};

const ComponentBehavior* GetComponentBehavior(ComponentType type);
void ApplyComponentInteractionResult(PlacedComponent* comp,
                                     const ComponentInteractionResult& result);

}  /* namespace plc */

#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_COMPONENT_BEHAVIOR_H_ */
