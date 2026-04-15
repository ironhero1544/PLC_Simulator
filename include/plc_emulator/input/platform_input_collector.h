#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_INPUT_PLATFORM_INPUT_COLLECTOR_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_INPUT_PLATFORM_INPUT_COLLECTOR_H_

namespace plc {

class PlatformInputCollector {
 public:
  void BeginFrame();

  void RegisterWin32RightClick();
  void RegisterWin32SideClick();
  void RegisterWin32SideDown(bool is_down);

  bool ConsumeWin32RightClick();
  bool ConsumeWin32SideClick();
  bool IsWin32SideDown() const;
  void SetWin32SideDown(bool is_down);

 private:
  bool win32_right_click_ = false;
  bool win32_side_click_ = false;
  bool win32_side_down_ = false;
};

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_INPUT_PLATFORM_INPUT_COLLECTOR_H_
