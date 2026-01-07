// component_manager.cpp
//
// Implementation of component manager.

// component_manager.cpp
// Component management implementation
// 컴포넌트 관리 구현

#include "plc_emulator/core/component_manager.h"

#include <algorithm>

namespace plc {

ComponentManager::ComponentManager()
    : selected_component_id_(-1), next_instance_id_(0) {}

ComponentManager::~ComponentManager() {}

int ComponentManager::AddComponent(const PlacedComponent& component) {
  PlacedComponent new_comp = component;
  new_comp.instanceId = next_instance_id_++;
  placed_components_.push_back(new_comp);
  return new_comp.instanceId;
}

bool ComponentManager::RemoveComponent(int instance_id) {
  auto it = std::find_if(placed_components_.begin(), placed_components_.end(),
                         [instance_id](const PlacedComponent& comp) {
                           return comp.instanceId == instance_id;
                         });

  if (it != placed_components_.end()) {
    placed_components_.erase(it);
    if (selected_component_id_ == instance_id) {
      selected_component_id_ = -1;
    }
    return true;
  }
  return false;
}

PlacedComponent* ComponentManager::GetComponent(int instance_id) {
  auto it = std::find_if(placed_components_.begin(), placed_components_.end(),
                         [instance_id](const PlacedComponent& comp) {
                           return comp.instanceId == instance_id;
                         });

  if (it != placed_components_.end()) {
    return &(*it);
  }
  return nullptr;
}

const PlacedComponent* ComponentManager::GetComponent(int instance_id) const {
  auto it = std::find_if(placed_components_.begin(), placed_components_.end(),
                         [instance_id](const PlacedComponent& comp) {
                           return comp.instanceId == instance_id;
                         });

  if (it != placed_components_.end()) {
    return &(*it);
  }
  return nullptr;
}

void ComponentManager::Clear() {
  placed_components_.clear();
  selected_component_id_ = -1;
  next_instance_id_ = 0;
}

}  // namespace plc
