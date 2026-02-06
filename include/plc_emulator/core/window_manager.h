/*
 * window_manager.h
 *
 * GLFW 윈도우 생성과 생명주기를 관리합니다.
 * Manages GLFW window creation and lifecycle.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_WINDOW_MANAGER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_WINDOW_MANAGER_H_

struct GLFWwindow;

namespace plc {

/*
 * 윈도우 초기화, 이벤트 처리, 버퍼 교환을 담당합니다.
 * Handles window init, event polling, and buffer swapping.
 */
class WindowManager {
 public:
  WindowManager();
  ~WindowManager();

  WindowManager(const WindowManager&) = delete;
  WindowManager& operator=(const WindowManager&) = delete;

  bool Initialize(int width, int height, const char* title);
  void Shutdown();
  bool ShouldClose() const;
  void PollEvents();
  void SwapBuffers();
  void GetFramebufferSize(int* width, int* height) const;
  GLFWwindow* window() const { return window_; }

 private:
  GLFWwindow* window_;
  int width_;
  int height_;
};

}  /* namespace plc */

#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_WINDOW_MANAGER_H_ */
