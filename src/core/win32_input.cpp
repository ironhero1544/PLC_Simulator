// win32_input.cpp
//
// Win32 input handling helpers.

#include "plc_emulator/core/win32_input.h"

#include "plc_emulator/core/application.h"
#include "imgui.h"

#ifdef _WIN32
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
#define WIN32_LEAN_AND_MEAN

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif
#ifndef WINVER
#define WINVER 0x0602
#endif

#include <windows.h>
#include <windowsx.h>

#ifndef WM_TABLET_QUERYSYSTEMGESTURESTATUS
#define WM_TABLET_QUERYSYSTEMGESTURESTATUS 0x02CC
#endif

#ifndef TABLET_DISABLE_PRESSANDHOLD
#define TABLET_DISABLE_PRESSANDHOLD 0x00000001
#define TABLET_DISABLE_PENTAPFEEDBACK 0x00000008
#define TABLET_DISABLE_PENBARRELFEEDBACK 0x00000010
#define TABLET_DISABLE_TOUCHUIFORCEON 0x00000100
#define TABLET_DISABLE_TOUCHUIFORCEOFF 0x00000200
#define TABLET_DISABLE_TOUCHSWITCH 0x00008000
#define TABLET_DISABLE_FLICKS 0x00010000
#endif

#include <cmath>
#include <string>
#include <unordered_map>

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace plc {
namespace {

WNDPROC g_prev_wndproc = nullptr;
Application* g_app_instance = nullptr;
HWND g_hwnd = nullptr;
bool g_use_pointer_input = false;
bool g_logged_input_pipeline_revision = false;
constexpr const char* kInputPipelineRevision = "input-rewrite-2026-04-12-r2";
constexpr const char* kInputPipelineBuildStamp = __DATE__ " " __TIME__;

std::unordered_map<UINT32, POINT> g_touch_points;
float g_prev_pinch_distance = 0.0f;
POINT g_prev_pinch_center = {0, 0};
POINT g_prev_pinch_center_client = {0, 0};
UINT32 g_active_touch_id = 0;
POINT g_last_touch_client = {0, 0};
bool g_has_last_touch = false;
std::unordered_map<UINT32, bool> g_pen_side_down;

struct InputMessageSourceCompat {
  DWORD deviceType;
  DWORD originId;
};

using GetCurrentInputMessageSourceFn =
    BOOL(WINAPI*)(InputMessageSourceCompat*);

bool GetCurrentInputMessageSourceData(InputMessageSourceCompat* out_source) {
  static HMODULE user32_module = GetModuleHandleA("user32.dll");
  static GetCurrentInputMessageSourceFn get_input_source =
      user32_module
          ? reinterpret_cast<GetCurrentInputMessageSourceFn>(
                GetProcAddress(user32_module, "GetCurrentInputMessageSource"))
          : nullptr;
  if (!get_input_source || !out_source) {
    return false;
  }

  InputMessageSourceCompat source = {};
  if (!get_input_source(&source)) {
    return false;
  }
  *out_source = source;
  return true;
}

bool IsPhysicalCtrlDown() {
  return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
}

void LogInputPipelineRevisionIfNeeded() {
  if (!g_app_instance || g_logged_input_pipeline_revision ||
      !g_app_instance->IsDebugEnabled()) {
    return;
  }
  g_logged_input_pipeline_revision = true;
  g_app_instance->DebugLog(std::string("[INPUT] PIPELINE_REV=") +
                           kInputPipelineRevision);
  char exe_path[MAX_PATH] = {0};
  DWORD len = GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
  if (len > 0) {
    g_app_instance->DebugLog(std::string("[INPUT] EXE_PATH=") +
                             std::string(exe_path, exe_path + len));
  }
  g_app_instance->DebugLog(std::string("[INPUT] BUILD_STAMP=") +
                           kInputPipelineBuildStamp);
  g_app_instance->DebugLog(std::string("[INPUT] PID=") +
                           std::to_string(GetCurrentProcessId()));
}

DWORD GetCurrentInputMessageDeviceType() {
  InputMessageSourceCompat source = {};
  if (!GetCurrentInputMessageSourceData(&source)) {
    return 0;
  }
  return source.deviceType;
}

void UpdateTouchGestureFromPoints() {
  if (!g_app_instance) {
    return;
  }

  if (g_touch_points.size() < 2) {
    g_prev_pinch_distance = 0.0f;
    g_has_last_touch = false;
    g_app_instance->UpdateTouchGesture(0.0f, ImVec2(0, 0), false);
    return;
  }

  auto it = g_touch_points.begin();
  POINT p1 = it->second;
  ++it;
  POINT p2 = it->second;

  float dx = static_cast<float>(p2.x - p1.x);
  float dy = static_cast<float>(p2.y - p1.y);
  float distance = std::sqrt(dx * dx + dy * dy);
  POINT center = {(p1.x + p2.x) / 2, (p1.y + p2.y) / 2};
  POINT center_client = center;
  if (g_hwnd) {
    ScreenToClient(g_hwnd, &center_client);
  }

  float zoom_delta = 0.0f;
  ImVec2 pan_delta(0.0f, 0.0f);
  if (g_prev_pinch_distance > 0.0f) {
    zoom_delta = (distance - g_prev_pinch_distance) / g_prev_pinch_distance;
    pan_delta.x =
        static_cast<float>(center_client.x - g_prev_pinch_center_client.x);
    pan_delta.y =
        static_cast<float>(center_client.y - g_prev_pinch_center_client.y);
  }

  if (zoom_delta > 0.15f) {
    zoom_delta = 0.15f;
  }
  if (zoom_delta < -0.15f) {
    zoom_delta = -0.15f;
  }
  zoom_delta *= 0.6f;

  g_prev_pinch_distance = distance;
  g_prev_pinch_center = center;
  g_prev_pinch_center_client = center_client;

  const ImVec2 center_imvec(static_cast<float>(center_client.x),
                            static_cast<float>(center_client.y));
  const bool over_overlay =
      g_app_instance->ShouldBlockCanvasNavigationAtScreenPos(center_imvec, true);

  if (over_overlay) {
    if (g_app_instance->IsDebugEnabled()) {
      g_app_instance->DebugLog(
          "[INPUT] Touch gesture suppressed: canvas navigation blocked");
    }
    g_app_instance->UpdateTouchGesture(0.0f, ImVec2(0, 0), false);
    return;
  }

  g_app_instance->SetTouchAnchor(center_imvec);
  g_app_instance->UpdateTouchGesture(zoom_delta, pan_delta, true);
}

LRESULT CALLBACK PlcWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (!g_app_instance) {
    if (g_prev_wndproc) {
      return CallWindowProc(g_prev_wndproc, hWnd, msg, wParam, lParam);
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
  }

  if (msg == WM_TABLET_QUERYSYSTEMGESTURESTATUS) {
    return TABLET_DISABLE_PRESSANDHOLD | TABLET_DISABLE_PENTAPFEEDBACK |
           TABLET_DISABLE_PENBARRELFEEDBACK | TABLET_DISABLE_FLICKS;
  }

  if ((msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) &&
      g_app_instance->IsDebugEnabled()) {
    LogInputPipelineRevisionIfNeeded();
    const DWORD device_type = GetCurrentInputMessageDeviceType();
    const char* msg_name =
        (msg == WM_MOUSEHWHEEL) ? "WM_MOUSEHWHEEL" : "WM_MOUSEWHEEL";
    const float wheel_delta =
        static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
        static_cast<float>(WHEEL_DELTA);
    const bool ctrl_modified = (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0;
    std::string log_msg =
        "[INPUT] RAW_WHEEL msg=" + std::string(msg_name) +
        " deviceType=0x" + std::to_string(device_type) +
        " ctrl=" + std::to_string(ctrl_modified ? 1 : 0) +
        " delta=" + std::to_string(wheel_delta);
    g_app_instance->DebugLog(log_msg);
  }

#if defined(WM_POINTERDOWN)
  if (msg == WM_POINTERDOWN || msg == WM_POINTERUPDATE ||
      msg == WM_POINTERUP) {
    bool handled_pointer = false;
    UINT32 pointer_id = GET_POINTERID_WPARAM(wParam);
    POINTER_INFO pointer_info;
    if (GetPointerInfo(pointer_id, &pointer_info)) {
      bool is_pen = (pointer_info.pointerType == PT_PEN);
      bool is_touch = (pointer_info.pointerType == PT_TOUCH);
      const bool in_contact =
          ((pointer_info.pointerFlags & POINTER_FLAG_INCONTACT) != 0) ||
          (IS_POINTER_INCONTACT_WPARAM(wParam) != 0);
      ImVec2 screen_pos(static_cast<float>(pointer_info.ptPixelLocation.x),
                        static_cast<float>(pointer_info.ptPixelLocation.y));
      const bool over_canvas = g_app_instance->IsPointInCanvas(screen_pos);
      const bool contact_like =
          in_contact || (IS_POINTER_FIRSTBUTTON_WPARAM(wParam) != 0) ||
          (IS_POINTER_SECONDBUTTON_WPARAM(wParam) != 0);
      if (is_pen) {
        g_app_instance->SetPanInputActive(in_contact);
      }

      if (is_touch) {
        if (g_app_instance->IsDebugEnabled()) {
          const char* msg_name = "WM_POINTERUPDATE";
          if (msg == WM_POINTERDOWN) {
            msg_name = "WM_POINTERDOWN";
          } else if (msg == WM_POINTERUP) {
            msg_name = "WM_POINTERUP";
          }
          g_app_instance->DebugLog(
              "[INPUT] TOUCH_POINTER msg=" + std::string(msg_name) +
              " id=" + std::to_string(pointer_id) +
              " count=" + std::to_string(g_touch_points.size()));
        }
        POINT client_pt = pointer_info.ptPixelLocation;
        if (g_hwnd) {
          ScreenToClient(g_hwnd, &client_pt);
        }
        g_active_touch_id = pointer_id;
        if (msg == WM_POINTERUP) {
          g_touch_points.erase(pointer_id);
          if (pointer_id == g_active_touch_id) {
            g_has_last_touch = false;
          }
        } else {
          g_touch_points[pointer_id] = pointer_info.ptPixelLocation;
        }

        const ImVec2 touch_pos(static_cast<float>(client_pt.x),
                               static_cast<float>(client_pt.y));
        const bool over_overlay =
            g_app_instance->ShouldBlockCanvasNavigationAtScreenPos(
                touch_pos, true);

        if (over_overlay) {
          g_app_instance->UpdateTouchGesture(0.0f, ImVec2(0, 0), false);
          // 캔버스 navigation이 막힌 영역이면 Win32/ImGui 기본 루프로 흘려보냄
          handled_pointer = false;
        } else {
          if (g_touch_points.size() >= 2) {
            UpdateTouchGestureFromPoints();
          } else if (msg != WM_POINTERUP) {
            if (g_has_last_touch) {
              ImVec2 pan_delta(
                  static_cast<float>(client_pt.x - g_last_touch_client.x),
                  static_cast<float>(client_pt.y - g_last_touch_client.y));
              g_app_instance->SetTouchAnchor(touch_pos);
              g_app_instance->UpdateTouchGesture(0.0f, pan_delta, true);
            }
            g_last_touch_client = client_pt;
            g_has_last_touch = true;
            g_app_instance->SetTouchAnchor(touch_pos);
          }
          handled_pointer = true;
        }
      }

      bool pen_side_down = false;
      if (is_pen) {
        const bool secondary =
            (IS_POINTER_SECONDBUTTON_WPARAM(wParam) != 0) ||
            ((pointer_info.pointerFlags & POINTER_FLAG_SECONDBUTTON) != 0);
        const bool third =
            (IS_POINTER_THIRDBUTTON_WPARAM(wParam) != 0) ||
            ((pointer_info.pointerFlags & POINTER_FLAG_THIRDBUTTON) != 0);
        const bool fourth =
            (IS_POINTER_FOURTHBUTTON_WPARAM(wParam) != 0) ||
            ((pointer_info.pointerFlags & POINTER_FLAG_FOURTHBUTTON) != 0);
        bool pen_flag_button = false;
        POINTER_PEN_INFO pen_info = {};
        bool has_pen_info = false;
        bool pressure_contact = false;
        if (GetPointerPenInfo(pointer_id, &pen_info)) {
          has_pen_info = true;
          pen_flag_button =
              (pen_info.penFlags & (PEN_FLAG_BARREL | PEN_FLAG_ERASER)) != 0;
          pressure_contact = (pen_info.pressure > 0);
        }
        const bool pen_contact_like = contact_like || pressure_contact;
        pen_side_down =
            (secondary || third || fourth || pen_flag_button) && pen_contact_like;
        const bool was_down = g_pen_side_down[pointer_id];
        if (pen_side_down && !was_down) {
          g_app_instance->RegisterWin32SideClick();
        }
        g_pen_side_down[pointer_id] = pen_side_down;
        g_app_instance->RegisterWin32SideDown(pen_side_down);
        if (msg == WM_POINTERUP) {
          g_pen_side_down.erase(pointer_id);
          g_app_instance->RegisterWin32SideDown(false);
        }
        if (g_app_instance->IsDebugEnabled()) {
          const char* msg_name = "WM_POINTERUPDATE";
          if (msg == WM_POINTERDOWN) {
            msg_name = "WM_POINTERDOWN";
          } else if (msg == WM_POINTERUP) {
            msg_name = "WM_POINTERUP";
          }
          std::string log_msg = "[INPUT] " + std::string(msg_name) +
                                " id=" + std::to_string(pointer_id) +
                                " type=" +
                                (is_pen ? "PEN" : (is_touch ? "TOUCH" : "OTHER")) +
                                " flags=0x" +
                                std::to_string(pointer_info.pointerFlags) +
                                " penFlags=0x" +
                                std::to_string(pen_info.penFlags) +
                                " pressure=" +
                                std::to_string(has_pen_info
                                                   ? static_cast<int>(pen_info.pressure)
                                                   : -1) +
                                " in_contact=" +
                                std::to_string(in_contact ? 1 : 0) +
                                " primary=" +
                                std::to_string(
                                    IS_POINTER_FIRSTBUTTON_WPARAM(wParam) != 0) +
                                " secondary=" +
                                std::to_string(
                                    IS_POINTER_SECONDBUTTON_WPARAM(wParam) != 0) +
                                " third=" +
                                std::to_string(
                                    IS_POINTER_THIRDBUTTON_WPARAM(wParam) != 0) +
                                " fourth=" +
                                std::to_string(
                                    IS_POINTER_FOURTHBUTTON_WPARAM(wParam) != 0);
          g_app_instance->DebugLog(log_msg);
        }
      }
      if (is_pen) {
        POINT client_pt = pointer_info.ptPixelLocation;
        if (g_hwnd) {
          ScreenToClient(g_hwnd, &client_pt);
        }
        g_app_instance->SetTouchAnchor(
            ImVec2(static_cast<float>(client_pt.x),
                   static_cast<float>(client_pt.y)));
      }
      if (msg == WM_POINTERUP) {
        g_pen_side_down.erase(pointer_id);
        g_app_instance->RegisterWin32SideDown(false);
      }
      if (is_pen && over_canvas) {
        handled_pointer = true;
      }
    }
    if (handled_pointer) {
      return 0;
    }
  }
#endif

  if (msg == WM_RBUTTONDOWN) {
    g_app_instance->RegisterWin32RightClick();
    if (g_app_instance->IsDebugEnabled()) {
      g_app_instance->DebugLog("[INPUT] WM_RBUTTONDOWN");
    }
  }
  if (msg == WM_XBUTTONDOWN) {
    WORD button = GET_XBUTTON_WPARAM(wParam);
    if (button == XBUTTON1) {
      g_app_instance->RegisterWin32SideClick();
      if (g_app_instance->IsDebugEnabled()) {
        g_app_instance->DebugLog("[INPUT] WM_XBUTTONDOWN XBUTTON1");
      }
    }
  }

  if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
    POINT client_pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (g_hwnd) {
      ScreenToClient(g_hwnd, &client_pt);
    }
    ImVec2 client_screen_pos(static_cast<float>(client_pt.x),
                             static_cast<float>(client_pt.y));
    ImVec2 imgui_screen_pos = client_screen_pos;
    if (ImGui::GetCurrentContext()) {
      imgui_screen_pos = ImGui::GetIO().MousePos;
    }
    InputMessageSourceCompat source = {};
    GetCurrentInputMessageSourceData(&source);
    const DWORD device_type = source.deviceType;
    const DWORD origin_id = source.originId;
    const bool reported_touchpad = (device_type == 0x00000010u);
    const bool over_canvas =
        g_app_instance->IsPointInCanvas(client_screen_pos) ||
        g_app_instance->IsPointInCanvas(imgui_screen_pos);
    const bool over_wheel_component =
        g_app_instance->HasWheelResponsiveComponentAtScreenPos(
            imgui_screen_pos) ||
        g_app_instance->HasWheelResponsiveComponentAtScreenPos(
            client_screen_pos);
    const bool over_overlay =
        g_app_instance->IsPointInOverlayCaptureRect(client_screen_pos) ||
        g_app_instance->IsPointInOverlayCaptureRect(imgui_screen_pos);

    const bool route_to_canvas = over_canvas && !over_overlay;
    const float wheel_delta =
        static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
        static_cast<float>(WHEEL_DELTA);

    const bool ctrl_modified = (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0;
    const bool physical_ctrl_down = IsPhysicalCtrlDown();
    const bool physical_shift_down = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool synthetic_pinch = ctrl_modified && !physical_ctrl_down;
    // deviceType=0x2 (IMDT_MOUSE) with origin_id!=0 is a precision touchpad
    // reporting as mouse. Also accept all 0x2 sources when delta is fractional
    // (sub-WHEEL_DELTA), which real mice never produce.
    const int raw_delta = GET_WHEEL_DELTA_WPARAM(wParam);
    const bool fractional_delta = (raw_delta % WHEEL_DELTA) != 0;
    const bool touchpad_like_stream =
        reported_touchpad || synthetic_pinch ||
        (device_type == 0x00000002u && (origin_id != 0 || fractional_delta));

    if (g_app_instance->IsDebugEnabled()) {
      const char* msg_name =
          (msg == WM_MOUSEHWHEEL) ? "WM_MOUSEHWHEEL" : "WM_MOUSEWHEEL";
      std::string log_msg =
          "[INPUT] WHEEL_INPUT msg=" + std::string(msg_name) +
          " deviceType=0x" + std::to_string(device_type) +
          " originId=" + std::to_string(origin_id) +
          " reported_touchpad=" + std::to_string(reported_touchpad ? 1 : 0) +
          " ctrl=" + std::to_string(ctrl_modified ? 1 : 0) +
          " physical_ctrl=" + std::to_string(physical_ctrl_down ? 1 : 0) +
          " synthetic_pinch=" + std::to_string(synthetic_pinch ? 1 : 0) +
          " touchpad_like=" + std::to_string(touchpad_like_stream ? 1 : 0) +
          " delta=" + std::to_string(wheel_delta) +
          " over_canvas=" + std::to_string(over_canvas ? 1 : 0) +
          " over_wheel_component=" +
          std::to_string(over_wheel_component ? 1 : 0) +
          " over_overlay=" + std::to_string(over_overlay ? 1 : 0) +
          " route_to_canvas=" + std::to_string(route_to_canvas ? 1 : 0) +
          " client=(" + std::to_string(static_cast<int>(client_screen_pos.x)) +
          "," + std::to_string(static_cast<int>(client_screen_pos.y)) + ")" +
          " imgui=(" + std::to_string(static_cast<int>(imgui_screen_pos.x)) +
          "," + std::to_string(static_cast<int>(imgui_screen_pos.y)) + ")" +
          " " +
          g_app_instance->DescribeInputTargetAtScreenPos(imgui_screen_pos);
      g_app_instance->DebugLog(log_msg);
    }
    if (over_overlay) {
      if (g_app_instance->IsDebugEnabled()) {
        g_app_instance->DebugLog(
            "[INPUT] WHEEL_INPUT deferred to overlay window");
      }
      if (g_prev_wndproc) {
        return CallWindowProc(g_prev_wndproc, hWnd, msg, wParam, lParam);
      }
      return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    if (route_to_canvas) {
      float vertical_delta = 0.0f;
      float horizontal_delta = 0.0f;
      if (msg == WM_MOUSEWHEEL) {
        vertical_delta = wheel_delta;
      } else {
        horizontal_delta = wheel_delta;
      }
      g_app_instance->QueueCanvasWheelInput(vertical_delta, horizontal_delta,
                                            imgui_screen_pos,
                                            physical_ctrl_down,
                                            physical_shift_down,
                                            synthetic_pinch,
                                            touchpad_like_stream);
      if (g_app_instance->IsDebugEnabled()) {
        g_app_instance->DebugLog(
            "[INPUT] WHEEL_INPUT dispatch=canvas_raw" +
            std::string(over_wheel_component ? " component_candidate"
                                             : " canvas"));
      }
      return 0;
    }
    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) {
      if (g_app_instance->IsDebugEnabled()) {
        g_app_instance->DebugLog("[INPUT] WHEEL_INPUT deferred to imgui");
      }
      if (g_prev_wndproc) {
        return CallWindowProc(g_prev_wndproc, hWnd, msg, wParam, lParam);
      }
      return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    if (g_app_instance->IsDebugEnabled()) {
      g_app_instance->DebugLog(
          "[INPUT] WHEEL_INPUT ignored: outside canvas or left to default path");
    }
  }

  if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN ||
      msg == WM_MBUTTONDOWN) {
    LogInputPipelineRevisionIfNeeded();
    if (!g_use_pointer_input) {
      g_app_instance->SetPanInputActive(false);
    }
  }

  if (g_prev_wndproc) {
    return CallWindowProc(g_prev_wndproc, hWnd, msg, wParam, lParam);
  }
  return DefWindowProc(hWnd, msg, wParam, lParam);
}

}  // namespace

void InstallWin32InputHook(GLFWwindow* window, Application* app) {
  if (!window || !app) {
    return;
  }
  HWND hwnd = glfwGetWin32Window(window);
  g_hwnd = hwnd;
  g_app_instance = app;

#if defined(WM_POINTERDOWN)
  RegisterPointerInputTarget(hwnd, PT_TOUCH);
  RegisterPointerInputTarget(hwnd, PT_PEN);
  g_use_pointer_input = true;
#endif

  GESTURECONFIG configs[2];
  configs[0].dwID = GID_ZOOM;
  configs[0].dwWant = 0;
  configs[0].dwBlock = GC_ZOOM;
  configs[1].dwID = GID_PAN;
  configs[1].dwWant = 0;
  configs[1].dwBlock = GC_PAN;
  SetGestureConfig(hwnd, 0, 2, configs, sizeof(GESTURECONFIG));

  if (!g_prev_wndproc) {
    g_prev_wndproc =
        reinterpret_cast<WNDPROC>(SetWindowLongPtr(
            hwnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(PlcWndProc)));
  }
}

void UninstallWin32InputHook(GLFWwindow* window) {
  if (!window || !g_prev_wndproc) {
    return;
  }
  HWND hwnd = glfwGetWin32Window(window);
  SetWindowLongPtr(hwnd, GWLP_WNDPROC,
                   reinterpret_cast<LONG_PTR>(g_prev_wndproc));
  g_prev_wndproc = nullptr;
  g_app_instance = nullptr;
  g_hwnd = nullptr;
}

}  // namespace plc
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#endif  // _WIN32
