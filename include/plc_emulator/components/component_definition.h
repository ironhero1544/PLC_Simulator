#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_COMPONENT_DEFINITION_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_COMPONENT_DEFINITION_H_

#include "plc_emulator/core/data_types.h"

struct ImDrawList;
struct ImVec2;

namespace plc {

enum class ComponentCategory {
  ALL,
  AUTOMATION,
  SEMICONDUCTOR,
  BOTH
};

struct ComponentPortDef {
  int id;
  Position rel_pos;
  PortType type;
  bool is_input;
  Color color;
  const char* role;
};

struct ComponentDefinition {
  ComponentType type;
  const char* display_name;
  const char* description;
  Size size;
  const ComponentPortDef* ports;
  int port_count;
  void (*Render)(ImDrawList* draw_list,
                 const PlacedComponent& comp,
                 ImVec2 screen_pos,
                 float zoom);
  void (*InitDefaultState)(PlacedComponent* comp);
  ComponentCategory category;
  const char* icon_id;
  const char* short_name;
};

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_COMPONENT_DEFINITION_H_
