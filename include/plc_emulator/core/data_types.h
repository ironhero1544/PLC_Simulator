/*
 * data_types.h
 *
 * 공통 데이터 타입과 구조체 정의.
 * Common data types and structures.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_DATA_TYPES_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_DATA_TYPES_H_

#include <map>
#include <string>
#include <vector>

struct ImVec2;

namespace plc {

struct Position {
  float x = 0.0f;
  float y = 0.0f;
  Position() = default;
  Position(float px, float py) : x(px), y(py) {}
  Position(const ImVec2& vec);
  operator ImVec2() const;
};

struct Size {
  float width = 0.0f;
  float height = 0.0f;
};

struct Color {
  float r = 1.0f;
  float g = 1.0f;
  float b = 1.0f;
  float a = 1.0f;
};

enum class Mode { WIRING, PROGRAMMING };
enum class PlcInputMode { SINK, SOURCE };
enum class PlcOutputMode { SINK, SOURCE };
enum class SensorOutputMode { PNP, NPN };

enum class ComponentType {
  PLC,
  FRL,
  MANIFOLD,
  LIMIT_SWITCH,
  SENSOR,
  CYLINDER,
  VALVE_SINGLE,
  VALVE_DOUBLE,
  BUTTON_UNIT,
  POWER_SUPPLY,
  WORKPIECE_METAL,
  WORKPIECE_NONMETAL,
  RING_SENSOR,
  METER_VALVE,
  INDUCTIVE_SENSOR,
  CONVEYOR,
  PROCESSING_CYLINDER,
  BOX,
  TOWER_LAMP,
  EMERGENCY_STOP
};

enum class ToolType { SELECT, TAG, PNEUMATIC, ELECTRIC };
enum class PortType { ELECTRIC, PNEUMATIC };
enum class WireEditMode { NONE, MOVING_POINT };

struct PlacedComponent {
  int instanceId = 0;
  ComponentType type;
  Position position;
  Size size;
  bool selected = false;
  float rotation = 0.0f;
  int z_order = 0;
  int rotation_quadrants = 0;
  bool flip_x = false;
  bool flip_y = false;
  std::map<std::string, float> internalStates;
};

struct Port {
  int id = 0;
  Position relativePos;
  Color color;
  bool isInput = true;
  PortType type;
  std::string role;
};

struct Wire {
  int id = 0;
  int fromComponentId = 0;
  int fromPortId = 0;
  int toComponentId = 0;
  int toPortId = 0;
  std::vector<Position> wayPoints;
  bool selected = false;
  bool isElectric;
  Color color;
  float thickness;
  bool isTagged = false;
  std::string tagText;
  int tagColorIndex = 0;
};

struct SnapSettings {
  bool enableGridSnap = true;
  bool enablePortSnap = true;
  bool enableVerticalSnap = true;
  bool enableHorizontalSnap = true;
  float snapDistance = 15.0f;
  float gridSize = 25.0f;
};

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_DATA_TYPES_H_ */
