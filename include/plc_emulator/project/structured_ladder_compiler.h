#pragma once

#include "plc_emulator/programming/execution_program.h"

#include <string>

namespace plc {
struct LadderProgram;
}

namespace plc_emulator::programming {
size_t LadderFingerprint(const plc::LadderProgram& ladder);
bool CompileLadder(const plc::LadderProgram& ladder, ExecutionProgram* program,
                   std::string* error);
}  // namespace plc_emulator::programming
