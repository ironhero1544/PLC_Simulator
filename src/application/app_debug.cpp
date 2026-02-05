// app_debug.cpp
//
// Application debug and diagnostics.

#include "plc_emulator/core/application.h"

#include "imgui.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/lang/lang_manager.h"

#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace plc {

void Application::ToggleDebugMode() {
  debug_mode_ = !debug_mode_;

  if (debug_mode_) {
#ifdef _WIN32
    if (!GetConsoleWindow()) {
      AllocConsole();
      freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
      freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
      freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
    }
#endif

    PrintDebugToConsole(
        "[DEBUG MODE ON] Physics Engine Debug System Activated");
    PrintDebugToConsole("Restart without --Debug to disable debug mode");
    PrintDebugToConsole("==========================================");
  } else {
    PrintDebugToConsole("[DEBUG MODE OFF] Debug System Deactivated");
    PrintDebugToConsole("==========================================");
  }
}

void Application::UpdateDebugLogging() {
  debug_update_counter_++;

  if (debug_update_counter_ % 180 == 0) {
    debug_log_buffer_.clear();

    PrintDebugToConsole("========== DEBUG STATUS UPDATE ==========");
    PrintDebugToConsole("Frame: " + std::to_string(debug_update_counter_) +
                        " | PLC: " + (is_plc_running_ ? "RUN" : "STOP"));

    LogComponentStates();

    if (physics_engine_ && physics_engine_->isInitialized) {
      LogPhysicsEngineStates();
    } else {
      PrintDebugToConsole("Physics Engine: NOT INITIALIZED or LEGACY MODE");
    }

    PrintDebugToConsole("============================================");
  }
}

bool Application::IsDebugEnabled() const {
  return debug_mode_ || debug_mode_requested_;
}

void Application::DebugLog(const std::string& message) {
  PrintDebugToConsole(message);
#ifdef _WIN32
  OutputDebugStringA((message + "\n").c_str());
#endif
}

void Application::QueuePhysicsWarningDialog(const std::string& message) {
  std::lock_guard<std::mutex> lock(physics_warning_mutex_);
  physics_warning_message_ = message;
  show_physics_warning_dialog_ = true;
}

void Application::RenderPhysicsWarningDialog() {
  bool show_dialog = false;
  std::string message;
  {
    std::lock_guard<std::mutex> lock(physics_warning_mutex_);
    show_dialog = show_physics_warning_dialog_;
    message = physics_warning_message_;
  }

  if (!show_dialog)
    return;

  if (!ImGui::IsPopupOpen("Physics Warning")) {
    ImGui::OpenPopup("Physics Warning");
  }

  bool open = true;
  if (ImGui::BeginPopupModal("Physics Warning", &open,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(
        TR("ui.debug.fixed_buffer_limit", "Fixed buffer limit reached."));
    ImGui::Separator();
    ImGui::TextWrapped("%s", message.c_str());

    if (ImGui::Button("OK", ImVec2(120, 0))) {
      std::lock_guard<std::mutex> lock(physics_warning_mutex_);
      show_physics_warning_dialog_ = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  if (!open) {
    std::lock_guard<std::mutex> lock(physics_warning_mutex_);
    show_physics_warning_dialog_ = false;
  }
}

void Application::LogComponentStates() {
  PrintDebugToConsole("=== COMPONENT STATES ===");

  int cylinderCount = 0, limitSwitchCount = 0, sensorCount = 0;

  for (const auto& comp : placed_components_) {
    switch (comp.type) {
      case ComponentType::CYLINDER: {
        cylinderCount++;
        float position = comp.internalStates.count(state_keys::kPosition)
                             ? comp.internalStates.at(state_keys::kPosition)
                             : 0.0f;
        float velocity = comp.internalStates.count(state_keys::kVelocity)
                             ? comp.internalStates.at(state_keys::kVelocity)
                             : 0.0f;
        float pressureA = comp.internalStates.count(state_keys::kPressureA)
                              ? comp.internalStates.at(state_keys::kPressureA)
                              : 0.0f;
        float pressureB = comp.internalStates.count(state_keys::kPressureB)
                              ? comp.internalStates.at(state_keys::kPressureB)
                              : 0.0f;

        PrintDebugToConsole("  Cylinder[" + std::to_string(comp.instanceId) +
                            "] Pos:" + std::to_string(position) +
                            "mm Vel:" + std::to_string(velocity) +
                            "mm/s PA:" + std::to_string(pressureA) +
                            "bar PB:" + std::to_string(pressureB) + "bar");
        break;
      }

      case ComponentType::LIMIT_SWITCH: {
        limitSwitchCount++;
        bool isPressed = comp.internalStates.count(state_keys::kIsPressed) &&
                         comp.internalStates.at(state_keys::kIsPressed) > 0.5f;
        bool manualPress =
            comp.internalStates.count(state_keys::kIsPressedManual) &&
            comp.internalStates.at(state_keys::kIsPressedManual) > 0.5f;

        float closestDistance = 999999.0f;
        int closestCylinderId = -1;
        for (const auto& cylinder : placed_components_) {
          if (cylinder.type == ComponentType::CYLINDER) {
            float cylinderX_body_start = cylinder.position.x + 170.0f;
            float pistonPosition =
                cylinder.internalStates.count(state_keys::kPosition)
                    ? cylinder.internalStates.at(state_keys::kPosition)
                    : 0.0f;
            float pistonX_tip = cylinderX_body_start - pistonPosition;
            float pistonY = cylinder.position.y + 25.0f;

            float sensorX_center = comp.position.x + comp.size.width / 2.0f;
            float sensorY_center = comp.position.y + comp.size.height / 2.0f;

            float dx = pistonX_tip - sensorX_center;
            float dy = pistonY - sensorY_center;
            float distance = std::sqrt(dx * dx + dy * dy);

            if (distance < closestDistance) {
              closestDistance = distance;
              closestCylinderId = cylinder.instanceId;
            }
          }
        }

        PrintDebugToConsole(
            "  LimitSwitch[" + std::to_string(comp.instanceId) +
            "] Pressed:" + (isPressed ? "YES" : "NO") +
            " Manual:" + (manualPress ? "YES" : "NO") +
            " Pos:(" +
            std::to_string(static_cast<int>(comp.position.x)) + "," +
            std::to_string(static_cast<int>(comp.position.y)) + ")" +
            " ClosestCyl[" + std::to_string(closestCylinderId) + "]:" +
            std::to_string(static_cast<int>(closestDistance)) + "mm");
        break;
      }

      case ComponentType::SENSOR: {
        sensorCount++;
        bool isDetected = comp.internalStates.count(state_keys::kIsDetected) &&
                          comp.internalStates.at(state_keys::kIsDetected) >
                              0.5f;
        bool isPowered = comp.internalStates.count(state_keys::kIsPowered) &&
                         comp.internalStates.at(state_keys::kIsPowered) > 0.5f;

        PrintDebugToConsole(
            "  Sensor[" + std::to_string(comp.instanceId) +
            "] Detected:" + (isDetected ? "YES" : "NO") +
            " Powered:" + (isPowered ? "YES" : "NO") +
            " Pos:(" + std::to_string(static_cast<int>(comp.position.x)) +
            "," + std::to_string(static_cast<int>(comp.position.y)) + ")");
        break;
      }

      default:
        break;
    }
  }

  PrintDebugToConsole("Total Components: " + std::to_string(cylinderCount) +
                      " Cylinders, " + std::to_string(limitSwitchCount) +
                      " LimitSwitches, " + std::to_string(sensorCount) +
                      " Sensors");

  std::string activeOutputs = "";
  for (const auto& pair : plc_device_states_) {
    if (pair.first[0] == 'Y' && pair.second) {
      if (!activeOutputs.empty())
        activeOutputs += ", ";
      activeOutputs += pair.first;
    }
  }
  PrintDebugToConsole("Active PLC Outputs: " +
                      (activeOutputs.empty() ? "NONE" : activeOutputs));
}

void Application::LogPhysicsEngineStates() {
  PrintDebugToConsole("=== PHYSICS ENGINE STATES ===");

  if (!physics_engine_) {
    PrintDebugToConsole("ERROR: Physics Engine: NULL POINTER");
    return;
  }

  PrintDebugToConsole("Engine Status: " +
                      std::string(physics_engine_->isInitialized
                                      ? "INITIALIZED"
                                      : "NOT INITIALIZED"));
  PrintDebugToConsole("Active Components: " +
                      std::to_string(physics_engine_->activeComponents) + "/" +
                      std::to_string(physics_engine_->maxComponents));

  LogElectricalNetwork();
  LogPneumaticNetwork();
  LogMechanicalSystem();
}

void Application::LogElectricalNetwork() {
  if (!physics_engine_->electricalNetwork) {
    PrintDebugToConsole("Electrical Network: NULL");
    return;
  }

  ElectricalNetwork* elec = physics_engine_->electricalNetwork;
  PrintDebugToConsole("Electrical Network: " + std::to_string(elec->nodeCount) +
                      " nodes, " + std::to_string(elec->edgeCount) + " edges");
  PrintDebugToConsole(
      "Convergence: " + std::string(elec->isConverged ? "YES" : "NO") +
      " Iterations: " + std::to_string(elec->currentIteration));
}

void Application::LogPneumaticNetwork() {
  if (!physics_engine_->pneumaticNetwork) {
    PrintDebugToConsole("Pneumatic Network: NULL");
    return;
  }

  PneumaticNetwork* pneu = physics_engine_->pneumaticNetwork;
  PrintDebugToConsole("Pneumatic Network: " + std::to_string(pneu->nodeCount) +
                      " nodes, " + std::to_string(pneu->edgeCount) + " edges");
  PrintDebugToConsole(
      "Convergence: " + std::string(pneu->isConverged ? "YES" : "NO") +
      " Iterations: " + std::to_string(pneu->currentIteration));
}

void Application::LogMechanicalSystem() {
  if (!physics_engine_->mechanicalSystem) {
    PrintDebugToConsole("Mechanical System: NULL");
    return;
  }

  MechanicalSystem* mech = physics_engine_->mechanicalSystem;
  PrintDebugToConsole("Mechanical System: " + std::to_string(mech->nodeCount) +
                      " nodes, " + std::to_string(mech->edgeCount) + " edges");
  PrintDebugToConsole(
      "Stability: " + std::string(mech->isStable ? "STABLE" : "UNSTABLE") +
      " Time: " + std::to_string(mech->currentTime) + "s");
}

void Application::PrintDebugToConsole(const std::string& message) {
  std::cout << message << std::endl;

#ifdef _WIN32
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hConsole != INVALID_HANDLE_VALUE) {
    DWORD written;
    std::string fullMessage = message + "\n";
    WriteConsoleA(hConsole, fullMessage.c_str(),
                  static_cast<DWORD>(fullMessage.length()), &written, NULL);
  }
#endif
}

}  // namespace plc
