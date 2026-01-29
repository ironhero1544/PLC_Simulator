#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_UI_SETTINGS_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_UI_SETTINGS_H_

namespace plc {

struct UiSettings {
  float ui_scale;
  float font_scale;
  float layout_scale;
  bool restart_required;
};

void SetDefaultUiSettings(UiSettings* settings);
bool LoadUiSettings(UiSettings* settings);
bool SaveUiSettings(const UiSettings& settings);
void MarkUiSettingsRestartRequired(UiSettings* settings);

}  // namespace plc

#endif  // PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_UI_SETTINGS_H_
