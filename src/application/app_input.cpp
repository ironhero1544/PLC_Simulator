// app_input.cpp
//
// Application input handling.

#include "plc_emulator/core/application.h"

#include "imgui.h"
#include "plc_emulator/programming/programming_mode.h"

#include <GLFW/glfw3.h>

namespace plc {

void Application::ProcessInput() {
  platform_input_collector_.BeginFrame();
  glfwPollEvents();
}

void Application::ProcessFrameKeyboardInput() {
  ImGuiIO& io = ImGui::GetIO();

  // '?' is typically Shift + '/', and we also accept Ctrl + '/'.
  if (!io.WantTextInput && io.KeyCtrl &&
      ImGui::IsKeyPressed(ImGuiKey_Slash, false)) {
    show_shortcut_help_popup_ = true;
  }

  if (!io.WantTextInput && io.KeyCtrl &&
      ImGui::IsKeyPressed(ImGuiKey_S, false) &&
      (current_mode_ == Mode::PROGRAMMING || current_mode_ == Mode::WIRING)) {
    PromptSaveProjectPackageDialog();
    return;
  }

  if (current_mode_ == Mode::PROGRAMMING) {
    if (programming_mode_) {
      if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        programming_mode_->HandleKeyboardInput(ImGuiKey_Escape);
      if (ImGui::IsKeyPressed(ImGuiKey_F5, false))
        programming_mode_->HandleKeyboardInput(ImGuiKey_F5);
      if (ImGui::IsKeyPressed(ImGuiKey_F6, false))
        programming_mode_->HandleKeyboardInput(ImGuiKey_F6);
      if (ImGui::IsKeyPressed(ImGuiKey_F7, false))
        programming_mode_->HandleKeyboardInput(ImGuiKey_F7);
      if (ImGui::IsKeyPressed(ImGuiKey_F9, false))
        programming_mode_->HandleKeyboardInput(ImGuiKey_F9);
      if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
        programming_mode_->HandleKeyboardInput(ImGuiKey_F2);
      if (ImGui::IsKeyPressed(ImGuiKey_F3, false) && !is_plc_running_)
        programming_mode_->HandleKeyboardInput(ImGuiKey_F3);
      if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        programming_mode_->HandleKeyboardInput(ImGuiKey_Delete);
      if (ImGui::IsKeyPressed(ImGuiKey_Insert, false))
        programming_mode_->HandleKeyboardInput(ImGuiKey_Insert);
      if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false))
        programming_mode_->HandleKeyboardInput(ImGuiKey_GraveAccent);
      if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
          ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
        programming_mode_->HandleKeyboardInput(ImGuiKey_Enter);
      }
      if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
        programming_mode_->HandleKeyboardInput(ImGuiKey_Z);
      if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
        programming_mode_->HandleKeyboardInput(ImGuiKey_Y);
      if (ImGui::IsKeyPressed(ImGuiKey_C, false))
        programming_mode_->HandleKeyboardInput(ImGuiKey_C);
      if (ImGui::IsKeyPressed(ImGuiKey_V, false))
        programming_mode_->HandleKeyboardInput(ImGuiKey_V);
      if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
        programming_mode_->HandleKeyboardInput(ImGuiKey_LeftArrow);
      if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
        programming_mode_->HandleKeyboardInput(ImGuiKey_RightArrow);
      if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
        programming_mode_->HandleKeyboardInput(ImGuiKey_UpArrow);
      if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
        programming_mode_->HandleKeyboardInput(ImGuiKey_DownArrow);
    }
    return;
  }

  if (io.WantCaptureKeyboard) {
    return;
  }

  if (!io.KeyCtrl && !io.KeyAlt &&
      ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
    switch (current_tool_) {
      case ToolType::SELECT:
        current_tool_ = ToolType::PNEUMATIC;
        break;
      case ToolType::PNEUMATIC:
        current_tool_ = ToolType::ELECTRIC;
        break;
      case ToolType::ELECTRIC:
        current_tool_ = ToolType::TAG;
        break;
      case ToolType::TAG:
      default:
        current_tool_ = ToolType::SELECT;
        break;
    }
  }

  if (io.KeyCtrl) {
    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
      if (io.KeyShift) {
        RedoWiringState();
      } else {
        UndoWiringState();
      }
    } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
      RedoWiringState();
    }
  }

  static bool delete_key_pressed = false;
  const bool delete_key_down =
      glfwGetKey(window_, GLFW_KEY_DELETE) == GLFW_PRESS;
  if (delete_key_down && !delete_key_pressed) {
    if (selected_component_id_ != -1 || HasSelectedComponents()) {
      DeleteSelectedComponent();
    }
    if (selected_wire_id_ != -1 || HasSelectedWires()) {
      DeleteSelectedWires();
    }
  }
  delete_key_pressed = delete_key_down;

  if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
    if (io.KeyShift) {
      RotateSelectedComponent(-1);
    } else {
      RotateSelectedComponent(1);
    }
  }
}

void Application::RegisterWin32RightClick() {
  platform_input_collector_.RegisterWin32RightClick();
}

void Application::RegisterWin32SideClick() {
  platform_input_collector_.RegisterWin32SideClick();
}

void Application::RegisterWin32SideDown(bool is_down) {
  platform_input_collector_.RegisterWin32SideDown(is_down);
}

}  // namespace plc
