/*
 * component_transform.h
 *
 * 컴포넌트 좌표 변환 유틸리티 선언.
 * Declarations for component coordinate transform helpers.
 */

#ifndef PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_COMPONENT_TRANSFORM_H_
#define PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_COMPONENT_TRANSFORM_H_

#include "imgui.h"
#include "plc_emulator/core/data_types.h"

namespace plc {

inline int NormalizeRotationQuadrants(int quadrants) {
  int value = quadrants % 4;
  if (value < 0) {
    value += 4;
  }
  return value;
}

inline Size GetComponentDisplaySize(const PlacedComponent& comp) {
  const int quadrants = NormalizeRotationQuadrants(comp.rotation_quadrants);
  if (quadrants % 2 != 0) {
    return {comp.size.height, comp.size.width};
  }
  return comp.size;
}

inline ImVec2 GetComponentWorldCenter(const PlacedComponent& comp) {
  const Size display = GetComponentDisplaySize(comp);
  return ImVec2(comp.position.x + display.width * 0.5f,
                comp.position.y + display.height * 0.5f);
}

inline ImVec2 RotatePoint(ImVec2 point, int quadrants) {
  const int q = NormalizeRotationQuadrants(quadrants);
  switch (q) {
    case 1:
      return ImVec2(-point.y, point.x);
    case 2:
      return ImVec2(-point.x, -point.y);
    case 3:
      return ImVec2(point.y, -point.x);
    default:
      return point;
  }
}

inline ImVec2 LocalToWorld(const PlacedComponent& comp, ImVec2 local) {
  ImVec2 center = GetComponentWorldCenter(comp);
  ImVec2 base_center(comp.size.width * 0.5f, comp.size.height * 0.5f);
  ImVec2 offset(local.x - base_center.x, local.y - base_center.y);
  if (comp.flip_x) {
    offset.x = -offset.x;
  }
  if (comp.flip_y) {
    offset.y = -offset.y;
  }
  ImVec2 rotated = RotatePoint(offset, comp.rotation_quadrants);
  return ImVec2(center.x + rotated.x, center.y + rotated.y);
}

inline ImVec2 WorldToLocal(const PlacedComponent& comp, ImVec2 world) {
  ImVec2 center = GetComponentWorldCenter(comp);
  ImVec2 offset(world.x - center.x, world.y - center.y);
  ImVec2 rotated = RotatePoint(offset, -comp.rotation_quadrants);
  if (comp.flip_x) {
    rotated.x = -rotated.x;
  }
  if (comp.flip_y) {
    rotated.y = -rotated.y;
  }
  ImVec2 base_center(comp.size.width * 0.5f, comp.size.height * 0.5f);
  return ImVec2(rotated.x + base_center.x, rotated.y + base_center.y);
}

inline bool IsPointInsideComponent(const PlacedComponent& comp,
                                   ImVec2 world) {
  ImVec2 local = WorldToLocal(comp, world);
  return local.x >= 0.0f && local.x <= comp.size.width && local.y >= 0.0f &&
         local.y <= comp.size.height;
}

}  /* namespace plc */

#endif  /* PLC_EMULATOR_INCLUDE_PLC_EMULATOR_CORE_COMPONENT_TRANSFORM_H_ */
