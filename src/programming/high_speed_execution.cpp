#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>

namespace plc {
namespace {
namespace model = plc_emulator::programming;
struct CounterInputs {
  int first;
  int second;
  int reset;
  int start;
  bool quadrature;
};
constexpr std::array<CounterInputs, 21> kCounterInputs = {
    {{0, -1, -1, -1, false}, {1, -1, -1, -1, false}, {2, -1, -1, -1, false},
     {3, -1, -1, -1, false}, {4, -1, -1, -1, false}, {5, -1, -1, -1, false},
     {0, -1, 1, -1, false},  {2, -1, 3, -1, false},  {4, -1, 5, -1, false},
     {0, -1, 1, 6, false},   {2, -1, 3, 7, false},   {0, 1, -1, -1, false},
     {0, 1, 2, -1, false},   {3, 4, 5, -1, false},   {0, 1, 2, 6, false},
     {3, 4, 5, 7, false},    {0, 1, -1, -1, true},   {0, 1, 2, -1, true},
     {3, 4, 5, -1, true},    {0, 1, 2, 6, true},     {3, 4, 5, 7, true}}};
}  // namespace

bool CompiledPLCExecutor::SetPhysicalInput(int address, bool state) {
  if (address < 0 || address >= 256)
    return false;
  const bool previous = physical_inputs_[address];
  if (previous == state)
    return true;
  physical_inputs_[address] = state;
  physical_input_age_[address] = 0;
  if (address < 6)
    RequestInterrupt(address * 100 + (state ? 1 : 0));
  HandleAxisInputs();
  HandleInputTransition(address, previous, state);
  return true;
}

bool CompiledPLCExecutor::GetPhysicalOutput(int address) const {
  return address >= 0 && address < 256 && physical_outputs_[address];
}

bool CompiledPLCExecutor::SetMatrixInput(int output, int row, bool state) {
  if (output < 0 || output >= 256 || row < 0 || row >= 8)
    return false;
  const uint8_t mask = static_cast<uint8_t>(1u << row);
  matrix_inputs_[output] =
      state ? matrix_inputs_[output] | mask : matrix_inputs_[output] & ~mask;
  matrix_configured_[output] = true;
  return true;
}

void CompiledPLCExecutor::RefreshInputs(int begin, int count, int filter_ms) {
  for (int input = begin; input < begin + count; ++input) {
    const int delay = input < 16 ? filter_ms : 10;
    if (physical_input_age_[input] >= delay)
      memory_.X[input] = physical_inputs_[input];
  }
}

void CompiledPLCExecutor::HandleInputTransition(int address, bool previous,
                                                bool state) {
  if (state && !previous)
    ++input_pulse_totals_[address];
  for (int counter = 235; counter <= 255; ++counter) {
    if (!counter_last_power_[counter])
      continue;
    auto pins = kCounterInputs[counter - 235];
    if (memory_.special_m[388]) {
      if (counter == 244 && memory_.special_m[390])
        pins = {6, -1, -1, -1, false};
      if (counter == 245 && memory_.special_m[391])
        pins = {7, -1, -1, -1, false};
      if (counter == 248 && memory_.special_m[392])
        pins = {3, 4, -1, -1, false};
      if (counter == 253 && memory_.special_m[392])
        pins.reset = -1;
    }
    const bool inverse = memory_.special_m[388] && memory_.special_m[389];
    if (pins.reset >= 0 && physical_inputs_[pins.reset] != inverse) {
      memory_.C[counter] = 0;
      counter_contacts_[counter] = false;
      if (address == pins.reset && memory_.special_m[25])
        ProcessHighSpeedComparisons(counter);
      continue;
    }
    if (pins.start >= 0 && !physical_inputs_[pins.start])
      continue;
    int direction = 0;
    if (pins.quadrature) {
      const bool four =
          memory_.special_m[counter == 253 || counter == 255 ? 199 : 198];
      if (four && (address == pins.first || address == pins.second)) {
        const bool a = physical_inputs_[pins.first];
        const bool b = physical_inputs_[pins.second];
        direction = (address == pins.first ? a != b : a == b) ? 1 : -1;
      } else if (address == pins.first && state && !previous) {
        direction = physical_inputs_[pins.second] ? -1 : 1;
      }
      if (direction)
        memory_.special_m[counter] = direction < 0;
    } else if (state && !previous) {
      if (address == pins.first)
        direction = pins.second < 0 && memory_.special_m[counter] ? -1 : 1;
      else if (address == pins.second)
        direction = -1;
    }
    if (direction)
      InjectCounterPulses(counter, direction);
  }
}

bool CompiledPLCExecutor::InjectCounterPulses(int counter, int32_t pulses) {
  if (counter < 235 || counter > 255 || pulses < -1000000 || pulses > 1000000)
    return false;
  if (!counter_last_power_[counter])
    return true;
  const int direction = pulses < 0 ? -1 : 1;
  for (int pulse = 0; pulse < std::abs(pulses); ++pulse) {
    const int32_t previous = memory_.C[counter];
    memory_.C[counter] =
        std::bit_cast<int32_t>(static_cast<uint32_t>(previous) +
                               (direction < 0 ? UINT32_MAX : uint32_t{1}));
    const int32_t current = memory_.C[counter];
    const int32_t preset = counter_presets_[counter];
    if (direction > 0 && previous < preset && current >= preset)
      counter_contacts_[counter] = true;
    if (direction < 0 && previous >= preset && current < preset)
      counter_contacts_[counter] = false;
    ProcessHighSpeedComparisons(counter);
  }
  return true;
}

void CompiledPLCExecutor::ProcessHighSpeedComparisons(int counter) {
  using model::Opcode;
  const int32_t current = memory_.C[counter];
  for (size_t index = 0; index < resolved_program_.instructions.size();
       ++index) {
    const auto& instruction = resolved_program_.instructions[index];
    const auto opcode = instruction.opcode;
    if (opcode < Opcode::kHighSpeedSet || opcode > Opcode::kHighSpeedTable ||
        !handy_states_[index].active)
      continue;
    const auto& operands = instruction.operands;
    const int counter_operand =
        opcode == Opcode::kHighSpeedSet || opcode == Opcode::kHighSpeedReset
            ? 1
            : 2;
    if (operands[counter_operand].device.index !=
        static_cast<uint32_t>(counter))
      continue;
    const auto write = [this](model::DeviceAddress device, bool value) {
      WriteBit(device, value);
      if (device.kind == model::DeviceKind::kY)
        physical_outputs_[device.index] = value;
    };
    if (opcode == Opcode::kHighSpeedSet || opcode == Opcode::kHighSpeedReset) {
      if (current == ReadValue(operands[0], true)) {
        if (operands[2].kind == model::OperandKind::kInterruptPointer)
          RequestInterrupt(operands[2].immediate);
        else
          write(operands[2].device, opcode == Opcode::kHighSpeedSet);
      }
      continue;
    }
    const auto destination = operands[3].device;
    const bool special_table =
        opcode == Opcode::kHighSpeedZone &&
        destination.kind == model::DeviceKind::kSpecialM &&
        (destination.index == 130 || destination.index == 132);
    if (opcode == Opcode::kHighSpeedZone && !special_table) {
      const int32_t lower = ReadValue(operands[0], true);
      const int32_t upper = std::max(lower, ReadValue(operands[1], true));
      write(destination, current < lower);
      write(model::OffsetBitAddress(destination, 1),
            current >= lower && current <= upper);
      write(model::OffsetBitAddress(destination, 2), current > upper);
      continue;
    }
    const bool frequency = special_table && destination.index == 132;
    const int position_register = opcode == Opcode::kHighSpeedTable ? 8138
                                  : frequency                       ? 8131
                                                                    : 8130;
    const int stride = opcode == Opcode::kHighSpeedTable ? 3 : 4;
    const int rows = ReadValue(operands[1]);
    int position = memory_.D[position_register];
    if (position < 0 || position >= rows)
      position = 0;
    auto compared = operands[0];
    compared.device.index += position * stride;
    if (current != ReadValue(compared, true))
      continue;
    auto output = compared;
    output.device.index += 2;
    if (frequency) {
      // The next row supplies the new frequency after this comparison point.
    } else if (opcode == Opcode::kHighSpeedTable) {
      const uint16_t pattern = static_cast<uint16_t>(ReadValue(output));
      const int bits = ReadValue(operands[4]);
      for (int bit = 0; bit < bits; ++bit)
        write(model::OffsetBitAddress(destination, bit), (pattern >> bit) & 1);
    } else {
      const uint16_t output_number = static_cast<uint16_t>(ReadValue(output));
      const uint32_t output_index = ((output_number >> 8) & 7) * 64 +
                                    ((output_number >> 4) & 7) * 8 +
                                    (output_number & 7);
      output.device.index += 1;
      write({model::DeviceKind::kY, output_index}, ReadValue(output) != 0);
    }
    if (++position == rows) {
      position = 0;
      memory_.special_m[position_register - 8000 +
                        (opcode == Opcode::kHighSpeedTable ? 0
                         : frequency                       ? 2
                                                           : 1)] = true;
    }
    memory_.D[position_register] = static_cast<int16_t>(position);
    if (frequency) {
      compared.device.index = operands[0].device.index + position * 4;
      WriteWord({model::DeviceKind::kD, 8134}, ReadValue(compared, true), true);
      compared.device.index += 2;
      WriteWord({model::DeviceKind::kD, 8132}, ReadValue(compared, true), true);
    }
  }
}

bool CompiledPLCExecutor::ExecuteHighSpeedOperation(
    const model::OpenPLCInstruction& instruction, size_t index, bool power) {
  using model::Opcode;
  const auto opcode = instruction.opcode;
  const auto& operands = instruction.operands;
  auto& state = handy_states_[index];
  const bool previous = state.active;
  const auto fail = [this](const char* message) {
    SetError(message);
    return false;
  };
  const auto bit_range = [](model::DeviceAddress device, int count) {
    if (count < 1)
      return false;
    const auto last = model::OffsetBitAddress(device, count - 1);
    return model::ParseDeviceAddress(model::FormatDeviceAddress(last)) ==
           std::optional(last);
  };
  if (opcode >= Opcode::kHighSpeedSet && opcode <= Opcode::kHighSpeedTable) {
    state.active = power;
    const int counter_operand =
        opcode == Opcode::kHighSpeedSet || opcode == Opcode::kHighSpeedReset
            ? 1
            : 2;
    const auto counter = operands[counter_operand].device;
    if (counter.kind != model::DeviceKind::kC || counter.index < 235 ||
        counter.index > 255)
      return fail("High-speed comparison requires C235 to C255");
    const auto destination =
        operands[instruction.operand_count -
                 (opcode == Opcode::kHighSpeedTable ? 2 : 1)]
            .device;
    const bool special = opcode == Opcode::kHighSpeedZone &&
                         destination.kind == model::DeviceKind::kSpecialM &&
                         (destination.index == 130 || destination.index == 132);
    if (special || opcode == Opcode::kHighSpeedTable) {
      const int rows = ReadValue(operands[1]);
      const int stride = special ? 4 : 3;
      const auto source = operands[0].device;
      if (rows < 1 || rows > 128 || source.kind != model::DeviceKind::kD ||
          source.index + rows * stride > 8000)
        return fail("High-speed table exceeds comparison points or D range");
      const int position_register = opcode == Opcode::kHighSpeedTable ? 8138
                                    : destination.index == 132        ? 8131
                                                                      : 8130;
      if (!power || !previous) {
        memory_.D[position_register] = 0;
        memory_.special_m[opcode == Opcode::kHighSpeedTable ? 138
                          : destination.index == 132        ? 133
                                                            : 131] = false;
      }
      if (opcode == Opcode::kHighSpeedTable) {
        const int bits = ReadValue(operands[4]);
        if (bits < 1 || bits > 16 || !bit_range(destination, bits) ||
            (destination.kind == model::DeviceKind::kY &&
             destination.index % 8))
          return fail("HSCT output pattern exceeds bit range");
      }
      if (special && destination.index == 132) {
        if (!power) {
          WriteWord({model::DeviceKind::kD, 8132}, 0, true);
        } else if (!previous) {
          auto source_operand = operands[0];
          WriteWord({model::DeviceKind::kD, 8134},
                    ReadValue(source_operand, true), true);
          source_operand.device.index += 2;
          WriteWord({model::DeviceKind::kD, 8132},
                    ReadValue(source_operand, true), true);
        }
      }
    } else if (opcode == Opcode::kHighSpeedZone && !bit_range(destination, 3)) {
      return fail("HSZ results exceed bit range");
    }
    return true;
  }
  if (opcode == Opcode::kSpeedMeasure) {
    const auto source = operands[0].device;
    const auto destination = operands[2].device;
    const int width = instruction.wide ? 2 : 1;
    const int duration = ReadValue(operands[1], instruction.wide);
    if (source.kind != model::DeviceKind::kX || source.index >= 8 ||
        duration < 1 ||
        destination.index + width * 3 > WordMemory(destination).size())
      return fail("SPD exceeds input, sample period or destination range");
    if (!power) {
      state.active = false;
      state.elapsed_ms = 0;
      for (int word = 0; word < width * 3; ++word) {
        auto device = destination;
        device.index += word;
        WriteWord(device, 0);
      }
      return true;
    }
    if (!state.active) {
      state.active = true;
      state.elapsed_ms = 0;
      speed_pulse_starts_[index] = input_pulse_totals_[source.index];
    } else {
      state.elapsed_ms += current_elapsed_ms_;
    }
    const uint64_t pulses =
        input_pulse_totals_[source.index] - speed_pulse_starts_[index];
    auto present = destination;
    present.index += width;
    auto remaining = present;
    remaining.index += width;
    if (state.elapsed_ms >= duration) {
      WriteWord(destination, pulses, instruction.wide);
      state.elapsed_ms = 0;
      speed_pulse_starts_[index] = input_pulse_totals_[source.index];
      WriteWord(present, 0, instruction.wide);
    } else {
      WriteWord(present, pulses, instruction.wide);
    }
    return WriteWord(remaining, duration - state.elapsed_ms, instruction.wide);
  }
  if (opcode == Opcode::kInputMatrix) {
    const auto input = operands[0].device;
    const auto output = operands[1].device;
    const auto destination = operands[2].device;
    const int columns = ReadValue(operands[3]);
    if (input.kind != model::DeviceKind::kX ||
        output.kind != model::DeviceKind::kY || input.index % 8 ||
        output.index % 8 || input.index + 8 > 256 || columns < 2 ||
        columns > 8 || output.index + columns > 256 ||
        !bit_range(destination,
                   (columns -
                    1) * (destination.kind == model::DeviceKind::kY ? 8 : 10) +
                       8) ||
        destination.index %
            (destination.kind == model::DeviceKind::kY ? 8 : 10))
      return fail("Invalid MTR matrix devices or columns");
    if (!power) {
      state = {};
      memory_.special_m[29] = false;
      for (int column = 0; column < columns; ++column)
        WriteBit(model::OffsetBitAddress(output, column), false);
      return true;
    }
    if (!state.active) {
      state.active = true;
      state.elapsed_ms = 0;
      state.off_elapsed_ms = 0;
    } else {
      state.elapsed_ms += current_elapsed_ms_;
    }
    if (state.elapsed_ms >= 20) {
      const int column = static_cast<int>(state.off_elapsed_ms);
      for (int row = 0; row < 8; ++row) {
        const bool value =
            matrix_configured_[output.index + column]
                ? (matrix_inputs_[output.index + column] >> row) & 1
                : physical_inputs_[input.index + row];
        const int offset = destination.kind == model::DeviceKind::kY
                               ? column * 8 + row
                               : column * 10 + row;
        WriteBit(model::OffsetBitAddress(destination, offset), value);
      }
      state.off_elapsed_ms = (column + 1) % columns;
      state.elapsed_ms %= 20;
      if (state.off_elapsed_ms == 0)
        memory_.special_m[29] = true;
    }
    for (int column = 0; column < columns; ++column) {
      const bool value = column == state.off_elapsed_ms;
      memory_.Y[output.index + column] = value;
      physical_outputs_[output.index + column] = value;
    }
    return true;
  }
  if (!power)
    return true;
  if (opcode == Opcode::kRefreshFilter) {
    const int delay = ReadValue(operands[0]);
    if (delay < 0 || delay > 60)
      return fail("REFF input filter must be 0 to 60ms");
    RefreshInputs(0, 16, delay);
    return true;
  }
  const auto device = operands[0].device;
  const int count = ReadValue(operands[1]);
  if ((device.kind != model::DeviceKind::kX &&
       device.kind != model::DeviceKind::kY) ||
      device.index % 8 || count < 8 || count % 8 || device.index + count > 256)
    return fail("REF requires X/Y groups of eight bits");
  if (device.kind == model::DeviceKind::kX)
    RefreshInputs(device.index, count, std::clamp<int>(memory_.D[8020], 0, 60));
  else
    std::copy_n(memory_.Y + device.index, count,
                physical_outputs_.begin() + device.index);
  return true;
}
}  // namespace plc
