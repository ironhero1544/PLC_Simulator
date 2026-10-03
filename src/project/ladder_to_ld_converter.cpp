#include "plc_emulator/project/ladder_to_ld_converter.h"

#include "plc_emulator/project/instruction_codec.h"
#include "plc_emulator/project/openplc_compiler_integration.h"
#include "plc_emulator/project/structured_ladder_compiler.h"

#include <fstream>
#include <sstream>

namespace plc {
namespace {
namespace model = plc_emulator::programming;
std::vector<std::string> Lines(const model::ExecutionProgram& program) {
  std::vector<std::string> result;
  for (const auto& statement : model::SerializeInstructions(program)) {
    std::string line = statement.mnemonic;
    for (const auto& operand : statement.operands)
      line += " " + operand;
    result.push_back(std::move(line));
  }
  return result;
}
}  // namespace

LadderToLDConverter::LadderToLDConverter() = default;
LadderToLDConverter::~LadderToLDConverter() = default;

bool LadderToLDConverter::ConvertToLDFile(const LadderProgram& program,
                                          const std::string& path) {
  const std::string content = ConvertToLDString(program);
  if (content.empty())
    return false;
  std::ofstream file(path);
  if (!file) {
    last_error_ = "Cannot open LD file: " + path;
    return false;
  }
  file << content;
  return file.good();
}

std::string LadderToLDConverter::ConvertToLDString(
    const LadderProgram& ladder) {
  last_error_.clear();
  model::ExecutionProgram program;
  if (!model::CompileLadder(ladder, &program, &last_error_))
    return {};
  std::ostringstream content;
  content << "PROGRAM PLC_PRG\nVAR\nEND_VAR\n";
  for (const auto& line : Lines(program)) {
    if (line != "END")
      content << line << ";\n";
  }
  content << "END_PROGRAM\n";
  return content.str();
}

std::string LadderToLDConverter::ConvertToLDStringWithIR(
    const LadderProgram& ladder) {
  return ConvertToLDString(ladder);
}

std::vector<std::string> LadderToLDConverter::GenerateStackInstructions(
    const LadderIRProgram& ir) {
  OpenPLCCompilerIntegration compiler;
  const auto result = compiler.CompileIRProgram(ir);
  if (!result.success) {
    last_error_ = result.errorMessage;
    return {};
  }
  return Lines(result.program);
}
}  // namespace plc
