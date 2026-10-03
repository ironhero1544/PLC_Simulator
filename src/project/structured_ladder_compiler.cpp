#include "plc_emulator/project/structured_ladder_compiler.h"

#include "plc_emulator/programming/ladder_program.h"

#include <algorithm>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <utility>

namespace plc_emulator::programming {
namespace {
using CellType = plc::LadderInstructionType;

bool IsOutput(CellType type) {
  return type != CellType::EMPTY && type != CellType::HLINE &&
         type != CellType::XIC && type != CellType::XIO &&
         type != CellType::COMPARISON && type != CellType::RISING_CONTACT &&
         type != CellType::FALLING_CONTACT &&
         type != CellType::kWrappingSource &&
         type != CellType::kWrappingDestination;
}

// A bus is a connected set of grid boundaries at the same column.
// Memoizing each bus builds a DAG, without enumerating paths.
class ConditionBuilder {
 public:
  ConditionBuilder(const plc::LadderProgram& ladder, int width)
      : ladder_(ladder),
        width_(width),
        buses_(ladder.rungs.size() * (width + 1)),
        networks_(ladder.rungs.size()) {
    std::iota(buses_.begin(), buses_.end(), 0);
    std::iota(networks_.begin(), networks_.end(), 0);
    std::map<std::string, std::pair<int, int>> destinations;
    for (int row = 0; row < static_cast<int>(ladder.rungs.size()); ++row) {
      const auto& rung = ladder.rungs[row];
      if (rung.isEndRung)
        continue;
      for (int column = 0; column < static_cast<int>(rung.cells.size());
           ++column) {
        const auto& cell = rung.cells[column];
        if (cell.type != CellType::kWrappingSource &&
            cell.type != CellType::kWrappingDestination)
          continue;
        const auto number = ParseOperand(cell.address);
        if (!number || number->kind != OperandKind::kImmediateDecimal ||
            number->immediate < 0 || cell.address.front() != 'K')
          throw std::runtime_error("Invalid wrapping number");
        const bool source = cell.type == CellType::kWrappingSource;
        if ((source && column == 0) || (!source && column != 0))
          throw std::runtime_error("Invalid wrapping symbol column");
        auto& symbols = source ? wrapping_sources_ : destinations;
        if (!symbols.emplace(FormatOperand(*number), std::pair(row, column))
                 .second)
          throw std::runtime_error("Duplicate wrapping symbol: " +
                                   cell.address);
      }
    }
    if (wrapping_sources_.size() != destinations.size())
      throw std::runtime_error("Unpaired wrapping symbol");
    for (const auto& [number, destination] : destinations) {
      const auto found = wrapping_sources_.find(number);
      if (found == wrapping_sources_.end())
        throw std::runtime_error("Unpaired wrapping symbol: " + number);
      wrapping_destinations_[destination.first] = found->second;
    }
    for (const plc::VerticalConnection& connection :
         ladder.verticalConnections) {
      if (connection.x < 0 || connection.x > width ||
          connection.rungs.size() < 2) {
        throw std::runtime_error("Invalid vertical connection");
      }
      int first = -1;
      for (const int row : connection.rungs) {
        if (row < 0 || row >= static_cast<int>(ladder.rungs.size()) ||
            ladder.rungs[row].isEndRung) {
          throw std::runtime_error(
              "Vertical connection references invalid rung");
        }
        const int bus = Root(Index(row, connection.x));
        if (first < 0)
          first = bus;
        buses_[bus] = Root(first);
        networks_[Network(row)] = Network(connection.rungs.front());
      }
    }
    for (const auto& [row, source] : wrapping_destinations_) {
      networks_[Network(row)] = Network(source.first);
    }
  }

  std::vector<LogicGate> Build(int row, int column) {
    const int network = Network(row);
    if (network != group_) {
      gates_.clear();
      observations_.clear();
      cache_.clear();
      group_ = network;
    }
    root_ = Boundary(row, column);
    return gates_;
  }
  uint32_t root() const { return root_; }
  int group() const { return group_; }
  const std::vector<CellObservation>& observations() const {
    return observations_;
  }

 private:
  int Index(int row, int column) const { return row * (width_ + 1) + column; }
  int Root(int index) {
    while (buses_[index] != index) {
      buses_[index] = buses_[buses_[index]];
      index = buses_[index];
    }
    return index;
  }
  int Network(int row) {
    while (networks_[row] != row)
      row = networks_[row];
    return row;
  }
  uint32_t Add(LogicGate gate) {
    gates_.push_back(gate);
    return static_cast<uint32_t>(gates_.size() - 1);
  }
  uint32_t Boundary(int row, int column) {
    const int bus = Root(Index(row, column));
    if (const auto found = cache_.find(bus); found != cache_.end()) {
      return found->second;
    }
    if (!visiting_.insert(bus).second)
      throw std::runtime_error("Cyclic wrapping topology");
    if (column == 0) {
      const uint32_t result = Add({GateKind::kConstant, 0, 0, true});
      cache_[bus] = result;
      visiting_.erase(bus);
      return result;
    }
    std::optional<uint32_t> combined;
    for (int source = 0; source < static_cast<int>(ladder_.rungs.size());
         ++source) {
      const plc::Rung& rung = ladder_.rungs[source];
      if (rung.isEndRung || Root(Index(source, column)) != bus ||
          column > static_cast<int>(rung.cells.size()))
        continue;
      const plc::LadderInstruction& cell = rung.cells[column - 1];
      if (cell.type == CellType::EMPTY)
        continue;
      if (IsOutput(cell.type)) {
        throw std::runtime_error("Output cannot feed a series contact");
      }
      if (cell.type == CellType::kWrappingSource)
        throw std::runtime_error("Instruction after wrapping source");
      uint32_t value;
      if (cell.type == CellType::kWrappingDestination) {
        const auto continuation = wrapping_destinations_.find(source);
        if (continuation == wrapping_destinations_.end())
          throw std::runtime_error("Unpaired wrapping destination");
        value =
            Boundary(continuation->second.first, continuation->second.second);
      } else
        value = Boundary(source, column - 1);
      if (cell.type == CellType::COMPARISON) {
        if (cell.mnemonic == "INV" || cell.mnemonic == "MEP" ||
            cell.mnemonic == "MEF") {
          const GateKind kind = cell.mnemonic == "INV"   ? GateKind::kNot
                                : cell.mnemonic == "MEP" ? GateKind::kRising
                                                         : GateKind::kFalling;
          value = Add({kind, value});
        } else {
          if (cell.operands.size() != 2)
            throw std::runtime_error("Comparison operands missing");
          LogicGate comparison;
          if (cell.mnemonic == "=")
            comparison.kind = GateKind::kEqual;
          else if (cell.mnemonic == "<>")
            comparison.kind = GateKind::kNotEqual;
          else if (cell.mnemonic == ">")
            comparison.kind = GateKind::kGreater;
          else if (cell.mnemonic == "<")
            comparison.kind = GateKind::kLess;
          else if (cell.mnemonic == ">=")
            comparison.kind = GateKind::kGreaterEqual;
          else if (cell.mnemonic == "<=")
            comparison.kind = GateKind::kLessEqual;
          else
            throw std::runtime_error("Unsupported comparison");
          comparison.first = cell.operands[0];
          comparison.second = cell.operands[1];
          comparison.wide = cell.wide;
          value = Add({GateKind::kAnd, value, Add(comparison)});
        }
      }
      if (cell.type == CellType::XIC || cell.type == CellType::XIO ||
          cell.type == CellType::RISING_CONTACT ||
          cell.type == CellType::FALLING_CONTACT) {
        const auto operand = ParseOperand(cell.address);
        if (!operand || operand->kind != OperandKind::kBitDevice) {
          throw std::runtime_error("Invalid contact address: " + cell.address);
        }
        LogicGate contact{GateKind::kContact};
        contact.first = *operand;
        uint32_t condition = Add(contact);
        if (cell.type == CellType::XIO) {
          condition = Add({GateKind::kNot, condition});
        }
        if (cell.type == CellType::RISING_CONTACT ||
            cell.type == CellType::FALLING_CONTACT) {
          condition =
              Add({cell.type == CellType::RISING_CONTACT ? GateKind::kRising
                                                         : GateKind::kFalling,
                   condition});
        }
        value = Add({GateKind::kAnd, value, condition});
      }
      observations_.push_back({source, column - 1, value, {}});
      combined = combined ? Add({GateKind::kOr, *combined, value}) : value;
    }
    const uint32_t result =
        combined ? *combined : Add({GateKind::kConstant, 0, 0, false});
    cache_[bus] = result;
    visiting_.erase(bus);
    return result;
  }

  const plc::LadderProgram& ladder_;
  int width_;
  std::vector<int> buses_;
  std::vector<int> networks_;
  uint32_t root_ = 0;
  int group_ = -1;
  std::vector<LogicGate> gates_;
  std::vector<CellObservation> observations_;
  std::map<int, uint32_t> cache_;
  std::set<int> visiting_;
  std::map<std::string, std::pair<int, int>> wrapping_sources_;
  std::map<int, std::pair<int, int>> wrapping_destinations_;
};

OpenPLCInstruction ConvertOutput(const plc::LadderInstruction& cell) {
  std::string_view mnemonic;
  switch (cell.type) {
    case CellType::OTE:
      mnemonic = "OUT";
      break;
    case CellType::SET:
      mnemonic = "SET";
      break;
    case CellType::RST:
    case CellType::RST_TMR_CTR:
      mnemonic = "RST";
      break;
    case CellType::TON:
      mnemonic = "TON";
      break;
    case CellType::CTU:
      mnemonic = "CTU";
      break;
    case CellType::APPLICATION:
      mnemonic = cell.mnemonic;
      break;
    case CellType::BKRST:
      mnemonic = "ZRST";
      break;
    default:
      throw std::runtime_error("Unsupported output instruction");
  }
  const InstructionDef* definition = FindInstruction(mnemonic);
  if (!definition)
    throw std::runtime_error("Missing instruction definition");
  OpenPLCInstruction instruction;
  instruction.opcode = definition->opcode;
  instruction.pulse = cell.pulse;
  instruction.wide = cell.wide;
  instruction.operand_count = definition->operand_count;
  if (instruction.operand_count > 0) {
    const auto device = ParseOperand(cell.address);
    if (!device)
      throw std::runtime_error("Invalid output address: " + cell.address);
    instruction.operands[0] = *device;
  }
  if (instruction.operand_count > 1 && cell.operands.empty()) {
    const auto preset = ParseOperand(cell.preset);
    if (!preset)
      throw std::runtime_error("Invalid preset: " + cell.preset);
    instruction.operands[1] = *preset;
  }
  if (!cell.operands.empty()) {
    instruction.operand_count = static_cast<uint8_t>(cell.operands.size());
    if (cell.operands.size() > kMaxOperands)
      throw std::runtime_error("Too many operands");
    std::copy(cell.operands.begin(), cell.operands.end(),
              instruction.operands.begin());
  }
  if (cell.type == CellType::BKRST) {
    const auto count = ParseOperand(cell.preset);
    if (!count || count->kind != OperandKind::kImmediateDecimal ||
        count->immediate <= 0) {
      throw std::runtime_error("Invalid BKRST count");
    }
    instruction.operands[1] = instruction.operands[0];
    instruction.operands[1].device.index += count->immediate - 1;
  }
  std::string error;
  if (!ValidateOperands(
          *definition, {instruction.operands.data(), instruction.operand_count},
          &error)) {
    throw std::runtime_error(error);
  }
  return instruction;
}
}  // namespace

size_t LadderFingerprint(const plc::LadderProgram& ladder) {
  size_t fingerprint = 0;
  const auto combine = [&fingerprint](size_t value) {
    fingerprint ^= value + 0x9e3779b9 + (fingerprint << 6) + (fingerprint >> 2);
  };
  for (const auto& rung : ladder.rungs) {
    combine(rung.isEndRung);
    if (rung.isEndRung)
      continue;
    size_t used = rung.cells.size();
    while (used > 0 && rung.cells[used - 1].type == CellType::EMPTY)
      --used;
    combine(used);
    for (size_t i = 0; i < used; ++i) {
      const auto& cell = rung.cells[i];
      combine(static_cast<size_t>(cell.type));
      combine(std::hash<std::string>{}(cell.address));
      combine(std::hash<std::string>{}(cell.preset));
      combine(std::hash<std::string>{}(cell.mnemonic));
      combine(cell.wide);
      combine(cell.pulse);
      for (const auto& operand : cell.operands)
        combine(std::hash<std::string>{}(FormatOperand(operand)));
    }
  }
  for (const auto& connection : ladder.verticalConnections) {
    combine(connection.x);
    for (const int row : connection.rungs)
      combine(row);
  }
  return fingerprint;
}

bool CompileLadder(const plc::LadderProgram& ladder, ExecutionProgram* program,
                   std::string* error) {
  if (!program)
    return false;
  if (ladder.canonical_program &&
      ladder.canonical_fingerprint == LadderFingerprint(ladder)) {
    if (!ValidateProgram(*ladder.canonical_program, error))
      return false;
    *program = *ladder.canonical_program;
    return true;
  }
  try {
    int width = 0;
    for (const plc::Rung& rung : ladder.rungs) {
      width = std::max(width, static_cast<int>(rung.cells.size()));
    }
    if (width > 512 || ladder.rungs.size() > 4096) {
      throw std::runtime_error("Ladder exceeds supported editor dimensions");
    }
    ConditionBuilder builder(ladder, width);
    ExecutionProgram candidate;
    for (int row = 0; row < static_cast<int>(ladder.rungs.size()); ++row) {
      const plc::Rung& rung = ladder.rungs[row];
      if (rung.isEndRung)
        break;
      for (int column = 0; column < static_cast<int>(rung.cells.size());
           ++column) {
        const plc::LadderInstruction& cell = rung.cells[column];
        if (!IsOutput(cell.type))
          continue;
        OpenPLCInstruction instruction = ConvertOutput(cell);
        instruction.rung = row;
        instruction.cell = column;
        if (IsIndependentInstruction(instruction.opcode)) {
          instruction.condition = {{GateKind::kConstant, 0, 0, true}};
          instruction.condition_root = 0;
          candidate.instructions.push_back(std::move(instruction));
          continue;
        }
        instruction.condition = builder.Build(row, column);
        instruction.observations = builder.observations();
        instruction.condition_root = builder.root();
        instruction.evaluation_group = builder.group();
        // A structurally disconnected output is an error, even when its
        // current contact values would happen to make it false.
        std::vector<bool> reachable(instruction.condition.size());
        for (size_t i = 0; i < reachable.size(); ++i) {
          const LogicGate& gate = instruction.condition[i];
          switch (gate.kind) {
            case GateKind::kConstant:
              reachable[i] = gate.constant;
              break;
            case GateKind::kContact:
            case GateKind::kEqual:
            case GateKind::kNotEqual:
            case GateKind::kGreater:
            case GateKind::kLess:
            case GateKind::kGreaterEqual:
            case GateKind::kLessEqual:
              reachable[i] = true;
              break;
            case GateKind::kRising:
            case GateKind::kFalling:
            case GateKind::kNot:
              reachable[i] = reachable[gate.left];
              break;
            case GateKind::kAnd:
              reachable[i] = reachable[gate.left] && reachable[gate.right];
              break;
            case GateKind::kOr:
              reachable[i] = reachable[gate.left] || reachable[gate.right];
              break;
            default:
              break;
          }
        }
        if (!reachable[instruction.condition_root]) {
          throw std::runtime_error("Disconnected output at rung " +
                                   std::to_string(row + 1));
        }
        candidate.instructions.push_back(std::move(instruction));
      }
    }
    if (!ValidateProgram(candidate, error))
      return false;
    *program = std::move(candidate);
    return true;
  } catch (const std::exception& exception) {
    if (error)
      *error = exception.what();
    return false;
  }
}
}  // namespace plc_emulator::programming
