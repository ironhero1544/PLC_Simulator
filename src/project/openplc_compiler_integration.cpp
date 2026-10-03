#include "plc_emulator/project/openplc_compiler_integration.h"

#include "plc_emulator/project/instruction_codec.h"
#include "plc_emulator/project/ladder_ir.h"
#include "plc_emulator/project/structured_ladder_compiler.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace plc {
namespace {
namespace model = plc_emulator::programming;

std::string StripComments(const std::string& source) {
  std::string result;
  bool comment = false;
  bool quoted = false;
  for (size_t i = 0; i < source.size(); ++i) {
    if (!comment && source[i] == '"') {
      result += source[i];
      if (quoted && i + 1 < source.size() && source[i + 1] == '"')
        result += source[++i];
      else
        quoted = !quoted;
    } else if (!comment && !quoted && source[i] == ';') {
      result += '\n';
    } else if (!comment && !quoted && i + 1 < source.size() &&
               source[i] == '(' && source[i + 1] == '*') {
      comment = true;
      ++i;
    } else if (comment && i + 1 < source.size() && source[i] == '*' &&
               source[i + 1] == ')') {
      comment = false;
      ++i;
    } else if (!comment || source[i] == '\n')
      result += source[i];
  }
  if (comment)
    return {};
  return result;
}
}  // namespace

OpenPLCCompilerIntegration::OpenPLCCompilerIntegration() = default;
OpenPLCCompilerIntegration::~OpenPLCCompilerIntegration() = default;

OpenPLCCompilerIntegration::CompilationResult
OpenPLCCompilerIntegration::CompileLDFile(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    CompilationResult result;
    result.errorMessage = "Cannot open LD file: " + path;
    return result;
  }
  std::ostringstream content;
  content << file.rdbuf();
  return CompileLDString(content.str());
}

OpenPLCCompilerIntegration::CompilationResult
OpenPLCCompilerIntegration::CompileLDString(const std::string& content) {
  CompilationResult result;
  std::vector<model::InstructionStatement> statements;
  std::string cleaned = StripComments(content);
  std::istringstream lines(cleaned);
  std::string line;
  bool variables = false;
  while (std::getline(lines, line)) {
    std::vector<std::string> tokens;
    try {
      tokens = model::TokenizeInstructionText(line);
    } catch (const std::exception& error) {
      result.errorMessage = error.what();
      return result;
    }
    model::InstructionStatement statement;
    if (tokens.empty())
      continue;
    statement.mnemonic = tokens[0];
    for (char& ch : statement.mnemonic) {
      ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    if (statement.mnemonic == "VAR") {
      variables = true;
      continue;
    }
    if (statement.mnemonic == "END_VAR") {
      variables = false;
      continue;
    }
    if (variables || statement.mnemonic == "PROGRAM")
      continue;
    if (statement.mnemonic == "END_PROGRAM") {
      statements.push_back({"END", {}});
      continue;
    }
    statement.operands.assign(tokens.begin() + 1, tokens.end());
    statements.push_back(std::move(statement));
  }
  result.success = model::CompileStatements(statements, &result.program,
                                            &result.errorMessage);
  result.inputCount = input_count_;
  result.outputCount = output_count_;
  result.memoryCount = memory_count_;
  return result;
}

void OpenPLCCompilerIntegration::SetIOConfiguration(int inputs, int outputs) {
  input_count_ = inputs;
  output_count_ = outputs;
}

bool OpenPLCCompilerIntegration::SaveLDProgram(const CompilationResult& result,
                                               const std::string& path) {
  if (!result.success)
    return false;
  std::ofstream file(path);
  if (!file)
    return false;
  for (const auto& statement : model::SerializeInstructions(result.program)) {
    file << statement.mnemonic;
    for (const auto& operand : statement.operands)
      file << ' ' << operand;
    file << '\n';
  }
  return file.good();
}

OpenPLCCompilerIntegration::CompilationResult
OpenPLCCompilerIntegration::CompileLadderProgramWithIR(
    const LadderProgram& ladder) {
  CompilationResult result;
  result.success =
      model::CompileLadder(ladder, &result.program, &result.errorMessage);
  result.inputCount = input_count_;
  result.outputCount = output_count_;
  result.memoryCount = memory_count_;
  return result;
}

OpenPLCCompilerIntegration::CompilationResult
OpenPLCCompilerIntegration::CompileIRProgram(const LadderIRProgram& ir) {
  // Adapt the existing graph directly; the grid compiler is the F4 entrypoint.
  CompilationResult result;
  model::ExecutionProgram candidate;
  for (size_t rung_index = 0; rung_index < ir.GetRungCount(); ++rung_index) {
    const IRRung& rung = ir.GetRung(rung_index);
    std::map<size_t, uint32_t> indices;
    std::vector<model::LogicGate> gates;
    for (const auto& node : rung.nodes) {
      if (!node) {
        result.errorMessage = "Null IR node";
        return result;
      }
      uint32_t condition = static_cast<uint32_t>(gates.size());
      gates.push_back({model::GateKind::kConstant, 0, 0, node->inputs.empty()});
      for (size_t i = 0; i < node->inputs.size(); ++i) {
        const auto found = indices.find(node->inputs[i]);
        if (found == indices.end()) {
          result.errorMessage = "IR is not topologically ordered";
          return result;
        }
        if (i == 0)
          condition = found->second;
        else {
          gates.push_back({model::GateKind::kOr, condition, found->second});
          condition = static_cast<uint32_t>(gates.size() - 1);
        }
      }
      if (node->nodeType == IRNodeType::NORMALLY_OPEN ||
          node->nodeType == IRNodeType::NORMALLY_CLOSED) {
        const auto operand = model::ParseOperand(node->deviceAddress);
        if (!operand) {
          result.errorMessage = "Invalid IR contact";
          return result;
        }
        model::LogicGate contact{model::GateKind::kContact};
        contact.first = *operand;
        gates.push_back(contact);
        uint32_t test = static_cast<uint32_t>(gates.size() - 1);
        if (node->nodeType == IRNodeType::NORMALLY_CLOSED) {
          gates.push_back({model::GateKind::kNot, test});
          test = static_cast<uint32_t>(gates.size() - 1);
        }
        gates.push_back({model::GateKind::kAnd, condition, test});
        condition = static_cast<uint32_t>(gates.size() - 1);
      } else if (node->IsOutputNode()) {
        const auto operand = model::ParseOperand(node->deviceAddress);
        if (!operand) {
          result.errorMessage = "Invalid IR output";
          return result;
        }
        model::OpenPLCInstruction instruction;
        instruction.opcode =
            node->nodeType == IRNodeType::SET_COIL     ? model::Opcode::kSet
            : node->nodeType == IRNodeType::RESET_COIL ? model::Opcode::kReset
                                                       : model::Opcode::kOut;
        instruction.operands[0] = *operand;
        instruction.operand_count = 1;
        instruction.condition = gates;
        instruction.condition.push_back(
            {model::GateKind::kAnd, condition, condition});
        candidate.instructions.push_back(std::move(instruction));
      } else if (node->nodeType != IRNodeType::HORIZONTAL_LINE &&
                 node->nodeType != IRNodeType::VERTICAL_LINE &&
                 node->nodeType != IRNodeType::OR_BLOCK) {
        result.errorMessage = "Unsupported legacy IR node";
        return result;
      }
      indices[node->nodeId] = condition;
    }
  }
  result.success = model::ValidateProgram(candidate, &result.errorMessage);
  if (result.success)
    result.program = std::move(candidate);
  return result;
}
}  // namespace plc
