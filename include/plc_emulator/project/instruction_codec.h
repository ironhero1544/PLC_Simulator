#pragma once

#include "plc_emulator/programming/ladder_program.h"

#include <string>
#include <vector>

namespace plc_emulator::programming {

struct InstructionStatement {
  std::string mnemonic;
  std::vector<std::string> operands;
};
std::vector<std::string> TokenizeInstructionText(std::string_view text);

bool CompileStatements(const std::vector<InstructionStatement>& statements,
                       ExecutionProgram* program, std::string* error);
bool MaterializeLadder(const ExecutionProgram& program,
                       plc::LadderProgram* ladder, std::string* error);
std::vector<InstructionStatement> SerializeInstructions(
    const ExecutionProgram& program);
bool ParseCellInput(const std::string& text,
                    plc::LadderInstructionType default_type,
                    plc::LadderInstruction* cell, std::string* error);
std::string FormatCellInput(const plc::LadderInstruction& cell);

}  // namespace plc_emulator::programming
