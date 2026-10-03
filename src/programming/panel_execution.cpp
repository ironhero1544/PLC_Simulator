#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>
#include <bit>

namespace plc {
namespace {
namespace model = plc_emulator::programming;
}  // namespace

bool CompiledPLCExecutor::ExecutePanelOperation(
    const model::OpenPLCInstruction& instruction, size_t index, bool power) {
  using model::Opcode;
  const auto opcode = instruction.opcode;
  const auto& operands = instruction.operands;
  auto& state = panel_states_[index];
  const auto fail = [this](const char* message) {
    SetError(message);
    return false;
  };
  const auto bits = [](model::DeviceAddress device, int count) {
    if (count < 1 || device.kind == model::DeviceKind::kT ||
        device.kind == model::DeviceKind::kC)
      return false;
    const auto last = model::OffsetBitAddress(device, count - 1);
    return model::ParseDeviceAddress(model::FormatDeviceAddress(last)) ==
           std::optional(last);
  };
  const auto write = [this](model::DeviceAddress head, int offset, bool value) {
    const auto device = model::OffsetBitAddress(head, offset);
    WriteBit(device, value);
    if (device.kind == model::DeviceKind::kY)
      physical_outputs_[device.index] = value;
  };
  const auto pressed = [this](model::DeviceAddress head, int count) {
    uint16_t mask = 0;
    for (int bit = 0; bit < count; ++bit)
      if (ReadBit(model::OffsetBitAddress(head, bit)))
        mask |= uint16_t{1} << bit;
    return mask;
  };
  if (opcode == Opcode::kPrint) {
    const auto source = operands[0].device;
    const auto output = operands[1].device;
    if (output.kind != model::DeviceKind::kY || !bits(output, 10))
      return fail("PR requires ten consecutive Y outputs");
    if (!power) {
      const bool complete = state.active && state.scan >= state.values[0] * 2;
      memory_.special_m[29] = complete && state.values[1] && state.keys == 16;
      state = {};
      for (int bit = 0; bit < 10; ++bit)
        write(output, bit, false);
      return true;
    }
    const auto byte = [&](int position) {
      auto operand = operands[0];
      operand.device.index += position / 2;
      return static_cast<uint8_t>(static_cast<uint16_t>(ReadValue(operand)) >>
                                  ((position % 2) * 8));
    };
    if (!state.active) {
      state = {};
      state.active = true;
      state.values[1] = memory_.special_m[27];
      state.keys = state.values[1] ? 16 : 8;
      const uint32_t limit = source.kind == model::DeviceKind::kT ? 512
                             : source.kind == model::DeviceKind::kC
                                 ? 200
                                 : model::WordDeviceLimit(source);
      if (source.index + state.keys / 2 > limit)
        return fail("PR ASCII data exceeds device range");
      state.values[0] = state.keys;
      if (state.values[1])
        for (int position = 0; position < 16; ++position)
          if (!byte(position)) {
            state.values[0] = position;
            break;
          }
    }
    memory_.special_m[29] = false;
    if (state.scan >= state.values[0] * 2) {
      write(output, 8, false);
      write(output, 9, false);
      if (state.scan == state.values[0] * 2 && state.values[1] &&
          state.values[0] < 16)
        memory_.special_m[29] = true;
      state.scan = state.values[0] * 2 + 1;
      return true;
    }
    const uint8_t character = byte(state.scan / 2);
    for (int bit = 0; bit < 8; ++bit)
      write(output, bit, (character >> bit) & 1);
    write(output, 8, state.scan % 2 != 0);
    write(output, 9, true);
    ++state.scan;
    return true;
  }
  if (opcode == Opcode::kTenKey) {
    const auto source = operands[0].device;
    const auto destination = operands[2].device;
    if (!bits(source, 10) || !bits(destination, 11))
      return fail("TKY exceeds ten input or eleven result bits");
    const uint16_t keys = power ? pressed(source, 10) : 0;
    for (int key = 0; key < 10; ++key)
      write(destination, key, (keys >> key) & 1);
    write(destination, 10, keys != 0);
    const uint16_t rising = keys & ~state.previous_keys;
    if (power && rising && !(keys & state.previous_keys)) {
      const int key = std::countr_zero(rising);
      const int64_t previous = ReadValue(operands[1], instruction.wide);
      const int64_t limit = instruction.wide ? 100000000 : 10000;
      if (previous < 0 || previous >= limit)
        return fail("TKY numeric input exceeds decimal digits");
      if (!WriteValue(operands[1], (previous * 10 + key) % limit,
                      instruction.wide))
        return false;
    }
    state.previous_keys = keys;
    return true;
  }
  const bool arrows = opcode == Opcode::kArrowSwitch;
  const bool display = arrows || opcode == Opcode::kSevenSegmentLatch;
  const auto output = operands[display ? arrows ? 2 : 1 : 1].device;
  const int parameter = display ? ReadValue(operands[arrows ? 3 : 2])
                        : opcode == Opcode::kDigitalSwitch
                            ? ReadValue(operands[3])
                            : 0;
  if (output.kind != model::DeviceKind::kY ||
      !bits(output, display ? !arrows && parameter >= 4 ? 12 : 8 : 4) ||
      (display && (parameter < 0 || parameter > (arrows ? 3 : 7))))
    return fail(
        "Panel display requires valid Y outputs and polarity parameter");
  if (!power) {
    state = {};
    memory_.special_m[29] = false;
    if (!display) {
      for (int column = 0; column < 4; ++column)
        write(output, column, false);
      if (opcode == Opcode::kHexKey)
        for (int bit = 0; bit < 8; ++bit)
          write(operands[3].device, bit, false);
    }
    return true;
  }
  const bool starting = !state.active;
  if (starting) {
    state.active = true;
    state.scan = 0;
    state.digit = 3;
  }
  if (display) {
    const auto value_operand = operands[arrows ? 1 : 0];
    if (arrows) {
      if (!bits(operands[0].device, 4))
        return fail("ARWS needs four arrow inputs");
      const uint16_t keys = pressed(operands[0].device, 4);
      const uint16_t rising = keys & ~state.previous_keys;
      if (rising & 4)
        state.digit = (state.digit + 3) % 4;
      if (rising & 8)
        state.digit = (state.digit + 1) % 4;
      int value = ReadValue(value_operand);
      if (value < 0 || value > 9999)
        return fail("ARWS requires four decimal digits");
      int scale = 1;
      for (int digit = 0; digit < state.digit; ++digit)
        scale *= 10;
      const int digit = value / scale % 10;
      if (rising & 3) {
        const int changed = (digit + ((rising & 1) ? 1 : 9)) % 10;
        value += (changed - digit) * scale;
        if (!WriteValue(value_operand, value))
          return false;
      }
      state.previous_keys = keys;
    }
    const int column = state.scan / 3;
    const int phase = state.scan % 3;
    const int sets = !arrows && parameter >= 4 ? 2 : 1;
    for (int set = 0; set < sets; ++set) {
      auto source = value_operand;
      source.device.index += set;
      const int value = ReadValue(source);
      if (value < 0 || value > 9999)
        return fail("SEGL requires four decimal digits");
      int scale = 1;
      for (int digit = 0; digit < column; ++digit)
        scale *= 10;
      const int bcd = value / scale % 10;
      for (int bit = 0; bit < 4; ++bit)
        write(output, set * 8 + bit,
              ((bcd >> bit) & 1) != ((parameter & 2) != 0));
    }
    for (int digit = 0; digit < 4; ++digit)
      write(output, 4 + digit,
            (digit == column && phase == 1) != ((parameter & 1) != 0));
    state.scan = (state.scan + 1) % 12;
    memory_.special_m[29] = state.scan == 0;
    return true;
  }
  const auto input = operands[0].device;
  const bool digital = opcode == Opcode::kDigitalSwitch;
  if (input.kind != model::DeviceKind::kX ||
      !bits(input, digital && parameter == 2 ? 8 : 4) ||
      (digital && (parameter < 1 || parameter > 2)) ||
      (!digital && !bits(operands[3].device, 8)))
    return fail("Invalid keyboard or thumbwheel input devices");
  const int column = digital ? state.scan : state.scan / 2;
  bool sample = !digital && state.scan % 2 == 1;
  if (digital) {
    if (!starting)
      state.elapsed_ms += current_elapsed_ms_;
    sample = state.elapsed_ms >= 100;
  }
  const uint8_t matrix = matrix_configured_[output.index + column]
                             ? matrix_inputs_[output.index + column]
                             : static_cast<uint8_t>(pressed(
                                   input, digital && parameter == 2 ? 8 : 4));
  if (sample) {
    if (digital) {
      for (int set = 0; set < parameter; ++set) {
        const int digit = (matrix >> (set * 4)) & 15;
        if (digit > 9)
          return fail("DSW contains a non-BCD digit");
        int scale = 1;
        for (int place = 0; place < column; ++place)
          scale *= 10;
        const int previous_digit = state.values[set] / scale % 10;
        state.values[set] += (digit - previous_digit) * scale;
      }
      state.elapsed_ms %= 100;
      state.scan = (state.scan + 1) % 4;
      if (state.scan == 0) {
        for (int set = 0; set < parameter; ++set) {
          auto device = operands[2].device;
          device.index += set;
          if (!WriteWord(device, state.values[set]))
            return false;
        }
      }
    } else {
      state.keys = static_cast<uint16_t>((state.keys & ~(15u << (column * 4))) |
                                         ((matrix & 15u) << (column * 4)));
    }
  }
  if (!digital)
    state.scan = (state.scan + 1) % 8;
  const bool complete = state.scan == 0 && sample;
  memory_.special_m[29] = complete;
  if (complete && !digital) {
    const bool hexadecimal = memory_.special_m[167];
    const uint16_t rising = state.keys & ~state.previous_keys;
    if (rising && !(state.keys & state.previous_keys)) {
      const int key = std::countr_zero(rising);
      if (hexadecimal || key < 10) {
        const uint32_t value =
            static_cast<uint32_t>(ReadValue(operands[2], instruction.wide));
        const uint32_t next =
            hexadecimal ? (value << 4) | key
                        : static_cast<uint32_t>(
                              (static_cast<uint64_t>(value) * 10 + key) %
                              (instruction.wide ? 100000000u : 10000u));
        if (!WriteValue(operands[2], next, instruction.wide))
          return false;
      }
    }
    for (int key = 0; key < 6; ++key)
      write(operands[3].device, key, (state.keys >> (key + 10)) & 1);
    write(operands[3].device, 6, (state.keys & 0xfc00) != 0);
    write(operands[3].device, 7, (state.keys & 0x03ff) != 0);
    state.previous_keys = state.keys;
  }
  const int selected = digital ? state.scan : state.scan / 2;
  for (int digit = 0; digit < 4; ++digit)
    write(output, digit, digit == selected);
  return true;
}
}  // namespace plc
