// win32_input.h
//
// Win32 input handling helpers.

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_WIN32_INPUT_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_WIN32_INPUT_H_

struct GLFWwindow;

namespace plc {

class Application;

// Installs a Win32 WndProc hook for pointer/pen/touch handling.
void InstallWin32InputHook(GLFWwindow* window, Application* app);

// Restores the previous Win32 WndProc hook (if installed).
void UninstallWin32InputHook(GLFWwindow* window);

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_WIN32_INPUT_H_
