#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_COMPONENT_BEHAVIOR_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_COMPONENT_BEHAVIOR_H_

#include "plc_emulator/core/data_types.h"

struct ImGuiIO;
struct ImVec2;

namespace plc {

struct ComponentBehavior {
  bool (*OnMouseDown)(PlacedComponent* comp, ImVec2 world_pos, int button);
  bool (*OnMouseUp)(PlacedComponent* comp, ImVec2 world_pos, int button);
  bool (*OnDoubleClick)(PlacedComponent* comp, ImVec2 world_pos, int button);
  bool (*OnMouseWheel)(PlacedComponent* comp, ImVec2 world_pos,
                       const ImGuiIO& io);
  bool (*BuildTooltip)(const PlacedComponent& comp, ImVec2 world_pos);
};

const ComponentBehavior* GetComponentBehavior(ComponentType type);

}  /* namespace plc */

#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_COMPONENT_BEHAVIOR_H_ */
