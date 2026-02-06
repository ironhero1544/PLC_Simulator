// ui_settings.cpp
//
// UI settings load/save for scale configuration.

#include "plc_emulator/core/ui_settings.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <cwchar>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace plc {
namespace {

void Trim(char* text) {
  if (!text) {
    return;
  }
  char* start = text;
  while (*start && std::isspace(static_cast<unsigned char>(*start))) {
    ++start;
  }
  if (start != text) {
    std::memmove(text, start, std::strlen(start) + 1);
  }
  size_t len = std::strlen(text);
  while (len > 0 &&
         std::isspace(static_cast<unsigned char>(text[len - 1]))) {
    text[len - 1] = '\0';
    --len;
  }
}

void ClampScale(float* value) {
  if (!value) {
    return;
  }
  if (*value < 0.75f) {
    *value = 0.75f;
  } else if (*value > 1.5f) {
    *value = 1.5f;
  }
}

void CopyLimited(char* dest, size_t dest_size, const char* src) {
  if (!dest || dest_size == 0) {
    return;
  }
  if (!src) {
    dest[0] = '\0';
    return;
  }
  size_t len = std::strlen(src);
  if (len >= dest_size) {
    len = dest_size - 1;
  }
  std::memcpy(dest, src, len);
  dest[len] = '\0';
}

#ifdef _WIN32
bool WideToUtf8(const wchar_t* src, char* dest, size_t dest_size) {
  if (!src || !dest || dest_size == 0) {
    return false;
  }
  int written = WideCharToMultiByte(CP_UTF8, 0, src, -1, dest,
                                    static_cast<int>(dest_size), nullptr,
                                    nullptr);
  if (written <= 0 || static_cast<size_t>(written) > dest_size) {
    dest[0] = '\0';
    return false;
  }
  return true;
}

bool Utf8ToWide(const char* src, wchar_t* dest, size_t dest_size) {
  if (!src || !dest || dest_size == 0) {
    return false;
  }
  int written = MultiByteToWideChar(CP_UTF8, 0, src, -1, dest,
                                    static_cast<int>(dest_size));
  if (written <= 0 || static_cast<size_t>(written) > dest_size) {
    dest[0] = L'\0';
    return false;
  }
  return true;
}

bool GetLocalAppDataUtf8(char* out_dir, size_t out_size) {
  if (!out_dir || out_size == 0) {
    return false;
  }
  out_dir[0] = '\0';
  wchar_t appdata[260] = {0};
  DWORD len = GetEnvironmentVariableW(
      L"LOCALAPPDATA", appdata,
      static_cast<DWORD>(sizeof(appdata) / sizeof(appdata[0])));
  if (len > 0 && len < sizeof(appdata) / sizeof(appdata[0])) {
    return WideToUtf8(appdata, out_dir, out_size);
  }
  len = GetEnvironmentVariableW(
      L"APPDATA", appdata,
      static_cast<DWORD>(sizeof(appdata) / sizeof(appdata[0])));
  if (len > 0 && len < sizeof(appdata) / sizeof(appdata[0])) {
    return WideToUtf8(appdata, out_dir, out_size);
  }
  return false;
}
#endif

bool GetExecutableDir(char* out_dir, size_t out_size) {
  if (!out_dir || out_size == 0) {
    return false;
  }
  out_dir[0] = '\0';
#ifdef _WIN32
  wchar_t path[MAX_PATH] = {0};
  DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return false;
  }
  for (int i = static_cast<int>(len) - 1; i >= 0; --i) {
    if (path[i] == L'\\' || path[i] == L'/') {
      path[i] = L'\0';
      break;
    }
  }
  return WideToUtf8(path, out_dir, out_size);
#else
  (void)out_dir;
  (void)out_size;
  return false;
#endif
}

bool StripLastPathComponent(char* path) {
  if (!path) {
    return false;
  }
  size_t len = std::strlen(path);
  if (len == 0) {
    return false;
  }
  for (size_t i = len; i-- > 0;) {
    if (path[i] == '\\' || path[i] == '/') {
      path[i] = '\0';
      return i > 0;
    }
  }
  return false;
}

bool BuildSettingsPath(char* out_path, size_t out_size) {
  if (!out_path || out_size == 0) {
    return false;
  }
  char base_dir[260] = {0};
  if (!GetExecutableDir(base_dir, sizeof(base_dir))) {
    return false;
  }
  std::snprintf(out_path, out_size, "%s\\resources\\config\\ui_settings.cfg",
                base_dir);
  return true;
}

bool BuildUserSettingsPath(char* out_path, size_t out_size) {
  if (!out_path || out_size == 0) {
    return false;
  }
#ifdef _WIN32
  char base_dir[260] = {0};
  if (GetLocalAppDataUtf8(base_dir, sizeof(base_dir))) {
    std::snprintf(out_path, out_size, "%s\\PLCSimulator\\ui_settings.cfg",
                  base_dir);
    return true;
  }
#endif
  return false;
}

bool OpenFileRead(const char* path, FILE** out_file) {
  if (!out_file) {
    return false;
  }
  *out_file = nullptr;
  if (!path || path[0] == '\0') {
    return false;
  }
#ifdef _WIN32
  wchar_t wpath[520] = {0};
  if (!Utf8ToWide(path, wpath, sizeof(wpath) / sizeof(wpath[0]))) {
    return false;
  }
  *out_file = _wfopen(wpath, L"r");
#else
  *out_file = std::fopen(path, "r");
#endif
  return *out_file != nullptr;
}

bool OpenFileWrite(const char* path, FILE** out_file) {
  if (!out_file) {
    return false;
  }
  *out_file = nullptr;
  if (!path || path[0] == '\0') {
    return false;
  }
#ifdef _WIN32
  wchar_t wpath[520] = {0};
  if (!Utf8ToWide(path, wpath, sizeof(wpath) / sizeof(wpath[0]))) {
    return false;
  }
  *out_file = _wfopen(wpath, L"w");
#else
  *out_file = std::fopen(path, "w");
#endif
  return *out_file != nullptr;
}

}  // namespace

void SetDefaultUiSettings(UiSettings* settings) {
  if (!settings) {
    return;
  }
  settings->ui_scale = 1.0f;
  settings->font_scale = 1.0f;
  settings->layout_scale = 1.0f;
  settings->vsync_enabled = false;
  settings->frame_limit_enabled = true;
  settings->restart_required = false;
}

bool LoadUiSettings(UiSettings* settings) {
  if (!settings) {
    return false;
  }
  SetDefaultUiSettings(settings);
  FILE* file = nullptr;
  char path[260] = {0};
  if (BuildUserSettingsPath(path, sizeof(path)) &&
      OpenFileRead(path, &file)) {
    // Loaded from user path.
  } else if (BuildSettingsPath(path, sizeof(path)) &&
             OpenFileRead(path, &file)) {
    // Loaded from legacy path.
  } else {
    return false;
  }
  char line[128] = {0};
  while (std::fgets(line, sizeof(line), file)) {
    Trim(line);
    if (line[0] == '\0' || line[0] == '#') {
      continue;
    }
    if (std::strncmp(line, "ui_scale=", 9) == 0) {
      std::sscanf(line + 9, "%f", &settings->ui_scale);
    } else if (std::strncmp(line, "font_scale=", 11) == 0) {
      std::sscanf(line + 11, "%f", &settings->font_scale);
    } else if (std::strncmp(line, "layout_scale=", 13) == 0) {
      std::sscanf(line + 13, "%f", &settings->layout_scale);
    } else if (std::strncmp(line, "vsync=", 6) == 0) {
      int value = 0;
      std::sscanf(line + 6, "%d", &value);
      settings->vsync_enabled = (value != 0);
    } else if (std::strncmp(line, "frame_limit=", 12) == 0) {
      int value = 0;
      std::sscanf(line + 12, "%d", &value);
      settings->frame_limit_enabled = (value != 0);
    }
  }
  std::fclose(file);
  ClampScale(&settings->ui_scale);
  ClampScale(&settings->font_scale);
  ClampScale(&settings->layout_scale);
  return true;
}

bool SaveUiSettings(const UiSettings& settings) {
  char path[260] = {0};
  bool has_user_path = BuildUserSettingsPath(path, sizeof(path));
  if (!has_user_path && !BuildSettingsPath(path, sizeof(path))) {
    return false;
  }
#ifdef _WIN32
  char dir_path[260] = {0};
  CopyLimited(dir_path, sizeof(dir_path), path);
  StripLastPathComponent(dir_path);
  wchar_t wdir[520] = {0};
  if (Utf8ToWide(dir_path, wdir, sizeof(wdir) / sizeof(wdir[0]))) {
    CreateDirectoryW(wdir, nullptr);
  }
#endif
  FILE* file = nullptr;
  if (!OpenFileWrite(path, &file)) {
    return false;
  }
  std::fprintf(file, "ui_scale=%.2f\n", settings.ui_scale);
  std::fprintf(file, "font_scale=%.2f\n", settings.font_scale);
  std::fprintf(file, "layout_scale=%.2f\n", settings.layout_scale);
  std::fprintf(file, "vsync=%d\n", settings.vsync_enabled ? 1 : 0);
  std::fprintf(file, "frame_limit=%d\n",
               settings.frame_limit_enabled ? 1 : 0);
  std::fclose(file);
  return true;
}

void MarkUiSettingsRestartRequired(UiSettings* settings) {
  if (!settings) {
    return;
  }
  settings->restart_required = true;
}

}  // namespace plc
