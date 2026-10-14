#include "imgui.h"
#include "plc_emulator/components/storage_sensor_def.h"
#include "plc_emulator/core/application.h"
#include "plc_emulator/lang/lang_manager.h"
#include "plc_emulator/physics/storage_sensor_simulation.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

namespace plc {
namespace {

void EditText(const char* label, std::string* value, size_t capacity) {
  char buffer[160];
  std::snprintf(buffer, sizeof(buffer), "%s", value->c_str());
  ImGui::PushID(label);
  ImGui::TextUnformatted(label);
  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputText("##value", buffer, capacity))
    *value = buffer;
  ImGui::PopID();
}

void ClampWidget(HmiWidget* widget) {
  widget->size.width = std::clamp(widget->size.width, 12.0f, kHmiScreenWidth);
  widget->size.height =
      std::clamp(widget->size.height, 12.0f, kHmiScreenHeight);
  widget->position.x = std::clamp(widget->position.x, 0.0f,
                                  kHmiScreenWidth - widget->size.width);
  widget->position.y = std::clamp(widget->position.y, 0.0f,
                                  kHmiScreenHeight - widget->size.height);
}

void SetUnusedButtonAddress(const HmiSettings& settings, HmiWidget* widget) {
  if (widget->type != HmiWidgetType::kButton)
    return;
  for (int address = 100; address <= 7679; ++address) {
    widget->address = "M" + std::to_string(address);
    if (std::none_of(settings.widgets.begin(), settings.widgets.end(),
                     [&](const auto& other) {
                       return other.type == HmiWidgetType::kButton &&
                              other.address == widget->address;
                     }))
      return;
  }
}

void PlaceAvailableWidget(const HmiSettings& settings, HmiWidget* widget) {
  for (float y = 10; y + widget->size.height <= kHmiScreenHeight; y += 10) {
    for (float x = 10; x + widget->size.width <= kHmiScreenWidth; x += 10) {
      const bool overlap =
          std::any_of(settings.widgets.begin(), settings.widgets.end(),
                      [&](const auto& other) {
                        return other.page == widget->page &&
                               x < other.position.x + other.size.width + 4 &&
                               x + widget->size.width + 4 > other.position.x &&
                               y < other.position.y + other.size.height + 4 &&
                               y + widget->size.height + 4 > other.position.y;
                      });
      if (!overlap) {
        widget->position = {x, y};
        return;
      }
    }
  }
  widget->position.x += 10;
  widget->position.y += 10;
  ClampWidget(widget);
}

float Snap(float value, bool enabled) {
  return enabled ? std::round(value / 5) * 5 : value;
}

}  // namespace

bool Application::OpenHmiEditor(int component_id) {
  const auto* component = FindComponentById(component_id);
  if (!component || component->type != ComponentType::kTouchPanel)
    return false;
  physics_paused_ = true;
  for (auto& panel : placed_components_)
    ReleaseHmiButtons(&panel);
  hmi_edit_settings_ = component->hmi_settings;
  hmi_edit_page_ = component->hmi_state.active_page;
  hmi_selected_widget_ = -1;
  hmi_editor_component_id_ = component_id;
  hmi_editor_open_pending_ = true;
  return true;
}

void Application::CloseHmiEditor() {
  hmi_editor_component_id_ = -1;
  hmi_editor_open_pending_ = false;
  hmi_selected_widget_ = -1;
  physics_paused_ = false;
  physics_time_initialized_ = false;
}

void Application::RenderHmiEditor(int component_id) {
  if (hmi_editor_open_pending_) {
    ImGui::OpenPopup(TR("ui.hmi.editor", "HMI screen editor"));
    hmi_editor_open_pending_ = false;
  }
  const char* title = TR("ui.hmi.editor", "HMI screen editor");
  if (hmi_editor_component_id_ >= 0 && !ImGui::IsPopupOpen(title)) {
    CloseHmiEditor();
    return;
  }
  ImGui::SetNextWindowSize({1000, 760}, ImGuiCond_Appearing);
  ImGui::SetNextWindowSizeConstraints({860, 620}, {FLT_MAX, FLT_MAX});
  bool open = true;
  if (!ImGui::BeginPopupModal(title, &open)) {
    if (!open)
      CloseHmiEditor();
    return;
  }
  ImGui::TextUnformatted(
      TR("ui.hmi.editor_hint",
         "Select and drag on the screen; Apply saves changes."));
  ImGui::TextColored({0.85f, 0.4f, 0.05f, 1}, "%s",
                     TR("ui.hmi.paused_hint",
                        "Simulation paused; closing this editor resumes it."));
  const char* types[] = {
      TR("ui.hmi.button", "Button"), TR("ui.hmi.lamp", "Lamp"),
      TR("ui.hmi.number", "Number"), TR("ui.hmi.text", "Text"),
      TR("ui.hmi.message", "Status message")};
  ImGui::BeginDisabled(!physics_paused_);
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const float content_height =
      std::max(280.0f, available.y - ImGui::GetFrameHeightWithSpacing() - 44);
  ImGui::BeginChild("layout", {available.x * 0.60f, content_height}, true);
  hmi_edit_page_ = std::clamp(
      hmi_edit_page_, 0, static_cast<int>(hmi_edit_settings_.pages.size()) - 1);
  if (ImGui::BeginCombo(TR("ui.hmi.page", "Screen"),
                        hmi_edit_settings_.pages[hmi_edit_page_].c_str())) {
    for (int i = 0; i < hmi_edit_settings_.pages.size(); ++i) {
      ImGui::PushID(i);
      if (ImGui::Selectable(hmi_edit_settings_.pages[i].c_str(),
                            i == hmi_edit_page_)) {
        hmi_edit_page_ = i;
        hmi_selected_widget_ = -1;
      }
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }
  ImGui::BeginDisabled(hmi_edit_settings_.pages.size() >= 8);
  if (ImGui::Button(TR("ui.hmi.add_page", "Add screen"))) {
    hmi_edit_settings_.pages.push_back(
        "Screen " + std::to_string(hmi_edit_settings_.pages.size() + 1));
    hmi_edit_page_ = static_cast<int>(hmi_edit_settings_.pages.size()) - 1;
    hmi_selected_widget_ = -1;
  }
  ImGui::EndDisabled();
  EditText(TR("ui.hmi.page_name", "Screen name"),
           &hmi_edit_settings_.pages[hmi_edit_page_], 129);
  ImGui::Checkbox(TR("ui.hmi.snap", "Snap to 5 px grid"), &hmi_grid_snap_);
  ImGui::SameLine();
  ImGui::Text("%.0f x %.0f (4:3)", kHmiScreenWidth, kHmiScreenHeight);
  for (int i = 0; i < 5; ++i) {
    if (i)
      ImGui::SameLine();
    ImGui::PushID(i);
    ImGui::BeginDisabled(hmi_edit_settings_.widgets.size() >= kMaxHmiWidgets);
    char add_label[64];
    std::snprintf(add_label, sizeof(add_label), "+ %s", types[i]);
    if (ImGui::Button(add_label)) {
      HmiWidget widget;
      widget.type = static_cast<HmiWidgetType>(i);
      widget.page = hmi_edit_page_;
      widget.text = types[i];
      if (widget.type == HmiWidgetType::kNumber ||
          widget.type == HmiWidgetType::kMessage) {
        widget.address = "D100";
        widget.size = {110, 38};
      } else if (widget.type == HmiWidgetType::kText)
        widget.address.clear();
      SetUnusedButtonAddress(hmi_edit_settings_, &widget);
      PlaceAvailableWidget(hmi_edit_settings_, &widget);
      hmi_edit_settings_.widgets.push_back(std::move(widget));
      hmi_selected_widget_ =
          static_cast<int>(hmi_edit_settings_.widgets.size()) - 1;
    }
    ImGui::EndDisabled();
    ImGui::PopID();
  }
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 preview_space = ImGui::GetContentRegionAvail();
  const float scale =
      std::min(preview_space.x / kHmiScreenWidth,
               std::max(100.0f, preview_space.y - 120) / kHmiScreenHeight);
  HmiState preview;
  preview.active_page = hmi_edit_page_;
  plc_emulator::RenderHmiScreen(ImGui::GetWindowDrawList(), hmi_edit_settings_,
                                preview, origin, scale, hmi_grid_snap_);
  ImGui::InvisibleButton("screen",
                         {kHmiScreenWidth * scale, kHmiScreenHeight * scale});
  if (ImGui::IsItemClicked()) {
    hmi_selected_widget_ = -1;
    hmi_resize_drag_ = false;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float x = (mouse.x - origin.x) / scale;
    const float y = (mouse.y - origin.y) / scale;
    for (int i = static_cast<int>(hmi_edit_settings_.widgets.size()) - 1;
         i >= 0; --i) {
      const auto& widget = hmi_edit_settings_.widgets[i];
      if (widget.page != hmi_edit_page_)
        continue;
      if (x >= widget.position.x && y >= widget.position.y &&
          x < widget.position.x + widget.size.width &&
          y < widget.position.y + widget.size.height) {
        hmi_selected_widget_ = i;
        hmi_drag_start_position_ = widget.position;
        hmi_drag_start_size_ = widget.size;
        hmi_resize_drag_ = x >= widget.position.x + widget.size.width - 8 &&
                           y >= widget.position.y + widget.size.height - 8;
        break;
      }
    }
  }
  if (hmi_selected_widget_ >= 0 &&
      hmi_selected_widget_ <
          static_cast<int>(hmi_edit_settings_.widgets.size())) {
    auto& widget = hmi_edit_settings_.widgets[hmi_selected_widget_];
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
      const ImVec2 delta = ImGui::GetMouseDragDelta(0);
      if (hmi_resize_drag_) {
        widget.size = {
            std::clamp(Snap(hmi_drag_start_size_.width + delta.x / scale,
                            hmi_grid_snap_),
                       12.0f, kHmiScreenWidth - widget.position.x),
            std::clamp(Snap(hmi_drag_start_size_.height + delta.y / scale,
                            hmi_grid_snap_),
                       12.0f, kHmiScreenHeight - widget.position.y)};
      } else {
        widget.position = {
            Snap(hmi_drag_start_position_.x + delta.x / scale, hmi_grid_snap_),
            Snap(hmi_drag_start_position_.y + delta.y / scale, hmi_grid_snap_)};
      }
      ClampWidget(&widget);
    }
    if (!ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive()) {
      const float step = ImGui::GetIO().KeyShift ? 10.0f : 1.0f;
      if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
        widget.position.x -= step;
      if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        widget.position.x += step;
      if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
        widget.position.y -= step;
      if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
        widget.position.y += step;
      ClampWidget(&widget);
    }
    const ImVec2 start{origin.x + widget.position.x * scale,
                       origin.y + widget.position.y * scale};
    const ImVec2 end{start.x + widget.size.width * scale,
                     start.y + widget.size.height * scale};
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    if (ImGui::IsItemHovered() && mouse.x >= end.x - 8 * scale &&
        mouse.x <= end.x && mouse.y >= end.y - 8 * scale && mouse.y <= end.y)
      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
    ImGui::GetWindowDrawList()->AddRect(start, end, IM_COL32(40, 120, 220, 255),
                                        0, 0, 2);
    ImGui::GetWindowDrawList()->AddRectFilled(
        {end.x - 8 * scale, end.y - 8 * scale}, end,
        IM_COL32(40, 120, 220, 255));
  }
  ImGui::Separator();
  ImGui::TextUnformatted(TR("ui.hmi.widgets", "Widgets"));
  ImGui::BeginChild("widget_list", {0, 0}, true);
  for (size_t i = 0; i < hmi_edit_settings_.widgets.size(); ++i) {
    ImGui::PushID(static_cast<int>(i));
    const auto& widget = hmi_edit_settings_.widgets[i];
    if (widget.page != hmi_edit_page_) {
      ImGui::PopID();
      continue;
    }
    char label[192];
    std::snprintf(label, sizeof(label), "%s  |  %s  %s",
                  types[static_cast<int>(widget.type)], widget.text.c_str(),
                  widget.address.c_str());
    if (ImGui::Selectable(label, hmi_selected_widget_ == static_cast<int>(i)))
      hmi_selected_widget_ = static_cast<int>(i);
    ImGui::PopID();
  }
  ImGui::EndChild();
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("properties", {0, content_height}, true);
  if (hmi_selected_widget_ >= 0 &&
      hmi_selected_widget_ <
          static_cast<int>(hmi_edit_settings_.widgets.size())) {
    auto& widget = hmi_edit_settings_.widgets[hmi_selected_widget_];
    ImGui::TextUnformatted(types[static_cast<int>(widget.type)]);
    ImGui::BeginDisabled(hmi_edit_settings_.widgets.size() >= kMaxHmiWidgets);
    if (ImGui::Button(TR("ui.hmi.duplicate", "Duplicate"))) {
      HmiWidget copy = widget;
      SetUnusedButtonAddress(hmi_edit_settings_, &copy);
      PlaceAvailableWidget(hmi_edit_settings_, &copy);
      hmi_edit_settings_.widgets.push_back(std::move(copy));
      hmi_selected_widget_ =
          static_cast<int>(hmi_edit_settings_.widgets.size()) - 1;
    }
    ImGui::EndDisabled();
    // Reacquire after duplication because the vector may have grown.
    auto& selected = hmi_edit_settings_.widgets[hmi_selected_widget_];
    EditText(TR("ui.hmi.label", "Label"), &selected.text, 129);
    EditText(TR("ui.hmi.visibility", "Visible while bit is ON (blank: always)"),
             &selected.visibility_address, 16);
    if (selected.type == HmiWidgetType::kButton) {
      bool navigate = selected.target_page >= 0;
      if (ImGui::Checkbox(TR("ui.hmi.navigate", "Screen switch button"),
                          &navigate)) {
        selected.target_page = navigate ? hmi_edit_page_ : -1;
        selected.momentary = !navigate;
      }
      if (navigate) {
        if (ImGui::BeginCombo(
                TR("ui.hmi.target_page", "Target screen"),
                hmi_edit_settings_.pages[selected.target_page].c_str())) {
          for (int i = 0; i < hmi_edit_settings_.pages.size(); ++i) {
            ImGui::PushID(i);
            if (ImGui::Selectable(hmi_edit_settings_.pages[i].c_str(),
                                  i == selected.target_page))
              selected.target_page = i;
            ImGui::PopID();
          }
          ImGui::EndCombo();
        }
      }
    }
    if (selected.type != HmiWidgetType::kText && selected.target_page < 0) {
      EditText(TR("ui.hmi.address", "PLC address"), &selected.address, 16);
      ImGui::TextDisabled("%s", selected.type == HmiWidgetType::kButton ? "M100"
                                : (selected.type == HmiWidgetType::kNumber ||
                                   selected.type == HmiWidgetType::kMessage)
                                    ? "D100"
                                    : "M100 / X0 / Y0");
    }
    float position[] = {selected.position.x, selected.position.y};
    float size[] = {selected.size.width, selected.size.height};
    ImGui::TextUnformatted(TR("ui.hmi.position", "Position"));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat2("##position", position, 1)) {
      selected.position = {position[0], position[1]};
      ClampWidget(&selected);
    }
    ImGui::TextUnformatted(TR("ui.hmi.size", "Size"));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat2("##size", size, 1)) {
      selected.size = {size[0], size[1]};
      ClampWidget(&selected);
    }
    const char* horizontal[] = {TR("ui.hmi.left", "Left"),
                                TR("ui.hmi.center", "Center"),
                                TR("ui.hmi.right", "Right")};
    for (int i = 0; i < 3; ++i) {
      if (i)
        ImGui::SameLine();
      if (ImGui::Button(horizontal[i]))
        selected.position.x = i == 0 ? 0
                              : i == 1
                                  ? (kHmiScreenWidth - selected.size.width) / 2
                                  : kHmiScreenWidth - selected.size.width;
    }
    const char* vertical[] = {TR("ui.hmi.top", "Top"),
                              TR("ui.hmi.middle", "Middle"),
                              TR("ui.hmi.bottom", "Bottom")};
    for (int i = 0; i < 3; ++i) {
      if (i)
        ImGui::SameLine();
      if (ImGui::Button(vertical[i]))
        selected.position.y =
            i == 0   ? 0
            : i == 1 ? (kHmiScreenHeight - selected.size.height) / 2
                     : kHmiScreenHeight - selected.size.height;
    }
    ImGui::TextUnformatted(TR("ui.hmi.font_size", "Font size"));
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##font_size", &selected.font_size, 6, 32, "%.0f");
    if (selected.type == HmiWidgetType::kButton && selected.target_page < 0)
      ImGui::Checkbox(TR("ui.hmi.momentary", "Momentary button"),
                      &selected.momentary);
    if (selected.type == HmiWidgetType::kMessage) {
      for (int i = 0; i < 3; ++i) {
        ImGui::PushID(i);
        ImGui::Text("D = %d", i);
        EditText(TR("ui.hmi.message_text", "Message text"),
                 &selected.messages[i], 129);
        ImGui::PopID();
      }
    }
    if (selected.type == HmiWidgetType::kNumber) {
      EditText(TR("ui.hmi.unit", "Unit"), &selected.unit, 25);
      ImGui::TextUnformatted(TR("ui.hmi.decimals", "Decimal places"));
      ImGui::SetNextItemWidth(-1);
      ImGui::SliderInt("##decimals", &selected.decimals, 0, 3);
    }
    if (selected.type == HmiWidgetType::kLamp) {
      ImVec4 color = ImGui::ColorConvertU32ToFloat4(selected.on_color);
      float rgb[] = {color.x, color.y, color.z};
      ImGui::TextUnformatted(TR("ui.hmi.on_color", "ON color"));
      ImGui::SetNextItemWidth(-1);
      if (ImGui::ColorEdit3("##color", rgb))
        selected.on_color =
            ImGui::ColorConvertFloat4ToU32({rgb[0], rgb[1], rgb[2], 1});
    }
    if (ImGui::Button(TR("ui.hmi.delete_widget", "Delete widget"))) {
      hmi_edit_settings_.widgets.erase(hmi_edit_settings_.widgets.begin() +
                                       hmi_selected_widget_);
      hmi_selected_widget_ = -1;
    }
  } else {
    ImGui::TextWrapped(
        "%s", TR("ui.hmi.select_hint",
                 "Select a widget on the screen or in the list to edit it."));
  }
  ImGui::EndChild();
  const bool valid = plc_emulator::IsValidHmiSettings(hmi_edit_settings_);
  if (!valid)
    ImGui::TextWrapped(
        "%s", TR("ui.hmi.invalid",
                 "Check addresses, duplicate buttons and screen bounds."));
  ImGui::BeginDisabled(!valid);
  if (ImGui::Button(TR("ui.hmi.apply", "Apply")) &&
      ApplyHmiSettings(component_id, hmi_edit_settings_)) {
    ImGui::CloseCurrentPopup();
    CloseHmiEditor();
  }
  ImGui::EndDisabled();
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button(TR("ui.hmi.cancel", "Cancel"))) {
    ImGui::CloseCurrentPopup();
    CloseHmiEditor();
  }
  const ImVec2 window_position = ImGui::GetWindowPos();
  const ImVec2 window_size = ImGui::GetWindowSize();
  RegisterOverlayInputCaptureRect(
      window_position,
      {window_position.x + window_size.x, window_position.y + window_size.y},
      true);
  ImGui::EndPopup();
}

}  // namespace plc
