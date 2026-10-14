#include "plc_emulator/physics/storage_sensor_simulation.h"

#include "plc_emulator/components/pneumatic_component.h"
#include "plc_emulator/components/state_keys.h"
#include "plc_emulator/core/component_transform.h"
#include "plc_emulator/physics/z_layer.h"
#include "plc_emulator/physics/ring_feed_simulation.h"
#include "plc_emulator/programming/execution_program.h"

#include <algorithm>
#include <cmath>

namespace plc_emulator {
namespace {
float Voltage(const std::map<plc::PortRef, float>& voltages, int id, int port) {
  const auto found = voltages.find({id, port});
  return found == voltages.end() ? -1 : found->second;
}
bool Powered(const plc::PlacedComponent& part,
             const std::map<plc::PortRef, float>& voltages) {
  return Voltage(voltages, part.instanceId, 0) > 23 &&
         std::abs(Voltage(voltages, part.instanceId, 1)) < 0.1f;
}
const plc::PlacedComponent* Find(const std::vector<plc::PlacedComponent>& parts,
                                 int id) {
  const auto found =
      std::find_if(parts.begin(), parts.end(),
                   [id](const auto& part) { return part.instanceId == id; });
  return found == parts.end() ? nullptr : &*found;
}
int Linked(const std::vector<plc::Wire>& wires, int id, int port,
           plc::WireKind kind, int other_port) {
  int result = -1;
  for (const auto& wire : wires) {
    if (wire.kind != kind)
      continue;
    int other = -1;
    if (wire.fromComponentId == id && wire.fromPortId == port &&
        wire.toPortId == other_port)
      other = wire.toComponentId;
    if (wire.toComponentId == id && wire.toPortId == port &&
        wire.fromPortId == other_port)
      other = wire.fromComponentId;
    if (other < 0)
      continue;
    if (result >= 0)
      return -1;
    result = other;
  }
  return result;
}
bool DecimalAddress(const std::string& address, char prefix, int maximum) {
  if (address.size() < 2 || address.size() > 5 || address[0] != prefix)
    return false;
  if (address.size() > 2 && address[1] == '0')
    return false;
  int value = 0;
  for (size_t i = 1; i < address.size(); ++i) {
    if (address[i] < '0' || address[i] > '9')
      return false;
    value = value * 10 + address[i] - '0';
  }
  return value <= maximum;
}
void PlaceInSlot(const plc::PlacedComponent& warehouse,
                 plc::PlacedComponent* piece) {
  const int slot = piece->carry_state.slot_id;
  const ImVec2 center =
      plc::LocalToWorld(warehouse, {(slot % 2 ? 45.0f : 115.0f) +
          (warehouse.type == plc::ComponentType::kDrivenWarehouse
               ? 190.0f + ReadPneumaticState(warehouse, "position") : 0.0f), 50});
  const plc::Size size = plc::GetComponentDisplaySize(*piece);
  piece->position = {center.x - size.width / 2, center.y - size.height / 2};
  piece->z_state.current_height = WarehouseSlotHeight(warehouse, slot);
  piece->internalStates["vel_x"] = 0;
  piece->internalStates["vel_y"] = 0;
}
bool RayHits(const plc::Aabb& bounds, ImVec2 start, ImVec2 end, float width) {
  const float minimum[] = {bounds.min_x - width, bounds.min_y - width};
  const float maximum[] = {bounds.max_x + width, bounds.max_y + width};
  const float origin[] = {start.x, start.y};
  const float delta[] = {end.x - start.x, end.y - start.y};
  float entry = 0;
  float exit = 1;
  for (int axis = 0; axis < 2; ++axis) {
    if (std::abs(delta[axis]) < 0.00001f) {
      if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis])
        return false;
    } else {
      const float a = (minimum[axis] - origin[axis]) / delta[axis];
      const float b = (maximum[axis] - origin[axis]) / delta[axis];
      entry = std::max(entry, std::min(a, b));
      exit = std::min(exit, std::max(a, b));
    }
  }
  return entry <= exit;
}
}  // namespace

bool IsStorageSensorPart(plc::ComponentType type) {
  return plc::IsWarehouseType(type) ||
         type == plc::ComponentType::kFiberAmplifier ||
         type == plc::ComponentType::kFiberTransmitter ||
         type == plc::ComponentType::kFiberReceiver ||
         type == plc::ComponentType::kTouchPanel;
}
bool IsValidFiberSettings(const plc::FiberSettings& settings) {
  return std::isfinite(settings.max_distance) && settings.max_distance > 0 &&
         settings.max_distance <= 100000 &&
         std::isfinite(settings.alignment_tolerance) &&
         settings.alignment_tolerance >= 0 &&
         settings.alignment_tolerance <= 100 &&
         std::isfinite(settings.beam_width) && settings.beam_width > 0 &&
         settings.beam_width <= 100;
}
bool IsValidHmiSettings(const plc::HmiSettings& settings) {
  if (settings.pages.empty() || settings.pages.size() > 8 ||
      std::any_of(
          settings.pages.begin(), settings.pages.end(),
          [](const auto& name) { return name.empty() || name.size() > 128; }))
    return false;
  if (settings.widgets.size() > plc::kMaxHmiWidgets)
    return false;
  for (size_t i = 0; i < settings.widgets.size(); ++i) {
    const auto& widget = settings.widgets[i];
    if (!std::isfinite(widget.position.x) ||
        !std::isfinite(widget.position.y) ||
        !std::isfinite(widget.size.width) ||
        !std::isfinite(widget.size.height) ||
        !std::isfinite(widget.font_size) || widget.position.x < 0 ||
        widget.position.y < 0 || widget.size.width < 12 ||
        widget.size.height < 12 ||
        widget.position.x + widget.size.width > plc::kHmiScreenWidth ||
        widget.position.y + widget.size.height > plc::kHmiScreenHeight ||
        widget.font_size < 6 || widget.font_size > 32 || widget.decimals < 0 ||
        widget.decimals > 3 || widget.text.size() > 128 ||
        widget.unit.size() > 24)
      return false;
    if (widget.page < 0 ||
        widget.page >= static_cast<int>(settings.pages.size()) ||
        widget.target_page < -1 ||
        widget.target_page >= static_cast<int>(settings.pages.size()) ||
        std::any_of(
            widget.messages.begin(), widget.messages.end(),
            [](const auto& message) { return message.size() > 128; }))
      return false;
    if (!widget.visibility_address.empty()) {
      const auto address =
          programming::ParseDeviceAddress(widget.visibility_address);
      if (!address || address->bit >= 0 || address->index_kind != 0 ||
          programming::FormatDeviceAddress(*address) !=
              widget.visibility_address ||
          (address->kind != programming::DeviceKind::kM &&
           address->kind != programming::DeviceKind::kX &&
           address->kind != programming::DeviceKind::kY))
        return false;
    }
    switch (widget.type) {
      case plc::HmiWidgetType::kButton:
        if (widget.target_page >= 0)
          break;
        if (!DecimalAddress(widget.address, 'M', 7679))
          return false;
        for (size_t j = 0; j < i; ++j)
          if (settings.widgets[j].type == plc::HmiWidgetType::kButton &&
              settings.widgets[j].target_page < 0 &&
              settings.widgets[j].address == widget.address)
            return false;
        break;
      case plc::HmiWidgetType::kLamp: {
        const auto address = programming::ParseDeviceAddress(widget.address);
        if (!address || address->bit >= 0 || address->index_kind != 0 ||
            programming::FormatDeviceAddress(*address) != widget.address ||
            (address->kind != programming::DeviceKind::kM &&
             address->kind != programming::DeviceKind::kX &&
             address->kind != programming::DeviceKind::kY))
          return false;
        break;
      }
      case plc::HmiWidgetType::kMessage:
      case plc::HmiWidgetType::kNumber:
        if (!DecimalAddress(widget.address, 'D', 7999))
          return false;
        break;
      case plc::HmiWidgetType::kText:
        break;
      default:
        return false;
    }
  }
  return true;
}
int FindHmiPlc(const plc::PlacedComponent& panel,
               const std::vector<plc::PlacedComponent>& parts,
               const std::map<plc::PortRef, float>& voltages) {
  if (!Powered(panel, voltages))
    return -1;
  int id = -1;
  for (const auto& part : parts) {
    if (part.type != plc::ComponentType::PLC)
      continue;
    if (id >= 0)
      return -1;
    id = part.instanceId;
  }
  return id;
}
float WarehouseSlotHeight(const plc::PlacedComponent& warehouse, int slot) {
  return warehouse.z_config.min_layer +
         ((slot - 1) / 2) *
             (warehouse.z_config.max_layer - warehouse.z_config.min_layer) / 2;
}
bool StoreWorkpieceInBox(std::vector<plc::PlacedComponent>* parts,
                         const plc::PlacedComponent& box,
                         plc::PlacedComponent* workpiece) {
  if (!parts || !workpiece || box.type != plc::ComponentType::BOX ||
      !plc::IsWorkpiece(workpiece->type) ||
      !CanInteractAtHeight(box, *workpiece))
    return false;
  constexpr float kMargin = 4;
  constexpr float kGap = 4;
  static thread_local std::vector<plc::PlacedComponent*> occupants;
  occupants.clear();
  occupants.reserve(parts->size());
  for (auto& piece : *parts) {
    if (&piece == workpiece || !plc::IsWorkpiece(piece.type) ||
        piece.carry_state.owner_id >= 0 ||
        ReadPneumaticState(piece, plc::state_keys::kIsManualDrag) > 0.5f ||
        ReadPneumaticState(piece, plc::state_keys::kIsStuckBox) < 0.5f ||
        !CanInteractAtHeight(box, piece))
      continue;
    const float owner = ReadPneumaticState(piece, plc::state_keys::kBoxId, -1);
    if (owner >= 0 ? owner == box.instanceId
                   : plc::IsPointInsideComponent(
                         box, plc::GetComponentWorldCenter(piece)))
      occupants.push_back(&piece);
  }
  std::sort(occupants.begin(), occupants.end(), [](const auto* first,
                                                 const auto* second) {
    const float first_slot = ReadPneumaticState(*first, plc::state_keys::kBoxSlot);
    const float second_slot = ReadPneumaticState(*second, plc::state_keys::kBoxSlot);
    return first_slot == second_slot ? first->instanceId < second->instanceId
                                    : first_slot < second_slot;
  });
  occupants.push_back(workpiece);
  float cell_width = 0;
  float cell_height = 0;
  for (const auto* piece : occupants) {
    const bool sideways = plc::NormalizeRotationQuadrants(
        piece->rotation_quadrants - box.rotation_quadrants) % 2 != 0;
    cell_width = std::max(cell_width, sideways ? piece->size.height
                                             : piece->size.width);
    cell_height = std::max(cell_height, sideways ? piece->size.width
                                               : piece->size.height);
  }
  const int count = static_cast<int>(occupants.size());
  const int columns = std::min(count, static_cast<int>(
      (box.size.width - 2 * kMargin + kGap) / (cell_width + kGap)));
  if (columns <= 0)
    return false;
  const int rows = (count + columns - 1) / columns;
  const float grid_width = columns * (cell_width + kGap) - kGap;
  const float grid_height = rows * (cell_height + kGap) - kGap;
  if (grid_height > box.size.height - 2 * kMargin)
    return false;
  for (int index = 0; index < count; ++index) {
    auto* piece = occupants[index];
    const ImVec2 center = plc::LocalToWorld(box, {
        (box.size.width - grid_width) / 2 + cell_width / 2 +
            (index % columns) * (cell_width + kGap),
        (box.size.height - grid_height) / 2 + cell_height / 2 +
            (index / columns) * (cell_height + kGap)});
    const plc::Size size = plc::GetComponentDisplaySize(*piece);
    piece->position = {center.x - size.width / 2, center.y - size.height / 2};
    piece->internalStates[plc::state_keys::kIsStuckBox] = 1;
    piece->internalStates[plc::state_keys::kBoxId] =
        static_cast<float>(box.instanceId);
    piece->internalStates[plc::state_keys::kBoxSlot] = static_cast<float>(index + 1);
    piece->internalStates[plc::state_keys::kVelX] = 0;
    piece->internalStates[plc::state_keys::kVelY] = 0;
  }
  return true;
}

void UpdateWarehouseWorkpieces(std::vector<plc::PlacedComponent>* parts,
                               bool capture_free_workpieces) {
  if (!parts)
    return;
  // Translate the warehouse base before updating its stored occupants.
  // Delta offsets preserve manual placement and avoid accumulating motion.
  for (auto& warehouse : *parts) {
    if (!plc::IsWarehouseType(warehouse.type))
      continue;
    if (warehouse.type == plc::ComponentType::kDrivenWarehouse) continue;
    const auto* cylinder = Find(*parts, warehouse.warehouse_cylinder_id);
    plc::Position offset{0, 0};
    if (cylinder && (cylinder->type == plc::ComponentType::CYLINDER ||
                     cylinder->type == plc::ComponentType::kSensorCylinder)) {
      const float fraction = std::clamp(
          ReadPneumaticState(*cylinder, "position") / kCylinderStroke, 0.0f,
          1.0f);
      const ImVec2 start = plc::LocalToWorld(*cylinder, {kCylinderStroke, 0});
      const ImVec2 end = plc::LocalToWorld(*cylinder, {0, 0});
      const float distance =
          fraction * warehouse.warehouse_travel / kCylinderStroke;
      offset = {(end.x - start.x) * distance, (end.y - start.y) * distance};
    }
    warehouse.position.x += offset.x - warehouse.warehouse_drive_offset.x;
    warehouse.position.y += offset.y - warehouse.warehouse_drive_offset.y;
    warehouse.warehouse_drive_offset = offset;
  }
  for (auto& piece : *parts) {
    if (!plc::IsWorkpiece(piece.type))
      continue;
    const auto* owner = Find(*parts, piece.carry_state.owner_id);
    const bool manual = ReadPneumaticState(piece, "is_manual_drag") > 0.5f;
    if (manual) {
      piece.carry_state = {};
      continue;
    }
    if (owner && IsGravityFeedSource(owner->type)) continue;
    if (piece.carry_state.slot_id >= 1) {
      if (owner && plc::IsWarehouseType(owner->type) && !manual &&
          piece.carry_state.slot_id <= 6) {
        PlaceInSlot(*owner, &piece);
        continue;
      }
      piece.carry_state = {};
    }
    if (!capture_free_workpieces || piece.carry_state.owner_id >= 0)
      continue;
    for (const auto& warehouse : *parts) {
      if (!plc::IsWarehouseType(warehouse.type))
        continue;
      ImVec2 center =
          plc::WorldToLocal(warehouse, plc::GetComponentWorldCenter(piece));
      if (warehouse.type == plc::ComponentType::kDrivenWarehouse) {
        center.x -= 190.0f + ReadPneumaticState(warehouse, "position");
      }
      const int rotation = plc::NormalizeRotationQuadrants(
          piece.rotation_quadrants - warehouse.rotation_quadrants);
      const plc::Size size =
          rotation % 2 ? plc::Size{piece.size.height, piece.size.width}
                       : piece.size;
      if (size.width > 64 || size.height > 80)
        continue;
      const int column = center.x >= 80 ? 1 : 0;
      if (center.x + size.width / 2 <= 10 + column * 70 ||
          center.x - size.width / 2 >= 80 + column * 70 ||
          center.y + size.height / 2 <= 10 || center.y - size.height / 2 >= 90)
        continue;
      for (int row = 0; row < 3; ++row) {
        const int slot = row * 2 + column + 1;
        if (!CanInteractAtHeight(WarehouseSlotHeight(warehouse, slot),
                                 ResolveContactHeight(piece)))
          continue;
        const bool occupied =
            std::any_of(parts->begin(), parts->end(), [&](const auto& other) {
              return other.carry_state.owner_id == warehouse.instanceId &&
                     other.carry_state.slot_id == slot;
            });
        if (occupied)
          break;
        piece.carry_state.owner_id = warehouse.instanceId;
        piece.carry_state.slot_id = slot;
        piece.carry_state.release_source_id = -1;
        PlaceInSlot(warehouse, &piece);
        break;
      }
      if (!capture_free_workpieces || piece.carry_state.owner_id >= 0)
        break;
    }
    const auto* released_from = Find(*parts, piece.carry_state.release_source_id);
    if (!released_from || !IsGravityFeedSource(released_from->type))
      piece.carry_state.release_source_id = -1;
  }
}
void UpdateFiberSensors(std::vector<plc::PlacedComponent>* parts,
                        const std::vector<plc::Wire>& wires,
                        const std::map<plc::PortRef, float>& voltages) {
  if (!parts)
    return;
  for (auto& amp : *parts) {
    if (amp.type != plc::ComponentType::kFiberAmplifier)
      continue;
    amp.fiber_state = {};
    if (!Powered(amp, voltages))
      continue;
    amp.fiber_state.fault = 2;
    const auto* tx = Find(
        *parts, Linked(wires, amp.instanceId, 3, plc::WireKind::kOptical, 0));
    const auto* rx = Find(
        *parts, Linked(wires, amp.instanceId, 4, plc::WireKind::kOptical, 0));
    if (!tx || !rx || tx->type != plc::ComponentType::kFiberTransmitter ||
        rx->type != plc::ComponentType::kFiberReceiver)
      continue;
    amp.fiber_state.fault = 3;
    if (!IsValidFiberSettings(amp.fiber_settings) ||
        !CanInteractAtHeight(*tx, *rx))
      continue;
    // Both heads emit/look along their local negative X axis.
    const ImVec2 start = plc::LocalToWorld(*tx, {5, 15});
    const ImVec2 end = plc::LocalToWorld(*rx, {5, 15});
    const ImVec2 tx_forward = plc::LocalToWorld(*tx, {-5, 15});
    const ImVec2 rx_forward = plc::LocalToWorld(*rx, {-5, 15});
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double distance = std::hypot(dx, dy);
    const double tolerance = amp.fiber_settings.alignment_tolerance;
    if (distance < 1 || distance > amp.fiber_settings.max_distance ||
        dx * (tx_forward.x - start.x) + dy * (tx_forward.y - start.y) <= 0 ||
        -dx * (rx_forward.x - end.x) - dy * (rx_forward.y - end.y) <= 0 ||
        std::abs(dx * (tx_forward.y - start.y) -
                 dy * (tx_forward.x - start.x)) /
                10 >
            tolerance ||
        std::abs(dx * (rx_forward.y - end.y) - dy * (rx_forward.x - end.x)) /
                10 >
            tolerance)
      continue;
    amp.fiber_state.fault = 0;
    for (const auto& piece : *parts) {
      if (plc::IsWorkpiece(piece.type) && CanInteractAtHeight(*tx, piece) &&
          RayHits(plc::GetAabb(piece), start, end,
                  static_cast<float>(amp.fiber_settings.beam_width / 2))) {
        amp.fiber_state.blocked = true;
        break;
      }
    }
    amp.fiber_state.received = amp.fiber_state.blocked ? 0 : 1000;
    amp.fiber_state.output = amp.fiber_settings.dark_on
                                 ? amp.fiber_state.blocked
                                 : !amp.fiber_state.blocked;
  }
}
}  // namespace plc_emulator
