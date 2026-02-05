// application_physics.cpp
//
// Thin delegators for physics update.

#include "plc_emulator/core/application.h"

namespace plc {

void Application::UpdatePhysics() {
  UpdatePhysicsImpl();
}

void Application::SimulateElectrical() {
  SimulateElectricalImpl();
}

void Application::UpdateComponentLogic() {
  UpdateComponentLogicImpl();
}

void Application::SimulatePneumatic() {
  SimulatePneumaticImpl();
}

void Application::UpdateActuators(float delta_time,
                                  bool skip_cylinder_update) {
  UpdateActuatorsImpl(delta_time, skip_cylinder_update);
}

void Application::UpdateWorkpieceInteractions(float delta_time) {
  UpdateWorkpieceInteractionsImpl(delta_time);
}

void Application::UpdateBasicPhysics(float delta_time) {
  UpdateBasicPhysicsImpl(delta_time);
}

}  // namespace plc
