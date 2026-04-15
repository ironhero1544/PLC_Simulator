#include "plc_emulator/core/haptics_bridge_api.h"

#include <iostream>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Haptics.h>

using namespace winrt;
using namespace winrt::Windows::Devices::Haptics;

namespace {

InputHapticsManager g_manager{nullptr};
SimpleHapticsController g_controller{nullptr};
SimpleHapticsControllerFeedback g_waveform{nullptr};
bool g_initialized = false;

bool IsPreferredWaveform(uint16_t id) {
  return id == KnownSimpleHapticsControllerWaveforms::Press() ||
         id == KnownSimpleHapticsControllerWaveforms::Release() ||
         id == KnownSimpleHapticsControllerWaveforms::Click() ||
         id == KnownSimpleHapticsControllerWaveforms::Hover() ||
         id == KnownSimpleHapticsControllerWaveforms::Success() ||
         id == KnownSimpleHapticsControllerWaveforms::Error();
}

void RefreshControllerFromCurrentInputDevice() {
  g_controller = nullptr;
  g_waveform = nullptr;

  if (!g_manager) {
    std::cout << "[HapticsDLL] no InputHapticsManager\n";
    return;
  }

  auto controller = g_manager.CurrentHapticsController();
  if (!controller) {
    std::cout << "[HapticsDLL] CurrentHapticsController is null\n";
    return;
  }

  auto feedbacks = controller.SupportedFeedback();
  std::cout << "[HapticsDLL] feedback count = " << feedbacks.Size() << "\n";

  for (auto const& feedback : feedbacks) {
    const uint16_t id = feedback.Waveform();
    std::cout << "[HapticsDLL] waveform id = 0x" << std::hex << id
              << std::dec << "\n";

    if (IsPreferredWaveform(id)) {
      g_controller = controller;
      g_waveform = feedback;
      std::cout << "[HapticsDLL] selected waveform id = 0x" << std::hex << id
                << std::dec << "\n";
      return;
    }
  }

  std::cout << "[HapticsDLL] no preferred waveform found\n";
}

}  // namespace

extern "C" HAPTICS_BRIDGE_API void Haptics_Init() {
  std::cout << "[HapticsDLL] Haptics_Init called\n";

  if (g_initialized) {
    std::cout << "[HapticsDLL] already initialized\n";
    return;
  }
  g_initialized = true;

  try {
    init_apartment(apartment_type::single_threaded);
    std::cout << "[HapticsDLL] init_apartment ok\n";

    if (!InputHapticsManager::IsSupported()) {
      std::cout << "[HapticsDLL] InputHapticsManager not supported\n";
      return;
    }

    g_manager = InputHapticsManager::GetForCurrentThread();
    if (!g_manager) {
      std::cout << "[HapticsDLL] GetForCurrentThread returned null\n";
      return;
    }

    std::cout << "[HapticsDLL] InputHapticsManager acquired\n";
    RefreshControllerFromCurrentInputDevice();
  } catch (const winrt::hresult_error& e) {
    std::wcerr << L"[HapticsDLL] init hresult_error: "
               << e.message().c_str() << L"\n";
  } catch (...) {
    std::cout << "[HapticsDLL] init unknown exception\n";
  }
}

extern "C" HAPTICS_BRIDGE_API void Haptics_Click() {
  std::cout << "[HapticsDLL] Haptics_Click called\n";

  try {
    if (!g_manager) {
      std::cout << "[HapticsDLL] manager null\n";
      return;
    }

    // 입력 장치가 바뀔 수 있으니 매번 새로 확인
    RefreshControllerFromCurrentInputDevice();

    if (!g_controller || !g_waveform) {
      std::cout << "[HapticsDLL] controller/waveform null\n";
      return;
    }

    g_controller.SendHapticFeedback(g_waveform);
    std::cout << "[HapticsDLL] feedback sent\n";
  } catch (const winrt::hresult_error& e) {
    std::wcerr << L"[HapticsDLL] click hresult_error: "
               << e.message().c_str() << L"\n";
  } catch (...) {
    std::cout << "[HapticsDLL] click unknown exception\n";
  }
}

extern "C" HAPTICS_BRIDGE_API void Haptics_Shutdown() {
  std::cout << "[HapticsDLL] Haptics_Shutdown called\n";
  g_waveform = nullptr;
  g_controller = nullptr;
  g_manager = nullptr;
  g_initialized = false;
}