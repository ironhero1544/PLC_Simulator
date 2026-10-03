#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>

namespace plc {
namespace {
namespace model = plc_emulator::programming;
constexpr int kBaudRates[11] = {0,    0,    0,    300,   600,  1200,
                                2400, 4800, 9600, 19200, 38400};
constexpr char kHex[] = "0123456789ABCDEF";
constexpr int kChannelOffset[3] = {0, 30, 50};

int HexDigit(uint8_t value) {
  if (value >= '0' && value <= '9')
    return value - '0';
  if (value >= 'A' && value <= 'F')
    return value - 'A' + 10;
  if (value >= 'a' && value <= 'f')
    return value - 'a' + 10;
  return -1;
}
}  // namespace

bool CompiledPLCExecutor::InjectSerialReceived(int channel,
                                               std::span<const uint8_t> bytes) {
  if (channel < 0 || channel >= 3)
    return false;
  auto& port = serial_ports_[channel];
  if (bytes.size() > port.incoming.size() - port.incoming_count)
    return false;
  for (uint8_t byte : bytes) {
    port.incoming[(port.incoming_head + port.incoming_count) %
                  port.incoming.size()] = byte;
    ++port.incoming_count;
  }
  return true;
}

size_t CompiledPLCExecutor::ReadSerialTransmitted(int channel,
                                                  std::span<uint8_t> output) {
  if (channel < 0 || channel >= 3)
    return 0;
  auto& port = serial_ports_[channel];
  const size_t count =
      std::min(output.size(), static_cast<size_t>(port.outgoing_count));
  for (size_t i = 0; i < count; ++i)
    output[i] = port.outgoing[(port.outgoing_head + i) % port.outgoing.size()];
  port.outgoing_head = (port.outgoing_head + count) % port.outgoing.size();
  port.outgoing_count -= static_cast<int>(count);
  return count;
}

bool CompiledPLCExecutor::StartSerialSend(SerialPort* port,
                                          const model::Operand& source) {
  port->send_total = port->send_position = 0;
  int checksum = 0;
  for (int i = 0; i < port->header_size; ++i)
    port->send_data[port->send_total++] = port->header[i];
  for (int i = 0; i < port->send_data_count; ++i) {
    uint8_t byte = 0;
    if (source.kind == model::OperandKind::kImmediateString) {
      if (i < static_cast<int>(source.text.size()))
        byte = static_cast<uint8_t>(source.text[i]);
    } else {
      auto device = source.device;
      const bool byte_mode = !port->rs2 && memory_.special_m[161];
      device.index += byte_mode ? i : i / 2;
      const uint16_t word = static_cast<uint16_t>(GetWordValue(device));
      byte = static_cast<uint8_t>(word >> (byte_mode ? 0 : (i % 2) * 8));
    }
    checksum = (checksum + byte) & 255;
    port->send_data[port->send_total++] = byte;
  }
  for (int i = 0; i < port->terminator_size; ++i) {
    checksum = (checksum + port->terminator[i]) & 255;
    port->send_data[port->send_total++] = port->terminator[i];
  }
  if (port->checksum) {
    port->send_data[port->send_total++] = kHex[checksum >> 4];
    port->send_data[port->send_total++] = kHex[checksum & 15];
    memory_.D[port->rs2 ? 8386 + kChannelOffset[port->channel] : 8126] =
        checksum;
  }
  port->sending = port->send_total != 0;
  if (!port->sending)
    memory_.special_m[port->request_relay] = false;
  memory_.D[port->send_monitor] = static_cast<int16_t>(port->send_data_count);
  return true;
}

void CompiledPLCExecutor::FinishSerialReceive(SerialPort* port) {
  auto destination = port->receive_device;
  const bool byte_mode = !port->rs2 && memory_.special_m[161];
  for (int i = 0; i < port->received; ++i) {
    destination.index = port->receive_device.index + (byte_mode ? i : i / 2);
    const uint16_t previous =
        byte_mode ? 0 : static_cast<uint16_t>(GetWordValue(destination));
    const uint16_t mask = i % 2 ? 0x00FF : 0xFF00;
    const uint16_t next =
        byte_mode ? port->receive_data[i]
                  : static_cast<uint16_t>(
                        (previous & mask) |
                        (uint16_t{port->receive_data[i]} << ((i % 2) * 8)));
    WriteWord(destination, next);
  }
  memory_.D[port->receive_monitor] = static_cast<int16_t>(port->received);
  memory_.special_m[port->receive_relay] = true;
  port->receiving = false;
}

void CompiledPLCExecutor::ReceiveSerialByte(SerialPort* port, uint8_t value) {
  if (!port->receiving) {
    if (port->header_size == 0) {
      port->receiving = true;
      port->received = port->send_checksum = 0;
    } else {
      if (value == port->header[port->matched_header])
        ++port->matched_header;
      else
        port->matched_header = value == port->header[0] ? 1 : 0;
      if (port->matched_header == port->header_size) {
        port->receiving = true;
        port->matched_header = 0;
        port->received = port->send_checksum = 0;
      }
      return;
    }
  }
  port->idle_ms = 0;
  if (port->receive_checksum) {
    const int digit = HexDigit(value);
    if (digit < 0) {
      memory_.special_m[port->channel == 2 ? 438 : 62 + port->channel] = true;
      memory_.D[port->channel == 2 ? 8438 : 8062 + port->channel] = 6304;
      port->receiving = false;
      port->receive_checksum = false;
      return;
    }
    port->received_checksum = port->received_checksum * 16 + digit;
    if (++port->checksum_digits == 2) {
      const int sum = port->send_checksum & 255;
      memory_.D[8384 + kChannelOffset[port->channel]] =
          static_cast<int16_t>(port->received_checksum);
      memory_.D[8385 + kChannelOffset[port->channel]] =
          static_cast<int16_t>(sum);
      if (port->received_checksum == sum)
        FinishSerialReceive(port);
      else {
        memory_.special_m[port->channel == 2 ? 438 : 62 + port->channel] = true;
        memory_.D[port->channel == 2 ? 8438 : 8062 + port->channel] = 6304;
        port->receiving = false;
      }
      port->receive_checksum = false;
    }
    return;
  }
  if (port->terminator_size > 0) {
    if (value == port->terminator[port->matched_terminator]) {
      ++port->matched_terminator;
      if (port->matched_terminator == port->terminator_size) {
        for (int i = 0; i < port->terminator_size; ++i)
          port->send_checksum += port->terminator[i];
        port->matched_terminator = 0;
        if (port->checksum) {
          port->receive_checksum = true;
          port->received_checksum = port->checksum_digits = 0;
        } else
          FinishSerialReceive(port);
      }
      return;
    }
    for (int i = 0; i < port->matched_terminator; ++i) {
      if (port->received < port->receive_limit) {
        port->receive_data[port->received++] = port->terminator[i];
        port->send_checksum += port->terminator[i];
      }
    }
    port->matched_terminator = 0;
  }
  if (port->received < port->receive_limit) {
    port->receive_data[port->received++] = value;
    port->send_checksum += value;
  }
  if (port->received == port->receive_limit && !port->checksum)
    FinishSerialReceive(port);
}

void CompiledPLCExecutor::AdvanceSerialPorts() {
  for (auto& port : serial_ports_) {
    if (port.owner == SIZE_MAX)
      continue;
    if (port.sending) {
      port.byte_credit += static_cast<double>(current_elapsed_ms_) * port.baud /
                          (1000.0 * port.bits_per_character);
      int allowance = static_cast<int>(port.byte_credit);
      port.byte_credit -= allowance;
      while (allowance-- > 0 && port.send_position < port.send_total &&
             port.outgoing_count < static_cast<int>(port.outgoing.size())) {
        port.outgoing[(port.outgoing_head + port.outgoing_count) %
                      port.outgoing.size()] =
            port.send_data[port.send_position++];
        ++port.outgoing_count;
        memory_.D[port.send_monitor] = static_cast<int16_t>(std::max(
            0, port.send_data_count - (port.send_position - port.header_size)));
      }
      if (port.send_position == port.send_total) {
        port.sending = false;
        memory_.special_m[port.request_relay] = false;
      }
    }
    if (memory_.special_m[port.receive_relay])
      continue;
    memory_.special_m[port.timeout_relay] = false;
    if (port.incoming_count == 0 && port.receiving && port.received > 0) {
      port.idle_ms += current_elapsed_ms_;
      const int setting = port.rs2
                              ? memory_.D[8379 + kChannelOffset[port.channel]]
                              : memory_.D[8129];
      const int timeout = setting <= 0 ? 100 : setting * 10;
      if (port.idle_ms >= timeout) {
        memory_.special_m[port.timeout_relay] = true;
        FinishSerialReceive(&port);
      }
    }
    if (port.receive_limit == 0)
      continue;
    while (port.incoming_count > 0 && !memory_.special_m[port.receive_relay]) {
      const uint8_t value = port.incoming[port.incoming_head];
      port.incoming_head = (port.incoming_head + 1) % port.incoming.size();
      --port.incoming_count;
      ReceiveSerialByte(&port, value);
    }
  }
}

bool CompiledPLCExecutor::ExecuteSerialOperation(
    const model::OpenPLCInstruction& instruction, size_t index, bool power) {
  const bool rs2 = instruction.opcode == model::Opcode::kSerial2;
  if (!power) {
    for (auto& port : serial_ports_)
      if (port.owner == index) {
        port.owner = SIZE_MAX;
        port.sending = port.receiving = false;
        memory_.special_m[port.request_relay] = false;
      }
    return true;
  }
  const int channel = rs2 ? ReadValue(instruction.operands[4]) : 1;
  if (channel < 0 || channel >= 3) {
    SetError("RS2 channel must be 0, 1 or 2");
    return false;
  }
  auto& port = serial_ports_[channel];
  if (channel >= 1 && inverter_owners_[channel - 1] != SIZE_MAX) {
    SetError("Serial port is occupied by inverter communication");
    return false;
  }
  if (modbus_owner_ != SIZE_MAX &&
      modbus_transfers_[modbus_owner_].channel == channel) {
    SetError("Serial port is occupied by MODBUS communication");
    return false;
  }
  if (channel >= 1 && cf_cards_[channel - 1]) {
    SetError("Serial port is configured for a CF-ADP");
    return false;
  }
  if (port.owner != SIZE_MAX && port.owner != index) {
    SetError("RS and RS2 cannot drive the same communication port together");
    return false;
  }
  if (port.owner != index) {
    const int format = static_cast<uint16_t>(
        memory_.D[rs2 ? 8370 + kChannelOffset[channel] : 8120]);
    const int code = (format >> 4) & 15;
    if (code < 3 || code > 10 || ((format >> 1) & 3) == 2 ||
        (rs2 && (format & 8192) && !(format & 512))) {
      SetError(
          "RS/RS2 serial format, baud rate or checksum setting is invalid");
      return false;
    }
    port.owner = index;
    port.byte_credit = 0;
    port.rs2 = rs2;
    port.channel = channel;
    port.baud = kBaudRates[code];
    port.bits_per_character = 1 + (format & 1 ? 8 : 7) +
                              (((format >> 1) & 3) != 0) + (format & 8 ? 2 : 1);
    port.send_data_count = ReadValue(instruction.operands[1]);
    port.receive_limit = ReadValue(instruction.operands[3]);
    port.send_device = instruction.operands[0].device;
    port.receive_device = instruction.operands[2].device;
    if (port.send_data_count < 0 || port.send_data_count > 4096 ||
        port.receive_limit < 0 || port.receive_limit > 4096) {
      SetError("RS/RS2 transfer size must be 0..4096 bytes");
      port.owner = SIZE_MAX;
      return false;
    }
    port.request_relay = rs2 ? 372 + kChannelOffset[channel] : 122;
    port.receive_relay = rs2 ? 373 + kChannelOffset[channel] : 123;
    port.timeout_relay = rs2 ? 379 + kChannelOffset[channel] : 129;
    port.send_monitor = rs2 ? 8372 + kChannelOffset[channel] : 8122;
    port.receive_monitor = rs2 ? 8373 + kChannelOffset[channel] : 8123;
    port.checksum = rs2 && (format & 8192);
    port.header_size = port.terminator_size = 0;
    const int header = rs2 ? 8380 + kChannelOffset[channel] : 8124;
    const int terminator = rs2 ? 8382 + kChannelOffset[channel] : 8125;
    if (format & 256)
      for (int i = 0; i < (rs2 ? 4 : 1); ++i) {
        const uint8_t value =
            static_cast<uint16_t>(memory_.D[header + i / 2]) >> ((i % 2) * 8);
        if (!value)
          break;
        port.header[port.header_size++] = value;
      }
    if (format & 512)
      for (int i = 0; i < (rs2 ? 4 : 1); ++i) {
        const uint8_t value =
            static_cast<uint16_t>(memory_.D[terminator + i / 2]) >>
            ((i % 2) * 8);
        if (!value)
          break;
        port.terminator[port.terminator_size++] = value;
      }
    const bool byte_mode = !rs2 && memory_.special_m[161];
    const int source_words =
        byte_mode ? port.send_data_count : (port.send_data_count + 1) / 2;
    const int destination_words =
        byte_mode ? port.receive_limit : (port.receive_limit + 1) / 2;
    const auto& source = instruction.operands[0];
    if ((source.kind != model::OperandKind::kImmediateString &&
         source.device.index + source_words >
             model::WordDeviceLimit(source.device)) ||
        port.receive_device.index + destination_words >
            model::WordDeviceLimit(port.receive_device)) {
      SetError("RS/RS2 send or receive register block exceeds memory");
      port.owner = SIZE_MAX;
      return false;
    }
    port.receiving = port.header_size == 0 && port.receive_limit > 0;
    port.received = port.matched_header = port.matched_terminator = 0;
    port.send_checksum = 0;
    port.idle_ms = 0;
    memory_.D[port.receive_monitor] = 0;
    memory_.D[port.send_monitor] = 0;
  }
  if (memory_.special_m[port.request_relay] && !port.sending &&
      !StartSerialSend(&port, instruction.operands[0]))
    return false;
  return true;
}
}  // namespace plc
