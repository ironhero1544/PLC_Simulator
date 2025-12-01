// ladder_ir.h
// Copyright 2024 PLC Emulator Project
//
// Intermediate representation for ladder logic.

/**
 * @file LadderIR.h
 * @brief Ladder Intermediate Representation - 래더 다이어그램 중간 표현
  * L추가er Intermedi에서e Represent에서i위 - 래더 다이어그램 중간 표현
 *
 * 이 파일은 UI 래더 다이어그램과 PLC 스택 머신 코드 사이의 중간 표현을
 * 정의합니다. 병렬(OR) 논리 회로의 정확한 변환을 위해 설계되었습니다.
 *
 * @author Claude Code Analysis Engine
 * @date 2025-01-08
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LADDER_IR_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LADDER_IR_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace plc {

// 전방 선언 - 순환 참조 방지
struct LadderProgram;
struct Rung;
struct LadderInstruction;
struct VerticalConnection;

/**
 * @brief IR 노드 타입 정의
 * 래더 다이어그램의 각 요소를 중간 표현으로 변환할 때 사용되는 노드 타입들
 */
enum class IRNodeType {
  NORMALLY_OPEN,    // 일반 개방 접점 (NO)
  NORMALLY_CLOSED,  // 일반 폐쇄 접점 (NC)

  OUTPUT_COIL,  // 출력 코일
  SET_COIL,     // 셋 코일 (S)
  RESET_COIL,   // 리셋 코일 (R)

  AND_BLOCK,  // AND 논리 블록 (직렬 연결)
  OR_BLOCK,   // OR 논리 블록 (병렬 연결)

  TIMER_ON,      // 온딜레이 타이머 (TON)
  TIMER_OFF,     // 오프딜레이 타이머 (TOF)
  COUNTER_UP,    // 업카운터 (CTU)
  COUNTER_DOWN,  // 다운카운터 (CTD)

  EMPTY,           // 빈 노드
  VERTICAL_LINE,   // 세로선 (논리적 연결)
  HORIZONTAL_LINE  // 가로선 (전류 흐름)
};

/**
 * @brief IR 연결 정보
 * 노드 간의 연결 관계를 나타내는 구조체
 */
struct IRConnection {
  size_t fromNodeId;  ///< 시작 노드 ID
  size_t toNodeId;    ///< 끝 노드 ID
  int outputPort;     ///< 출력 포트 번호 (0=기본, 1=TRUE, 2=FALSE 등)
  int inputPort;      ///< 입력 포트 번호

  IRConnection(size_t from, size_t to, int outPort = 0, int inPort = 0)
      : fromNodeId(from),
        toNodeId(to),
        outputPort(outPort),
        inputPort(inPort) {}
};

/**
 * @brief IR 노드 기본 클래스
 * 모든 IR 노드의 공통 인터페이스를 정의
 */
class IRNode {
 public:
  IRNodeType nodeType;          ///< 노드 타입
  size_t nodeId;                ///< 고유 노드 ID
  std::string deviceAddress;    ///< 디바이스 주소 (X0, Y0, T0 등)
  std::vector<size_t> inputs;   ///< 입력 연결된 노드 ID들
  std::vector<size_t> outputs;  ///< 출력 연결된 노드 ID들

  // 위치 정보 (원본 래더에서의 좌표)
  int rungIndex;  ///< 룽 인덱스
  int cellIndex;  ///< 셀 인덱스

  IRNode(IRNodeType type, size_t id, const std::string& address = "")
      : nodeType(type),
        nodeId(id),
        deviceAddress(address),
        rungIndex(-1),
        cellIndex(-1) {}

  virtual ~IRNode() = default;

  // 노드별 특화 정보 가져오기
  virtual std::string GetDisplayName() const;
  virtual bool IsInputNode() const;
  virtual bool IsOutputNode() const;
  virtual bool IsLogicNode() const;
};

/**
 * @brief 타이머 IR 노드
 * 타이머 특화 정보를 포함하는 노드
 */
class IRTimerNode : public IRNode {
 public:
  float timeValue;       ///< 타이머 설정값 (초 단위)
  std::string timeUnit;  ///< 시간 단위 ("s", "ms" 등)

  IRTimerNode(size_t id, const std::string& address, float value = 1.0f)
      : IRNode(IRNodeType::TIMER_ON, id, address),
        timeValue(value),
        timeUnit("s") {}
};

/**
 * @brief 카운터 IR 노드
 * 카운터 특화 정보를 포함하는 노드
 */
class IRCounterNode : public IRNode {
 public:
  int countValue;  ///< 카운터 설정값

  IRCounterNode(size_t id, const std::string& address, int value = 1)
      : IRNode(IRNodeType::COUNTER_UP, id, address), countValue(value) {}
};

/**
 * @brief OR 블록 IR 노드
 * 병렬 연결(OR 논리)을 나타내는 특수 노드
 */
class IROrBlockNode : public IRNode {
 public:
  std::vector<std::vector<size_t>> parallelBranches;  ///< 병렬 분기들

  IROrBlockNode(size_t id) : IRNode(IRNodeType::OR_BLOCK, id) {}

  void AddBranch(const std::vector<size_t>& branch) {
    parallelBranches.push_back(branch);
  }
};

/**
 * @brief 래더 IR 룽 (Rung)
 * 하나의 룽을 IR 노드들로 표현
 */
struct IRRung {
  std::vector<std::unique_ptr<IRNode>> nodes;  ///< 룽의 모든 노드들
  std::vector<IRConnection> connections;       ///< 노드 간 연결 정보
  size_t startNodeId;                          ///< 시작 노드 ID (좌측 전원선)
  std::vector<size_t> endNodeIds;              ///< 끝 노드 ID들 (출력 코일들)

  IRRung() : startNodeId(0) {}

  // 노드 추가
  void AddNode(std::unique_ptr<IRNode> node);

  // 연결 추가
  void AddConnection(size_t fromId, size_t toId, int outPort = 0,
                     int inPort = 0);

  // 노드 검색
  IRNode* FindNode(size_t nodeId);
  const IRNode* FindNode(size_t nodeId) const;

  // OR 블록 생성 (병렬 연결 분석)
  void AnalyzeParallelConnections();
};

/**
 * @brief 래더 IR 프로그램
 * 전체 래더 프로그램의 IR 표현
 */
class LadderIRProgram {
 private:
  std::vector<IRRung> rungs_;  ///< 모든 룽들
  std::map<std::string, size_t>
      device_node_map_;  ///< 디바이스 주소 → 노드 ID 매핑
  size_t next_node_id_;  ///< 다음 노드 ID

 public:
  LadderIRProgram() : next_node_id_(1) {}  // 0은 전원선용 예약

  // 룽 관리
  void AddRung(IRRung&& rung);
  size_t GetRungCount() const { return rungs_.size(); }
  IRRung& GetRung(size_t index) { return rungs_[index]; }
  const IRRung& GetRung(size_t index) const { return rungs_[index]; }

  // 노드 ID 관리
  size_t GenerateNodeId() { return next_node_id_++; }

  // 디바이스 매핑
  void MapDeviceToNode(const std::string& deviceAddr, size_t nodeId);
  size_t FindNodeByDevice(const std::string& deviceAddr) const;

  // IR 분석 및 최적화
  void AnalyzeLogicStructure();
  void OptimizeConnections();

  // 디버그 출력
  void PrintIRStructure() const;
};

/**
 * @brief 래더 → IR 변환기
 * UI LadderProgram을 LadderIRProgram으로 변환
 */
class LadderToIRConverter {
 public:
  /**
   * @brief UI 래더 프로그램을 IR로 변환
   * @param ladderProgram 원본 UI 래더 프로그램
   * @return 변환된 IR 프로그램
   */
  static LadderIRProgram ConvertToIR(const LadderProgram& ladderProgram);

 private:
  // 룽별 변환
  static IRRung ConvertRungToIR(const Rung& rung, LadderIRProgram& irProgram);

  // 셀별 변환
  static std::unique_ptr<IRNode> ConvertCellToNode(
      const LadderInstruction& cell, size_t nodeId, int rungIndex,
      int cellIndex);

  // 세로 연결선 분석
  static void AnalyzeVerticalConnections(
      const std::vector<VerticalConnection>& verticalConnections,
      IRRung& irRung);

  // 논리 구조 분석
  static void AnalyzeLogicFlow(IRRung& irRung);
};

/**
 * @brief IR → 스택 머신 코드 변환기
 * LadderIRProgram을 PLC 스택 명령어로 변환
 */
class IRToStackConverter {
 public:
  /**
   * @brief IR 프로그램을 스택 명령어로 변환
   * @param irProgram IR 프로그램
   * @return PLC 스택 명령어 리스트
   */
  static std::vector<std::string> ConvertToStackInstructions(
      const LadderIRProgram& irProgram);

 private:
  // 룽별 스택 명령어 생성
  static std::vector<std::string> ConvertRungToStack(const IRRung& rung);

  // OR 블록 스택 명령어 생성
  static std::vector<std::string> ConvertOrBlockToStack(
      const IROrBlockNode& orBlock);

  // 개별 노드 스택 명령어 생성
  static std::string ConvertNodeToStack(const IRNode& node);

  // 스택 명령어 최적화
  static void OptimizeStackInstructions(std::vector<std::string>& instructions);
};

}  // namespace plc
#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LADDER_IR_H_
