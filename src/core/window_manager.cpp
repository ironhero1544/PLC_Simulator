// window_manager.cpp
//
// Implementation of window manager.

// window_manager.cpp
// Window management implementation
// 윈도우 관리 구현

#include "plc_emulator/core/window_manager.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

namespace plc {

WindowManager::WindowManager() : window_(nullptr), width_(0), height_(0) {}

WindowManager::~WindowManager() {
  Shutdown();
}

bool WindowManager::Initialize(int width, int height, const char* title) {
  width_ = width;
  height_ = height;

  // Initialize GLFW
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW\n";
    return false;
  }

  // Set OpenGL version hints
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  // Create window
  window_ = glfwCreateWindow(width_, height_, title, nullptr, nullptr);
  if (!window_) {
    std::cerr << "Failed to create GLFW window\n";
    glfwTerminate();
    return false;
  }

  glfwMakeContextCurrent(window_);
  glfwSwapInterval(1);  // Enable vsync

  // Load OpenGL function pointers
  if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
    std::cerr << "Failed to initialize GLAD\n";
    glfwDestroyWindow(window_);
    glfwTerminate();
    return false;
  }

  // Set viewport
  glViewport(0, 0, width_, height_);

  return true;
}

void WindowManager::Shutdown() {
  if (window_) {
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }
  glfwTerminate();
}

bool WindowManager::ShouldClose() const {
  return window_ && glfwWindowShouldClose(window_);
}

void WindowManager::PollEvents() {
  glfwPollEvents();
}

void WindowManager::SwapBuffers() {
  if (window_) {
    glfwSwapBuffers(window_);
  }
}

void WindowManager::GetFramebufferSize(int* width, int* height) const {
  if (window_) {
    glfwGetFramebufferSize(window_, width, height);
  }
}

}  // namespace plc
