#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>

namespace plc {
namespace {
namespace model = plc_emulator::programming;

int InverterSlot(int channel, int station) {
  return channel >= 1 && channel <= 2 && station >= 0 && station < 32
             ? (channel - 1) * 32 + station
             : -1;
}
}  // namespace

bool CompiledPLCExecutor::ConfigureInverter(int channel, int station,
                                            bool connected, int response_ms) {
  const int slot = InverterSlot(channel, station);
  if (slot < 0 || response_ms < 0 || response_ms > 3276700)
    return false;
  if (!inverters_[slot]) {
    inverters_[slot] = std::make_unique<VirtualInverter>();
    inverters_[slot]->parameters.resize(10000);
  }
  inverters_[slot]->connected = connected;
  inverters_[slot]->response_ms = response_ms;
  return true;
}

bool CompiledPLCExecutor::SetInverterParameter(int channel, int station,
                                               int parameter, int16_t value) {
  const int slot = InverterSlot(channel, station);
  if (slot < 0 || !inverters_[slot] || parameter < 0 || parameter >= 10000)
    return false;
  inverters_[slot]->parameters[parameter] = value;
  return true;
}

std::optional<int16_t> CompiledPLCExecutor::GetInverterParameter(
    int channel, int station, int parameter) const {
  const int slot = InverterSlot(channel, station);
  if (slot < 0 || !inverters_[slot] || parameter < 0 || parameter >= 10000)
    return std::nullopt;
  return inverters_[slot]->parameters[parameter];
}

bool CompiledPLCExecutor::SetInverterMonitor(int channel, int station, int code,
                                             uint16_t value) {
  const int slot = InverterSlot(channel, station);
  if (slot < 0 || !inverters_[slot] || code < 0 || code > 255)
    return false;
  inverters_[slot]->monitors[code] = value;
  return true;
}

int CompiledPLCExecutor::ApplyInverterCommand(
    const model::OpenPLCInstruction& instruction, InverterTransfer* transfer,
    VirtualInverter* inverter) {
  using model::Opcode;
  const auto& operands = instruction.operands;
  const int code = ReadValue(operands[1]);
  const uint16_t value = static_cast<uint16_t>(ReadValue(operands[2]));
  const auto write = [this](const model::Operand& destination, uint16_t data) {
    return WriteValue(destination, data) ? 0 : 5;
  };
  switch (instruction.opcode) {
    case Opcode::kInverterCheck:
      if (code < 0x6D || code > 0x7B || code == 0x78)
        return 267;
      return write(operands[2], inverter->monitors[code]);
    case Opcode::kInverterDrive:
      if (code != 0xFB && code != 0xF3 && code != 0xF9 && code != 0xFA &&
          code != 0xEE && code != 0xED && code != 0xFD && code != 0xF4 &&
          code != 0xFC && code != 0xFF)
        return 267;
      if (code == 0xFD && value != 0x9696)
        return 268;
      inverter->commands[code] = value;
      if (code == 0xFD) {
        inverter->commands.fill(0);
        inverter->monitors[0x79] = inverter->monitors[0x7A] = 0;
      } else if (code == 0xF4) {
        std::fill(inverter->monitors.begin() + 0x74,
                  inverter->monitors.begin() + 0x78, 0);
      } else if (code == 0xFC) {
        std::fill(inverter->parameters.begin(), inverter->parameters.end(), 0);
      } else {
        inverter->monitors[code - 0x80] = value;
      }
      return 0;
    case Opcode::kInverterRead:
    case Opcode::kInverterWrite:
      if (code < 0 || code >= static_cast<int>(inverter->parameters.size()))
        return 5;
      if (instruction.opcode == Opcode::kInverterRead)
        return write(operands[2], inverter->parameters[code]);
      inverter->parameters[code] = static_cast<int16_t>(value);
      return 0;
    case Opcode::kInverterBlockWrite: {
      const auto table = WordMemory(operands[2].device);
      const size_t head = operands[2].device.index + transfer->progress * 2;
      if (code < 1 || head + 1 >= table.size())
        return 5;
      const int parameter = table[head];
      if (parameter < 0 || parameter >= 10000) {
        const int offset = (transfer->channel - 1) * 5;
        memory_.special_m[154 + offset] = true;
        memory_.D[8154 + offset] = static_cast<int16_t>(parameter);
        return 5;
      }
      inverter->parameters[parameter] = table[head + 1];
      return ++transfer->progress == code ? 0 : -1;
    }
    case Opcode::kInverterMulti: {
      if (code != 0 && code != 1 && code != 0x10 && code != 0x11)
        return 267;
      auto second = operands[2].device;
      ++second.index;
      const uint16_t frequency = static_cast<uint16_t>(GetWordValue(second));
      inverter->commands[0xF9] = value;
      inverter->commands[0xED] = frequency;
      inverter->monitors[0x79] = value;
      inverter->monitors[0x6D] = frequency;
      if (code & 0x10) {
        inverter->commands[0xEE] = frequency;
        inverter->monitors[0x6E] = frequency;
      }
      auto destination = operands[3].device;
      if (!WriteWord(destination, inverter->monitors[0x79]))
        return 5;
      ++destination.index;
      return WriteWord(destination, inverter->monitors[code & 1 ? 0x72 : 0x6F])
                 ? 0
                 : 5;
    }
    default:
      return 267;
  }
}

bool CompiledPLCExecutor::ExecuteInverterOperation(
    const model::OpenPLCInstruction& instruction, size_t index, bool power) {
  auto& transfer = inverter_transfers_[index];
  if (!power) {
    if (transfer.active && inverter_owners_[transfer.channel - 1] == index) {
      inverter_owners_[transfer.channel - 1] = SIZE_MAX;
      memory_.special_m[151 + (transfer.channel - 1) * 5] = false;
    }
    if (transfer.completed)
      memory_.special_m[29] = false;
    transfer = {};
    return true;
  }
  if (transfer.completed) {
    memory_.special_m[29] = false;
    transfer.completed = false;
    return true;
  }
  const int channel =
      ReadValue(instruction.operands[instruction.operand_count - 1]);
  const int station = ReadValue(instruction.operands[0]);
  const int slot = InverterSlot(channel, station);
  if (slot < 0) {
    SetError("Inverter channel must be 1/2 and station must be 0..31");
    return false;
  }
  const int offset = (channel - 1) * 5;
  const auto finish = [&](int error) {
    transfer.active = false;
    transfer.completed = true;
    inverter_owners_[channel - 1] = SIZE_MAX;
    memory_.special_m[151 + offset] = false;
    memory_.special_m[29] = true;
    if (error) {
      const int serial_relay = channel == 1 ? 63 : 438;
      memory_.special_m[serial_relay] = true;
      memory_.D[channel == 1 ? 8063 : 8438] = channel == 1 ? 6320 : 3820;
      if (!memory_.special_m[152 + offset]) {
        memory_.D[8152 + offset] = static_cast<int16_t>(error);
        memory_.D[8153 + offset] =
            static_cast<int16_t>(instruction_step_numbers_[index]);
      }
      memory_.special_m[152 + offset] = memory_.special_m[153 + offset] = true;
      if (error == 5 || error == 6)
        memory_.D[8067] = error == 5 ? 6706 : 6762;
    }
    return true;
  };
  if (!transfer.active) {
    if (serial_ports_[channel].owner != SIZE_MAX || cf_cards_[channel - 1] ||
        (modbus_owner_ != SIZE_MAX &&
         modbus_transfers_[modbus_owner_].channel == channel))
      return finish(6);
    if (inverter_owners_[channel - 1] != SIZE_MAX)
      return true;
    transfer.active = true;
    transfer.channel = channel;
    transfer.station = station;
    transfer.elapsed_ms = transfer.progress = 0;
    inverter_owners_[channel - 1] = index;
    memory_.special_m[151 + offset] = true;
    memory_.D[8151 + offset] =
        static_cast<int16_t>(instruction_step_numbers_[index]);
    memory_.special_m[29] = false;
    return true;
  }
  if (transfer.channel != channel || transfer.station != station) {
    SetError("Inverter communication operands changed while busy");
    return false;
  }
  transfer.elapsed_ms += current_elapsed_ms_;
  const bool reset = instruction.opcode == model::Opcode::kInverterDrive &&
                     ReadValue(instruction.operands[1]) == 0xFD;
  auto* inverter = inverters_[slot].get();
  if (reset && transfer.elapsed_ms < 2200)
    return true;
  if (reset && (!inverter || !inverter->connected))
    return finish(0);
  const int timeout = std::max<int>(1, memory_.D[8150 + offset]) * 100;
  if (!inverter || !inverter->connected || inverter->response_ms > timeout)
    return transfer.elapsed_ms >= timeout ? finish(1) : true;
  if (!reset && transfer.elapsed_ms < inverter->response_ms)
    return true;
  const int error = ApplyInverterCommand(instruction, &transfer, inverter);
  if (error < 0) {
    transfer.elapsed_ms = 0;
    return true;
  }
  return finish(error);
}
}  // namespace plc
