#pragma once

#include <string>

#include "plc_emulator/programming/compiled_plc_executor.h"
#include "plc_emulator/programming/ladder_program.h"

namespace plc_emulator::programming {

inline std::string FormatTimerCounterMonitorValues(
    const plc::LadderInstruction& instruction,
    const plc::CompiledPLCExecutor& executor) {
  const auto device = executor.ResolveDeviceAddress(instruction.address);
  if (!device) return {};
  const bool wide = instruction.type == plc::LadderInstructionType::CTU &&
                    device->index >= 200;
  std::string preset_value = "?";
  const auto preset_device = executor.ResolveDeviceAddress(instruction.preset);
  if (preset_device) {
    // Show the live operand, not the runtime's clamped or previously sampled
    // threshold. A later rung may have written this register in the same scan.
    preset_value = std::to_string(executor.GetWordValue(*preset_device, wide));
  } else if (const auto operand = ParseOperand(instruction.preset)) {
    if (operand->kind == OperandKind::kImmediateDecimal ||
        operand->kind == OperandKind::kImmediateHex)
      preset_value = std::to_string(operand->immediate);
  }
  return instruction.address + "=" +
         std::to_string(executor.GetWordValue(*device, wide)) + "  " +
         instruction.preset + "=" + preset_value;
}

}  // namespace plc_emulator::programming
