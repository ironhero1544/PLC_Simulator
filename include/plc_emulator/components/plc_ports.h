#pragma once

namespace plc_emulator {

constexpr int kPlcInputCount = 24;
constexpr int kPlcOutputCount = 16;
constexpr int kPlcPortCapacity = 41;

// Keep original X0-X17/Y0-Y17 IDs. ID 32 is a retired COM terminal;
// leave it reserved so legacy COM wires remain invalid.
constexpr int PlcInputPort(int index) {
  return index < 16 ? index : index + 17;
}

constexpr int PlcInputIndex(int port) {
  if (port >= 0 && port < 16)
    return port;
  if (port >= 33 && port < kPlcPortCapacity)
    return port - 17;
  return -1;
}

}  // namespace plc_emulator
