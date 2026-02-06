/*
 * ladder_ir.h
 *
 * 래더 IR 중간 표현 선언.
 * Declarations for ladder IR.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LADDER_IR_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LADDER_IR_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace plc {

  /*
   * 전방 선언으로 순환 참조를 방지합니다.
   * Forward declarations to avoid circular dependencies.
   */
struct LadderProgram;
struct Rung;
struct LadderInstruction;
struct VerticalConnection;

/*
 * IR 노드 타입 정의.
 * IR node type definitions.
 */
enum class IRNodeType {
  NORMALLY_OPEN,
  NORMALLY_CLOSED,

  OUTPUT_COIL,
  SET_COIL,
  RESET_COIL,

  AND_BLOCK,
  OR_BLOCK,

  TIMER_ON,
  TIMER_OFF,
  COUNTER_UP,
  COUNTER_DOWN,

  EMPTY,
  VERTICAL_LINE,
  HORIZONTAL_LINE
};

/*
 * IR 노드 연결 정보.
 * IR node connection data.
 */
struct IRConnection {
  size_t fromNodeId;
  size_t toNodeId;
  int outputPort;
  int inputPort;

  IRConnection(size_t from, size_t to, int outPort = 0, int inPort = 0)
      : fromNodeId(from),
        toNodeId(to),
        outputPort(outPort),
        inputPort(inPort) {}
};

/*
 * IR 노드 기본 클래스.
 * Base class for IR nodes.
 */
class IRNode {
 public:
  IRNodeType nodeType;
  size_t nodeId;
  std::string deviceAddress;
  std::vector<size_t> inputs;
  std::vector<size_t> outputs;

  int rungIndex;
  int cellIndex;

  IRNode(IRNodeType type, size_t id, const std::string& address = "")
      : nodeType(type),
        nodeId(id),
        deviceAddress(address),
        rungIndex(-1),
        cellIndex(-1) {}

  virtual ~IRNode() = default;

  virtual std::string GetDisplayName() const;
  virtual bool IsInputNode() const;
  virtual bool IsOutputNode() const;
  virtual bool IsLogicNode() const;
};

/*
 * 타이머 IR 노드.
 * Timer IR node.
 */
class IRTimerNode : public IRNode {
 public:
  float timeValue;
  std::string timeUnit;

  IRTimerNode(size_t id, const std::string& address, float value = 1.0f)
      : IRNode(IRNodeType::TIMER_ON, id, address),
        timeValue(value),
        timeUnit("s") {}
};

/*
 * 카운터 IR 노드.
 * Counter IR node.
 */
class IRCounterNode : public IRNode {
 public:
  int countValue;

  IRCounterNode(size_t id, const std::string& address, int value = 1)
      : IRNode(IRNodeType::COUNTER_UP, id, address), countValue(value) {}
};

/*
 * OR 블록 IR 노드.
 * OR-block IR node.
 */
class IROrBlockNode : public IRNode {
 public:
  std::vector<std::vector<size_t>> parallelBranches;

  IROrBlockNode(size_t id) : IRNode(IRNodeType::OR_BLOCK, id) {}

  void AddBranch(const std::vector<size_t>& branch) {
    parallelBranches.push_back(branch);
  }
};

/*
 * IR 룽 데이터.
 * IR rung data.
 */
struct IRRung {
  std::vector<std::unique_ptr<IRNode>> nodes;
  std::vector<IRConnection> connections;
  size_t startNodeId;
  std::vector<size_t> endNodeIds;

  IRRung() : startNodeId(0) {}

  void AddNode(std::unique_ptr<IRNode> node);

  void AddConnection(size_t fromId, size_t toId, int outPort = 0,
                     int inPort = 0);

  IRNode* FindNode(size_t nodeId);
  const IRNode* FindNode(size_t nodeId) const;

  void AnalyzeParallelConnections();
};

/*
 * 래더 IR 프로그램.
 * Ladder IR program container.
 */
class LadderIRProgram {
 private:
  std::vector<IRRung> rungs_;
  std::map<std::string, size_t>
      device_node_map_;
  size_t next_node_id_;

 public:
  LadderIRProgram() : next_node_id_(1) {}

  void AddRung(IRRung&& rung);
  size_t GetRungCount() const { return rungs_.size(); }
  IRRung& GetRung(size_t index) { return rungs_[index]; }
  const IRRung& GetRung(size_t index) const { return rungs_[index]; }

  size_t GenerateNodeId() { return next_node_id_++; }

  void MapDeviceToNode(const std::string& deviceAddr, size_t nodeId);
  size_t FindNodeByDevice(const std::string& deviceAddr) const;

  void AnalyzeLogicStructure();
  void OptimizeConnections();

  void PrintIRStructure() const;
};

/*
 * 래더 프로그램을 IR로 변환합니다.
 * Converts ladder programs to IR.
 */
class LadderToIRConverter {
 public:
  static LadderIRProgram ConvertToIR(const LadderProgram& ladderProgram);

 private:
  static IRRung ConvertRungToIR(const Rung& rung, LadderIRProgram& irProgram);

  static std::unique_ptr<IRNode> ConvertCellToNode(
      const LadderInstruction& cell, size_t nodeId, int rungIndex,
      int cellIndex);

  static void AnalyzeVerticalConnections(
      const std::vector<VerticalConnection>& verticalConnections,
      IRRung& irRung);

  static void AnalyzeLogicFlow(IRRung& irRung);
};

/*
 * IR을 스택 명령으로 변환합니다.
 * Converts IR to stack instructions.
 */
class IRToStackConverter {
 public:
  static std::vector<std::string> ConvertToStackInstructions(
      const LadderIRProgram& irProgram);

 private:
  static std::vector<std::string> ConvertRungToStack(const IRRung& rung);

  static std::vector<std::string> ConvertOrBlockToStack(
      const IROrBlockNode& orBlock);

  static std::string ConvertNodeToStack(const IRNode& node);

  static void OptimizeStackInstructions(std::vector<std::string>& instructions);
};

}  /* namespace plc */
#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_PROJECT_LADDER_IR_H_ */
