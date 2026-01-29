#include "plc_emulator/lang/lang_manager.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace plc {

namespace {

struct LangEntry {
  char key[64];
  char value[256];
};

const int kMaxEntries = 512;
LangEntry g_entries[kMaxEntries];
int g_entry_count = 0;

char g_active_lang[8] = "en";
char g_user_lang[8] = "";
bool g_restart_required = false;

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

bool StartsWith(const char* text, const char* prefix) {
  if (!text || !prefix) {
    return false;
  }
  size_t prefix_len = std::strlen(prefix);
  return std::strncmp(text, prefix, prefix_len) == 0;
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

bool BuildLangPath(const char* filename, char* out_path, size_t out_size) {
  if (!filename || !out_path || out_size == 0) {
    return false;
  }
  char base_dir[260] = {0};
  if (!GetExecutableDir(base_dir, sizeof(base_dir))) {
    return false;
  }
  std::snprintf(out_path, out_size, "%s\\resources\\lang\\%s", base_dir,
                filename);
  return true;
}

bool BuildUserLangPath(char* out_path, size_t out_size) {
  return BuildLangPath("user_lang.txt", out_path, out_size);
}

bool LoadLangFile(const char* path) {
  if (!path || path[0] == '\0') {
    return false;
  }
  FILE* file = std::fopen(path, "r");
  if (!file) {
    return false;
  }

  g_entry_count = 0;
  char line[512] = {0};
  while (std::fgets(line, sizeof(line), file)) {
    Trim(line);
    if (line[0] == '\0' || line[0] == '#') {
      continue;
    }
    char* equals = std::strchr(line, '=');
    if (!equals) {
      continue;
    }
    *equals = '\0';
    char* key = line;
    char* value = equals + 1;
    Trim(key);
    Trim(value);
    if (key[0] == '\0') {
      continue;
    }
    if (g_entry_count >= kMaxEntries) {
      break;
    }
    CopyLimited(g_entries[g_entry_count].key,
                sizeof(g_entries[g_entry_count].key), key);
    CopyLimited(g_entries[g_entry_count].value,
                sizeof(g_entries[g_entry_count].value), value);
    ++g_entry_count;
  }
  std::fclose(file);
  return true;
}

const char* DetectWindowsLangCode() {
#ifdef _WIN32
  WCHAR buffer[256] = {0};
  ULONG count = 0;
  ULONG size = static_cast<ULONG>(sizeof(buffer) / sizeof(buffer[0]));
  if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, buffer, &size)) {
    return "en";
  }
  if (buffer[0] == L'\0') {
    return "en";
  }
  char utf8[256] = {0};
  int len = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, utf8,
                                static_cast<int>(sizeof(utf8)), nullptr,
                                nullptr);
  if (len <= 0) {
    return "en";
  }
  if (StartsWith(utf8, "ko")) {
    return "ko";
  }
  if (StartsWith(utf8, "ja")) {
    return "ja";
  }
  return "en";
#else
  return "en";
#endif
}

void ResolveInitialLanguage() {
  char path[260] = {0};
  if (BuildUserLangPath(path, sizeof(path))) {
    FILE* file = std::fopen(path, "r");
    if (file) {
      char line[32] = {0};
      if (std::fgets(line, sizeof(line), file)) {
        Trim(line);
        CopyLimited(g_user_lang, sizeof(g_user_lang), line);
      }
      std::fclose(file);
    }
  }

  const char* selected = g_user_lang[0] != '\0' ? g_user_lang
                                                 : DetectWindowsLangCode();

  const char* filename = "en.lang";
  if (std::strcmp(selected, "ko") == 0) {
    filename = "ko.lang";
  } else if (std::strcmp(selected, "ja") == 0) {
    filename = "ja.lang";
  }

  char lang_path[260] = {0};
  if (!BuildLangPath(filename, lang_path, sizeof(lang_path)) ||
      !LoadLangFile(lang_path)) {
    BuildLangPath("en.lang", lang_path, sizeof(lang_path));
    LoadLangFile(lang_path);
    CopyLimited(g_active_lang, sizeof(g_active_lang), "en");
    return;
  }
  CopyLimited(g_active_lang, sizeof(g_active_lang), selected);
}

bool WriteUserLangFile(const char* code) {
  char path[260] = {0};
  if (!BuildUserLangPath(path, sizeof(path))) {
    return false;
  }
#ifdef _WIN32
  char dir_path[260] = {0};
  if (GetExecutableDir(dir_path, sizeof(dir_path))) {
    std::strncat(dir_path, "\\resources", sizeof(dir_path) - std::strlen(dir_path) - 1);
    CreateDirectoryA(dir_path, nullptr);
    std::strncat(dir_path, "\\lang", sizeof(dir_path) - std::strlen(dir_path) - 1);
    CreateDirectoryA(dir_path, nullptr);
  }
#endif
  FILE* file = std::fopen(path, "w");
  if (!file) {
    return false;
  }
  std::fprintf(file, "%s\n", code);
  std::fclose(file);
  return true;
}

}  // namespace

void InitializeLanguage() {
  ResolveInitialLanguage();
}

const char* Translate(const char* key, const char* fallback) {
  if (!key || key[0] == '\0') {
    return fallback ? fallback : "";
  }
  for (int i = 0; i < g_entry_count; ++i) {
    if (std::strcmp(g_entries[i].key, key) == 0) {
      return g_entries[i].value[0] != '\0' ? g_entries[i].value
                                           : (fallback ? fallback : "");
    }
  }
  return fallback ? fallback : "";
}

bool FormatString(char* out,
                  unsigned int out_size,
                  const char* key,
                  const char* fallback,
                  ...) {
  if (!out || out_size == 0) {
    return false;
  }
  const char* format = Translate(key, fallback);
  if (!format) {
    out[0] = '\0';
    return false;
  }
  va_list args;
  va_start(args, fallback);
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
  int written =
      std::vsnprintf(out, static_cast<size_t>(out_size), format, args);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  va_end(args);
  if (written < 0) {
    out[0] = '\0';
    return false;
  }
  out[out_size - 1] = '\0';
  return true;
}

const char* GetActiveLanguageCode() {
  return g_active_lang;
}

const char* GetUserLanguageCode() {
  return g_user_lang;
}

bool SetUserLanguageCode(const char* code) {
  if (!code || code[0] == '\0') {
    return false;
  }
  if (!WriteUserLangFile(code)) {
    return false;
  }
  CopyLimited(g_user_lang, sizeof(g_user_lang), code);
  if (std::strcmp(g_active_lang, code) != 0) {
    g_restart_required = true;
  }
  return true;
}

bool IsLanguageRestartRequired() {
  return g_restart_required;
}

}  // namespace plc
