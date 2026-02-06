// ladder_ir.cpp

#include "plc_emulator/project/ladder_ir.h"

#include "plc_emulator/programming/programming_mode.h"

#include <algorithm>
#include <iostream>
#include <queue>
#include <sstream>

namespace plc {

// ============================================================================
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
// ============================================================================

void IRRung::AddNode(std::unique_ptr<IRNode> node) {
  if (node) {
    nodes.push_back(std::move(node));
  }
}

void IRRung::AddConnection(size_t fromId, size_t toId, int outPort,
                           int inPort) {
  connections.emplace_back(fromId, toId, outPort, inPort);

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

  std::map<std::pair<size_t, size_t>, std::vector<size_t>> parallelGroups;

  for (const auto& node : nodes) {
    if (node->IsInputNode() && !node->inputs.empty() &&
        !node->outputs.empty()) {
      size_t inputNode = node->inputs[0];
      size_t outputNode = node->outputs[0];

      parallelGroups[{inputNode, outputNode}].push_back(node->nodeId);
    }
  }

  for (const auto& [inputOutput, nodeGroup] : parallelGroups) {
    if (nodeGroup.size() >= 2) {
      std::cout << "[IR] Found parallel group: " << nodeGroup.size()
                << " nodes\n";
    }
  }
}

// ============================================================================
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
  std::cout << "[IR] Optimizing connections...\n";

  for (auto& rung : rungs_) {
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
// ============================================================================

LadderIRProgram LadderToIRConverter::ConvertToIR(
    const LadderProgram& ladderProgram) {
  std::cout << "[IR] Converting LadderProgram to IR...\n";
  std::cout << "[IR] Input: " << ladderProgram.rungs.size() << " rungs\n";

  LadderIRProgram irProgram;

  for (size_t i = 0; i < ladderProgram.rungs.size(); ++i) {
    const auto& rung = ladderProgram.rungs[i];
    std::cout << "[IR] Converting rung " << i << " with " << rung.cells.size()
              << " cells\n";

    IRRung irRung = ConvertRungToIR(rung, irProgram);

    if (!ladderProgram.verticalConnections.empty()) {
      AnalyzeVerticalConnections(ladderProgram.verticalConnections, irRung);
    }

    AnalyzeLogicFlow(irRung);

    irProgram.AddRung(std::move(irRung));
  }

  irProgram.AnalyzeLogicStructure();
  irProgram.OptimizeConnections();

  std::cout << "[IR] Conversion completed.\n";
  return irProgram;
}

IRRung LadderToIRConverter::ConvertRungToIR(const Rung& rung,
                                            LadderIRProgram& irProgram) {
  IRRung irRung;

  irRung.startNodeId = 0;

  for (size_t j = 0; j < rung.cells.size(); ++j) {
    const auto& cell = rung.cells[j];

    if (cell.type != LadderInstructionType::EMPTY) {
      size_t nodeId = irProgram.GenerateNodeId();

      auto node = ConvertCellToNode(cell, nodeId, static_cast<int>(0),
                                    static_cast<int>(j));
      if (node) {
        if (!node->deviceAddress.empty()) {
          irProgram.MapDeviceToNode(node->deviceAddress, nodeId);
        }

        irRung.AddNode(std::move(node));
      }
    }
  }

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

      if (!cell.preset.empty()) {
        if (cell.preset[0] == 'K' || cell.preset[0] == 'k' ||
            (cell.preset[0] >= '0' && cell.preset[0] <= '9')) {
          try {
            int timerValue = std::stoi(
                (cell.preset[0] == 'K' || cell.preset[0] == 'k')
                    ? cell.preset.substr(1)
                    : cell.preset);
            timerNode->timeValue =
                static_cast<float>(timerValue) * 0.1f;
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

      if (!cell.preset.empty()) {
        if (cell.preset[0] == 'K' || cell.preset[0] == 'k' ||
            (cell.preset[0] >= '0' && cell.preset[0] <= '9')) {
          try {
            int countValue = std::stoi(
                (cell.preset[0] == 'K' || cell.preset[0] == 'k')
                    ? cell.preset.substr(1)
                    : cell.preset);
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

  for (const auto& vconn : verticalConnections) {
    std::cout << "[IR] Vertical connection at x=" << vconn.x
              << ", connecting rungs: ";
    for (size_t i = 0; i < vconn.rungs.size(); ++i) {
      if (i > 0)
        std::cout << ",";
      std::cout << vconn.rungs[i];
    }
    std::cout << "\n";

    std::vector<size_t> parallelNodes;
    for (const auto& node : irRung.nodes) {
      if (node->cellIndex == vconn.x) {
        parallelNodes.push_back(node->nodeId);
      }
    }

    if (parallelNodes.size() >= 2) {
      std::cout << "[IR] Creating OR block with " << parallelNodes.size()
                << " nodes\n";
    }
  }
}

void LadderToIRConverter::AnalyzeLogicFlow(IRRung& irRung) {
  std::cout << "[IR] Analyzing logic flow for rung with " << irRung.nodes.size()
            << " nodes\n";

}

// ============================================================================
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

    if (i < irProgram.GetRungCount() - 1) {
      allInstructions.push_back("");
    }
  }

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

  bool isFirstLoad = true;

  for (const auto& node : rung.nodes) {
    if (node->IsInputNode()) {
      if (isFirstLoad) {
        if (node->nodeType == IRNodeType::NORMALLY_OPEN) {
          instructions.push_back("LOAD " + node->deviceAddress);
        } else if (node->nodeType == IRNodeType::NORMALLY_CLOSED) {
          instructions.push_back("LOAD_NOT " + node->deviceAddress);
        }
        isFirstLoad = false;
      } else {
        if (node->nodeType == IRNodeType::NORMALLY_OPEN) {
          instructions.push_back("AND " + node->deviceAddress);
        } else if (node->nodeType == IRNodeType::NORMALLY_CLOSED) {
          instructions.push_back("AND_NOT " + node->deviceAddress);
        }
      }
    } else if (node->IsOutputNode()) {
      if (node->nodeType == IRNodeType::OUTPUT_COIL) {
        instructions.push_back("OUT " + node->deviceAddress);
      } else if (node->nodeType == IRNodeType::SET_COIL) {
        instructions.push_back("SET " + node->deviceAddress);
      } else if (node->nodeType == IRNodeType::RESET_COIL) {
        instructions.push_back("RESET " + node->deviceAddress);
      }
    } else if (node->nodeType == IRNodeType::TIMER_ON) {
      const IRTimerNode* timerNode =
          dynamic_cast<const IRTimerNode*>(node.get());
      if (timerNode) {
        int timeMs = static_cast<int>(timerNode->timeValue * 1000);
        instructions.push_back("TON " + node->deviceAddress + ", PT=T#" +
                               std::to_string(timeMs) + "ms");
      }
    } else if (node->nodeType == IRNodeType::COUNTER_UP) {
      const IRCounterNode* counterNode =
          dynamic_cast<const IRCounterNode*>(node.get());
      if (counterNode) {
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

  instructions.push_back("MPS");

  for (size_t i = 0; i < orBlock.parallelBranches.size(); ++i) {
    if (i > 0) {
      instructions.push_back("MPP");
    }

    const auto& branch = orBlock.parallelBranches[i];
    for (size_t nodeId : branch) {
      instructions.push_back("AND Node" + std::to_string(nodeId));
    }

    if (i < orBlock.parallelBranches.size() - 1) {
      instructions.push_back("ORB");
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

  std::cout << "[IR] Optimizing " << instructions.size()
            << " stack instructions...\n";

  instructions.erase(std::remove_if(instructions.begin(), instructions.end(),
                                    [](const std::string& instr) {
                                      return instr.empty() ||
                                             instr.find("//") == 0;
                                    }),
                     instructions.end());

  std::cout << "[IR] Optimized to " << instructions.size() << " instructions\n";
}

}  // namespace plc
