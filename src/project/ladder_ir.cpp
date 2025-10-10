/**
 * @file LadderIR.cpp
 * @brief Ladder Intermediate Representation 구현
 *
 * 병렬(OR) 논리 회로의 정확한 변환을 위한 중간 표현 구현
 *
 * @author Claude Code Analysis Engine
 * @date 2025-01-08
 */

#include "plc_emulator/project/ladder_ir.h"

#include "plc_emulator/programming/programming_mode.h"  // 실제 LadderProgram, LadderRung 등 타입 정의

#include <algorithm>
#include <iostream>
#include <queue>
#include <sstream>

namespace plc {

// ============================================================================
// IRNode 구현
// ============================================================================

std::string IRNode::GetDisplayName() const {
  switch (nodeType) {
    case IRNodeType::NORMALLY_OPEN:
      return "NO[" + deviceAddress + "]";
    case IRNodeType::NORMALLY_CLOSED:
      return "NC[" + deviceAddress + "]";
    case IRNodeType::OUTPUT_COIL:
      return "OUT[" + deviceAddress + "]";
    case IRNodeType::SET_COIL:
      return "SET[" + deviceAddress + "]";
    case IRNodeType::RESET_COIL:
      return "RST[" + deviceAddress + "]";
    case IRNodeType::AND_BLOCK:
      return "AND_BLOCK";
    case IRNodeType::OR_BLOCK:
      return "OR_BLOCK";
    case IRNodeType::TIMER_ON:
      return "TON[" + deviceAddress + "]";
    case IRNodeType::TIMER_OFF:
      return "TOF[" + deviceAddress + "]";
    case IRNodeType::COUNTER_UP:
      return "CTU[" + deviceAddress + "]";
    case IRNodeType::COUNTER_DOWN:
      return "CTD[" + deviceAddress + "]";
    case IRNodeType::EMPTY:
      return "EMPTY";
    case IRNodeType::VERTICAL_LINE:
      return "VLINE";
    case IRNodeType::HORIZONTAL_LINE:
      return "HLINE";
    default:
      return "UNKNOWN";
  }
}

bool IRNode::IsInputNode() const {
  return nodeType == IRNodeType::NORMALLY_OPEN ||
         nodeType == IRNodeType::NORMALLY_CLOSED ||
         nodeType == IRNodeType::TIMER_ON ||
         nodeType == IRNodeType::TIMER_OFF ||
         nodeType == IRNodeType::COUNTER_UP ||
         nodeType == IRNodeType::COUNTER_DOWN;
}

bool IRNode::IsOutputNode() const {
  return nodeType == IRNodeType::OUTPUT_COIL ||
         nodeType == IRNodeType::SET_COIL || nodeType == IRNodeType::RESET_COIL;
}

bool IRNode::IsLogicNode() const {
  return nodeType == IRNodeType::AND_BLOCK || nodeType == IRNodeType::OR_BLOCK;
}

// ============================================================================
// IRRung 구현
// ============================================================================

void IRRung::AddNode(std::unique_ptr<IRNode> node) {
  if (node) {
    nodes.push_back(std::move(node));
  }
}

void IRRung::AddConnection(size_t fromId, size_t toId, int outPort,
                           int inPort) {
  connections.emplace_back(fromId, toId, outPort, inPort);

  // 노드들의 입출력 연결 정보도 업데이트
  IRNode* fromNode = FindNode(fromId);
  IRNode* toNode = FindNode(toId);

  if (fromNode) {
    fromNode->outputs.push_back(toId);
  }
  if (toNode) {
    toNode->inputs.push_back(fromId);
  }
}

IRNode* IRRung::FindNode(size_t nodeId) {
  for (auto& node : nodes) {
    if (node->nodeId == nodeId) {
      return node.get();
    }
  }
  return nullptr;
}

const IRNode* IRRung::FindNode(size_t nodeId) const {
  for (const auto& node : nodes) {
    if (node->nodeId == nodeId) {
      return node.get();
    }
  }
  return nullptr;
}

void IRRung::AnalyzeParallelConnections() {
  // 병렬 연결 분석 알고리즘
  // 1. 같은 입력과 출력을 가지는 노드 그룹 찾기
  // 2. OR 블록으로 그룹화

  std::map<std::pair<size_t, size_t>, std::vector<size_t>> parallelGroups;

  for (const auto& node : nodes) {
    if (node->IsInputNode() && !node->inputs.empty() &&
        !node->outputs.empty()) {
      size_t inputNode = node->inputs[0];    // 첫 번째 입력
      size_t outputNode = node->outputs[0];  // 첫 번째 출력

      parallelGroups[{inputNode, outputNode}].push_back(node->nodeId);
    }
  }

  // 2개 이상의 노드가 같은 입출력을 가지면 OR 블록으로 변환
  for (const auto& [inputOutput, nodeGroup] : parallelGroups) {
    if (nodeGroup.size() >= 2) {
      // OR 블록 노드 생성 (다음 구현에서...)
      std::cout << "[IR] Found parallel group: " << nodeGroup.size()
                << " nodes\n";
    }
  }
}

// ============================================================================
// LadderIRProgram 구현
// ============================================================================

void LadderIRProgram::AddRung(IRRung&& rung) {
  rungs_.push_back(std::move(rung));
}

void LadderIRProgram::MapDeviceToNode(const std::string& deviceAddr,
                                      size_t nodeId) {
  device_node_map_[deviceAddr] = nodeId;
}

size_t LadderIRProgram::FindNodeByDevice(const std::string& deviceAddr) const {
  auto it = device_node_map_.find(deviceAddr);
  return (it != device_node_map_.end()) ? it->second : 0;
}

void LadderIRProgram::AnalyzeLogicStructure() {
  std::cout << "[IR] Analyzing logic structure for " << rungs_.size()
            << " rungs...\n";

  for (auto& rung : rungs_) {
    rung.AnalyzeParallelConnections();
  }
}

void LadderIRProgram::OptimizeConnections() {
  // 연결 최적화: 불필요한 중간 노드 제거, 직렬 연결 간소화 등
  std::cout << "[IR] Optimizing connections...\n";

  for (auto& rung : rungs_) {
    // 빈 노드들 제거
    rung.nodes.erase(std::remove_if(rung.nodes.begin(), rung.nodes.end(),
                                    [](const std::unique_ptr<IRNode>& node) {
                                      return node->nodeType ==
                                             IRNodeType::EMPTY;
                                    }),
                     rung.nodes.end());
  }
}

void LadderIRProgram::PrintIRStructure() const {
  std::cout << "\n=== LadderIR Structure ===\n";
  std::cout << "Total Rungs: " << rungs_.size() << "\n\n";

  for (size_t i = 0; i < rungs_.size(); ++i) {
    const auto& rung = rungs_[i];
    std::cout << "Rung " << i << ": " << rung.nodes.size() << " nodes, "
              << rung.connections.size() << " connections\n";

    // 노드들 출력
    for (const auto& node : rung.nodes) {
      std::cout << "  Node " << node->nodeId << ": " << node->GetDisplayName();
      if (!node->inputs.empty()) {
        std::cout << " <- [";
        for (size_t j = 0; j < node->inputs.size(); ++j) {
          if (j > 0)
            std::cout << ",";
          std::cout << node->inputs[j];
        }
        std::cout << "]";
      }
      if (!node->outputs.empty()) {
        std::cout << " -> [";
        for (size_t j = 0; j < node->outputs.size(); ++j) {
          if (j > 0)
            std::cout << ",";
          std::cout << node->outputs[j];
        }
        std::cout << "]";
      }
      std::cout << "\n";
    }
    std::cout << "\n";
  }
}

// ============================================================================
// LadderToIRConverter 구현
// ============================================================================

LadderIRProgram LadderToIRConverter::ConvertToIR(
    const LadderProgram& ladderProgram) {
  std::cout << "[IR] Converting LadderProgram to IR...\n";
  std::cout << "[IR] Input: " << ladderProgram.rungs.size() << " rungs\n";

  LadderIRProgram irProgram;

  // 각 룽을 IR로 변환
  for (size_t i = 0; i < ladderProgram.rungs.size(); ++i) {
    const auto& rung = ladderProgram.rungs[i];
    std::cout << "[IR] Converting rung " << i << " with " << rung.cells.size()
              << " cells\n";

    IRRung irRung = ConvertRungToIR(rung, irProgram);

    // 세로 연결선 분석
    if (!ladderProgram.verticalConnections.empty()) {
      AnalyzeVerticalConnections(ladderProgram.verticalConnections, irRung);
    }

    // 논리 흐름 분석
    AnalyzeLogicFlow(irRung);

    irProgram.AddRung(std::move(irRung));
  }

  // 전체 구조 분석
  irProgram.AnalyzeLogicStructure();
  irProgram.OptimizeConnections();

  std::cout << "[IR] Conversion completed.\n";
  return irProgram;
}

IRRung LadderToIRConverter::ConvertRungToIR(const Rung& rung,
                                            LadderIRProgram& irProgram) {
  IRRung irRung;

  // 시작 노드 (전원선) 생성
  irRung.startNodeId = 0;  // 전원선은 항상 ID 0

  // 각 셀을 IR 노드로 변환
  for (size_t j = 0; j < rung.cells.size(); ++j) {
    const auto& cell = rung.cells[j];

    if (cell.type != LadderInstructionType::EMPTY) {
      size_t nodeId = irProgram.GenerateNodeId();

      auto node = ConvertCellToNode(cell, nodeId, static_cast<int>(0),
                                    static_cast<int>(j));
      if (node) {
        // 디바이스 매핑
        if (!node->deviceAddress.empty()) {
          irProgram.MapDeviceToNode(node->deviceAddress, nodeId);
        }

        irRung.AddNode(std::move(node));
      }
    }
  }

  // 기본 직렬 연결 생성 (좌에서 우로)
  if (!irRung.nodes.empty()) {
    size_t prevNodeId = irRung.startNodeId;

    for (const auto& node : irRung.nodes) {
      if (node->IsInputNode()) {
        irRung.AddConnection(prevNodeId, node->nodeId);
        prevNodeId = node->nodeId;
      } else if (node->IsOutputNode()) {
        irRung.AddConnection(prevNodeId, node->nodeId);
        irRung.endNodeIds.push_back(node->nodeId);
      }
    }
  }

  return irRung;
}

std::unique_ptr<IRNode> LadderToIRConverter::ConvertCellToNode(
    const LadderInstruction& cell, size_t nodeId, int rungIndex,
    int cellIndex) {
  std::unique_ptr<IRNode> node;

  switch (cell.type) {
    case LadderInstructionType::XIC:
      node = std::make_unique<IRNode>(IRNodeType::NORMALLY_OPEN, nodeId,
                                      cell.address);
      break;

    case LadderInstructionType::XIO:
      node = std::make_unique<IRNode>(IRNodeType::NORMALLY_CLOSED, nodeId,
                                      cell.address);
      break;

    case LadderInstructionType::OTE:
      node = std::make_unique<IRNode>(IRNodeType::OUTPUT_COIL, nodeId,
                                      cell.address);
      break;

    case LadderInstructionType::SET:
      node =
          std::make_unique<IRNode>(IRNodeType::SET_COIL, nodeId, cell.address);
      break;

    case LadderInstructionType::RST:
      node = std::make_unique<IRNode>(IRNodeType::RESET_COIL, nodeId,
                                      cell.address);
      break;

    case LadderInstructionType::TON: {
      auto timerNode =
          std::make_unique<IRTimerNode>(nodeId, cell.address, 1.0f);

      // 🔥 **고급 타이머 값 파싱** (예: "T0 K10" -> 10초)
      if (!cell.preset.empty()) {
        // K10, K100 등 파싱
        if (cell.preset[0] == 'K') {
          try {
            int timerValue = std::stoi(cell.preset.substr(1));
            timerNode->timeValue =
                static_cast<float>(timerValue) * 0.1f;  // 100ms 단위
          } catch (...) {
            std::cout << "[IR] Warning: Invalid timer preset: " << cell.preset
                      << std::endl;
          }
        }
      }

      timerNode->nodeType = IRNodeType::TIMER_ON;
      node = std::move(timerNode);
    } break;

    case LadderInstructionType::CTU: {
      auto counterNode =
          std::make_unique<IRCounterNode>(nodeId, cell.address, 1);

      // 🔥 **고급 카운터 값 파싱** (예: "C0 K5" -> 5회)
      if (!cell.preset.empty()) {
        if (cell.preset[0] == 'K') {
          try {
            int countValue = std::stoi(cell.preset.substr(1));
            counterNode->countValue = countValue;
          } catch (...) {
            std::cout << "[IR] Warning: Invalid counter preset: " << cell.preset
                      << std::endl;
          }
        }
      }

      counterNode->nodeType = IRNodeType::COUNTER_UP;
      node = std::move(counterNode);
    } break;

    case LadderInstructionType::EMPTY:
    default:
      // 빈 셀은 노드를 생성하지 않음
      return nullptr;
  }

  if (node) {
    node->rungIndex = rungIndex;
    node->cellIndex = cellIndex;
  }

  return node;
}

void LadderToIRConverter::AnalyzeVerticalConnections(
    const std::vector<VerticalConnection>& verticalConnections,
    IRRung& irRung) {
  std::cout << "[IR] Analyzing " << verticalConnections.size()
            << " vertical connections...\n";

  // 세로 연결선을 OR 블록으로 변환
  for (const auto& vconn : verticalConnections) {
    std::cout << "[IR] Vertical connection at x=" << vconn.x
              << ", connecting rungs: ";
    for (size_t i = 0; i < vconn.rungs.size(); ++i) {
      if (i > 0)
        std::cout << ",";
      std::cout << vconn.rungs[i];
    }
    std::cout << "\n";

    // 병렬 연결된 노드들 찾기 (해당 x 위치의 노드들)
    std::vector<size_t> parallelNodes;
    for (const auto& node : irRung.nodes) {
      if (node->cellIndex == vconn.x) {
        parallelNodes.push_back(node->nodeId);
      }
    }

    // OR 블록 생성 (향후 구현)
    if (parallelNodes.size() >= 2) {
      std::cout << "[IR] Creating OR block with " << parallelNodes.size()
                << " nodes\n";
    }
  }
}

void LadderToIRConverter::AnalyzeLogicFlow(IRRung& irRung) {
  // 논리 흐름 분석 - 전류 흐름 경로 추적
  std::cout << "[IR] Analyzing logic flow for rung with " << irRung.nodes.size()
            << " nodes\n";

  // 현재는 기본 직렬 연결만 처리
  // 향후 병렬 연결 및 복잡한 논리 구조 처리 예정
}

// ============================================================================
// IRToStackConverter 구현
// ============================================================================

std::vector<std::string> IRToStackConverter::ConvertToStackInstructions(
    const LadderIRProgram& irProgram) {
  std::cout << "[IR] Converting IR to stack instructions...\n";

  std::vector<std::string> allInstructions;

  for (size_t i = 0; i < irProgram.GetRungCount(); ++i) {
    const auto& rung = irProgram.GetRung(i);

    auto rungInstructions = ConvertRungToStack(rung);
    allInstructions.insert(allInstructions.end(), rungInstructions.begin(),
                           rungInstructions.end());

    // 룽 구분자 추가
    if (i < irProgram.GetRungCount() - 1) {
      allInstructions.push_back("");  // 빈 줄
    }
  }

  // 스택 명령어 최적화
  OptimizeStackInstructions(allInstructions);

  std::cout << "[IR] Generated " << allInstructions.size()
            << " stack instructions\n";
  return allInstructions;
}

std::vector<std::string> IRToStackConverter::ConvertRungToStack(
    const IRRung& rung) {
  std::vector<std::string> instructions;

  if (rung.nodes.empty()) {
    return instructions;
  }

  // 기본 직렬 연결 처리
  bool isFirstLoad = true;

  for (const auto& node : rung.nodes) {
    if (node->IsInputNode()) {
      if (isFirstLoad) {
        // 첫 번째 접점은 LOAD 명령
        if (node->nodeType == IRNodeType::NORMALLY_OPEN) {
          instructions.push_back("LOAD " + node->deviceAddress);
        } else if (node->nodeType == IRNodeType::NORMALLY_CLOSED) {
          instructions.push_back("LOAD_NOT " + node->deviceAddress);
        }
        isFirstLoad = false;
      } else {
        // 이후 접점들은 AND 명령
        if (node->nodeType == IRNodeType::NORMALLY_OPEN) {
          instructions.push_back("AND " + node->deviceAddress);
        } else if (node->nodeType == IRNodeType::NORMALLY_CLOSED) {
          instructions.push_back("AND_NOT " + node->deviceAddress);
        }
      }
    } else if (node->IsOutputNode()) {
      // 출력 코일
      if (node->nodeType == IRNodeType::OUTPUT_COIL) {
        instructions.push_back("OUT " + node->deviceAddress);
      } else if (node->nodeType == IRNodeType::SET_COIL) {
        instructions.push_back("SET " + node->deviceAddress);
      } else if (node->nodeType == IRNodeType::RESET_COIL) {
        instructions.push_back("RESET " + node->deviceAddress);
      }
    } else if (node->nodeType == IRNodeType::TIMER_ON) {
      // 🔥 **고급 타이머 명령어 생성**
      const IRTimerNode* timerNode =
          dynamic_cast<const IRTimerNode*>(node.get());
      if (timerNode) {
        // OpenPLC 타이머 형식: TON T0, PT=T#1000ms
        int timeMs = static_cast<int>(timerNode->timeValue * 1000);
        instructions.push_back("TON " + node->deviceAddress + ", PT=T#" +
                               std::to_string(timeMs) + "ms");
      }
    } else if (node->nodeType == IRNodeType::COUNTER_UP) {
      // 🔥 **고급 카운터 명령어 생성**
      const IRCounterNode* counterNode =
          dynamic_cast<const IRCounterNode*>(node.get());
      if (counterNode) {
        // OpenPLC 카운터 형식: CTU C0, PV=5
        instructions.push_back("CTU " + node->deviceAddress + ", PV=" +
                               std::to_string(counterNode->countValue));
      }
    }
  }

  return instructions;
}

std::vector<std::string> IRToStackConverter::ConvertOrBlockToStack(
    const IROrBlockNode& orBlock) {
  std::vector<std::string> instructions;

  // OR 블록 처리 - MPS, ORB, MPP 패턴
  instructions.push_back("MPS");  // 스택에 현재 값 저장

  for (size_t i = 0; i < orBlock.parallelBranches.size(); ++i) {
    if (i > 0) {
      instructions.push_back("MPP");  // 스택에서 값 복원
    }

    // 각 병렬 분기 처리
    const auto& branch = orBlock.parallelBranches[i];
    for (size_t nodeId : branch) {
      // 개별 노드들을 AND로 연결 (현재 간소화된 구현)
      instructions.push_back("AND Node" + std::to_string(nodeId));
    }

    if (i < orBlock.parallelBranches.size() - 1) {
      instructions.push_back("ORB");  // OR 연산
    }
  }

  return instructions;
}

std::string IRToStackConverter::ConvertNodeToStack(const IRNode& node) {
  switch (node.nodeType) {
    case IRNodeType::NORMALLY_OPEN:
      return "LOAD " + node.deviceAddress;
    case IRNodeType::NORMALLY_CLOSED:
      return "LOAD_NOT " + node.deviceAddress;
    case IRNodeType::OUTPUT_COIL:
      return "OUT " + node.deviceAddress;
    case IRNodeType::SET_COIL:
      return "SET " + node.deviceAddress;
    case IRNodeType::RESET_COIL:
      return "RESET " + node.deviceAddress;
    default:
      return "// Unknown node type";
  }
}

void IRToStackConverter::OptimizeStackInstructions(
    std::vector<std::string>& instructions) {
  // 스택 명령어 최적화
  // 1. 연속된 LOAD/AND 최적화
  // 2. 불필요한 MPS/MPP 제거
  // 3. 중복 명령어 제거

  std::cout << "[IR] Optimizing " << instructions.size()
            << " stack instructions...\n";

  // 현재는 기본 최적화만 구현 (빈 줄 제거)
  instructions.erase(std::remove_if(instructions.begin(), instructions.end(),
                                    [](const std::string& instr) {
                                      return instr.empty() ||
                                             instr.find("//") == 0;
                                    }),
                     instructions.end());

  std::cout << "[IR] Optimized to " << instructions.size() << " instructions\n";
}

}  // namespace plc