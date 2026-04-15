#include "plc_emulator/core/win32_haptics.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <iostream>

namespace plc {
    namespace {

        using InitFn = void(*)();
        using ClickFn = void(*)();
        using ShutdownFn = void(*)();

        HMODULE g_haptics_dll = nullptr;
        InitFn g_init = nullptr;
        ClickFn g_click = nullptr;
        ShutdownFn g_shutdown = nullptr;
        bool g_attempted_load = false;

        void LoadBridgeIfNeeded() {
            if (g_attempted_load) {
                return;
            }
            g_attempted_load = true;

            g_haptics_dll = LoadLibraryA("haptics_bridge.dll");
            if (!g_haptics_dll) {
                std::cout << "[HapticsStub] LoadLibrary failed for haptics_bridge.dll\n";
                return;
            }

            g_init =
                reinterpret_cast<InitFn>(GetProcAddress(g_haptics_dll, "Haptics_Init"));
            g_click =
                reinterpret_cast<ClickFn>(GetProcAddress(g_haptics_dll, "Haptics_Click"));
            g_shutdown = reinterpret_cast<ShutdownFn>(
                GetProcAddress(g_haptics_dll, "Haptics_Shutdown"));

            std::cout << "[HapticsStub] DLL loaded\n";
            std::cout << "[HapticsStub] init=" << (g_init ? 1 : 0)
                      << " click=" << (g_click ? 1 : 0)
                      << " shutdown=" << (g_shutdown ? 1 : 0) << "\n";
        }

    }  // namespace

    void InitTouchpadHaptics() {
        LoadBridgeIfNeeded();
        if (g_init) {
            g_init();
        } else {
            std::cout << "[HapticsStub] Haptics_Init unavailable\n";
        }
    }

    void TriggerTouchpadHapticClick() {
        LoadBridgeIfNeeded();
        if (g_click) {
            g_click();
        } else {
            std::cout << "[HapticsStub] Haptics_Click unavailable\n";
        }
    }

    void ShutdownTouchpadHaptics() {
        if (g_shutdown) {
            g_shutdown();
        }

        if (g_haptics_dll) {
            FreeLibrary(g_haptics_dll);
            g_haptics_dll = nullptr;
        }

        g_init = nullptr;
        g_click = nullptr;
        g_shutdown = nullptr;
        g_attempted_load = false;
    }

}  // namespace plc

#else

namespace plc {
    void InitTouchpadHaptics() {}
    void TriggerTouchpadHapticClick() {}
    void ShutdownTouchpadHaptics() {}
}

#endif