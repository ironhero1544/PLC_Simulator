// Copyright 2024 PLC Emulator Project
// 배선 시스템 통합 테스트

#include "plc_emulator/core/component_manager.h"
#include "plc_emulator/core/data_types.h"

#include <gtest/gtest.h>

namespace plc {
namespace {

class WiringIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    component_mgr_ = std::make_unique<ComponentManager>();
  }

  std::unique_ptr<ComponentManager> component_mgr_;
};

TEST_F(WiringIntegrationTest, CreateAndConnectComponents) {
  // PLC 컴포넌트 생성
  PlacedComponent plc;
  plc.type = ComponentType::kPLC;
  plc.position = {100.0f, 100.0f};
  int plc_id = component_mgr_->AddComponent(plc);
  
  // FRL 컴포넌트 생성
  PlacedComponent frl;
  frl.type = ComponentType::kFRL;
  frl.position = {300.0f, 100.0f};
  int frl_id = component_mgr_->AddComponent(frl);
  
  // 두 컴포넌트가 추가되었는지 확인
  EXPECT_EQ(component_mgr_->GetComponentCount(), 2);
  
  auto* plc_comp = component_mgr_->GetComponent(plc_id);
  auto* frl_comp = component_mgr_->GetComponent(frl_id);
  
  ASSERT_NE(plc_comp, nullptr);
  ASSERT_NE(frl_comp, nullptr);
  
  EXPECT_EQ(plc_comp->type, ComponentType::kPLC);
  EXPECT_EQ(frl_comp->type, ComponentType::kFRL);
}

// TODO: Wire 연결 테스트 추가
// TODO: I/O 매핑 테스트 추가

}  // namespace
}  // namespace plc
