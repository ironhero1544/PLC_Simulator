#include "plc_emulator/programming/compiled_plc_executor.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace plc {
namespace {
namespace model = plc_emulator::programming;

bool NormalCfWord(model::DeviceAddress device, int count) {
  return device.bit < 0 && ((device.kind == model::DeviceKind::kD &&
                             device.index + count <= 8000) ||
                            (device.kind == model::DeviceKind::kR &&
                             device.index + count <= 32768));
}
}  // namespace

void CompiledPLCExecutor::RefreshCfStatus() {
  for (int channel = 0; channel < 2; ++channel) {
    if (!cf_cards_[channel])
      continue;
    const auto& card = *cf_cards_[channel];
    const int offset = channel * 20;
    memory_.special_m[404 + offset] = card.ready && card.owner == SIZE_MAX;
    memory_.special_m[405 + offset] = card.mounted;
    memory_.D[8408 + offset] = 100;
    memory_.D[8419 + offset] = 4;
    if (!memory_.special_m[410 + offset])
      memory_.D[8406 + offset] = (!card.mounted ? 1 : 0) |
                                 (card.used >= card.capacity ? 2 : 0) |
                                 (card.errors[0] ? 4 : 0);
  }
}

bool CompiledPLCExecutor::RotateCfFifo(CfCard* card) {
  if (card->ids[0] < 0)
    return false;
  const auto previous = card->files[card->ids[0]];
  std::array<char, 9> name{};
  std::snprintf(name.data(), name.size(), "FILE%04u",
                static_cast<unsigned>(card->generation % 1000));
  int slot = -1;
  for (size_t index = 0; index < card->files.size(); ++index) {
    auto& file = card->files[index];
    if (file.exists && file.fifo && file.name == name)
      DeleteCfFile(card, static_cast<int>(index));
    if (!file.exists && slot < 0)
      slot = static_cast<int>(index);
  }
  if (slot < 0)
    return false;
  auto& file = card->files[slot];
  file = previous;
  file.name = name;
  file.exists = file.fifo = true;
  file.generation = card->generation++;
  file.last_line = 0;
  file.next_line = 1;
  card->ids[0] = slot;
  while (static_cast<uint64_t>(card->used) * 100 >
         static_cast<uint64_t>(card->capacity) * file.policy) {
    int oldest = -1;
    for (size_t index = 0; index < card->files.size(); ++index)
      if (card->files[index].exists && card->files[index].fifo &&
          static_cast<int>(index) != slot &&
          (oldest < 0 ||
           card->files[index].generation < card->files[oldest].generation))
        oldest = static_cast<int>(index);
    if (oldest < 0)
      break;
    DeleteCfFile(card, oldest);
  }
  return true;
}

int CompiledPLCExecutor::CreateCfFile(
    const model::OpenPLCInstruction& instruction, CfCard* card) {
  const int id = ReadValue(instruction.operands[0]);
  if (id < 0 || id > 63)
    return 802;
  const auto head = instruction.operands[2].device;
  if (!NormalCfWord(head, 4))
    return 611;
  const auto parameters = WordMemory(head);
  const int timestamp = parameters[head.index];
  const int type = parameters[head.index + 1];
  const int maximum = parameters[head.index + 2];
  const int policy = parameters[head.index + 3];
  if (timestamp < 0 || timestamp > 7)
    return 810;
  if (type < 0 || type > 7)
    return 813;
  if (maximum < 1 || maximum > 32767)
    return 811;
  if (id == 0 ? policy < 10 || policy > 90 : policy != 0 && policy != 1)
    return 812;
  std::array<char, 9> name{};
  if (id != 0) {
    size_t length = 0;
    if (!ReadString(instruction.operands[1], name, &length) || length == 0 ||
        length > 8)
      return 808;
    for (size_t index = 0; index < length; ++index) {
      const unsigned char character = static_cast<unsigned char>(name[index]);
      if (!(std::isalnum(character) ||
            std::string_view("!#$%&'()+-@^_`~").find(character) !=
                std::string_view::npos))
        return 809;
      name[index] = static_cast<char>(std::toupper(character));
    }
  }
  int slot = card->ids[id];
  if (slot >= 0) {
    const auto& file = card->files[slot];
    return (id == 0 || file.name == name) && file.timestamp == timestamp &&
                   file.type == type && file.maximum_lines == maximum &&
                   file.policy == policy
               ? 0
               : 803;
  }
  if (id != 0)
    for (size_t index = 0; index < card->files.size(); ++index)
      if (card->files[index].exists && card->files[index].name == name) {
        if (std::find(card->ids.begin(), card->ids.end(),
                      static_cast<int>(index)) != card->ids.end())
          return 807;
        slot = static_cast<int>(index);
        if (card->files[index].last_line > maximum)
          return 804;
        break;
      }
  if (slot < 0) {
    for (size_t index = 0; index < card->files.size(); ++index)
      if (!card->files[index].exists) {
        slot = static_cast<int>(index);
        break;
      }
    if (slot < 0)
      return 903;
    auto& file = card->files[slot];
    file = {};
    file.exists = true;
    file.fifo = id == 0;
    file.name = name;
    if (id == 0) {
      std::snprintf(file.name.data(), file.name.size(), "FILE%04u",
                    static_cast<unsigned>(card->generation % 1000));
      file.generation = card->generation++;
    }
  }
  auto& file = card->files[slot];
  file.timestamp = timestamp;
  file.type = type;
  file.maximum_lines = maximum;
  file.policy = policy;
  card->ids[id] = slot;
  return 0;
}

int CompiledPLCExecutor::ReadCfStatus(int command,
                                      model::DeviceAddress destination,
                                      CfCard* card) {
  const int count = command >= 0 && command <= 63 ? 2
                    : command == 256              ? 4
                    : command == 512              ? 6
                    : command == 1280             ? 5
                                                  : 1;
  if (!NormalCfWord(destination, count))
    return 611;
  const auto write = [&](int offset, uint32_t value, bool wide = false) {
    auto device = destination;
    device.index += offset;
    return WriteWord(device, value, wide);
  };
  if (command >= 0 && command <= 63) {
    const int file = card->ids[command];
    if (file < 0)
      return 801;
    auto* row = FindCfRow(card, file, card->files[file].last_line, false);
    write(0, card->files[file].last_line);
    write(1, row ? row->count : 0);
  } else if (command == 256) {
    for (int word = 0; word < 4; ++word) {
      uint16_t bits = 0;
      for (int bit = 0; bit < 16; ++bit)
        if (card->ids[word * 16 + bit] >= 0)
          bits |= uint16_t{1} << bit;
      write(word, bits);
    }
  } else if (command == 512) {
    write(0, std::max<uint32_t>(1, (card->capacity + 1023) / 1024), true);
    write(2, std::max<uint32_t>(1, (card->used + 1023) / 1024), true);
    write(4, std::max<uint32_t>(1, (card->capacity - card->used + 1023) / 1024),
          true);
  } else if (command == 768) {
    write(0, 100);
  } else if (command == 1024) {
    write(0, (!card->mounted ? 1 : 0) | (card->used >= card->capacity ? 2 : 0) |
                 (card->errors[0] ? 4 : 0));
  } else if (command == 1280) {
    for (int index = 0; index < 5; ++index)
      write(index, card->errors[index]);
  } else {
    return 619;
  }
  return 0;
}

int CompiledPLCExecutor::ApplyCfOperation(
    const model::OpenPLCInstruction& instruction, CfCard* card) {
  using model::Opcode;
  const int command = ReadValue(instruction.operands[0]);
  const int channel =
      ReadValue(instruction.operands[instruction.operand_count - 1]);
  if (instruction.opcode == Opcode::kCfCommand) {
    if (card->mixed.active)
      return 701;
    if (command == 256 || command == 512) {
      SetCfCardMounted(channel, command == 256);
      return 0;
    }
    if (command == 1280) {
      card->errors.fill(0);
      memory_.special_m[418 + (channel - 1) * 20] = false;
      memory_.D[8418 + (channel - 1) * 20] = 0;
      return 0;
    }
    if (command < -1 || command > 63)
      return 619;
    if (!card->mounted)
      return 704;
    const int file = command < 0 ? -1 : card->ids[command];
    if (command >= 0 && file < 0)
      return 801;
    FlushCfBuffer(card, file);
    return 0;
  }
  if (instruction.opcode == Opcode::kCfStatus)
    return card->mixed.active
               ? 701
               : ReadCfStatus(command, instruction.operands[1].device, card);
  if (!card->mounted)
    return 704;
  if (instruction.opcode == Opcode::kCfWrite ||
      instruction.opcode == Opcode::kCfRead)
    return TransferCfData(instruction, card,
                          instruction.opcode == Opcode::kCfRead);
  if (card->mixed.active)
    return 701;
  if (instruction.opcode == Opcode::kCfCreate)
    return CreateCfFile(instruction, card);
  if (instruction.opcode == Opcode::kCfDelete) {
    const int method = ReadValue(instruction.operands[1]);
    if (command == 512) {
      if (method != 256)
        return 617;
      for (size_t file = 0; file < card->files.size(); ++file)
        if (card->files[file].exists)
          DeleteCfFile(card, static_cast<int>(file));
      card->generation = 0;
      return 0;
    }
    if (command < -1 || command > 63)
      return 802;
    if (method != 0 && method != 1)
      return 616;
    if (command == -1) {
      if (method == 1)
        card->ids.fill(-1);
      else
        for (size_t file = 0; file < card->files.size(); ++file)
          if (card->files[file].exists)
            DeleteCfFile(card, static_cast<int>(file));
      return 0;
    }
    if (card->ids[command] < 0)
      return 801;
    if (command == 0) {
      for (size_t file = 0; file < card->files.size(); ++file)
        if (card->files[file].exists && card->files[file].fifo)
          DeleteCfFile(card, static_cast<int>(file));
    } else if (method == 1) {
      card->ids[command] = -1;
    } else {
      DeleteCfFile(card, card->ids[command]);
    }
    return 0;
  }
  return 700;
}

bool CompiledPLCExecutor::ExecuteCfOperation(
    const model::OpenPLCInstruction& instruction, size_t index, bool power) {
  auto& transfer = cf_transfers_[index];
  if (transfer.clear_pulse) {
    memory_.special_m[29] = memory_.special_m[329] = false;
    transfer.clear_pulse = false;
  }
  if (!power) {
    if (transfer.active && cf_cards_[transfer.channel - 1]) {
      auto& card = *cf_cards_[transfer.channel - 1];
      if (card.owner == index) {
        card.owner = SIZE_MAX;
        card.mixed.active = false;
      }
    }
    transfer = {};
    return true;
  }
  if (transfer.completed)
    return true;
  const int channel =
      ReadValue(instruction.operands[instruction.operand_count - 1]);
  if (channel < 1 || channel > 2) {
    SetError("CF-ADP channel must be 1 or 2");
    return false;
  }
  const int offset = (channel - 1) * 20;
  auto* card = cf_cards_[channel - 1].get();
  const auto finish = [&](int error) {
    transfer.active = false;
    transfer.completed = true;
    transfer.clear_pulse = error || !card || !card->mixed.active;
    if (card && card->owner == index)
      card->owner = SIZE_MAX;
    memory_.special_m[402 + offset] = false;
    if (transfer.clear_pulse)
      memory_.special_m[error ? 329 : 29] = true;
    if (error) {
      if (error == 611) {
        memory_.special_m[67] = true;
        memory_.D[8067] = 6706;
      }
      memory_.special_m[418 + offset] = true;
      memory_.D[8418 + offset] = static_cast<int16_t>(error);
      memory_.D[8414 + offset] =
          static_cast<int16_t>(instruction_step_numbers_[index]);
      memory_.D[8415 + offset] =
          static_cast<int16_t>(instruction_step_numbers_[index] >> 16);
      if (card) {
        std::move_backward(card->errors.begin(), card->errors.end() - 1,
                           card->errors.end());
        card->errors[0] = static_cast<int16_t>(error);
        card->mixed.active = false;
      }
    }
    return true;
  };
  if (!card || !card->ready)
    return finish(700);
  if (!transfer.active) {
    if (serial_ports_[channel].owner != SIZE_MAX ||
        inverter_owners_[channel - 1] != SIZE_MAX ||
        (modbus_owner_ != SIZE_MAX &&
         modbus_transfers_[modbus_owner_].channel == channel))
      return finish(700);
    if (card->owner != SIZE_MAX)
      return true;
    transfer.active = true;
    transfer.channel = channel;
    card->owner = index;
    memory_.special_m[402 + offset] = true;
    memory_.special_m[29] = memory_.special_m[329] = false;
    memory_.D[8402 + offset] =
        static_cast<int16_t>(instruction_step_numbers_[index]);
    memory_.D[8403 + offset] =
        static_cast<int16_t>(instruction_step_numbers_[index] >> 16);
    return true;
  }
  if (transfer.channel != channel) {
    SetError("CF-ADP channel changed while an instruction was busy");
    return false;
  }
  return finish(ApplyCfOperation(instruction, card));
}
}  // namespace plc
