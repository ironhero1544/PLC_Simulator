#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_INPUT_UI_CAPTURE_REGISTRY_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_INPUT_UI_CAPTURE_REGISTRY_H_

#include "imgui.h"

#include <vector>

namespace plc {

class UiCaptureRegistry {
 public:
  struct Rect {
    ImVec2 min = {0.0f, 0.0f};
    ImVec2 max = {0.0f, 0.0f};
    bool capturing = false;
  };

  void BeginFrame();
  void RegisterRect(ImVec2 min, ImVec2 max, bool capturing);
  bool Contains(ImVec2 screen_pos) const;
  bool IsCapturingMouse(ImVec2 screen_pos) const;

 private:
  std::vector<Rect> current_rects_;
  std::vector<Rect> previous_rects_;
};

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_INPUT_UI_CAPTURE_REGISTRY_H_
