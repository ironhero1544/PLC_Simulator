/*
 * data_types.h
 *
 * 공통 데이터 타입과 구조체 정의.
 * Common data types and structures.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_DATA_TYPES_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_DATA_TYPES_H_

#include <array>
#include <cstdint>
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
enum class RtlLogicFamily { INDUSTRIAL_24V, CMOS_5V, TTL_5V };

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
  EMERGENCY_STOP,
  RTL_MODULE,
  kSensorCylinder,
  kSuctionCylinder,
  kStopperCylinder,
  kVacuumGenerator,
  kDcMotor,
  kServoMotor,
  kServoAmplifier,
  kServoLift,
  kLiftSuction,
  kAcSupply,
  kWarehouse,
  kFiberAmplifier,
  kFiberTransmitter,
  kFiberReceiver,
  kTouchPanel,
  kDcMotorDriver,
  kMagazine,
  kDrivenWarehouse
};

constexpr bool IsWarehouseType(ComponentType type) {
  return type == ComponentType::kWarehouse ||
         type == ComponentType::kDrivenWarehouse;
}


enum class ToolType { SELECT, TAG, kWire };
enum class PortType { ELECTRIC, PNEUMATIC, kOptical };
enum class WireKind { kElectric, kPneumatic, kOptical };

constexpr WireKind GetWireKind(PortType type) {
  switch (type) {
    case PortType::ELECTRIC:
      return WireKind::kElectric;
    case PortType::PNEUMATIC:
      return WireKind::kPneumatic;
    case PortType::kOptical:
      return WireKind::kOptical;
  }
  return WireKind::kElectric;
}

constexpr Color GetDefaultWireColor(WireKind kind) {
  switch (kind) {
    case WireKind::kElectric:
      return {1.0f, 0.27f, 0.0f, 1.0f};
    case WireKind::kPneumatic:
      return {0.13f, 0.59f, 0.95f, 1.0f};
    case WireKind::kOptical:
      return {0.6f, 0.35f, 0.8f, 1.0f};
  }
  return {};
}
enum class WireEditMode { NONE, MOVING_POINT };

struct Port {
  int id = 0;
  Position relativePos;
  Color color;
  bool isInput = true;
  PortType type;
  std::string role;
};

struct RtlPinBinding {
  std::string pinName;
  int portId = -1;
  bool isInput = true;
};

struct ZLayerConfig {
  float min_layer = 1.0f;
  float max_layer = 1.0f;
  float initial_height = 1.0f;
};

struct ZMotionState {
  float current_height = 1.0f;
};

struct WorkpieceCarryState {
  int owner_id = -1;
  int slot_id = -1;
  int release_source_id = -1;
  Position local_offset;
};

struct AxisSettings {
  double pulses_per_turn = 1000;
  double lead_mm = 5;
  double stroke_mm = 100;
  double max_speed = 10;
  double acceleration = 20;
};

struct AxisDriveState {
  double position = 0;
  double target = 0;
  double velocity = 0;
  uint64_t pulse_forward = 0;
  uint64_t pulse_reverse = 0;
  uint64_t pulse_epoch = 0;
  int pulse_source_id = -1;
  int pulse_output = -1;
  int amplifier_id = -1;
  int load_id = -1;
  int alarm = 0;
  bool pulse_baseline = false;
  bool ready = false;
  bool in_position = false;
};

struct FiberSettings {
  double max_distance = 1000;
  double alignment_tolerance = 3;
  double beam_width = 2;
  bool dark_on = true;
};
struct FiberState {
  int fault = 1;
  bool blocked = false;
  bool output = false;
  float received = 0;
};
constexpr size_t kMaxHmiWidgets = 32;
constexpr float kHmiScreenWidth = 288;
constexpr float kHmiScreenHeight = 216;
enum class HmiWidgetType { kButton, kLamp, kNumber, kText, kMessage };
struct HmiWidget {
  HmiWidgetType type = HmiWidgetType::kButton;
  std::string text = "Button";
  std::string address = "M100";
  Position position = {12, 38};
  Size size = {80, 38};
  bool momentary = true;
  std::string unit = "";
  int decimals = 0;
  float font_size = 14;
  uint32_t on_color = 0xff50c850;
  int page = 0;
  // -1 writes the PLC bit; a nonnegative value navigates to that page.
  int target_page = -1;
  std::string visibility_address;
  std::array<std::string, 3> messages = {"Stopped", "Running", "Paused"};
};
struct HmiSettings {
  std::vector<std::string> pages = {"Screen 1"};
  std::vector<HmiWidget> widgets = {
      {HmiWidgetType::kButton, "FWD", "M100", {12, 60}, {80, 38}},
      {HmiWidgetType::kButton, "REV", "M101", {104, 60}, {80, 38}},
      {HmiWidgetType::kButton, "HOME", "M102", {196, 60}, {80, 38}},
      {HmiWidgetType::kNumber, "", "D100", {66, 125}, {156, 46}}};
};
struct HmiState {
  int active_page = 0;
  bool online = false;
  bool value_valid = false;
  int plc_id = -1;
  int32_t display_value = 0;
  std::array<bool, kMaxHmiWidgets> held = {};
  std::array<bool, kMaxHmiWidgets> hidden = {};
  std::array<bool, kMaxHmiWidgets> values = {};
  std::array<int32_t, kMaxHmiWidgets> numbers = {};
  std::array<bool, kMaxHmiWidgets> number_valid = {};
};
struct PlacedComponent {
  int instanceId = 0;
  ComponentType type;
  Position position;
  Size size;
  bool selected = false;
  float rotation = 0.0f;
  int z_order = 0;
  float render_opacity = 1.0f;
  bool render_hidden = false;
  std::string render_group;
  ZLayerConfig z_config;
  ZMotionState z_state;
  WorkpieceCarryState carry_state;
  FiberSettings fiber_settings;
  FiberState fiber_state;
  HmiSettings hmi_settings;
  HmiState hmi_state;
  int warehouse_cylinder_id = -1;
  float warehouse_travel = 70;
  Position warehouse_drive_offset = {0, 0};
  AxisSettings axis_settings;
  AxisDriveState axis_state;
  int rotation_quadrants = 0;
  bool flip_x = false;
  bool flip_y = false;
  std::map<std::string, float> internalStates;
  std::string customLabel;
  bool show_custom_label = true;
  std::string rtlModuleId;
  std::string rtlSourceMode;
  std::string rtlOverrideId;
  std::string rtlClockPinName;
  std::string rtlResetPinName;
  std::string rtlPowerPinName;
  std::string rtlGroundPinName;
  RtlLogicFamily rtlLogicFamily = RtlLogicFamily::INDUSTRIAL_24V;
  bool rtlUseInternalClock = false;
  float rtlClockFrequencyHz = 1.0f;
  bool rtlUseStartupReset = false;
  float rtlResetPulseMs = 20.0f;
  bool rtlResetActiveLow = false;
  std::vector<Port> runtimePorts;
  std::vector<RtlPinBinding> rtlPinBindings;
};

struct Wire {
  int id = 0;
  int fromComponentId = 0;
  int fromPortId = 0;
  int toComponentId = 0;
  int toPortId = 0;
  std::vector<Position> wayPoints;
  bool selected = false;
  WireKind kind = WireKind::kElectric;
  Color color;
  float thickness = 2.0f;
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
