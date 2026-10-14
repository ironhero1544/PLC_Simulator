#pragma once

#include "plc_emulator/core/data_types.h"

namespace plc_emulator {

inline bool HasSingleConnection(const plc::Port& port) {
  return port.role == "DC_PWM" || port.role == "DC_DRIVE_PLUS" || port.role == "DC_DRIVE_MINUS" || port.role == "HMI_COM" || port.role == "PLC_COM" || port.type == plc::PortType::kOptical || port.role == "ENCODER" ||
      port.role == "SERVO_U" || port.role == "SERVO_V" ||
      port.role == "SERVO_W" || port.role == "PULSE" ||
      (port.isInput && port.role.starts_with("AC_"));
}

// Port roles are authoritative; drawing color and click direction are not.
inline bool CanConnectPorts(const plc::Port& first, const plc::Port& second) {
  if (first.type != second.type) {
    return false;
  }
  if (first.role == "DC_PWM" || second.role == "DC_PWM") {
    const auto& output = first.role == "DC_PWM" ? second : first;
    return !output.isInput && (output.role == "Y0" || output.role == "Y1" ||
                               output.role == "Y2" || output.role == "Y3");
  }
  if (first.role.starts_with("DC_DRIVE_") ||
      second.role.starts_with("DC_DRIVE_")) {
    const auto& drive = first.role.starts_with("DC_DRIVE_") ? first : second;
    const auto& motor = first.role.starts_with("DC_DRIVE_") ? second : first;
    return !drive.isInput && motor.isInput &&
           ((drive.role == "DC_DRIVE_PLUS" && motor.role == "DC_MOTOR_PLUS") ||
            (drive.role == "DC_DRIVE_MINUS" && motor.role == "DC_MOTOR_MINUS"));
  }
  if (first.role == "HMI_COM" || second.role == "HMI_COM" ||
      first.role == "PLC_COM" || second.role == "PLC_COM") {
    return (first.role == "HMI_COM" && second.role == "PLC_COM") ||
           (second.role == "HMI_COM" && first.role == "PLC_COM");
  }
  const auto special = [](const std::string& role) {
    return role.starts_with("AC_") || role == "SERVO_U" ||
        role == "SERVO_V" || role == "SERVO_W" ||
        role == "PROTECTIVE_EARTH" || role == "ENCODER";
  };
  if (special(first.role) || special(second.role)) {
    if (first.role != second.role) return false;
    if (first.role == "PROTECTIVE_EARTH") return true;
    return first.isInput != second.isInput;
  }
  if (first.role == "PULSE" || second.role == "PULSE") {
    const auto& output = first.role == "PULSE" ? second : first;
    return !output.isInput && (output.role == "Y0" || output.role == "Y1" ||
                               output.role == "Y2" || output.role == "Y3");
  }
  if (first.type == plc::PortType::PNEUMATIC) {
    const bool first_vacuum = first.role == "PNEUMATIC_VACUUM";
    const bool second_vacuum = second.role == "PNEUMATIC_VACUUM";
    if (first_vacuum || second_vacuum) {
      return first_vacuum && second_vacuum && first.isInput != second.isInput;
    }
  }
  if (first.type != plc::PortType::kOptical) {
    return true;
  }
  const auto is_pair = [&](const char* amplifier, const char* head) {
    return (first.role == amplifier && second.role == head) ||
           (second.role == amplifier && first.role == head);
  };
  return is_pair("OPTICAL_TX", "OPTICAL_TX_HEAD") ||
         is_pair("OPTICAL_RX", "OPTICAL_RX_HEAD");
}

}  // namespace plc_emulator
