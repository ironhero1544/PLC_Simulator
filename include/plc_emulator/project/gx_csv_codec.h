#pragma once

#include "plc_emulator/programming/ladder_program.h"

#include <string>

namespace plc_emulator::programming {
struct InstructionStatement;
enum class GXCSVEncoding { kUtf16LE, kUtf8 };
int GXInstructionStepCount(const InstructionStatement& statement);
bool ImportGXCSV(const std::string& content, plc::LadderProgram* ladder,
                 std::string* error);
bool ExportGXCSV(const plc::LadderProgram& ladder, const std::string& name,
                 std::string* content, std::string* error,
                 GXCSVEncoding encoding = GXCSVEncoding::kUtf16LE);
}  // namespace plc_emulator::programming
