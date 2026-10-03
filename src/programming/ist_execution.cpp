#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>

namespace plc {
namespace {
namespace model = plc_emulator::programming;
}  // namespace

bool CompiledPLCExecutor::ExecuteInitialState(
    const model::OpenPLCInstruction& instruction, bool power) {
  if (!power)
    return true;
  const auto selector = instruction.operands[0].device;
  const auto first = instruction.operands[1].device;
  const auto last = instruction.operands[2].device;
  const auto selected = [this, selector](int offset) {
    return ReadBit(model::OffsetBitAddress(selector, offset));
  };
  int mode = -1;
  for (int input = 0; input < 5; ++input) {
    if (!selected(input))
      continue;
    if (mode >= 0) {
      SetError("IST operation mode selectors overlap");
      return false;
    }
    mode = input;
  }
  const int category = mode < 2 ? mode : 2;
  const bool automatic = mode >= 2;
  const bool zero_start = selected(5);
  const bool auto_start = selected(6);
  const bool stop = selected(7);
  const bool start = (zero_start || auto_start) && mode != 0;
  const bool start_edge = start && !ist_start_previous_;
  ist_start_previous_ = start;
  // The published IST equivalent circuit retains M8040 until a start pulse.
  memory_.special_m[41] = automatic && !stop &&
                          (auto_start || (mode == 4 && memory_.special_m[41]));
  memory_.special_m[42] = start_edge;
  memory_.special_m[40] =
      mode == 0 || ((mode == 2 || ((mode == 1 || mode == 3) && stop) ||
                     first_scan_ || memory_.special_m[40]) &&
                    !start_edge);
  if (category != ist_mode_) {
    // Switching at an initial state preserves the completed zero return.
    const bool reset = ist_mode_ >= 0 && !memory_.special_m[43] &&
                       !(ist_mode_ == 1 && memory_.S[1]) &&
                       !(ist_mode_ == 2 && memory_.S[2]);
    if (reset) {
      std::fill(memory_.S + first.index, memory_.S + last.index + 1, false);
      if (!memory_.special_m[45])
        std::fill(std::begin(memory_.Y), std::end(memory_.Y), false);
    }
    memory_.S[0] = category == 0;
    memory_.S[1] = category == 1;
    memory_.S[2] = category == 2 && memory_.special_m[43];
    ist_mode_ = category;
  }
  if (mode == 0)
    memory_.special_m[43] = memory_.special_m[44];
  const bool auto_ready = automatic && memory_.special_m[43];
  if (auto_ready && !ist_auto_previous_)
    memory_.S[2] = true;
  ist_auto_previous_ = auto_ready;
  if ((memory_.S[1] && zero_start) || (memory_.S[2] && memory_.special_m[41]))
    memory_.special_m[43] = false;
  memory_.special_m[47] = true;
  return true;
}
}  // namespace plc
