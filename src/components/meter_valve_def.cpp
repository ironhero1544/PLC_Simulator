#include "plc_emulator/components/meter_valve_def.h"

#include "imgui.h"
#include "plc_emulator/components/state_keys.h"
#include "nanosvg.h"
#include "nanosvgrast.h"

#include <glad/glad.h>

#include <cfloat>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace plc {
namespace {

ImU32 ToImU32(const Color& c) {
  return IM_COL32(c.r * 255, c.g * 255, c.b * 255, c.a * 255);
}

struct SvgTexture {
  ImTextureID id = 0;
  int width = 0;
  int height = 0;
};

int g_active_meter_menu_id = -1;

const SvgTexture* GetSvgTexture(const char* path, int width, int height) {
  if (!path || width <= 0 || height <= 0) {
    return nullptr;
  }

  static std::map<std::string, SvgTexture> cache;
  std::string key = std::string(path) + "#" + std::to_string(width) + "x" +
                    std::to_string(height);
  auto it = cache.find(key);
  if (it != cache.end()) {
    return &it->second;
  }

  NSVGimage* image = nsvgParseFromFile(path, "px", 96.0f);
  if (!image || image->width <= 0.0f || image->height <= 0.0f) {
    if (image) {
      nsvgDelete(image);
    }
    return nullptr;
  }

  NSVGrasterizer* raster = nsvgCreateRasterizer();
  if (!raster) {
    nsvgDelete(image);
    return nullptr;
  }

  float minx = FLT_MAX;
  float miny = FLT_MAX;
  float maxx = -FLT_MAX;
  float maxy = -FLT_MAX;
  for (NSVGshape* shape = image->shapes; shape; shape = shape->next) {
    if (shape->bounds[0] < minx) {
      minx = shape->bounds[0];
    }
    if (shape->bounds[1] < miny) {
      miny = shape->bounds[1];
    }
    if (shape->bounds[2] > maxx) {
      maxx = shape->bounds[2];
    }
    if (shape->bounds[3] > maxy) {
      maxy = shape->bounds[3];
    }
  }

  if (!(minx < maxx && miny < maxy)) {
    minx = 0.0f;
    miny = 0.0f;
    maxx = image->width;
    maxy = image->height;
  }

  float bounds_w = maxx - minx;
  float bounds_h = maxy - miny;
  if (bounds_w <= 0.0f || bounds_h <= 0.0f) {
    nsvgDeleteRasterizer(raster);
    nsvgDelete(image);
    return nullptr;
  }

  std::vector<unsigned char> rgba(
      static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0);

  float scale_x = static_cast<float>(width) / bounds_w;
  float scale_y = static_cast<float>(height) / bounds_h;
  float scale = (scale_x < scale_y) ? scale_x : scale_y;
  if (scale <= 0.0f) {
    nsvgDeleteRasterizer(raster);
    nsvgDelete(image);
    return nullptr;
  }

  float offset_x = (static_cast<float>(width) - bounds_w * scale) * 0.5f;
  float offset_y = (static_cast<float>(height) - bounds_h * scale) * 0.5f;
  float tx = offset_x / scale - minx;
  float ty = offset_y / scale - miny;
  nsvgRasterize(raster, image, tx, ty, scale, rgba.data(), width, height,
                width * 4);

  nsvgDeleteRasterizer(raster);
  nsvgDelete(image);

  GLuint tex = 0;
  glGenTextures(1, &tex);
  if (tex == 0) {
    return nullptr;
  }

  GLint prev_tex = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, rgba.data());
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_tex));
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

  SvgTexture entry;
  entry.id = reinterpret_cast<ImTextureID>(static_cast<intptr_t>(tex));
  entry.width = width;
  entry.height = height;
  auto inserted = cache.emplace(key, entry);
  return &inserted.first->second;
}

void DrawMeterSymbol(ImDrawList* draw_list,
                     ImVec2 min,
                     ImVec2 max,
                     bool meter_in) {
  if (!draw_list) {
    return;
  }

  float w = max.x - min.x;
  float h = max.y - min.y;
  if (w <= 1.0f || h <= 1.0f) {
    return;
  }

  ImU32 stroke = IM_COL32(0, 0, 0, 255);
  ImU32 fill = IM_COL32(255, 255, 255, 255);
  float min_dim = (w < h) ? w : h;
  float stroke_w = min_dim * 0.08f;
  if (stroke_w < 1.0f) {
    stroke_w = 1.0f;
  }

  float cx = min.x + w * 0.5f;
  float box_top_y = min.y + h * 0.22f;
  float box_bot_y = min.y + h * 0.78f;

  draw_list->AddLine({cx, min.y}, {cx, box_top_y}, stroke, stroke_w);
  draw_list->AddLine({cx, box_bot_y}, {cx, min.y + h}, stroke, stroke_w);

  float box_half_w = w * 0.4f;
  ImVec2 box_min = {cx - box_half_w, box_top_y};
  ImVec2 box_max = {cx + box_half_w, box_bot_y};
  draw_list->AddRect(box_min, box_max, stroke, 0.0f, 0, stroke_w);

  float inner_spacing_x = w * 0.15f;
  float inner_start_y = box_top_y + h * 0.1f;
  float inner_end_y = box_bot_y - h * 0.1f;

  ImVec2 l_top = {cx - inner_spacing_x, inner_start_y};
  ImVec2 l_bot = {cx - inner_spacing_x, inner_end_y};
  draw_list->AddLine({cx, inner_start_y}, l_top, stroke, stroke_w);
  draw_list->AddLine(l_top, l_bot, stroke, stroke_w);
  draw_list->AddLine(l_bot, {cx, inner_end_y}, stroke, stroke_w);

  ImVec2 r_top = {cx + inner_spacing_x, inner_start_y};
  ImVec2 r_bot = {cx + inner_spacing_x, inner_end_y};
  draw_list->AddLine({cx, inner_start_y}, r_top, stroke, stroke_w);
  draw_list->AddLine(r_top, r_bot, stroke, stroke_w);
  draw_list->AddLine(r_bot, {cx, inner_end_y}, stroke, stroke_w);

  ImVec2 arrow_p1 = {cx - inner_spacing_x - w * 0.15f, inner_end_y};
  ImVec2 arrow_p2 = {cx + w * 0.05f, inner_start_y};
  draw_list->AddLine(arrow_p1, arrow_p2, stroke, stroke_w);

  ImVec2 dir = {arrow_p2.x - arrow_p1.x, arrow_p2.y - arrow_p1.y};
  float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
  if (len > 0.001f) {
    dir.x /= len;
    dir.y /= len;
  }
  float arrow_sz = w * 0.1f;
  ImVec2 arrow_wing1 = {arrow_p2.x - dir.x * arrow_sz - dir.y * arrow_sz * 0.5f,
                        arrow_p2.y - dir.y * arrow_sz + dir.x * arrow_sz * 0.5f};
  ImVec2 arrow_wing2 = {arrow_p2.x - dir.x * arrow_sz + dir.y * arrow_sz * 0.5f,
                        arrow_p2.y - dir.y * arrow_sz - dir.x * arrow_sz * 0.5f};
  draw_list->AddTriangleFilled(arrow_p2, arrow_wing1, arrow_wing2, stroke);

  float circle_radius = w * 0.1f;
  ImVec2 circle_center = {cx + inner_spacing_x, min.y + h * 0.5f};
  draw_list->AddCircleFilled(circle_center, circle_radius, fill);
  draw_list->AddCircle(circle_center, circle_radius, stroke, 0, stroke_w);

  float v_sz = circle_radius * 0.6f;
  ImVec2 v_tip;
  ImVec2 v_l;
  ImVec2 v_r;
  if (meter_in) {
    v_tip = {circle_center.x, circle_center.y + v_sz * 0.5f};
    v_l = {circle_center.x - v_sz, circle_center.y - v_sz * 0.5f};
    v_r = {circle_center.x + v_sz, circle_center.y - v_sz * 0.5f};
  } else {
    v_tip = {circle_center.x, circle_center.y - v_sz * 0.5f};
    v_l = {circle_center.x - v_sz, circle_center.y + v_sz * 0.5f};
    v_r = {circle_center.x + v_sz, circle_center.y + v_sz * 0.5f};
  }
  draw_list->AddLine(v_l, v_tip, stroke, stroke_w);
  draw_list->AddLine(v_r, v_tip, stroke, stroke_w);

  float dot_r = min_dim * 0.03f;
  if (dot_r < 1.0f) {
    dot_r = 1.0f;
  }
  draw_list->AddCircleFilled({cx, box_top_y}, dot_r, stroke);
  draw_list->AddCircleFilled({cx, box_bot_y}, dot_r, stroke);

  if (min_dim > 26.0f) {
    const char* top_label = "2";
    const char* bottom_label = "1";
    ImVec2 top_size = ImGui::CalcTextSize(top_label);
    ImVec2 bot_size = ImGui::CalcTextSize(bottom_label);
    draw_list->AddText({cx - w * 0.3f - top_size.x * 0.5f,
                        box_top_y - h * 0.05f - top_size.y * 0.5f},
                       stroke, top_label);
    draw_list->AddText({cx - w * 0.3f - bot_size.x * 0.5f,
                        box_bot_y + h * 0.05f - bot_size.y * 0.5f},
                       stroke, bottom_label);
  }
}

const ComponentPortDef kPorts[] = {
    {0, {25.0f, 70.0f}, PortType::PNEUMATIC, true, {0.13f, 0.59f, 0.95f, 1},
     "PNEUMATIC_IN"},
    {1, {25.0f, 10.0f}, PortType::PNEUMATIC, false, {0.13f, 0.59f, 0.95f, 1},
     "PNEUMATIC_OUT"},
};

void RenderMeterValve(ImDrawList* draw_list,
                      const PlacedComponent& comp,
                      ImVec2 pos,
                      float zoom) {
  ImVec2 size = {50.0f * zoom, 80.0f * zoom};

  draw_list->AddRectFilled(pos, {pos.x + size.x, pos.y + size.y},
                           IM_COL32(240, 240, 240, 255));
  draw_list->AddRect(pos, {pos.x + size.x, pos.y + size.y},
                     IM_COL32(0, 0, 0, 255), 0, 2.0f * zoom);

  ImVec2 center = {pos.x + 25.0f * zoom, pos.y + 40.0f * zoom};
  draw_list->AddCircleFilled(center, 15.0f * zoom,
                             IM_COL32(180, 180, 180, 255));
  draw_list->AddCircle(center, 15.0f * zoom, IM_COL32(50, 50, 50, 255), 0,
                       2.0f * zoom);

  float angle = 0.0f;
  if (comp.internalStates.count(state_keys::kFlowSetting)) {
    float normalized = comp.internalStates.at(state_keys::kFlowSetting);
    angle = (normalized - 0.5f) * 5.0f;
  }
  ImVec2 rotated = ImVec2(center.x + std::sin(angle) * 12.0f * zoom,
                          center.y - std::cos(angle) * 12.0f * zoom);
  draw_list->AddLine(center, rotated, IM_COL32(0, 0, 0, 255), 2.0f * zoom);

  bool meter_in = true;
  if (comp.internalStates.count(state_keys::kMeterMode)) {
    meter_in = comp.internalStates.at(state_keys::kMeterMode) < 0.5f;
  }

  float arrow_size = 7.0f * zoom;
  float arrow_padding = 2.0f * zoom;
  ImVec2 arrow_min = {pos.x + size.x - arrow_padding - arrow_size,
                      center.y - arrow_size * 0.5f};
  ImVec2 arrow_max = {arrow_min.x + arrow_size, arrow_min.y + arrow_size};

  float arrow_rounding = 2.0f * zoom;
  bool arrow_hover = ImGui::IsMouseHoveringRect(arrow_min, arrow_max, true);
  ImU32 arrow_fill =
      arrow_hover ? IM_COL32(230, 230, 230, 255) : IM_COL32(245, 245, 245, 255);
  ImU32 arrow_border = IM_COL32(0, 0, 0, 255);
  draw_list->AddRectFilled(arrow_min, arrow_max, arrow_fill, arrow_rounding);
  draw_list->AddRect(arrow_min, arrow_max, arrow_border, arrow_rounding, 0,
                     1.5f * zoom);

  ImVec2 tri_center = {(arrow_min.x + arrow_max.x) * 0.5f,
                       (arrow_min.y + arrow_max.y) * 0.5f};
  float tri_half = arrow_size * 0.18f;
  draw_list->AddTriangleFilled(
      {tri_center.x - tri_half, tri_center.y - tri_half * 0.5f},
      {tri_center.x + tri_half, tri_center.y - tri_half * 0.5f},
      {tri_center.x, tri_center.y + tri_half}, arrow_border);

  ImGui::PushID(comp.instanceId);
  auto& mutable_comp = const_cast<PlacedComponent&>(comp);
  bool menu_open =
      comp.internalStates.count(state_keys::kMeterMenuOpen) &&
      comp.internalStates.at(state_keys::kMeterMenuOpen) > 0.5f;
  if (menu_open) {
    if (g_active_meter_menu_id == -1) {
      g_active_meter_menu_id = comp.instanceId;
    } else if (g_active_meter_menu_id != comp.instanceId) {
      menu_open = false;
      mutable_comp.internalStates[state_keys::kMeterMenuOpen] = 0.0f;
    }
  }
  bool toggle_request =
      ImGui::IsMouseClicked(ImGuiMouseButton_Left) && arrow_hover;
  if (toggle_request) {
    if (menu_open) {
      menu_open = false;
      mutable_comp.internalStates[state_keys::kMeterMenuOpen] = 0.0f;
      if (g_active_meter_menu_id == comp.instanceId) {
        g_active_meter_menu_id = -1;
      }
    } else {
      menu_open = true;
      mutable_comp.internalStates[state_keys::kMeterMenuOpen] = 1.0f;
      g_active_meter_menu_id = comp.instanceId;
    }
  }
  if (menu_open) {
    ImGui::OpenPopup("meter_mode_popup");
  }

  float popup_icon = 100.0f;
  float popup_pad = 8.0f;
  float popup_item = popup_icon + popup_pad * 2.0f;
  float popup_spacing = 12.0f;
  float popup_width = popup_item * 2.0f + popup_spacing;
  float arrow_center_x = (arrow_min.x + arrow_max.x) * 0.5f;
  ImVec2 popup_pos = {arrow_center_x - popup_width * 0.5f,
                      arrow_max.y + 10.0f};
  ImGui::SetNextWindowPos(popup_pos, ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(popup_spacing, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
  int icon_px = static_cast<int>(popup_icon + 0.5f);
  if (icon_px < 1) {
    icon_px = 1;
  }

  const SvgTexture* svg_in =
      GetSvgTexture("resources/symbol/meter-in.svg", icon_px, icon_px);
  const SvgTexture* svg_out =
      GetSvgTexture("resources/symbol/meter-out.svg", icon_px, icon_px);

  bool popup_open = ImGui::BeginPopup("meter_mode_popup",
                                      ImGuiWindowFlags_NoDecoration |
                                          ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoSavedSettings |
                                          ImGuiWindowFlags_AlwaysAutoResize |
                                          ImGuiWindowFlags_NoScrollbar |
                                          ImGuiWindowFlags_NoScrollWithMouse);
  if (popup_open) {
    ImDrawList* popup_draw = ImGui::GetWindowDrawList();
    bool just_opened = ImGui::IsWindowAppearing();
    bool close_popup = false;

    if (!menu_open) {
      close_popup = true;
    }

    ImGui::InvisibleButton("meter_mode_in", ImVec2(popup_item, popup_item));
    ImVec2 in_min = ImGui::GetItemRectMin();
    ImVec2 in_max = ImGui::GetItemRectMax();
    bool in_hover = ImGui::IsItemHovered();
    bool in_click = ImGui::IsItemClicked();
    bool in_selected = meter_in;
    ImU32 in_fill = (in_hover || in_selected)
                        ? IM_COL32(225, 225, 225, 255)
                        : IM_COL32(250, 250, 250, 255);
    popup_draw->AddRectFilled(in_min, in_max, in_fill, 3.0f * zoom);
    popup_draw->AddRect(in_min, in_max, IM_COL32(0, 0, 0, 255),
                        3.0f * zoom, 0, 1.5f * zoom);
    ImVec2 in_icon_min = {in_min.x + popup_pad, in_min.y + popup_pad};
    ImVec2 in_icon_max = {in_max.x - popup_pad, in_max.y - popup_pad};
    if (svg_in && svg_in->id) {
      popup_draw->AddImage(svg_in->id, in_icon_min, in_icon_max);
    } else {
      DrawMeterSymbol(popup_draw, in_icon_min, in_icon_max, true);
    }
    if (in_click) {
      mutable_comp.internalStates[state_keys::kMeterMode] = 0.0f;
      meter_in = true;
      mutable_comp.internalStates[state_keys::kMeterMenuOpen] = 0.0f;
      menu_open = false;
      if (g_active_meter_menu_id == comp.instanceId) {
        g_active_meter_menu_id = -1;
      }
      ImGui::CloseCurrentPopup();
    }
    if (in_hover) {
      ImGui::BeginTooltip();
      ImGui::TextUnformatted("Meter: IN");
      ImGui::EndTooltip();
    }

    ImGui::SameLine();

    ImGui::InvisibleButton("meter_mode_out", ImVec2(popup_item, popup_item));
    ImVec2 out_min = ImGui::GetItemRectMin();
    ImVec2 out_max = ImGui::GetItemRectMax();
    bool out_hover = ImGui::IsItemHovered();
    bool out_click = ImGui::IsItemClicked();
    bool out_selected = !meter_in;
    ImU32 out_fill = (out_hover || out_selected)
                         ? IM_COL32(225, 225, 225, 255)
                         : IM_COL32(250, 250, 250, 255);
    popup_draw->AddRectFilled(out_min, out_max, out_fill, 3.0f * zoom);
    popup_draw->AddRect(out_min, out_max, IM_COL32(0, 0, 0, 255),
                        3.0f * zoom, 0, 1.5f * zoom);
    ImVec2 out_icon_min = {out_min.x + popup_pad, out_min.y + popup_pad};
    ImVec2 out_icon_max = {out_max.x - popup_pad, out_max.y - popup_pad};
    if (svg_out && svg_out->id) {
      popup_draw->AddImage(svg_out->id, out_icon_min, out_icon_max);
    } else {
      DrawMeterSymbol(popup_draw, out_icon_min, out_icon_max, false);
    }
    if (out_click) {
      mutable_comp.internalStates[state_keys::kMeterMode] = 1.0f;
      meter_in = false;
      mutable_comp.internalStates[state_keys::kMeterMenuOpen] = 0.0f;
      menu_open = false;
      if (g_active_meter_menu_id == comp.instanceId) {
        g_active_meter_menu_id = -1;
      }
      ImGui::CloseCurrentPopup();
    }
    if (out_hover) {
      ImGui::BeginTooltip();
      ImGui::TextUnformatted("Meter: OUT");
      ImGui::EndTooltip();
    }

    if (!just_opened) {
      bool hovered =
          ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
      if (!hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        close_popup = true;
      }
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        close_popup = true;
      }
    }
    if (close_popup) {
      mutable_comp.internalStates[state_keys::kMeterMenuOpen] = 0.0f;
      menu_open = false;
      if (g_active_meter_menu_id == comp.instanceId) {
        g_active_meter_menu_id = -1;
      }
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  } else if (menu_open) {
    mutable_comp.internalStates[state_keys::kMeterMenuOpen] = 0.0f;
    menu_open = false;
    if (g_active_meter_menu_id == comp.instanceId) {
      g_active_meter_menu_id = -1;
    }
  }
  if (!menu_open && g_active_meter_menu_id == comp.instanceId) {
    g_active_meter_menu_id = -1;
  }
  ImGui::PopStyleVar(3);
  ImGui::PopID();

  for (const auto& p : kPorts) {
    draw_list->AddCircleFilled(
        {pos.x + p.rel_pos.x * zoom, pos.y + p.rel_pos.y * zoom}, 4 * zoom,
        ToImU32(p.color));
  }
}

void InitMeterValveDefaults(PlacedComponent* comp) {
  if (!comp) {
    return;
  }
  comp->internalStates[state_keys::kFlowSetting] = 0.0f;
  comp->internalStates[state_keys::kMeterMode] = 0.0f;
  comp->internalStates[state_keys::kMeterMenuOpen] = 0.0f;
}

const ComponentDefinition kDefinition = {
    ComponentType::METER_VALVE,
    "component.meter_valve.name",
    "component.meter_valve.desc",
    {50.0f, 80.0f},
    kPorts,
    static_cast<int>(sizeof(kPorts) / sizeof(kPorts[0])),
    &RenderMeterValve,
    &InitMeterValveDefaults,
    ComponentCategory::BOTH,
    "icon.meter_valve",
    "Meter",
};

}  // namespace

const ComponentDefinition* GetMeterValveDefinition() {
  return &kDefinition;
}

}  // namespace plc
