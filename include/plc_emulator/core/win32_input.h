/*
 * win32_input.h
 *
 * Win32 입력 훅 유틸리티 선언.
 * Declarations for Win32 input hook helpers.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_WIN32_INPUT_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_WIN32_INPUT_H_

struct GLFWwindow;

namespace plc {

class Application;

/*
 * 포인터/펜/터치 입력을 위한 WndProc 훅을 설치합니다.
 * Installs the WndProc hook for pointer/pen/touch input.
 */
void InstallWin32InputHook(GLFWwindow* window, Application* app);

/*
 * 설치된 WndProc 훅을 제거합니다.
 * Removes the installed WndProc hook.
 */
void UninstallWin32InputHook(GLFWwindow* window);

}  /* namespace plc */

#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_WIN32_INPUT_H_ */
