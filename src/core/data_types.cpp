// data_types.cpp
//
// Implementation of common data types.

#include "plc_emulator/core/data_types.h"

#include "imgui.h"  // ImVec2 definition.

namespace plc {

// Build Position from an ImVec2.
Position::Position(const ImVec2& vec) : x(vec.x), y(vec.y) {}

// Convert Position to ImVec2 for ImGui calls.
Position::operator ImVec2() const {
  return ImVec2(x, y);
}

}  // namespace plc
