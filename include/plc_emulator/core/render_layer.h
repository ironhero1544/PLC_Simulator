#pragma once

#include <algorithm>
#include <numeric>
#include <vector>

#include "plc_emulator/core/data_types.h"

namespace plc_emulator {

inline bool IsRenderVisible(const plc::PlacedComponent& component) {
  return !component.render_hidden && component.render_opacity > 0.0f;
}

inline void SortRenderLayerIndices(
    const std::vector<plc::PlacedComponent>& components,
    std::vector<size_t>* order) {
  if (!order) return;
  order->resize(components.size());
  std::iota(order->begin(), order->end(), size_t{0});
  std::sort(order->begin(), order->end(), [&](size_t left, size_t right) {
    const auto& a = components[left];
    const auto& b = components[right];
    return a.z_order != b.z_order ? a.z_order < b.z_order
                                  : a.instanceId < b.instanceId;
  });
}

}  // namespace plc_emulator
