#include "plc_emulator/input/platform_input_collector.h"

namespace plc {

void PlatformInputCollector::BeginFrame() {
  win32_right_click_ = false;
  win32_side_click_ = false;
}

void PlatformInputCollector::RegisterWin32RightClick() {
  win32_right_click_ = true;
}

void PlatformInputCollector::RegisterWin32SideClick() {
  win32_side_click_ = true;
}

void PlatformInputCollector::RegisterWin32SideDown(bool is_down) {
  win32_side_down_ = is_down;
}

bool PlatformInputCollector::ConsumeWin32RightClick() {
  const bool was_pressed = win32_right_click_;
  win32_right_click_ = false;
  return was_pressed;
}

bool PlatformInputCollector::ConsumeWin32SideClick() {
  const bool was_pressed = win32_side_click_;
  win32_side_click_ = false;
  return was_pressed;
}

bool PlatformInputCollector::IsWin32SideDown() const {
  return win32_side_down_;
}

void PlatformInputCollector::SetWin32SideDown(bool is_down) {
  win32_side_down_ = is_down;
}

}  // namespace plc
