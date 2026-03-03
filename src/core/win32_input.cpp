// win32_input.cpp
//
// Win32 input handling helpers.

#include "plc_emulator/core/win32_input.h"

#include "plc_emulator/core/application.h"

#ifdef _WIN32

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

bool IsTouchpadInputMessage() {
  static HMODULE user32_module = GetModuleHandleA("user32.dll");
  static GetCurrentInputMessageSourceFn get_input_source =
      user32_module
          ? reinterpret_cast<GetCurrentInputMessageSourceFn>(
                GetProcAddress(user32_module, "GetCurrentInputMessageSource"))
          : nullptr;
  if (!get_input_source) {
    return false;
  }

  InputMessageSourceCompat source = {};
  if (!get_input_source(&source)) {
    return false;
  }
  return source.deviceType == 0x00000010u;  // IMDT_TOUCHPAD
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
  g_app_instance->SetTouchAnchor(
      ImVec2(static_cast<float>(center_client.x),
             static_cast<float>(center_client.y)));
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
        if (g_touch_points.size() >= 2) {
          UpdateTouchGestureFromPoints();
        } else if (msg != WM_POINTERUP) {
          if (g_has_last_touch) {
            ImVec2 pan_delta(
                static_cast<float>(client_pt.x - g_last_touch_client.x),
                static_cast<float>(client_pt.y - g_last_touch_client.y));
            g_app_instance->SetTouchAnchor(
                ImVec2(static_cast<float>(client_pt.x),
                       static_cast<float>(client_pt.y)));
            g_app_instance->UpdateTouchGesture(0.0f, pan_delta, true);
          }
          g_last_touch_client = client_pt;
          g_has_last_touch = true;
          g_app_instance->SetTouchAnchor(
              ImVec2(static_cast<float>(client_pt.x),
                     static_cast<float>(client_pt.y)));
        }
        handled_pointer = true;
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

  if ((msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) && IsTouchpadInputMessage()) {
    POINT client_pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (g_hwnd) {
      ScreenToClient(g_hwnd, &client_pt);
    }
    ImVec2 screen_pos(static_cast<float>(client_pt.x),
                      static_cast<float>(client_pt.y));
    const bool over_canvas = g_app_instance->IsPointInCanvas(screen_pos);
    const bool ctrl_modified = (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0;
    if (msg == WM_MOUSEWHEEL && over_canvas && ctrl_modified) {
      float zoom_delta =
          static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
          static_cast<float>(WHEEL_DELTA);
      zoom_delta *= 0.08f;
      if (zoom_delta > 0.10f) {
        zoom_delta = 0.10f;
      } else if (zoom_delta < -0.10f) {
        zoom_delta = -0.10f;
      }
      if (zoom_delta != 0.0f) {
        g_app_instance->QueueTouchpadZoom(zoom_delta, screen_pos);
      }
    }
  }

  if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN ||
      msg == WM_MBUTTONDOWN) {
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

#endif  // _WIN32
