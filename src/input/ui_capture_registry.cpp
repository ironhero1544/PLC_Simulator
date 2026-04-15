#include "plc_emulator/input/ui_capture_registry.h"

namespace plc {

void UiCaptureRegistry::BeginFrame() {
  previous_rects_ = current_rects_;
  current_rects_.clear();
}

void UiCaptureRegistry::RegisterRect(ImVec2 min, ImVec2 max, bool capturing) {
  Rect rect;
  rect.min = min;
  rect.max = max;
  rect.capturing = capturing;
  current_rects_.push_back(rect);
}

bool UiCaptureRegistry::Contains(ImVec2 screen_pos) const {
  auto contains = [screen_pos](const Rect& rect) {
    return screen_pos.x >= rect.min.x && screen_pos.x <= rect.max.x &&
           screen_pos.y >= rect.min.y && screen_pos.y <= rect.max.y;
  };

  for (const auto& rect : current_rects_) {
    if (contains(rect)) {
      return true;
    }
  }
  for (const auto& rect : previous_rects_) {
    if (contains(rect)) {
      return true;
    }
  }
  return false;
}

bool UiCaptureRegistry::IsCapturingMouse(ImVec2 screen_pos) const {
  auto contains_capture = [screen_pos](const Rect& rect) {
    return rect.capturing &&
           screen_pos.x >= rect.min.x && screen_pos.x <= rect.max.x &&
           screen_pos.y >= rect.min.y && screen_pos.y <= rect.max.y;
  };

  for (const auto& rect : current_rects_) {
    if (contains_capture(rect)) {
      return true;
    }
  }
  for (const auto& rect : previous_rects_) {
    if (contains_capture(rect)) {
      return true;
    }
  }
  return false;
}

}  // namespace plc
