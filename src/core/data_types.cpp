// data_types.cpp
//
// Implementation of common data types.

// src/DataTypes.cpp

#include "plc_emulator/core/data_types.h"

#include "imgui.h"  // FIX: ImVec2의 완전한 정의를 위해 imgui.h를 포함합니다.

namespace plc {

// [PPT: 새로운 내용 2 - 생성자 오버로딩]
// ImVec2에서 Position으로의 변환 생성자 구현
Position::Position(const ImVec2& vec) : x(vec.x), y(vec.y) {}

// [PPT: 새로운 내용 2 - 형변환 연산자]
// Position에서 ImVec2로의 변환 연산자 구현
Position::operator ImVec2() const {
  return ImVec2(x, y);
}

}  // namespace plc