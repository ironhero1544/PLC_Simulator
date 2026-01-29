// ui_settings.cpp
//
// UI settings load/save for scale configuration.

#include "plc_emulator/core/ui_settings.h"

#include <cctype>
#include <cstdio>
#include <cstring>

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

bool GetExecutableDir(char* out_dir, size_t out_size) {
  if (!out_dir || out_size == 0) {
    return false;
  }
  out_dir[0] = '\0';
#ifdef _WIN32
  char path[MAX_PATH] = {0};
  DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return false;
  }
  for (int i = static_cast<int>(len) - 1; i >= 0; --i) {
    if (path[i] == '\\' || path[i] == '/') {
      path[i] = '\0';
      break;
    }
  }
  CopyLimited(out_dir, out_size, path);
  return out_dir[0] != '\0';
#else
  (void)out_dir;
  (void)out_size;
  return false;
#endif
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

}  // namespace

void SetDefaultUiSettings(UiSettings* settings) {
  if (!settings) {
    return;
  }
  settings->ui_scale = 1.0f;
  settings->font_scale = 1.0f;
  settings->layout_scale = 1.0f;
  settings->restart_required = false;
}

bool LoadUiSettings(UiSettings* settings) {
  if (!settings) {
    return false;
  }
  SetDefaultUiSettings(settings);
  char path[260] = {0};
  if (!BuildSettingsPath(path, sizeof(path))) {
    return false;
  }
  FILE* file = std::fopen(path, "r");
  if (!file) {
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
  if (!BuildSettingsPath(path, sizeof(path))) {
    return false;
  }
#ifdef _WIN32
  char dir_path[260] = {0};
  if (GetExecutableDir(dir_path, sizeof(dir_path))) {
    std::strncat(dir_path, "\\resources",
                 sizeof(dir_path) - std::strlen(dir_path) - 1);
    CreateDirectoryA(dir_path, nullptr);
    std::strncat(dir_path, "\\config",
                 sizeof(dir_path) - std::strlen(dir_path) - 1);
    CreateDirectoryA(dir_path, nullptr);
  }
#endif
  FILE* file = std::fopen(path, "w");
  if (!file) {
    return false;
  }
  std::fprintf(file, "ui_scale=%.2f\n", settings.ui_scale);
  std::fprintf(file, "font_scale=%.2f\n", settings.font_scale);
  std::fprintf(file, "layout_scale=%.2f\n", settings.layout_scale);
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
