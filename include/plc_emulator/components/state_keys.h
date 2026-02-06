#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_STATE_KEYS_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_STATE_KEYS_H_

namespace plc {
namespace state_keys {
constexpr const char* kPosition = "position";
constexpr const char* kVelocity = "velocity";
constexpr const char* kPressureA = "pressure_a";
constexpr const char* kPressureB = "pressure_b";
constexpr const char* kStatus = "status";
constexpr const char* kIsPressed = "is_pressed";
constexpr const char* kIsPressedManual = "is_pressed_manual";
constexpr const char* kIsDetected = "is_detected";
constexpr const char* kIsPowered = "is_powered";
constexpr const char* kAirPressure = "air_pressure";
constexpr const char* kSolenoidAActive = "solenoid_a_active";
constexpr const char* kSolenoidBActive = "solenoid_b_active";
constexpr const char* kLastActiveSolenoid = "last_active_solenoid";
constexpr const char* kLampOnPrefix = "lamp_on_";
constexpr const char* kPressedPrefix = "is_pressed_";
constexpr const char* kIsMetal = "is_metal";
constexpr const char* kIsProcessed = "is_processed";
constexpr const char* kMotorActive = "motor_active";
constexpr const char* kMotorOn = "motor_on";
constexpr const char* kRotationAngle = "rot_angle";
constexpr const char* kFlowSetting = "flow_setting";
constexpr const char* kMeterMode = "meter_mode";
constexpr const char* kMeterMenuOpen = "meter_menu_open";
constexpr const char* kUserText = "user_text";
constexpr const char* kLampRed = "lamp_r";
constexpr const char* kLampYellow = "lamp_y";
constexpr const char* kLampGreen = "lamp_g";
constexpr const char* kIsManualDrag = "is_manual_drag";
constexpr const char* kIsContacted = "is_contacted";
constexpr const char* kIsStuckBox = "is_stuck_box";
constexpr const char* kVelX = "vel_x";
constexpr const char* kVelY = "vel_y";
constexpr const char* kPlcRunning = "is_running";
constexpr const char* kPlcError = "plc_error";
constexpr const char* kPlcXPrefix = "plc_x_";
constexpr const char* kPlcYPrefix = "plc_y_";
}
}  /* namespace plc */

#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_STATE_KEYS_H_ */
