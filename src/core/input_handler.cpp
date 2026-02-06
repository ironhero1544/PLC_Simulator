// input_handler.cpp
//
// Implementation of input handler.

#include "plc_emulator/core/input_handler.h"

#include <GLFW/glfw3.h>

namespace plc {

InputHandler::InputHandler() : window_(nullptr) {}

InputHandler::~InputHandler() {}

void InputHandler::Initialize(GLFWwindow* window) {
  window_ = window;
}

void InputHandler::ProcessInput() {
  // ImGui handles most input; add custom handling here if needed.
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
