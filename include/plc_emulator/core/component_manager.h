// component_manager.h
// Copyright 2024 PLC Emulator Project
//
// Manages PLC components in the simulation.

// component_manager.h
// Component placement and lifecycle management
// 컴포넌트 배치 및 생명주기 관리

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_COMPONENT_MANAGER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_COMPONENT_MANAGER_H_

#include "plc_emulator/core/data_types.h"

#include <vector>

namespace plc {

// Manages component placement, selection, and lifecycle
// 컴포넌트 배치, 선택, 생명주기 관리
class ComponentManager {
 public:
  ComponentManager();
  ~ComponentManager();

  ComponentManager(const ComponentManager&) = delete;
  ComponentManager& operator=(const ComponentManager&) = delete;

  // Add new component to scene
  // 새 컴포넌트를 씬에 추가
  int AddComponent(const PlacedComponent& component);

  // Remove component by instance ID
  // 인스턴스 ID로 컴포넌트 제거
  bool RemoveComponent(int instance_id);

  // Get component by instance ID
  // 인스턴스 ID로 컴포넌트 가져오기
  PlacedComponent* GetComponent(int instance_id);
  const PlacedComponent* GetComponent(int instance_id) const;

  // Get all placed components
  // 모든 배치된 컴포넌트 가져오기
  std::vector<PlacedComponent>& GetComponents() { return placed_components_; }
  const std::vector<PlacedComponent>& GetComponents() const {
    return placed_components_;
  }

  // Clear all components
  // 모든 컴포넌트 제거
  void Clear();

  // Component selection
  // 컴포넌트 선택
  void SetSelectedComponent(int instance_id) { selected_component_id_ = instance_id; }
  int GetSelectedComponent() const { return selected_component_id_; }
  void ClearSelection() { selected_component_id_ = -1; }

  // Get next available instance ID
  // 다음 사용 가능한 인스턴스 ID 가져오기
  int GetNextInstanceId() const { return next_instance_id_; }

 private:
  std::vector<PlacedComponent> placed_components_;
  int selected_component_id_;
  int next_instance_id_;
};

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_COMPONENT_MANAGER_H_
