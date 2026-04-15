#pragma once

#ifdef _WIN32
  #ifdef HAPTICS_BRIDGE_EXPORTS
    #define HAPTICS_BRIDGE_API __declspec(dllexport)
  #else
    #define HAPTICS_BRIDGE_API __declspec(dllimport)
  #endif
#else
  #define HAPTICS_BRIDGE_API
#endif

extern "C" {
HAPTICS_BRIDGE_API void Haptics_Init();
HAPTICS_BRIDGE_API void Haptics_Click();
HAPTICS_BRIDGE_API void Haptics_Shutdown();
}