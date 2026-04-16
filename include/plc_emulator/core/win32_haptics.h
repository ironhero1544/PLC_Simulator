// win32_haptics.h
// Windows Precision Touchpad haptic feedback helper.
// Wraps the WinRT Windows.Devices.Haptics API.
// Falls back to a no-op on unsupported hardware / OS versions.

#pragma once
#ifdef _WIN32

namespace plc {

// Call once during app startup (from the main thread).
void InitTouchpadHaptics();

// Fire a single "click" sensation on the precision touchpad actuator.
// Safe to call from any thread; no-op if haptics are unavailable.
void TriggerTouchpadHapticClick();

// Call at app shutdown to release WinRT resources.
void ShutdownTouchpadHaptics();

}  // namespace plc

#endif  // _WIN32
