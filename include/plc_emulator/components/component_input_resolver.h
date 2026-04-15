#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_COMPONENT_INPUT_RESOLVER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_COMPONENT_INPUT_RESOLVER_H_

#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/core/data_types.h"

#include <string>

namespace plc {
namespace component_input {

inline bool ReadBinaryState(const PlacedComponent& comp, const char* key) {
  auto it = comp.internalStates.find(key);
  return it != comp.internalStates.end() && it->second > 0.5f;
}

inline void WriteBinaryState(PlacedComponent* comp, const char* key, bool value) {
  if (!comp) {
    return;
  }
  comp->internalStates[key] = value ? 1.0f : 0.0f;
}

inline std::string BuildButtonPressedKey(int button_index) {
  return std::string(state_keys::kPressedPrefix) + std::to_string(button_index);
}

inline bool IsButtonUnitPressed(const PlacedComponent& comp, int button_index) {
  if (button_index < 0 || button_index >= 3) {
    return false;
  }
  const std::string key = BuildButtonPressedKey(button_index);
  return ReadBinaryState(comp, key.c_str());
}

inline void SetButtonUnitPressed(PlacedComponent* comp, int button_index,
                                 bool pressed) {
  if (!comp || button_index < 0 || button_index >= 3) {
    return;
  }
  const std::string key = BuildButtonPressedKey(button_index);
  WriteBinaryState(comp, key.c_str(), pressed);
}

inline void ReleaseButtonUnitMomentaryButtons(PlacedComponent* comp) {
  SetButtonUnitPressed(comp, 0, false);
  SetButtonUnitPressed(comp, 1, false);
}

inline void ToggleButtonUnitLatch(PlacedComponent* comp, int button_index) {
  if (!comp || button_index < 0 || button_index >= 3) {
    return;
  }
  SetButtonUnitPressed(comp, button_index,
                       !IsButtonUnitPressed(*comp, button_index));
}

inline bool IsEmergencyStopPressed(const PlacedComponent& comp) {
  return ReadBinaryState(comp, state_keys::kIsPressed);
}

inline void SetEmergencyStopPressed(PlacedComponent* comp, bool pressed) {
  WriteBinaryState(comp, state_keys::kIsPressed, pressed);
}

inline void ToggleEmergencyStopPressed(PlacedComponent* comp) {
  if (!comp) {
    return;
  }
  SetEmergencyStopPressed(comp, !IsEmergencyStopPressed(*comp));
}

inline bool GetLimitSwitchManualOverride(const PlacedComponent& comp) {
  return ReadBinaryState(comp, state_keys::kIsPressedManual);
}

inline bool GetLimitSwitchPhysicalDetected(const PlacedComponent& comp) {
  return ReadBinaryState(comp, state_keys::kIsDetected);
}

inline bool ResolveLimitSwitchPressedState(const PlacedComponent& comp) {
  const bool has_detected =
      comp.internalStates.count(state_keys::kIsDetected) > 0;
  const bool has_manual =
      comp.internalStates.count(state_keys::kIsPressedManual) > 0;
  if (has_detected || has_manual) {
    return GetLimitSwitchPhysicalDetected(comp) ||
           GetLimitSwitchManualOverride(comp);
  }
  return ReadBinaryState(comp, state_keys::kIsPressed);
}

inline bool IsLimitSwitchPressed(const PlacedComponent& comp) {
  return ResolveLimitSwitchPressedState(comp);
}

inline void SyncLimitSwitchResolvedState(PlacedComponent* comp) {
  if (!comp) {
    return;
  }
  WriteBinaryState(comp, state_keys::kIsPressed,
                   GetLimitSwitchPhysicalDetected(*comp) ||
                       GetLimitSwitchManualOverride(*comp));
}

inline void SetLimitSwitchManualOverride(PlacedComponent* comp, bool active) {
  if (!comp) {
    return;
  }
  WriteBinaryState(comp, state_keys::kIsPressedManual, active);
  SyncLimitSwitchResolvedState(comp);
}

inline void ToggleLimitSwitchManualOverride(PlacedComponent* comp) {
  if (!comp) {
    return;
  }
  SetLimitSwitchManualOverride(comp, !GetLimitSwitchManualOverride(*comp));
}

inline void SetLimitSwitchPhysicalDetected(PlacedComponent* comp, bool detected) {
  if (!comp) {
    return;
  }
  WriteBinaryState(comp, state_keys::kIsDetected, detected);
  SyncLimitSwitchResolvedState(comp);
}

inline bool IsSensorDetected(const PlacedComponent& comp) {
  return ReadBinaryState(comp, state_keys::kIsDetected);
}

inline void SetSensorDetected(PlacedComponent* comp, bool detected) {
  WriteBinaryState(comp, state_keys::kIsDetected, detected);
}

}  // namespace component_input
}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_COMPONENTS_COMPONENT_INPUT_RESOLVER_H_
