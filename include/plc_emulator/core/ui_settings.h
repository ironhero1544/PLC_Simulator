/*
 * ui_settings.h
 *
 * UI 설정 구조체와 저장/로드 함수 선언.
 * UI settings structure and persistence helpers.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_UI_SETTINGS_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_UI_SETTINGS_H_

namespace plc {

/*
 * UI 스케일 및 렌더링 옵션을 보관합니다.
 * Holds UI scale and rendering options.
 */
struct UiSettings {
  float ui_scale;
  float font_scale;
  float layout_scale;
  bool vsync_enabled;
  bool frame_limit_enabled;
  bool restart_required;
};

/*
 * 기본값 설정과 저장/로드 인터페이스입니다.
 * Default initialization and load/save interfaces.
 */
void SetDefaultUiSettings(UiSettings* settings);
bool LoadUiSettings(UiSettings* settings);
bool SaveUiSettings(const UiSettings& settings);
void MarkUiSettingsRestartRequired(UiSettings* settings);

}  /* namespace plc */

#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_UI_SETTINGS_H_ */
