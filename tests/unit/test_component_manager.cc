// Copyright 2024 PLC Emulator Project
// ComponentManager 단위 테스트

#include "plc_emulator/core/component_manager.h"

#include <gtest/gtest.h>
#include <memory>

namespace plc {
namespace {

class ComponentManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    manager_ = std::make_unique<ComponentManager>();
  }

  std::unique_ptr<ComponentManager> manager_;
};

TEST_F(ComponentManagerTest, AddComponent) {
  PlacedComponent comp;
  comp.type = ComponentType::PLC;
  comp.position = {100.0f, 200.0f};
  
  int id = manager_->AddComponent(comp);
  
  EXPECT_GE(id, 0);
  EXPECT_EQ(manager_->GetComponents().size(), 1);
}

TEST_F(ComponentManagerTest, RemoveComponent) {
  PlacedComponent comp;
  comp.type = ComponentType::PLC;
  
  int id = manager_->AddComponent(comp);
  EXPECT_EQ(manager_->GetComponents().size(), 1);
  
  bool removed = manager_->RemoveComponent(id);
  EXPECT_TRUE(removed);
  EXPECT_EQ(manager_->GetComponents().size(), 0);
}

TEST_F(ComponentManagerTest, GetComponent) {
  PlacedComponent comp;
  comp.type = ComponentType::FRL;
  comp.position = {50.0f, 75.0f};
  
  int id = manager_->AddComponent(comp);
  
  auto* retrieved = manager_->GetComponent(id);
  ASSERT_NE(retrieved, nullptr);
  EXPECT_EQ(retrieved->type, ComponentType::FRL);
  EXPECT_FLOAT_EQ(retrieved->position.x, 50.0f);
  EXPECT_FLOAT_EQ(retrieved->position.y, 75.0f);
}

TEST_F(ComponentManagerTest, GetNonexistentComponent) {
  auto* comp = manager_->GetComponent(9999);
  EXPECT_EQ(comp, nullptr);
}

TEST_F(ComponentManagerTest, ClearAll) {
  for (int i = 0; i < 5; ++i) {
    PlacedComponent comp;
    comp.type = ComponentType::PLC;
    manager_->AddComponent(comp);
  }
  
  EXPECT_EQ(manager_->GetComponents().size(), 5);
  
  manager_->Clear();
  EXPECT_EQ(manager_->GetComponents().size(), 0);
}

}  // namespace
}  // namespace plc
