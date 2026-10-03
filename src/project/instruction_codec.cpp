#include "plc_emulator/project/instruction_codec.h"

#include "plc_emulator/project/structured_ladder_compiler.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <sstream>
#include <stdexcept>

namespace plc_emulator::programming {
namespace {
using Type = plc::LadderInstructionType;

std::string Upper(std::string text) {
  for (char& ch : text) {
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  }
  return text;
}

GateKind Comparison(std::string_view operation) {
  if (operation == "=")
    return GateKind::kEqual;
  if (operation == "<>")
    return GateKind::kNotEqual;
  if (operation == ">")
    return GateKind::kGreater;
  if (operation == "<")
    return GateKind::kLess;
  if (operation == ">=")
    return GateKind::kGreaterEqual;
  if (operation == "<=")
    return GateKind::kLessEqual;
  throw std::runtime_error("Unknown comparison");
}

std::string ComparisonName(GateKind kind) {
  switch (kind) {
    case GateKind::kEqual:
      return "=";
    case GateKind::kNotEqual:
      return "<>";
    case GateKind::kGreater:
      return ">";
    case GateKind::kLess:
      return "<";
    case GateKind::kGreaterEqual:
      return ">=";
    case GateKind::kLessEqual:
      return "<=";
    default:
      return "";
  }
}

Operand ParseRequired(const std::string& text) {
  const auto operand = ParseOperand(text);
  if (!operand)
    throw std::runtime_error("Invalid operand: " + text);
  return *operand;
}

std::vector<uint8_t> SavedConditionUses(const std::vector<LogicGate>& gates,
                                        uint32_t base) {
  std::vector<uint8_t> uses(gates.size());
  for (size_t index = 0; index < gates.size(); ++index) {
    const LogicGate& gate = gates[index];
    if (index == base)
      uses[index] = 1;
    else if (gate.kind == GateKind::kNot || gate.kind == GateKind::kRising ||
             gate.kind == GateKind::kFalling)
      uses[index] = uses[gate.left];
    else if (gate.kind == GateKind::kAnd || gate.kind == GateKind::kOr) {
      uses[index] = gate.left == gate.right
                        ? uses[gate.left]
                        : static_cast<uint8_t>(
                              std::min(2, uses[gate.left] + uses[gate.right]));
    }
  }
  return uses;
}

OpenPLCInstruction ParseOutput(InstructionStatement statement) {
  statement.mnemonic = Upper(statement.mnemonic);
  OpenPLCInstruction instruction;
  std::string name = statement.mnemonic;
  if (name.empty())
    throw std::runtime_error("Instruction is empty");
  if (name == "ST")
    name = "OUT";
  if (name == "S")
    name = "SET";
  if (name == "R")
    name = "RST";
  const bool base_instruction =
      FindInstruction(name) ||
      (name.front() == 'D' && FindInstruction(name.substr(1)));
  if (!base_instruction && name.size() > 1 && name.back() == 'P') {
    name.pop_back();
    instruction.pulse = true;
  }
  const InstructionDef* definition = FindInstruction(name);
  if (!definition && name.size() > 1 && name.front() == 'D') {
    name.erase(name.begin());
    definition = FindInstruction(name);
    instruction.wide = true;
  }
  if (name == "OUT" && statement.operands.size() == 2) {
    const Operand device = ParseRequired(statement.operands[0]);
    if (device.device.kind == DeviceKind::kT)
      definition = FindInstruction("TON");
    else if (device.device.kind == DeviceKind::kC)
      definition = FindInstruction("CTU");
  }
  if (!definition)
    throw std::runtime_error("Unknown instruction: " + statement.mnemonic);
  instruction.opcode = definition->opcode;
  instruction.operand_count = static_cast<uint8_t>(statement.operands.size());
  if (statement.operands.size() > kMaxOperands)
    throw std::runtime_error("Too many operands");
  for (size_t i = 0; i < statement.operands.size(); ++i) {
    if (instruction.opcode == Opcode::kAsciiConstant && i == 0 &&
        !statement.operands[i].empty() && statement.operands[i].front() != '"')
      instruction.operands[i] =
          ParseRequired("\"" + statement.operands[i] + "\"");
    else
      instruction.operands[i] = ParseRequired(statement.operands[i]);
  }
  std::string error;
  if (!ValidateOperands(
          *definition, {instruction.operands.data(), instruction.operand_count},
          &error)) {
    throw std::runtime_error(error);
  }
  return instruction;
}

plc::LadderInstruction OutputCell(const OpenPLCInstruction& instruction) {
  plc::LadderInstruction cell;
  switch (instruction.opcode) {
    case Opcode::kOut:
      cell.type = Type::OTE;
      break;
    case Opcode::kSet:
      cell.type = Type::SET;
      break;
    case Opcode::kReset:
      cell.type = Type::RST;
      break;
    case Opcode::kTimerOn:
      cell.type = Type::TON;
      break;
    case Opcode::kCounterUp:
      cell.type = Type::CTU;
      break;
    default:
      cell.type = Type::APPLICATION;
      break;
  }
  for (const InstructionDef& definition : GetInstructionDefinitions()) {
    if (definition.opcode == instruction.opcode) {
      cell.mnemonic = std::string(definition.mnemonic);
    }
  }
  cell.operands.assign(
      instruction.operands.begin(),
      instruction.operands.begin() + instruction.operand_count);
  if (instruction.operand_count > 0)
    cell.address = FormatOperand(instruction.operands[0]);
  if (instruction.operand_count > 1)
    cell.preset = FormatOperand(instruction.operands[1]);
  cell.wide = instruction.wide;
  cell.pulse = instruction.pulse;
  return cell;
}
}  // namespace

std::vector<std::string> TokenizeInstructionText(std::string_view text) {
  std::vector<std::string> tokens;
  size_t position = 0;
  while (position < text.size()) {
    if (std::isspace(static_cast<unsigned char>(text[position]))) {
      ++position;
      continue;
    }
    const size_t start = position;
    if (text[position] == '"') {
      ++position;
      bool closed = false;
      while (position < text.size()) {
        if (text[position++] != '"')
          continue;
        if (position < text.size() && text[position] == '"') {
          ++position;
          continue;
        }
        closed = true;
        break;
      }
      if (!closed ||
          (position < text.size() &&
           !std::isspace(static_cast<unsigned char>(text[position]))))
        throw std::runtime_error("Invalid quoted string operand");
    } else {
      while (position < text.size() &&
             !std::isspace(static_cast<unsigned char>(text[position])))
        ++position;
    }
    tokens.emplace_back(text.substr(start, position - start));
  }
  for (size_t index = 0; index + 2 < tokens.size(); ++index) {
    if ((tokens[index].front() == 'E' || tokens[index].front() == 'e') &&
        (tokens[index + 1] == "+" || tokens[index + 1] == "-")) {
      tokens[index] += tokens[index + 1] + tokens[index + 2];
      tokens.erase(tokens.begin() + index + 1, tokens.begin() + index + 3);
    }
  }
  return tokens;
}

bool CompileStatements(const std::vector<InstructionStatement>& statements,
                       ExecutionProgram* program, std::string* error) {
  if (!program)
    return false;
  try {
    ExecutionProgram candidate;
    std::vector<LogicGate> gates;
    std::vector<uint32_t> blocks;
    std::vector<uint32_t> branches;
    std::optional<uint32_t> current;
    bool after_output = true;
    bool ended = false;
    int group = -1;
    const auto add = [&gates](LogicGate gate) {
      gates.push_back(gate);
      return static_cast<uint32_t>(gates.size() - 1);
    };
    for (const InstructionStatement& statement : statements) {
      std::string name = Upper(statement.mnemonic);
      InstructionStatement action_statement = statement;
      const auto label = ParseOperand(name);
      if (label && (label->kind == OperandKind::kPointer ||
                    label->kind == OperandKind::kInterruptPointer)) {
        if (!statement.operands.empty())
          throw std::runtime_error("Label cannot have operands");
        action_statement = {"LABEL", {name}};
        name = "LABEL";
      }
      if (ended)
        throw std::runtime_error("Instruction after END");
      bool wide_contact = false;
      if (name.rfind("DLD", 0) == 0 || name.rfind("DAND", 0) == 0 ||
          name.rfind("DOR", 0) == 0) {
        wide_contact = true;
        name.erase(name.begin());
      }
      const bool load = name.rfind("LD", 0) == 0;
      const bool and_contact =
          name.rfind("AND", 0) == 0 || name == "ANI" || name == "ANDN";
      const bool or_contact =
          name.rfind("OR", 0) == 0 || name == "ORI" || name == "ORN";
      if (name == "END") {
        if (!statement.operands.empty() || !blocks.empty() ||
            !branches.empty()) {
          throw std::runtime_error("Unbalanced branch at END");
        }
        ended = true;
        candidate.source_topology.push_back({SequenceKind::kEnd});
        continue;
      }
      if (name == "ANB" || name == "ORB" || name == "INV" || name == "MEP" ||
          name == "MEF" || name == "MPS" || name == "MRD" || name == "MPP") {
        if (!current || !statement.operands.empty()) {
          throw std::runtime_error("Invalid branch instruction: " + name);
        }
        CanonicalStatement saved;
        saved.kind = name == "MEP"   ? SequenceKind::kRising
                     : name == "MEF" ? SequenceKind::kFalling
                     : name == "INV" ? SequenceKind::kInvert
                     : name == "ANB" ? SequenceKind::kAndBlock
                     : name == "ORB" ? SequenceKind::kOrBlock
                     : name == "MPS" ? SequenceKind::kPush
                     : name == "MRD" ? SequenceKind::kRead
                                     : SequenceKind::kPop;
        candidate.source_topology.push_back(saved);
        if (name == "INV" || name == "MEP" || name == "MEF")
          current = add({name == "INV"   ? GateKind::kNot
                         : name == "MEP" ? GateKind::kRising
                                         : GateKind::kFalling,
                         *current});
        else if (name == "ANB" || name == "ORB") {
          if (blocks.empty())
            throw std::runtime_error("Logic block underflow");
          current = add({name == "ANB" ? GateKind::kAnd : GateKind::kOr,
                         blocks.back(), *current});
          blocks.pop_back();
        } else if (name == "MPS")
          branches.push_back(*current);
        else {
          if (branches.empty())
            throw std::runtime_error("Branch stack underflow");
          current = branches.back();
          if (name == "MPP")
            branches.pop_back();
        }
        after_output = false;
        continue;
      }
      if (load || and_contact || or_contact) {
        if (load && after_output && branches.empty() && blocks.empty()) {
          gates.clear();
          current.reset();
        }
        if (!load && !current)
          throw std::runtime_error("Contact before LD");
        std::string suffix = name.substr(load ? 2 : and_contact ? 3 : 2);
        if (name == "ANI" || name == "ORI")
          suffix = "I";
        uint32_t contact;
        CanonicalStatement saved;
        saved.kind = load          ? SequenceKind::kLoad
                     : and_contact ? SequenceKind::kAnd
                                   : SequenceKind::kOr;
        if (suffix.empty() || suffix == "I" || suffix == "N" || suffix == "P" ||
            suffix == "F") {
          if (wide_contact)
            throw std::runtime_error("D prefix requires a comparison contact");
          if (statement.operands.size() != 1)
            throw std::runtime_error("Contact needs one operand");
          LogicGate gate{GateKind::kContact};
          if (Upper(statement.operands[0]) == "TRUE" ||
              Upper(statement.operands[0]) == "FALSE") {
            gate.kind = GateKind::kConstant;
            gate.constant = Upper(statement.operands[0]) == "TRUE";
          } else
            gate.first = ParseRequired(statement.operands[0]);
          saved.contact = gate;
          saved.inverted = suffix == "I" || suffix == "N";
          saved.rising = suffix == "P";
          saved.falling = suffix == "F";
          contact = add(gate);
          if (suffix == "I" || suffix == "N")
            contact = add({GateKind::kNot, contact});
          if (suffix == "P" || suffix == "F") {
            contact =
                add({suffix == "P" ? GateKind::kRising : GateKind::kFalling,
                     contact});
          }
        } else {
          if (statement.operands.size() != 2)
            throw std::runtime_error("Comparison needs two operands");
          LogicGate gate{Comparison(suffix)};
          gate.first = ParseRequired(statement.operands[0]);
          gate.second = ParseRequired(statement.operands[1]);
          gate.wide = wide_contact;
          saved.contact = gate;
          contact = add(gate);
        }
        candidate.source_topology.push_back(saved);
        if (load) {
          if (after_output && branches.empty() && blocks.empty())
            ++group;
          if (current && !after_output)
            blocks.push_back(*current);
          current = contact;
        } else
          current = add({and_contact ? GateKind::kAnd : GateKind::kOr,
                         current.value_or(0), contact});
        after_output = false;
      } else {
        OpenPLCInstruction instruction = ParseOutput(action_statement);
        const bool independent = IsIndependentInstruction(instruction.opcode);
        if (!current && !independent)
          throw std::runtime_error("Output before LD");
        if (independent && (!blocks.empty() || !branches.empty()))
          throw std::runtime_error("Unbalanced branch at routine boundary");
        CanonicalStatement saved;
        saved.kind = SequenceKind::kAction;
        saved.action = instruction;
        candidate.source_topology.push_back(saved);
        if (independent) {
          instruction.condition = {{GateKind::kConstant, 0, 0, true}};
          instruction.condition_root = 0;
          instruction.evaluation_group = -1;
          gates.clear();
          current.reset();
        } else {
          instruction.condition = gates;
          instruction.condition_root = *current;
          instruction.evaluation_group = group;
        }
        instruction.condition_begin = 0;
        candidate.instructions.push_back(std::move(instruction));
        if (candidate.instructions.back().opcode == Opcode::kStep) {
          current = add({GateKind::kConstant, 0, 0, true});
          ++group;
        }
        after_output = true;
      }
    }
    if (!blocks.empty() || !branches.empty())
      throw std::runtime_error("Unbalanced branch");
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

bool MaterializeLadder(const ExecutionProgram& program,
                       plc::LadderProgram* ladder, std::string* error) {
  if (!ladder || !ValidateProgram(program, error))
    return false;
  try {
    plc::LadderProgram candidate;
    candidate.rungs.clear();
    ExecutionProgram displayed_program = program;
    size_t instruction_index = 0;
    for (const OpenPLCInstruction& instruction : program.instructions) {
      auto& displayed = displayed_program.instructions[instruction_index++];
      displayed.observations.clear();
      const auto& gates = instruction.condition;
      std::vector<std::pair<int, int>> sizes(gates.size());
      for (size_t i = 0; i < gates.size(); ++i) {
        const LogicGate& gate = gates[i];
        if (gate.kind == GateKind::kAnd || gate.kind == GateKind::kOr) {
          if (gate.left == gate.right)
            sizes[i] = sizes[gate.left];
          else if (gate.kind == GateKind::kAnd) {
            sizes[i] = {
                sizes[gate.left].first + sizes[gate.right].first,
                std::max(sizes[gate.left].second, sizes[gate.right].second)};
          } else {
            sizes[i] = {
                std::max(sizes[gate.left].first, sizes[gate.right].first),
                sizes[gate.left].second + sizes[gate.right].second};
          }
        } else if ((gate.kind == GateKind::kNot ||
                    gate.kind == GateKind::kRising ||
                    gate.kind == GateKind::kFalling) &&
                   gates[gate.left].kind != GateKind::kContact) {
          sizes[i] = {sizes[gate.left].first + 1, sizes[gate.left].second};
        } else
          sizes[i] = {gate.kind == GateKind::kConstant && gate.constant ? 0 : 1,
                      1};
        if (sizes[i].first > 256 || sizes[i].second > 1024) {
          throw std::runtime_error(
              "Imported topology exceeds editor dimensions");
        }
      }
      const int start_row = static_cast<int>(candidate.rungs.size());
      const uint32_t root = instruction.condition_root == UINT32_MAX
                                ? static_cast<uint32_t>(gates.size() - 1)
                                : instruction.condition_root;
      const int width = sizes[root].first;
      const int height = sizes[root].second;
      const int columns = std::max(12, width + 1);
      candidate.rungs.resize(candidate.rungs.size() + height);
      for (int row = start_row; row < start_row + height; ++row) {
        candidate.rungs[row].cells.resize(columns);
      }
      const auto line = [&candidate](int row, int first, int end) {
        for (int col = first; col < end; ++col) {
          candidate.rungs[row].cells[col].type = Type::HLINE;
        }
      };
      std::vector<uint32_t> incoming_gates;
      std::function<void(uint32_t, int, int)> draw = [&](uint32_t index,
                                                         int row, int column) {
        const LogicGate& gate = gates[index];
        if ((gate.kind == GateKind::kAnd || gate.kind == GateKind::kOr) &&
            gate.left == gate.right) {
          draw(gate.left, row, column);
          return;
        }
        if (gate.kind == GateKind::kAnd) {
          draw(gate.left, row, column);
          incoming_gates.push_back(gate.left);
          draw(gate.right, row, column + sizes[gate.left].first);
          incoming_gates.pop_back();
        } else if (gate.kind == GateKind::kOr) {
          const int lower = row + sizes[gate.left].second;
          draw(gate.left, row, column);
          draw(gate.right, lower, column);
          const int end = column + sizes[index].first;
          line(row, column + sizes[gate.left].first, end);
          line(lower, column + sizes[gate.right].first, end);
          candidate.verticalConnections.emplace_back(column, row, lower);
          candidate.verticalConnections.emplace_back(end, row, lower);
        } else if (gate.kind == GateKind::kConstant && gate.constant) {
          return;
        } else {
          plc::LadderInstruction cell;
          if (gate.kind == GateKind::kContact) {
            cell.type = Type::XIC;
            cell.address = FormatOperand(gate.first);
          } else if ((gate.kind == GateKind::kNot ||
                      gate.kind == GateKind::kRising ||
                      gate.kind == GateKind::kFalling) &&
                     gates[gate.left].kind == GateKind::kContact) {
            cell.type = gate.kind == GateKind::kNot ? Type::XIO
                        : gate.kind == GateKind::kRising
                            ? Type::RISING_CONTACT
                            : Type::FALLING_CONTACT;
            cell.address = FormatOperand(gates[gate.left].first);
          } else if (gate.kind == GateKind::kNot ||
                     gate.kind == GateKind::kRising ||
                     gate.kind == GateKind::kFalling) {
            draw(gate.left, row, column);
            cell.type = Type::COMPARISON;
            cell.mnemonic = gate.kind == GateKind::kNot      ? "INV"
                            : gate.kind == GateKind::kRising ? "MEP"
                                                             : "MEF";
            candidate.rungs[row].cells[column + sizes[gate.left].first] =
                std::move(cell);
            return;
          } else if (gate.kind == GateKind::kConstant) {
            cell.type = gate.constant ? Type::XIC : Type::XIO;
            cell.address = "M8000";
          } else if (!ComparisonName(gate.kind).empty()) {
            cell.type = Type::COMPARISON;
            cell.mnemonic = ComparisonName(gate.kind);
            cell.operands = {gate.first, gate.second};
            cell.wide = gate.wide;
          } else {
            throw std::runtime_error(
                "Topology cannot be represented in editor");
          }
          candidate.rungs[row].cells[column] = std::move(cell);
          displayed.observations.push_back(
              {row, column, index, incoming_gates});
        }
      };
      draw(root, start_row, 0);
      line(start_row, width, columns - 1);
      candidate.rungs[start_row].cells.back() = OutputCell(instruction);
      displayed.rung = start_row;
      displayed.cell = columns - 1;
    }
    for (size_t i = 0; i < candidate.rungs.size(); ++i)
      candidate.rungs[i].number = static_cast<int>(i);
    candidate.rungs.emplace_back();
    candidate.rungs.back().isEndRung = true;
    candidate.canonical_program = std::move(displayed_program);
    candidate.canonical_fingerprint = LadderFingerprint(candidate);
    *ladder = std::move(candidate);
    return true;
  } catch (const std::exception& exception) {
    if (error)
      *error = exception.what();
    return false;
  }
}

std::vector<InstructionStatement> SerializeInstructions(
    const ExecutionProgram& program) {
  std::vector<InstructionStatement> statements;
  if (!program.source_topology.empty()) {
    for (const CanonicalStatement& saved : program.source_topology) {
      InstructionStatement statement;
      switch (saved.kind) {
        case SequenceKind::kLoad:
        case SequenceKind::kAnd:
        case SequenceKind::kOr:
          statement.mnemonic = saved.kind == SequenceKind::kLoad  ? "LD"
                               : saved.kind == SequenceKind::kAnd ? "AND"
                                                                  : "OR";
          if (saved.contact.kind == GateKind::kConstant) {
            statement.operands = {"M8000"};
            if (!saved.contact.constant)
              statement.mnemonic += "I";
          } else {
            if (saved.contact.wide)
              statement.mnemonic = "D" + statement.mnemonic;
            statement.operands = {FormatOperand(saved.contact.first)};
            if (saved.contact.kind != GateKind::kContact) {
              statement.mnemonic += ComparisonName(saved.contact.kind);
              statement.operands.push_back(FormatOperand(saved.contact.second));
            } else if (saved.inverted) {
              statement.mnemonic = saved.kind == SequenceKind::kAnd  ? "ANI"
                                   : saved.kind == SequenceKind::kOr ? "ORI"
                                                                     : "LDI";
            } else if (saved.rising)
              statement.mnemonic += "P";
            else if (saved.falling)
              statement.mnemonic += "F";
          }
          break;
        case SequenceKind::kAndBlock:
          statement.mnemonic = "ANB";
          break;
        case SequenceKind::kOrBlock:
          statement.mnemonic = "ORB";
          break;
        case SequenceKind::kInvert:
          statement.mnemonic = "INV";
          break;
        case SequenceKind::kRising:
          statement.mnemonic = "MEP";
          break;
        case SequenceKind::kFalling:
          statement.mnemonic = "MEF";
          break;
        case SequenceKind::kPush:
          statement.mnemonic = "MPS";
          break;
        case SequenceKind::kRead:
          statement.mnemonic = "MRD";
          break;
        case SequenceKind::kPop:
          statement.mnemonic = "MPP";
          break;
        case SequenceKind::kEnd:
          statement.mnemonic = "END";
          break;
        case SequenceKind::kAction:
          for (const InstructionDef& definition : GetInstructionDefinitions()) {
            if (definition.opcode == saved.action.opcode)
              statement.mnemonic = definition.mnemonic;
          }
          if (saved.action.opcode == Opcode::kTimerOn ||
              saved.action.opcode == Opcode::kCounterUp) {
            statement.mnemonic = "OUT";
          }
          if (saved.action.wide)
            statement.mnemonic = "D" + statement.mnemonic;
          if (saved.action.pulse)
            statement.mnemonic += "P";
          for (size_t i = 0; i < saved.action.operand_count; ++i) {
            statement.operands.push_back(
                FormatOperand(saved.action.operands[i]));
          }
          if (saved.action.opcode == Opcode::kLabel) {
            statement.mnemonic = statement.operands[0];
            statement.operands.clear();
          }
          break;
      }
      statements.push_back(std::move(statement));
    }
    if (statements.empty() || statements.back().mnemonic != "END")
      statements.push_back({"END", {}});
    return statements;
  }
  size_t saved_output_end = 0;
  uint32_t saved_base = UINT32_MAX;
  for (size_t instruction_index = 0;
       instruction_index < program.instructions.size(); ++instruction_index) {
    const OpenPLCInstruction& instruction =
        program.instructions[instruction_index];
    if (IsIndependentInstruction(instruction.opcode)) {
      InstructionStatement statement;
      if (instruction.opcode == Opcode::kLabel)
        statement.mnemonic = FormatOperand(instruction.operands[0]);
      else {
        for (const auto& definition : GetInstructionDefinitions()) {
          if (definition.opcode == instruction.opcode)
            statement.mnemonic = definition.mnemonic;
        }
        for (size_t i = 0; i < instruction.operand_count; ++i)
          statement.operands.push_back(FormatOperand(instruction.operands[i]));
      }
      statements.push_back(std::move(statement));
      saved_output_end = instruction_index + 1;
      saved_base = UINT32_MAX;
      continue;
    }
    const auto& gates = instruction.condition;
    std::function<void(uint32_t)> emit = [&](uint32_t index) {
      const LogicGate& gate = gates[index];
      if (gate.kind == GateKind::kAnd &&
          gates[gate.left].kind == GateKind::kConstant &&
          gates[gate.left].constant) {
        emit(gate.right);
        return;
      }
      if (gate.kind == GateKind::kAnd &&
          gates[gate.right].kind == GateKind::kConstant &&
          gates[gate.right].constant) {
        emit(gate.left);
        return;
      }
      if (gate.kind == GateKind::kAnd || gate.kind == GateKind::kOr) {
        emit(gate.left);
        if (gate.left != gate.right) {
          emit(gate.right);
          statements.push_back(
              {gate.kind == GateKind::kAnd ? "ANB" : "ORB", {}});
        }
      } else if (gate.kind == GateKind::kNot) {
        emit(gate.left);
        statements.push_back({"INV", {}});
      } else if (gate.kind == GateKind::kRising ||
                 gate.kind == GateKind::kFalling) {
        const LogicGate& contact = gates[gate.left];
        if (contact.kind != GateKind::kContact) {
          emit(gate.left);
          statements.push_back(
              {gate.kind == GateKind::kRising ? "MEP" : "MEF", {}});
        } else {
          statements.push_back({gate.kind == GateKind::kRising ? "LDP" : "LDF",
                                {FormatOperand(contact.first)}});
        }
      } else if (gate.kind == GateKind::kContact) {
        statements.push_back({"LD", {FormatOperand(gate.first)}});
      } else if (gate.kind == GateKind::kConstant) {
        statements.push_back({"LD", {"M8000"}});
        if (!gate.constant)
          statements.push_back({"INV", {}});
      } else {
        statements.push_back(
            {(gate.wide ? "DLD" : "LD") + ComparisonName(gate.kind),
             {FormatOperand(gate.first), FormatOperand(gate.second)}});
      }
    };
    const uint32_t root = instruction.condition_root == UINT32_MAX
                              ? static_cast<uint32_t>(gates.size() - 1)
                              : instruction.condition_root;
    if (instruction_index < saved_output_end) {
      statements.push_back(
          {instruction_index + 1 == saved_output_end ? "MPP" : "MRD", {}});
    } else {
      saved_output_end = instruction_index + 1;
      saved_base = UINT32_MAX;
      if (instruction.evaluation_group >= 0) {
        while (saved_output_end < program.instructions.size()) {
          const auto& next = program.instructions[saved_output_end];
          if (next.evaluation_group != instruction.evaluation_group)
            break;
          ++saved_output_end;
        }
      }
      for (size_t base = gates.size();
           base > 0 && saved_output_end > instruction_index + 1; --base) {
        bool shared = true;
        for (size_t output = instruction_index; output < saved_output_end;
             ++output) {
          const auto& next = program.instructions[output];
          const uint32_t next_root =
              next.condition_root == UINT32_MAX
                  ? static_cast<uint32_t>(next.condition.size() - 1)
                  : next.condition_root;
          if (SavedConditionUses(next.condition,
                                 static_cast<uint32_t>(base - 1))[next_root] !=
              1) {
            shared = false;
            break;
          }
        }
        if (shared) {
          saved_base = static_cast<uint32_t>(base - 1);
          break;
        }
      }
      if (saved_base == UINT32_MAX)
        saved_output_end = instruction_index + 1;
      emit(saved_base == UINT32_MAX ? root : saved_base);
      if (saved_base != UINT32_MAX)
        statements.push_back({"MPS", {}});
    }
    if (saved_base != UINT32_MAX) {
      const auto uses = SavedConditionUses(gates, saved_base);
      std::function<void(uint32_t)> extend = [&](uint32_t index) {
        if (index == saved_base)
          return;
        const LogicGate& gate = gates[index];
        if (gate.kind == GateKind::kNot || gate.kind == GateKind::kRising ||
            gate.kind == GateKind::kFalling) {
          extend(gate.left);
          statements.push_back({gate.kind == GateKind::kNot      ? "INV"
                                : gate.kind == GateKind::kRising ? "MEP"
                                                                 : "MEF",
                                {}});
        } else if (gate.left == gate.right)
          extend(gate.left);
        else {
          const bool left = uses[gate.left] == 1;
          extend(left ? gate.left : gate.right);
          emit(left ? gate.right : gate.left);
          statements.push_back(
              {gate.kind == GateKind::kAnd ? "ANB" : "ORB", {}});
        }
      };
      extend(root);
    }
    std::string name;
    for (const InstructionDef& definition : GetInstructionDefinitions()) {
      if (definition.opcode == instruction.opcode)
        name = definition.mnemonic;
    }
    if (instruction.opcode == Opcode::kTimerOn ||
        instruction.opcode == Opcode::kCounterUp)
      name = "OUT";
    if (instruction.wide)
      name = "D" + name;
    if (instruction.pulse)
      name += "P";
    InstructionStatement statement{name, {}};
    for (size_t i = 0; i < instruction.operand_count; ++i) {
      statement.operands.push_back(FormatOperand(instruction.operands[i]));
    }
    statements.push_back(std::move(statement));
  }
  statements.push_back({"END", {}});
  return statements;
}

bool ParseCellInput(const std::string& text, Type default_type,
                    plc::LadderInstruction* cell, std::string* error) {
  if (!cell)
    return false;
  try {
    const auto tokens = TokenizeInstructionText(text);
    if (tokens.empty())
      throw std::runtime_error("Instruction is empty");
    std::string name = Upper(tokens[0]);
    InstructionStatement statement{name, {}};
    statement.operands.assign(tokens.begin() + 1, tokens.end());
    if (default_type == Type::HLINE || default_type == Type::kWrappingSource ||
        default_type == Type::kWrappingDestination) {
      if (!statement.operands.empty())
        throw std::runtime_error("Horizontal line takes one value");
      const Operand value = ParseRequired(name);
      if (value.kind != OperandKind::kImmediateDecimal)
        throw std::runtime_error("Enter a line length or K wrapping number");
      plc::LadderInstruction line;
      if (name.front() == 'K') {
        if (value.immediate < 0)
          throw std::runtime_error("Wrapping number must be nonnegative");
        line.type = Type::kWrappingSource;
        line.address = FormatOperand(value);
      } else {
        line.type = Type::HLINE;
        line.preset = FormatOperand(value);
      }
      *cell = std::move(line);
      return true;
    }
    const auto entered_operand = ParseOperand(name);
    if (entered_operand &&
        (entered_operand->kind == OperandKind::kPointer ||
         entered_operand->kind == OperandKind::kInterruptPointer)) {
      if (!statement.operands.empty())
        throw std::runtime_error("Label cannot have operands");
      statement = {"LABEL", {name}};
      name = "LABEL";
    } else if (entered_operand) {
      statement.operands.insert(statement.operands.begin(), name);
      name = default_type == Type::XIC               ? "LD"
             : default_type == Type::XIO             ? "LDI"
             : default_type == Type::RISING_CONTACT  ? "LDP"
             : default_type == Type::FALLING_CONTACT ? "LDF"
                                                     : "OUT";
      statement.mnemonic = name;
    }
    plc::LadderInstruction candidate;
    if (name == "INV" || name == "MEP" || name == "MEF") {
      if (!statement.operands.empty())
        throw std::runtime_error("Logic modifier has no operands");
      candidate.type = Type::COMPARISON;
      candidate.mnemonic = name;
      *cell = std::move(candidate);
      return true;
    }
    if (name == "LD" || name == "LDI" || name == "LDP" || name == "LDF") {
      if (statement.operands.size() != 1)
        throw std::runtime_error("Contact needs one device");
      const Operand operand = ParseRequired(statement.operands[0]);
      if (operand.kind != OperandKind::kBitDevice)
        throw std::runtime_error("Contact requires bit device");
      candidate.type = name == "LD"    ? Type::XIC
                       : name == "LDI" ? Type::XIO
                       : name == "LDP" ? Type::RISING_CONTACT
                                       : Type::FALLING_CONTACT;
      candidate.address = FormatOperand(operand);
    } else if (name.rfind("LD", 0) == 0 || name.rfind("DLD", 0) == 0) {
      const bool wide = name.rfind("DLD", 0) == 0;
      Comparison(name.substr(wide ? 3 : 2));
      if (statement.operands.size() != 2)
        throw std::runtime_error("Comparison needs two operands");
      candidate.type = Type::COMPARISON;
      candidate.mnemonic = name.substr(wide ? 3 : 2);
      candidate.wide = wide;
      candidate.operands = {ParseRequired(statement.operands[0]),
                            ParseRequired(statement.operands[1])};
      constexpr InstructionDef kComparison{
          Opcode::kCompare,
          "comparison",
          0,
          2,
          {OperandRule::kValue, OperandRule::kValue}};
      std::string validation_error;
      if (!ValidateOperands(kComparison, candidate.operands,
                            &validation_error)) {
        throw std::runtime_error(validation_error);
      }
    } else {
      auto instruction = ParseOutput(statement);
      instruction.condition.push_back({GateKind::kConstant, 0, 0, true});
      ExecutionProgram checked;
      checked.instructions.push_back(instruction);
      std::string validation_error;
      if (!ValidateProgram(checked, &validation_error, false))
        throw std::runtime_error(validation_error);
      candidate = OutputCell(instruction);
    }
    *cell = std::move(candidate);
    return true;
  } catch (const std::exception& exception) {
    if (error)
      *error = exception.what();
    return false;
  }
}

std::string FormatCellInput(const plc::LadderInstruction& cell) {
  if (cell.type == Type::kWrappingSource ||
      cell.type == Type::kWrappingDestination)
    return cell.address;
  if (cell.type == Type::HLINE)
    return "1";
  if (cell.mnemonic == "LABEL")
    return cell.address;
  std::string name = cell.mnemonic;
  if (cell.type == Type::XIC)
    name = "LD";
  if (cell.type == Type::XIO)
    name = "LDI";
  if (cell.type == Type::RISING_CONTACT)
    name = "LDP";
  if (cell.type == Type::FALLING_CONTACT)
    name = "LDF";
  if (cell.type == Type::COMPARISON && name != "INV" && name != "MEP" &&
      name != "MEF")
    name = "LD" + name;
  if (name.empty())
    return cell.address + (cell.preset.empty() ? "" : " " + cell.preset);
  if (cell.wide)
    name = "D" + name;
  if (cell.pulse)
    name += "P";
  if (cell.operands.empty())
    return name + (cell.address.empty() ? "" : " " + cell.address);
  for (const Operand& operand : cell.operands)
    name += " " + FormatOperand(operand);
  return name;
}
}  // namespace plc_emulator::programming
