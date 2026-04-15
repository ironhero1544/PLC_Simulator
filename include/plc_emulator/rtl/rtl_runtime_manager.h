#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_RTL_RTL_RUNTIME_MANAGER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_RTL_RTL_RUNTIME_MANAGER_H_

#include "plc_emulator/core/data_types.h"
#include "plc_emulator/physics/component_physics_adapter.h"
#include "plc_emulator/rtl/rtl_project_manager.h"

#include <map>
#include <set>
#include <string>

// PLC_RTL_WSL_BACKEND: Windows + WSL2 기반 Verilator 런타임 백엔드.
// CMakeLists.txt의 target_compile_definitions 에서 활성화됩니다.
// 미정의 시 RTL 컴포넌트는 로드되지만 시뮬레이션은 비활성화됩니다.

namespace plc {

class RtlRuntimeManager {
 public:
  RtlRuntimeManager();
  ~RtlRuntimeManager();

  void SetProjectManager(const RtlProjectManager* project_manager);
  void SyncComponentInstances(const std::vector<PlacedComponent>& components);
  void InvalidateModule(const std::string& module_id);
  void ShutdownAll();

  bool EvaluateComponent(const PlacedComponent& comp,
                         const std::map<PortRef, float>& voltages,
                         std::map<int, RtlLogicValue>* output_values,
                         std::string* diagnostics);

 private:
  struct WorkerProcess;

  const RtlProjectManager* project_manager_ = nullptr;
  std::map<int, WorkerProcess> processes_;

  bool EnsureProcessStarted(const PlacedComponent& comp,
                            std::string* diagnostics);
  bool SendEvalRequest(WorkerProcess* process,
                       const PlacedComponent& comp,
                       const std::map<PortRef, float>& voltages,
                       std::map<int, RtlLogicValue>* output_values,
                       std::string* diagnostics);
  static std::string BuildEvalCommand(
      const PlacedComponent& comp,
      const std::map<PortRef, float>& voltages);
};

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_RTL_RTL_RUNTIME_MANAGER_H_
