// render_manager.cpp
// Implementation of render manager.

// render_manager.cpp
// Rendering management implementation
// 렌더링 관리 구현

#include "plc_emulator/core/render_manager.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace plc {

RenderManager::RenderManager() : initialized_(false) {}

RenderManager::~RenderManager() {
  Shutdown();
}

bool RenderManager::Initialize(GLFWwindow* window) {
  if (!window) {
    return false;
  }

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  initialized_ = true;
  return true;
}

void RenderManager::Shutdown() {
  if (initialized_) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
  }
}

void RenderManager::BeginFrame() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void RenderManager::EndFrame() {
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void RenderManager::SetupCustomStyle() {
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 8.0f;
  style.ChildRounding = 6.0f;
  style.FrameRounding = 4.0f;
  style.PopupRounding = 4.0f;
  style.ScrollbarRounding = 4.0f;
  style.GrabRounding = 4.0f;
  style.TabRounding = 4.0f;
  style.WindowPadding = ImVec2(12, 8);
  style.FramePadding = ImVec2(8, 4);
  style.ItemSpacing = ImVec2(8, 4);
  style.ItemInnerSpacing = ImVec2(4, 4);
  style.ScrollbarSize = 16.0f;

  ImVec4* colors = style.Colors;
  colors[ImGuiCol_Text] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
  colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
  colors[ImGuiCol_Border] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.90f, 0.90f, 0.90f, 0.75f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
  colors[ImGuiCol_Separator] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
  colors[ImGuiCol_SeparatorActive] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
}

}  // namespace plc
