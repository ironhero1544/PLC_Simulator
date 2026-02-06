#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_LANG_LANG_MANAGER_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_LANG_LANG_MANAGER_H_

namespace plc {

void InitializeLanguage();
const char* Translate(const char* key, const char* fallback);
bool FormatString(char* out,
                  unsigned int out_size,
                  const char* key,
                  const char* fallback,
                  ...);
const char* GetActiveLanguageCode();
const char* GetUserLanguageCode();
bool SetUserLanguageCode(const char* code);
bool IsLanguageRestartRequired();

}  /* namespace plc */

#define TR(key, fallback) ::plc::Translate((key), (fallback))

#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_LANG_LANG_MANAGER_H_ */
