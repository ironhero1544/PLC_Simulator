// app_input.cpp
//
// Application input handling.

#include "plc_emulator/core/application.h"

#include "imgui.h"
#include "plc_emulator/programming/programming_mode.h"

#include <GLFW/glfw3.h>

namespace plc {

void Application::ProcessInput() {
  glfwPollEvents();

  ImGuiIO& io = ImGui::GetIO();

  if (io.WantCaptureKeyboard)
    return;

  // '?' is typically Shift + '/', and we also accept Ctrl + '/'.
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Slash, false)) {
    show_shortcut_help_popup_ = true;
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
      if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
        programming_mode_->HandleKeyboardInput(ImGuiKey_LeftArrow);
      if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
        programming_mode_->HandleKeyboardInput(ImGuiKey_RightArrow);
      if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
        programming_mode_->HandleKeyboardInput(ImGuiKey_UpArrow);
      if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
        programming_mode_->HandleKeyboardInput(ImGuiKey_DownArrow);
    }
  } else {
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

    static bool deleteKeyPressed = false;

    bool deleteKeyDown = (glfwGetKey(window_, GLFW_KEY_DELETE) == GLFW_PRESS);

    if (deleteKeyDown && !deleteKeyPressed) {
      if (selected_component_id_ != -1)
        DeleteSelectedComponent();
      if (selected_wire_id_ != -1)
        DeleteWire(selected_wire_id_);
    }

    deleteKeyPressed = deleteKeyDown;

    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
      if (io.KeyShift) {
        RotateSelectedComponent(-1);
      } else {
        RotateSelectedComponent(1);
      }
    }
  }
}

void Application::RegisterWin32RightClick() {
  win32_right_click_ = true;
}

void Application::RegisterWin32SideClick() {
  win32_side_click_ = true;
}

void Application::RegisterWin32SideDown(bool is_down) {
  win32_side_down_ = is_down;
}

}  // namespace plc
