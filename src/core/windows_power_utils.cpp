#include "plc_emulator/core/windows_power_utils.h"

#include <cstring>
#include <limits>

namespace plc {

namespace {

#ifdef _WIN32
constexpr DWORD kPowerCheckIntervalMs = 10000;

struct WindowsPowerStateCache {
  bool initialized = false;
  DWORD lastRefreshTick = 0;
  DWORD stateMask = 0;
  DWORD currentProcessMask = std::numeric_limits<DWORD>::max();
  DWORD currentThreadMask = std::numeric_limits<DWORD>::max();
};

WindowsPowerStateCache& GetWindowsPowerStateCache() {
  static WindowsPowerStateCache cache;
  return cache;
}

bool HasBatteryPowerCapability(const SYSTEM_POWER_STATUS& power_status) {
  if (power_status.BatteryFlag == 128 || power_status.BatteryFlag == 255) {
    return false;
  }
  if (power_status.BatteryLifePercent != 255) {
    return true;
  }
  return (power_status.BatteryFlag & 0x7F) != 0;
}

DWORD QueryWindowsEfficiencyModeStateMask() {
  SYSTEM_POWER_STATUS power_status{};
  if (!GetSystemPowerStatus(&power_status)) {
    return 0;
  }

  const bool has_battery = HasBatteryPowerCapability(power_status);
  if (!has_battery || power_status.ACLineStatus != 0) {
    return 0;
  }

#if defined(PROCESS_POWER_THROTTLING_EXECUTION_SPEED)
  return PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
#else
  return 0;
#endif
}

DWORD RefreshWindowsEfficiencyModeStateMask() {
  WindowsPowerStateCache& cache = GetWindowsPowerStateCache();
  const DWORD now = GetTickCount();
  if (!cache.initialized ||
      now - cache.lastRefreshTick >= kPowerCheckIntervalMs) {
    const DWORD previous_mask = cache.stateMask;
    cache.stateMask = QueryWindowsEfficiencyModeStateMask();
    cache.lastRefreshTick = now;
    cache.initialized = true;
    if (previous_mask != cache.stateMask) {
      cache.currentProcessMask = std::numeric_limits<DWORD>::max();
      cache.currentThreadMask = std::numeric_limits<DWORD>::max();
    }
  }
  return cache.stateMask;
}
#endif

}  // namespace

bool ShouldUseWindowsEfficiencyMode() {
#ifdef _WIN32
  return RefreshWindowsEfficiencyModeStateMask() != 0;
#else
  return false;
#endif
}

#ifdef _WIN32
DWORD GetWindowsEfficiencyModeStateMask() {
  return RefreshWindowsEfficiencyModeStateMask();
}

DWORD GetPowerAwareTimeout(DWORD timeout) {
  if (!ShouldUseWindowsEfficiencyMode()) {
    return timeout;
  }
  if (timeout > (MAXDWORD / 2)) {
    return MAXDWORD;
  }
  return timeout * 2;
}

void ApplyWindowsEfficiencyModeCompatibility(HANDLE process_handle,
                                             HANDLE thread_handle) {
  if (!process_handle && !thread_handle) {
    return;
  }

  WindowsPowerStateCache& cache = GetWindowsPowerStateCache();
  const DWORD state_mask = GetWindowsEfficiencyModeStateMask();

#if defined(PROCESS_POWER_THROTTLING_CURRENT_VERSION) && \
    defined(PROCESS_POWER_THROTTLING_EXECUTION_SPEED)
  if (process_handle) {
    const bool is_current_process = process_handle == GetCurrentProcess();
    if (!(is_current_process && cache.currentProcessMask == state_mask)) {
      using SetProcessInformationFn =
          BOOL(WINAPI*)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);
      static SetProcessInformationFn set_process_information = []() {
        FARPROC proc = GetProcAddress(GetModuleHandleA("kernel32.dll"),
                                      "SetProcessInformation");
        SetProcessInformationFn typed = nullptr;
        std::memcpy(&typed, &proc, sizeof(typed));
        return typed;
      }();
      if (set_process_information) {
        PROCESS_POWER_THROTTLING_STATE throttling_state{};
        throttling_state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
        throttling_state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        throttling_state.StateMask = state_mask;
        set_process_information(process_handle, ProcessPowerThrottling,
                                &throttling_state,
                                static_cast<DWORD>(sizeof(throttling_state)));
        if (is_current_process) {
          cache.currentProcessMask = state_mask;
        }
      }
    }
  }
#endif

#ifdef ThreadPowerThrottling
  constexpr THREAD_INFORMATION_CLASS kThreadPowerThrottlingClass =
      ThreadPowerThrottling;
#else
  constexpr THREAD_INFORMATION_CLASS kThreadPowerThrottlingClass =
      static_cast<THREAD_INFORMATION_CLASS>(24);
#endif

  if (thread_handle) {
    const bool is_current_thread = thread_handle == GetCurrentThread();
    if (!(is_current_thread && cache.currentThreadMask == state_mask)) {
      struct LocalThreadPowerThrottlingState {
        ULONG Version;
        ULONG ControlMask;
        ULONG StateMask;
      };
      using SetThreadInformationFn =
          BOOL(WINAPI*)(HANDLE, THREAD_INFORMATION_CLASS, LPVOID, DWORD);
      static SetThreadInformationFn set_thread_information = []() {
        FARPROC proc = GetProcAddress(GetModuleHandleA("kernel32.dll"),
                                      "SetThreadInformation");
        SetThreadInformationFn typed = nullptr;
        std::memcpy(&typed, &proc, sizeof(typed));
        return typed;
      }();
      if (set_thread_information) {
        LocalThreadPowerThrottlingState throttling_state{};
        throttling_state.Version = 1;
        throttling_state.ControlMask = 0x1;
        throttling_state.StateMask = state_mask == 0 ? 0 : 0x1;
        set_thread_information(thread_handle, kThreadPowerThrottlingClass,
                               &throttling_state,
                               static_cast<DWORD>(sizeof(throttling_state)));
        if (is_current_thread) {
          cache.currentThreadMask = state_mask;
        }
      }
    }
  }
}
#endif

}  // namespace plc
