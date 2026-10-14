#include "plc_emulator/core/guide_viewer.h"

#include <windows.h>

#include <shellapi.h>

#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <thread>

#include <nlohmann/json.hpp>
#include <webview/webview.h>

namespace plc_emulator {
namespace {

std::string GuideUrl(const std::string& language) {
  wchar_t executable[32768] = {};
  const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
  if (length == 0 || length == 32768) {
    throw std::runtime_error("Cannot locate the application directory");
  }
  const auto path = std::filesystem::path(executable).parent_path() /
                    "resources/help/index.html";
  if (!std::filesystem::is_regular_file(path)) {
    throw std::runtime_error("Missing resources/help/index.html");
  }
  const auto utf8 = path.generic_u8string();
  std::string url = "file:///";
  constexpr char kHex[] = "0123456789ABCDEF";
  for (const char8_t character : utf8) {
    const auto byte = static_cast<unsigned char>(character);
    if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
        (byte >= '0' && byte <= '9') || byte == '/' || byte == ':' ||
        byte == '-' || byte == '_' || byte == '.') {
      url += static_cast<char>(byte);
    } else {
      url += '%';
      url += kHex[byte >> 4];
      url += kHex[byte & 15];
    }
  }
  return url + "?lang=" +
         (language == "en" || language == "ja" ? language : "ko");
}

class GuideViewer {
 public:
  ~GuideViewer() {
    {
      const std::lock_guard lock(mutex_);
      stopping_ = true;
      if (view_) {
        view_->terminate();
      }
    }
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  void Open(const std::string& title, HWND owner_window,
            const std::string& language) {
    const std::lock_guard lock(mutex_);
    if (view_) {
      const HWND window = static_cast<HWND>(view_->window().value());
      view_->dispatch([window] {
        ShowWindow(window, SW_RESTORE);
        SetForegroundWindow(window);
      });
      return;
    }
    if (starting_) {
      return;
    }
    if (thread_.joinable()) {
      thread_.join();
    }
    error_.clear();
    starting_ = true;
    thread_ = std::thread([this, title, owner_window, language] {
      Run(title, owner_window, language);
    });
  }

  std::string error() {
    const std::lock_guard lock(mutex_);
    return error_;
  }

 private:
  void Run(const std::string& title, HWND owner_window,
           const std::string& language) {
    try {
      webview::webview view(false, nullptr);
      view.set_title(title);
      view.set_size(1060, 800, WEBVIEW_HINT_NONE);
      view.set_size(460, 360, WEBVIEW_HINT_MIN);
      const HWND window = static_cast<HWND>(view.window().value());
      // An owned window stays above its owner without covering other apps.
      SetWindowLongPtrW(window, GWLP_HWNDPARENT,
                        reinterpret_cast<LONG_PTR>(owner_window));
      SetWindowPos(window, HWND_NOTOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
      view.bind("openExternal", [](const std::string& arguments) {
        const auto values = nlohmann::json::parse(arguments);
        const std::string url = values.at(0).get<std::string>();
        if (url.starts_with("https://") || url.starts_with("http://") ||
            url.starts_with("mailto:")) {
          ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr,
                        SW_SHOWNORMAL);
        }
        return std::string("null");
      });
      view.navigate(GuideUrl(language));
      {
        const std::lock_guard lock(mutex_);
        view_ = &view;
        starting_ = false;
        if (stopping_) {
          view.terminate();
        }
      }
      view.run();
      const std::lock_guard lock(mutex_);
      view_ = nullptr;
    } catch (const std::exception& error) {
      const std::lock_guard lock(mutex_);
      error_ = error.what();
      view_ = nullptr;
      starting_ = false;
    }
  }

  std::mutex mutex_;
  std::thread thread_;
  webview::webview* view_ = nullptr;
  bool starting_ = false;
  bool stopping_ = false;
  std::string error_;
};

GuideViewer& Viewer() {
  static GuideViewer viewer;
  return viewer;
}

}  // namespace

void OpenGuideViewer(const std::string& title, void* owner_window,
                     const std::string& language) {
  Viewer().Open(title, static_cast<HWND>(owner_window), language);
}

std::string GetGuideViewerError() {
  return Viewer().error();
}

}  // namespace plc_emulator
