// Copyright 2024 PLC Emulator Project
// 데이터 타입 단위 테스트

#include "plc_emulator/core/data_types.h"

#include <gtest/gtest.h>

namespace plc {
namespace {

// Position 구조체 테스트
TEST(DataTypesTest, PositionDefaultConstruction) {
  Position pos;
  EXPECT_FLOAT_EQ(pos.x, 0.0f);
  EXPECT_FLOAT_EQ(pos.y, 0.0f);
}

TEST(DataTypesTest, PositionDistance) {
  Position p1{0.0f, 0.0f};
  Position p2{3.0f, 4.0f};
  
  float dx = p2.x - p1.x;
  float dy = p2.y - p1.y;
  float distance = std::sqrt(dx * dx + dy * dy);
  
  EXPECT_FLOAT_EQ(distance, 5.0f);
}

// Color 구조체 테스트
TEST(DataTypesTest, ColorValues) {
  Color red{1.0f, 0.0f, 0.0f, 1.0f};
  
  EXPECT_FLOAT_EQ(red.r, 1.0f);
  EXPECT_FLOAT_EQ(red.g, 0.0f);
  EXPECT_FLOAT_EQ(red.b, 0.0f);
  EXPECT_FLOAT_EQ(red.a, 1.0f);
}

// Wire 구조체 테스트
TEST(DataTypesTest, WireCreation) {
  Wire wire;
  wire.fromComponentId = 1;
  wire.toComponentId = 2;
  wire.fromPortIndex = 0;
  wire.toPortIndex = 1;
  
  EXPECT_EQ(wire.fromComponentId, 1);
  EXPECT_EQ(wire.toComponentId, 2);
  EXPECT_EQ(wire.fromPortIndex, 0);
  EXPECT_EQ(wire.toPortIndex, 1);
}

// ComponentType enum 테스트
TEST(DataTypesTest, ComponentTypeEnum) {
  ComponentType type = ComponentType::kPLC;
  EXPECT_EQ(type, ComponentType::kPLC);
  
  type = ComponentType::kFRL;
  EXPECT_NE(type, ComponentType::kPLC);
}

}  // namespace
}  // namespace plc
