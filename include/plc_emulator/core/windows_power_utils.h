#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_WINDOWS_POWER_UTILS_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_WINDOWS_POWER_UTILS_H_

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace plc {

bool ShouldUseWindowsEfficiencyMode();

#ifdef _WIN32
DWORD GetWindowsEfficiencyModeStateMask();
DWORD GetPowerAwareTimeout(DWORD timeout);
void ApplyWindowsEfficiencyModeCompatibility(HANDLE process_handle,
                                             HANDLE thread_handle);
#else
inline unsigned long GetWindowsEfficiencyModeStateMask() { return 0; }
inline unsigned long GetPowerAwareTimeout(unsigned long timeout) {
  return timeout;
}
inline void ApplyWindowsEfficiencyModeCompatibility(void*, void*) {}
#endif

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_WINDOWS_POWER_UTILS_H_
