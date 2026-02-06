/*
 * component_manager.h
 *
 * 컴포넌트 배치와 선택을 관리하는 매니저 선언.
 * Declaration of the manager for component placement and selection.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_COMPONENT_MANAGER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_COMPONENT_MANAGER_H_

#include "plc_emulator/core/data_types.h"

#include <vector>

namespace plc {

/*
 * 컴포넌트의 추가/삭제/선택 상태를 관리합니다.
 * Manages component insertion, removal, and selection state.
 */
class ComponentManager {
 public:
  ComponentManager();
  ~ComponentManager();

  ComponentManager(const ComponentManager&) = delete;
  ComponentManager& operator=(const ComponentManager&) = delete;

  int AddComponent(const PlacedComponent& component);
  bool RemoveComponent(int instance_id);

  PlacedComponent* GetComponent(int instance_id);
  const PlacedComponent* GetComponent(int instance_id) const;

  std::vector<PlacedComponent>& GetComponents() { return placed_components_; }
  const std::vector<PlacedComponent>& GetComponents() const {
    return placed_components_;
  }

  void Clear();

  void SetSelectedComponent(int instance_id) {
    selected_component_id_ = instance_id;
  }
  int GetSelectedComponent() const { return selected_component_id_; }
  void ClearSelection() { selected_component_id_ = -1; }
  int GetNextInstanceId() const { return next_instance_id_; }

 private:
  std::vector<PlacedComponent> placed_components_;
  int selected_component_id_;
  int next_instance_id_;
};

}  /* namespace plc */

#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_COMPONENT_MANAGER_H_ */
