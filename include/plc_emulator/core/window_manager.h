// window_manager.h
// Window management for PLC Emulator application
// 윈도우 관리 및 초기화 담당

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_WINDOW_MANAGER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_WINDOW_MANAGER_H_

struct GLFWwindow;

namespace plc {

// Manages GLFW window creation, initialization, and lifecycle
// GLFW 윈도우 생성, 초기화, 생명주기 관리
class WindowManager {
 public:
  WindowManager();
  ~WindowManager();

  WindowManager(const WindowManager&) = delete;
  WindowManager& operator=(const WindowManager&) = delete;

  // Initialize GLFW and create window
  // GLFW 초기화 및 윈도우 생성
  bool Initialize(int width, int height, const char* title);

  // Shutdown and cleanup GLFW resources
  // GLFW 리소스 정리 및 종료
  void Shutdown();

  // Check if window should close
  // 윈도우가 닫혀야 하는지 확인
  bool ShouldClose() const;

  // Swap buffers and poll events
  // 버퍼 교환 및 이벤트 폴링
  void PollEvents();
  void SwapBuffers();

  // Get current window dimensions
  // 현재 윈도우 크기 가져오기
  void GetFramebufferSize(int* width, int* height) const;

  // Access to raw GLFW window pointer
  // GLFW 윈도우 포인터 접근
  GLFWwindow* window() const { return window_; }

 private:
  GLFWwindow* window_;
  int width_;
  int height_;
};

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_WINDOW_MANAGER_H_
