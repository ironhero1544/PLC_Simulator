// input_handler.cpp
// Input handling implementation
// 입력 처리 구현

#include "plc_emulator/core/input_handler.h"

#include <GLFW/glfw3.h>

namespace plc {

InputHandler::InputHandler() : window_(nullptr) {}

InputHandler::~InputHandler() {}

void InputHandler::Initialize(GLFWwindow* window) {
  window_ = window;
}

void InputHandler::ProcessInput() {
  // Input processing is mainly handled by ImGui
  // Additional custom input processing can be added here
  // 입력 처리는 주로 ImGui가 처리함
  // 여기에 추가적인 커스텀 입력 처리 추가 가능
}

bool InputHandler::IsKeyPressed(int key) const {
  if (!window_) {
    return false;
  }
  return glfwGetKey(window_, key) == GLFW_PRESS;
}

bool InputHandler::IsMouseButtonPressed(int button) const {
  if (!window_) {
    return false;
  }
  return glfwGetMouseButton(window_, button) == GLFW_PRESS;
}

void InputHandler::GetMousePosition(double* x, double* y) const {
  if (!window_) {
    if (x) *x = 0.0;
    if (y) *y = 0.0;
    return;
  }
  glfwGetCursorPos(window_, x, y);
}

}  // namespace plc
