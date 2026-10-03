#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <numeric>

namespace plc_emulator::programming {

size_t TableCountOperand(Opcode opcode) {
  if (opcode == Opcode::kTableSort || opcode == Opcode::kShiftRightCarry ||
      opcode == Opcode::kShiftLeftCarry)
    return 1;
  if (opcode == Opcode::kTableSearch ||
      (opcode >= Opcode::kBlockAdd && opcode <= Opcode::kBlockGreaterEqual))
    return 3;
  return 2;
}

bool ValidateTableOperation(const OpenPLCInstruction& instruction,
                            std::optional<int32_t> count, std::string* error) {
  const auto fail = [error](const char* message) {
    if (error)
      *error = message;
    return false;
  };
  const auto& operands = instruction.operands;
  const Opcode opcode = instruction.opcode;
  const uint32_t width = instruction.wide ? 2 : 1;
  if (std::any_of(operands.begin(),
                  operands.begin() + instruction.operand_count,
                  [](const Operand& operand) {
                    return operand.kind == OperandKind::kPackedBit;
                  }))
    return fail("Packed bit block operands are not supported yet");
  const auto words = [&](size_t operand, uint32_t length) {
    return IsRegisterDevice(operands[operand].device) &&
           operands[operand].device.index + length <=
               WordDeviceLimit(operands[operand].device);
  };
  if (opcode == Opcode::kTableSort) {
    const int rows = operands[1].immediate;
    const int columns = operands[2].immediate;
    const bool constant_key =
        operands[4].kind == OperandKind::kImmediateDecimal ||
        operands[4].kind == OperandKind::kImmediateHex;
    if (rows < 1 || rows > 32 || columns < 1 || columns > 6 ||
        !words(0, rows * columns) || !words(3, rows * columns) ||
        (constant_key &&
         (operands[4].immediate < 1 || operands[4].immediate > columns)))
      return fail("Invalid SORT dimensions, key or device range");
    return true;
  }
  if (opcode == Opcode::kTableSearch && !words(2, 5 * width))
    return fail("SER result exceeds device range");
  if (opcode == Opcode::kWordSum && !words(1, width * 2))
    return fail("WSUM result exceeds device range");
  if (!count)
    return true;
  const int32_t length = *count;
  if (length < 0)
    return fail("Table operation count must be nonnegative");
  if (opcode == Opcode::kShiftLeftCarry || opcode == Opcode::kShiftRightCarry)
    return true;
  if (opcode == Opcode::kTableInsert || opcode == Opcode::kTableDelete)
    return length > 0 || fail("Table position must be positive");
  if (opcode == Opcode::kStackPop)
    return (length >= 2 && length <= 512 && words(0, length)) ||
           fail("Invalid POP array length");
  if (opcode == Opcode::kNibbleCombine || opcode == Opcode::kNibbleSeparate) {
    if (length > 4 ||
        (length && !words(opcode == Opcode::kNibbleCombine ? 0 : 1, length)))
      return fail("Invalid UNI/DIS count or range");
    return true;
  }
  if (opcode == Opcode::kTableSearch &&
      (length < 1 || length > (instruction.wide ? 128 : 256)))
    return fail("SER count exceeds supported table length");
  if (opcode == Opcode::kWordSum && length == 0)
    return fail("WSUM count must be positive");
  if (length > static_cast<int32_t>(32768 / width))
    return fail("Table operation exceeds device range");
  if (opcode == Opcode::kTableSearch || opcode == Opcode::kWordSum)
    return words(0, length * width) ||
           fail("Source table exceeds device range");
  const bool compare =
      opcode >= Opcode::kBlockEqual && opcode <= Opcode::kBlockGreaterEqual;
  const bool first_table = operands[0].kind == OperandKind::kWordDevice;
  const bool second_table = operands[1].kind == OperandKind::kWordDevice;
  if ((first_table && !words(0, length * width)) ||
      (second_table && !words(1, length * width)))
    return fail("Block source exceeds device range");
  if (compare) {
    auto last = operands[2].device;
    if (length)
      last = OffsetBitAddress(last, length - 1);
    return !length ||
           ParseDeviceAddress(FormatDeviceAddress(last)) ==
               std::optional(last) ||
           fail("Block comparison destination exceeds bit range");
  }
  if (!words(2, length * width))
    return fail("Block destination exceeds device range");
  const auto overlap = [&](size_t source) {
    const uint32_t first = operands[source].device.index;
    const uint32_t destination = operands[2].device.index;
    return operands[source].device.kind == operands[2].device.kind &&
           operands[source].device.unit == operands[2].device.unit && length &&
           first < destination + length * width &&
           destination < first + length * width;
  };
  if ((first_table && overlap(0)) || (second_table && overlap(1)))
    return fail("BK+/BK- destination must not overlap source tables");
  return true;
}
}  // namespace plc_emulator::programming

namespace plc {
namespace {
namespace model = plc_emulator::programming;
}  // namespace

bool CompiledPLCExecutor::ExecuteSortOperation(
    const model::OpenPLCInstruction& instruction, bool power, size_t index) {
  const bool row_major = instruction.opcode == model::Opcode::kTableSort2;
  SortState* selected = &sort_state_;
  if (row_major) {
    const auto slot =
        std::find(sort2_indices_.begin(), sort2_indices_.end(), index);
    if (slot == sort2_indices_.end()) {
      SetError("SORT2 instruction has no execution slot");
      return false;
    }
    selected = &sort2_states_[slot - sort2_indices_.begin()];
  }
  SortState& state = *selected;
  if (!power) {
    state = {};
    memory_.special_m[29] = false;
    return true;
  }
  const auto& operands = instruction.operands;
  const bool wide = instruction.wide;
  const int width = wide ? 2 : 1;
  const int rows = ReadValue(operands[1], wide);
  const int columns = operands[2].immediate;
  const int key = ReadValue(operands[4], wide);
  const auto valid = [&](model::DeviceAddress device) {
    return model::IsRegisterDevice(device) &&
           device.index + rows * columns * width <=
               model::WordDeviceLimit(device);
  };
  if (rows < 1 || rows > 32 || columns < 1 || columns > 6 || key < 1 ||
      key > columns || !valid(operands[0].device) ||
      !valid(operands[3].device)) {
    SetError("Invalid sort dimensions, key or device range");
    return false;
  }
  const uint32_t source = operands[0].device.index;
  const uint32_t destination_head = operands[3].device.index;
  const uint32_t size = rows * columns * width;
  if (row_major && operands[0].device.kind == operands[3].device.kind &&
      operands[0].device.unit == operands[3].device.unit &&
      source != destination_head && source < destination_head + size &&
      destination_head < source + size) {
    SetError("SORT2 permits identical tables but not partial overlap");
    return false;
  }
  if (!state.active) {
    state.active = true;
    state.rows = rows;
    state.columns = columns;
    state.key = key;
    state.destination = operands[3].device;
    state.descending = row_major && memory_.special_m[165];
    for (int item = 0; item < rows * columns; ++item) {
      auto operand = operands[0];
      operand.device.index += item * width;
      state.data[item] = ReadValue(operand, wide);
    }
    std::iota(state.order.begin(), state.order.end(), 0);
    memory_.special_m[29] = false;
  } else if (state.rows != rows || state.columns != columns ||
             state.key != key || state.destination != operands[3].device) {
    SetError("Sort operands changed while busy");
    return false;
  }
  if (state.progress == rows)
    return true;
  const auto position = [&](int row, int column) {
    return row_major ? row * columns + column : column * rows + row;
  };
  const int current = state.progress;
  const int row = state.order[current];
  int place = current;
  while (place > 0) {
    const int32_t previous =
        state.data[position(state.order[place - 1], key - 1)];
    const int32_t value = state.data[position(row, key - 1)];
    if (state.descending ? previous >= value : previous <= value)
      break;
    state.order[place] = state.order[place - 1];
    --place;
  }
  state.order[place] = row;
  if (++state.progress != rows)
    return true;
  for (int column = 0; column < columns; ++column) {
    for (int line = 0; line < rows; ++line) {
      auto destination = state.destination;
      destination.index += position(line, column) * width;
      if (!WriteWord(destination,
                     state.data[position(state.order[line], column)], wide))
        return false;
    }
  }
  memory_.special_m[29] = true;
  return true;
}

bool CompiledPLCExecutor::ExecuteTableOperation(
    const model::OpenPLCInstruction& instruction) {
  using model::Opcode;
  const auto& operands = instruction.operands;
  const Opcode opcode = instruction.opcode;
  const int width = instruction.wide ? 2 : 1;
  const int count =
      ReadValue(operands[model::TableCountOperand(opcode)], instruction.wide);
  std::string error;
  if (!model::ValidateTableOperation(instruction, count, &error)) {
    SetError(error);
    return false;
  }
  const auto read = [&](size_t operand, int index) {
    auto value = operands[operand];
    if (value.kind == model::OperandKind::kWordDevice)
      value.device.index += index * width;
    return ReadValue(value, instruction.wide);
  };
  const auto write = [&](size_t operand, int index, int64_t value) {
    auto destination = operands[operand].device;
    destination.index += index * width;
    return WriteWord(destination, value, instruction.wide);
  };
  if (opcode == Opcode::kTableSort)
    return ExecuteSortOperation(instruction, true);
  if (opcode == Opcode::kShiftRightCarry || opcode == Opcode::kShiftLeftCarry) {
    const int shift = count % 16;
    if (!shift)
      return true;
    const uint16_t source = static_cast<uint16_t>(read(0, 0));
    const bool right = opcode == Opcode::kShiftRightCarry;
    memory_.special_m[22] = right ? ((source >> (shift - 1)) & 1) != 0
                                  : ((source >> (16 - shift)) & 1) != 0;
    return write(0, 0, right ? source >> shift : source << shift);
  }
  if (opcode == Opcode::kNibbleCombine) {
    if (!count)
      return true;
    uint32_t combined = 0;
    for (int index = 0; index < count; ++index)
      combined |= (static_cast<uint32_t>(read(0, index)) & 15) << (index * 4);
    return write(1, 0, combined);
  }
  if (opcode == Opcode::kNibbleSeparate) {
    const uint16_t source = static_cast<uint16_t>(read(0, 0));
    for (int index = 0; index < count; ++index)
      write(1, index, (source >> (index * 4)) & 15);
    return true;
  }
  if (opcode == Opcode::kTableSearch) {
    const int32_t searched = read(1, 0);
    std::array<int32_t, 5> results{};
    int32_t minimum = read(0, 0);
    int32_t maximum = minimum;
    for (int index = 0; index < count; ++index) {
      const int32_t value = read(0, index);
      if (value == searched) {
        if (!results[0])
          results[1] = index;
        ++results[0];
        results[2] = index;
      }
      if (value <= minimum) {
        minimum = value;
        results[3] = index;
      }
      if (value >= maximum) {
        maximum = value;
        results[4] = index;
      }
    }
    for (int index = 0; index < 5; ++index)
      write(2, index, results[index]);
    return true;
  }
  if (opcode == Opcode::kWordSum) {
    int64_t sum = 0;
    for (int index = 0; index < count; ++index)
      sum += read(0, index);
    auto destination = operands[1].device;
    const uint64_t bits = static_cast<uint64_t>(sum);
    for (int index = 0; index < width * 2; ++index) {
      WriteWord(destination, (bits >> (index * 16)) & 0xFFFF);
      destination = model::OffsetBitAddress(destination, 1);
    }
    return true;
  }
  if (opcode == Opcode::kStackPop) {
    const uint32_t head = operands[0].device.index;
    auto storage = WordMemory(operands[0].device);
    const int pointer = storage[head];
    if (pointer < 0 || pointer >= count) {
      SetError("POP pointer is outside data array");
      return false;
    }
    memory_.special_m[20] = pointer <= 1;
    if (!pointer)
      return true;
    const int value = storage[head + pointer];
    storage[head] = static_cast<int16_t>(pointer - 1);
    return write(1, 0, value);
  }
  if (opcode == Opcode::kTableInsert || opcode == Opcode::kTableDelete) {
    const uint32_t head = operands[1].device.index;
    auto storage = WordMemory(operands[1].device);
    const int stored = storage[head];
    const bool insert = opcode == Opcode::kTableInsert;
    if (stored < 0 || count > stored + (insert ? 1 : 0) ||
        head + stored + (insert ? 2u : 1u) > storage.size()) {
      SetError("FINS/FDEL table position or size is outside device range");
      return false;
    }
    if (insert) {
      const int value = read(0, 0);
      for (int index = stored; index >= count; --index)
        storage[head + index + 1] = storage[head + index];
      storage[head + count] = static_cast<int16_t>(value);
      storage[head] = static_cast<int16_t>(stored + 1);
      return true;
    }
    const int removed = storage[head + count];
    for (int index = count; index < stored; ++index)
      storage[head + index] = storage[head + index + 1];
    storage[head + stored] = 0;
    storage[head] = static_cast<int16_t>(stored - 1);
    return write(0, 0, removed);
  }
  const bool compare =
      opcode >= Opcode::kBlockEqual && opcode <= Opcode::kBlockGreaterEqual;
  for (int index = 0; index < count; ++index) {
    const int64_t first = read(0, index);
    const int64_t second = read(1, index);
    if (!compare) {
      write(2, index,
            opcode == Opcode::kBlockAdd ? first + second : first - second);
      continue;
    }
    bool value = false;
    switch (opcode) {
      case Opcode::kBlockEqual:
        value = first == second;
        break;
      case Opcode::kBlockGreater:
        value = first > second;
        break;
      case Opcode::kBlockLess:
        value = first < second;
        break;
      case Opcode::kBlockNotEqual:
        value = first != second;
        break;
      case Opcode::kBlockLessEqual:
        value = first <= second;
        break;
      case Opcode::kBlockGreaterEqual:
        value = first >= second;
        break;
      default:
        return false;
    }
    auto destination = operands[2].device;
    destination.index += index;
    WriteBit(destination, value);
  }
  return true;
}
}  // namespace plc
