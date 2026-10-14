#pragma once

#include "plc_emulator/core/data_types.h"

#include <array>
#include <cmath>

namespace plc_emulator {

struct DcDriverOutput {
  bool powered = false;
  bool running = false;
  bool reverse = false;
  bool braking = false;
  bool fault = false;
  float motor_plus = 0;
  float motor_minus = 0;
};

inline DcDriverOutput ResolveDcDriverVoltages(
    const std::array<float, 6>& voltages, double duty_ratio, bool pwm_connected,
    bool active_low) {
  const auto voltage = [&](int port) {
    return std::isfinite(voltages[port]) ? voltages[port] : -1.0f;
  };
  const auto active = [&](int port) {
    return voltage(port) >= 0 &&
           (active_low ? voltage(port) <= 2 : voltage(port) >= 12);
  };
  DcDriverOutput output;
  output.powered = voltage(0) >= 22 && voltage(0) <= 26 && voltage(1) >= 0 &&
                   voltage(1) <= 2;
  output.reverse = active(3);
  output.braking = active(4);
  output.fault = output.powered && active(2) &&
                 (!pwm_connected || !std::isfinite(duty_ratio) ||
                  duty_ratio < 0 || duty_ratio > 1);
  output.running =
      output.powered && active(2) && !output.braking && !output.fault;
  const float drive = output.running ? static_cast<float>(duty_ratio * 24) : 0;
  output.motor_plus = output.reverse ? 0 : drive;
  output.motor_minus = output.reverse ? drive : 0;
  return output;
}

}  // namespace plc_emulator
