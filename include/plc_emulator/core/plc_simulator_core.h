/*
 * plc_simulator_core.h
 *
 * PLC 시뮬레이터의 중앙 데이터 모델 인터페이스.
 * Interface for the PLC simulator's central data model.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_PLC_SIMULATOR_CORE_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_PLC_SIMULATOR_CORE_H_

#include "plc_emulator/core/data_types.h"
#include "plc_emulator/io/io_mapping.h"
#include "plc_emulator/programming/programming_mode.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace plc {

/*
 * 전방 선언: 저장소, 매퍼, 이벤트 디스패처.
 * Forward declarations for repositories, mapper, and dispatcher.
 */
struct ComponentRepository;
struct WiringRepository;
struct LadderRepository;
struct IORepository;
struct IOMapper;
struct EventDispatcher;

/*
 * 실배선/프로그래밍 모드 데이터를 동기화합니다.
 * Synchronizes wiring and programming mode data.
 */
class PLCSimulatorCore {
 public:
  PLCSimulatorCore();
  ~PLCSimulatorCore();

  bool Initialize();
  void Shutdown();

  std::vector<PlacedComponent>& GetPlacedComponents() {
    return placed_components_;
  }
  const std::vector<PlacedComponent>& GetPlacedComponents() const {
    return placed_components_;
  }

  std::vector<Wire>& GetWires() { return wires_; }
  const std::vector<Wire>& GetWires() const { return wires_; }

  LadderProgram& GetLadderProgram() { return ladder_program_; }
  const LadderProgram& GetLadderProgram() const { return ladder_program_; }

  std::map<std::string, bool>& GetDeviceStates() { return device_states_; }
  const std::map<std::string, bool>& GetDeviceStates() const {
    return device_states_;
  }

  /*
   * 모드 간 데이터 동기화.
   * Cross-mode data synchronization.
   */
  void SyncFromProgrammingMode(const ProgrammingMode* programmingMode);
  void SyncToProgrammingMode(ProgrammingMode* programmingMode);

  /*
   * I/O 매핑 관리.
   * I/O mapping management.
   */
  bool UpdateIOMapping();
  const IOMapping& GetCurrentIOMapping() const;
  const MappingResult& GetLastMappingResult() const;
  void SyncIOMapping();
  bool ValidateIOMapping() const;

  /*
   * 변경 알림.
   * Change notifications.
   */
  void NotifyDataChanged();
  void NotifyModeChanged(Mode newMode);

 private:
  std::vector<PlacedComponent> placed_components_;
  std::vector<Wire> wires_;

  LadderProgram ladder_program_;
  std::map<std::string, bool> device_states_;
  std::map<std::string, TimerState> timer_states_;
  std::map<std::string, CounterState> counter_states_;

  std::unique_ptr<ComponentRepository> component_repo_;
  std::unique_ptr<WiringRepository> wiring_repo_;
  std::unique_ptr<LadderRepository> ladder_repo_;
  std::unique_ptr<IORepository> io_repo_;

  std::unique_ptr<IOMapper> io_mapper_;
  IOMapping current_io_mapping_;
  MappingResult last_mapping_result_;

  std::unique_ptr<EventDispatcher> event_dispatcher_;

  void InitializeDefaultData();
  void ClearAllData();
};

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_PLC_SIMULATOR_CORE_H_ */
