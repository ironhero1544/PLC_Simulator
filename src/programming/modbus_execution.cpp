#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>

namespace plc {
namespace {
namespace model = plc_emulator::programming;

int SlaveSlot(int channel, int station) {
  return channel >= 1 && channel <= 2 && station >= 1 && station <= 32
             ? (channel - 1) * 32 + station - 1
             : -1;
}
}  // namespace

bool CompiledPLCExecutor::ConfigureModbusSlave(int channel, int station,
                                               bool connected,
                                               int response_ms) {
  const int slot = SlaveSlot(channel, station);
  if (slot < 0 || response_ms < 0 || response_ms > 32767)
    return false;
  if (!modbus_slaves_[slot]) {
    modbus_slaves_[slot] = std::make_unique<ModbusSlave>();
    for (auto& data : modbus_slaves_[slot]->data)
      data.resize(65536);
  }
  modbus_slaves_[slot]->connected = connected;
  modbus_slaves_[slot]->response_ms = response_ms;
  return true;
}

bool CompiledPLCExecutor::SetModbusValue(int channel, int station,
                                         ModbusSpace space, int address,
                                         uint16_t value) {
  const int slot = SlaveSlot(channel, station);
  const size_t area = static_cast<size_t>(space);
  if (slot < 0 || !modbus_slaves_[slot] || area >= 4 || address < 0 ||
      address > 65535 || (area < 2 && value > 1))
    return false;
  modbus_slaves_[slot]->data[area][address] = value;
  return true;
}

std::optional<uint16_t> CompiledPLCExecutor::GetModbusValue(int channel,
                                                            int station,
                                                            ModbusSpace space,
                                                            int address) const {
  const int slot = SlaveSlot(channel, station);
  const size_t area = static_cast<size_t>(space);
  if (slot < 0 || !modbus_slaves_[slot] || area >= 4 || address < 0 ||
      address > 65535)
    return std::nullopt;
  return modbus_slaves_[slot]->data[area][address];
}

int CompiledPLCExecutor::ApplyModbusCommand(
    const model::OpenPLCInstruction& instruction, ModbusSlave* slave,
    int station) {
  const auto& operands = instruction.operands;
  const int function = ReadValue(operands[1]);
  const int address = static_cast<uint16_t>(ReadValue(operands[2]));
  const int count = static_cast<uint16_t>(ReadValue(operands[3]));
  const auto& data_operand = operands[4];
  const auto head = data_operand.device;
  const bool constant =
      data_operand.kind == model::OperandKind::kImmediateDecimal ||
      data_operand.kind == model::OperandKind::kImmediateHex;
  const bool word =
      head.kind == model::DeviceKind::kD || head.kind == model::DeviceKind::kR;
  const bool bit = head.kind == model::DeviceKind::kX ||
                   head.kind == model::DeviceKind::kY ||
                   head.kind == model::DeviceKind::kM ||
                   head.kind == model::DeviceKind::kS;
  const auto range = [&](int words, int bits, bool writing_plc) {
    if (constant)
      return !writing_plc;
    if (word)
      return head.bit < 0 &&
             head.index + words <=
                 (head.kind == model::DeviceKind::kD ? 8000u : 32768u);
    if (!bit || (writing_plc && head.kind == model::DeviceKind::kX))
      return false;
    const uint32_t limit = head.kind == model::DeviceKind::kM   ? 7680
                           : head.kind == model::DeviceKind::kS ? 4096
                                                                : 256;
    return bits > 0 && head.index + static_cast<uint32_t>(bits) <= limit;
  };
  const auto read_word = [&](int offset) {
    auto source = data_operand;
    if (!constant)
      source.device.index += offset;
    return static_cast<uint16_t>(ReadValue(source));
  };
  const auto write_word = [&](int offset, uint16_t value) {
    auto destination = head;
    destination.index += offset;
    return WriteWord(destination, value);
  };
  const auto read_bit = [&](int offset) {
    return bit && !constant
               ? ReadBit(model::OffsetBitAddress(head, offset))
               : ((read_word(offset / 16) >> (offset % 16)) & 1) != 0;
  };
  ++slave->counters[0];
  ++slave->counters[3];
  slave->log[(slave->log_head + slave->log_count) % 64] =
      static_cast<uint8_t>(function);
  if (slave->log_count < 64)
    ++slave->log_count;
  else
    slave->log_head = (slave->log_head + 1) % 64;
  if (function == 1 || function == 2 || function == 0x0F) {
    const bool receiving = function != 0x0F;
    if (count < 1 || count > (receiving ? 2000 : 1968))
      return -4;
    if (address + count > 65536)
      return 2;
    if (!range((count + 15) / 16, count, receiving))
      return -5;
    auto& remote = slave->data[function == 2 ? 1 : 0];
    if (!receiving) {
      for (int index = 0; index < count; ++index)
        remote[address + index] = read_bit(index);
    } else if (word) {
      for (int index = 0; index < count; index += 16) {
        uint16_t value = 0;
        for (int bit_index = 0; bit_index < std::min(16, count - index);
             ++bit_index)
          value |= remote[address + index + bit_index] << bit_index;
        if (!write_word(index / 16, value))
          return -5;
      }
    } else {
      for (int index = 0; index < count; ++index)
        WriteBit(model::OffsetBitAddress(head, index), remote[address + index]);
    }
    return 0;
  }
  if (function == 3 || function == 4 || function == 0x10) {
    const bool receiving = function != 0x10;
    if (count < 1 || count > (receiving ? 125 : 123))
      return -4;
    if (address + count > 65536)
      return 2;
    if ((!constant && !word) || !range(count, 0, receiving))
      return -5;
    auto& remote = slave->data[function == 4 ? 3 : 2];
    for (int index = 0; index < count; ++index)
      if (receiving) {
        if (!write_word(index, remote[address + index]))
          return -5;
      } else {
        remote[address + index] = read_word(index);
      }
    return 0;
  }
  if (function == 5 || function == 6 || function == 0x16) {
    if (function != 0x16 && count != 0)
      return -4;
    if (!range(1, 1, false) || (function != 5 && !constant && !word))
      return -5;
    if (function == 5)
      slave->data[0][address] = read_bit(0);
    else if (function == 6)
      slave->data[2][address] = read_word(0);
    else
      slave->data[2][address] =
          (slave->data[2][address] & count) | (read_word(0) & ~count);
    return 0;
  }
  if (function == 0x17) {
    auto read_address_operand = operands[2];
    auto read_count_operand = operands[3];
    if (read_address_operand.kind != model::OperandKind::kWordDevice ||
        read_count_operand.kind != model::OperandKind::kWordDevice)
      return -3;
    ++read_address_operand.device.index;
    ++read_count_operand.device.index;
    if (read_address_operand.device.index >=
            model::WordDeviceLimit(read_address_operand.device) ||
        read_count_operand.device.index >=
            model::WordDeviceLimit(read_count_operand.device))
      return -3;
    const int read_address =
        static_cast<uint16_t>(ReadValue(read_address_operand));
    const int read_count = static_cast<uint16_t>(ReadValue(read_count_operand));
    if (count < 1 || count > 121 || read_count < 1 || read_count > 125)
      return -4;
    if (address + count > 65536 || read_address + read_count > 65536)
      return 2;
    if (!word || !range(count + read_count, 0, true))
      return -5;
    for (int index = 0; index < count; ++index)
      slave->data[2][address + index] = read_word(index);
    for (int index = 0; index < read_count; ++index)
      if (!write_word(count + index, slave->data[2][read_address + index]))
        return -5;
    return 0;
  }
  if (function == 8) {
    if (!word || !range(1, 0, true))
      return -5;
    uint16_t result = count;
    switch (address) {
      case 0:
        break;
      case 1:
        if (count != 0 && count != 0xFF00)
          return 3;
        slave->listen_only = false;
        if (count == 0xFF00) {
          slave->log_head = slave->log_count = 0;
          slave->counters.fill(0);
        }
        break;
      case 2:
        if (count != 0)
          return 3;
        result = slave->diagnostic;
        break;
      case 3:
        if (count > 255)
          return 3;
        slave->delimiter = static_cast<uint8_t>(count);
        break;
      case 4:
        if (count != 0)
          return 3;
        slave->listen_only = true;
        return 0;
      case 0x0A:
        if (count != 0)
          return 3;
        slave->diagnostic = 0;
        slave->counters.fill(0);
        break;
      default:
        if (address < 0x0B || address > 0x12)
          return 1;
        if (count != 0)
          return 3;
        result = slave->counters[address - 0x0B];
        break;
    }
    return write_word(0, result) ? 0 : -5;
  }
  const int occupied = function == 0x0C ? 36 : function == 7 ? 1 : 2;
  if (function == 7 || function == 0x0B || function == 0x0C ||
      function == 0x11) {
    if (address != 0 || count != 0)
      return -3;
    if (!word || !range(occupied, 0, true))
      return -5;
    if (function == 7)
      return write_word(0, slave->exception_status) ? 0 : -5;
    if (function == 0x11)
      return write_word(0, station) && write_word(1, 0xFF) ? 0 : -5;
    write_word(0, 0);
    write_word(1, slave->counters[3]);
    if (function == 0x0C) {
      write_word(2, slave->counters[0]);
      write_word(3, slave->log_count);
      for (int index = 0; index < 32; ++index) {
        uint16_t value = 0;
        for (int byte = 0; byte < 2; ++byte)
          if (index * 2 + byte < slave->log_count)
            value |= static_cast<uint16_t>(
                         slave->log[(slave->log_head + index * 2 + byte) % 64])
                     << (byte * 8);
        write_word(4 + index, value);
      }
    }
    return 0;
  }
  return 1;
}

bool CompiledPLCExecutor::ExecuteModbusOperation(
    const model::OpenPLCInstruction& instruction, size_t index, bool power) {
  auto& transfer = modbus_transfers_[index];
  if (!power) {
    if (transfer.active && modbus_owner_ == index) {
      modbus_owner_ = SIZE_MAX;
      memory_.special_m[401 + (transfer.channel - 1) * 20] = false;
    }
    transfer = {};
    return true;
  }
  if (transfer.completed)
    return true;
  const bool channel1 = (memory_.D[8401] & 0x11) == 1;
  const bool channel2 = (memory_.D[8421] & 0x11) == 1;
  const int channel = channel2 ? 2 : 1;
  const int offset = (channel - 1) * 20;
  const int station = ReadValue(instruction.operands[0]);
  const int function = ReadValue(instruction.operands[1]);
  const auto finish = [&](int error, int detail = 0) {
    transfer.active = false;
    transfer.completed = true;
    if (modbus_owner_ == index)
      modbus_owner_ = SIZE_MAX;
    memory_.special_m[401 + offset] = false;
    memory_.special_m[29] = true;
    if (error) {
      memory_.special_m[402 + offset] = true;
      memory_.special_m[403 + offset] = true;
      memory_.special_m[channel == 1 ? 63 : 438] = true;
      memory_.D[channel == 1 ? 8063 : 8438] = channel == 1 ? 6321 : 3821;
      memory_.D[8402 + offset] = static_cast<int16_t>(error);
      memory_.D[8403 + offset] = static_cast<int16_t>(detail);
      memory_.D[8404 + offset] =
          static_cast<int16_t>(instruction_step_numbers_[index]);
      if (error == 218) {
        memory_.special_m[67] = true;
        memory_.D[8067] = 6706;
      }
    }
    return true;
  };
  if (!channel1 && !channel2)
    return finish(217);
  if (channel1 && channel2)
    return finish(203);
  if (station < 0 || station > 32)
    return finish(218, 1);
  const bool broadcasting = station == 0;
  if (broadcasting && function != 5 && function != 6 && function != 0x0F &&
      function != 0x10 && function != 0x16)
    return finish(215, function);
  if (!transfer.active) {
    if (serial_ports_[channel].owner != SIZE_MAX || cf_cards_[channel - 1] ||
        inverter_owners_[channel - 1] != SIZE_MAX)
      return finish(203);
    if (modbus_owner_ != SIZE_MAX)
      return true;
    transfer.active = true;
    transfer.channel = channel;
    transfer.station = station;
    transfer.elapsed_ms = transfer.retries = 0;
    modbus_owner_ = index;
    memory_.special_m[401 + offset] = true;
    memory_.special_m[402 + offset] = memory_.special_m[408 + offset] =
        memory_.special_m[409 + offset] = false;
    memory_.special_m[29] = false;
    return true;
  }
  if (channel != transfer.channel || station != transfer.station) {
    SetError("ADPRW station or channel changed during communication");
    return false;
  }
  transfer.elapsed_ms += current_elapsed_ms_;
  if (broadcasting) {
    if (transfer.elapsed_ms < std::max<int>(1, memory_.D[8410 + offset]))
      return true;
    for (int target = 1; target <= 32; ++target) {
      auto* slave = modbus_slaves_[SlaveSlot(channel, target)].get();
      if (slave && slave->connected && !slave->listen_only) {
        const int error = ApplyModbusCommand(instruction, slave, target);
        if (error < 0)
          return finish(218, -error);
      }
    }
    return finish(0);
  }
  auto* slave = modbus_slaves_[SlaveSlot(channel, station)].get();
  const bool restart = function == 8 && ReadValue(instruction.operands[2]) == 1;
  const int timeout =
      memory_.D[8409 + offset] > 0 ? memory_.D[8409 + offset] : 1000;
  if (!slave || !slave->connected || (slave->listen_only && !restart) ||
      slave->response_ms > timeout) {
    if (transfer.elapsed_ms < timeout)
      return true;
    memory_.special_m[409 + offset] = true;
    if (transfer.retries++ < std::max<int>(0, memory_.D[8412 + offset])) {
      transfer.elapsed_ms = 0;
      memory_.special_m[408 + offset] = true;
      return true;
    }
    return finish(211);
  }
  if (transfer.elapsed_ms <
      slave->response_ms + std::max<int>(0, memory_.D[8411 + offset]))
    return true;
  const int error = ApplyModbusCommand(instruction, slave, station);
  if (error < 0)
    return finish(218, -error);
  if (error) {
    ++slave->counters[2];
    return finish(212, ((function | 0x80) << 8) | error);
  }
  return finish(0);
}
}  // namespace plc
