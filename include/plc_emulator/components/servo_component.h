#pragma once

#include "plc_emulator/core/data_types.h"

#include <cmath>

namespace plc_emulator {
inline bool IsValidAxisSettings(const plc::AxisSettings& settings) {
  return std::isfinite(settings.pulses_per_turn) &&
         std::isfinite(settings.lead_mm) && std::isfinite(settings.stroke_mm) &&
         std::isfinite(settings.max_speed) &&
         std::isfinite(settings.acceleration) && settings.pulses_per_turn > 0 &&
         settings.pulses_per_turn <= 1e9 && settings.lead_mm > 0 &&
         settings.lead_mm <= 1e6 && settings.stroke_mm > 0 &&
         settings.stroke_mm <= 1e6 && settings.max_speed > 0 &&
         settings.max_speed <= 10000 && settings.acceleration > 0 &&
         settings.acceleration <= 1e6;
}
constexpr bool IsServoLift(plc::ComponentType type) {
  return type == plc::ComponentType::kServoLift ||
         type == plc::ComponentType::kLiftSuction;
}
constexpr bool IsServoLoad(plc::ComponentType type) {
  return type == plc::ComponentType::kServoMotor || IsServoLift(type);
}
constexpr bool IsServoSystem(plc::ComponentType type) {
  return IsServoLoad(type) || type == plc::ComponentType::kServoAmplifier ||
         type == plc::ComponentType::kDcMotor ||
         type == plc::ComponentType::kAcSupply;
}
constexpr int ServoPortBase(plc::ComponentType type) {
  return type == plc::ComponentType::kLiftSuction ? 10 : 0;
}
}  // namespace plc_emulator
