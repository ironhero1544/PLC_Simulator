// physics_box2d.cpp
//
// Box2D integration for mechanical collisions and sensor queries.

#include "plc_emulator/core/application.h"

#include "box2d/box2d.h"
#include "plc_emulator/application_physics/physics_helpers.h"
#include "plc_emulator/components/state_keys.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace plc {
namespace {

constexpr float kBox2dScale = 0.01f;
constexpr float kMinHalfExtent = 0.001f;
constexpr float kWorkpieceCenterRadius = 0.5f;
constexpr float kRodTipRadius = 1.0f;
constexpr float kRingCenterX = 30.0f;
constexpr float kRingCenterY = 28.0f;
constexpr float kRingDetectRadius = 18.0f;

enum class ShapeRole : uint8_t {
  kWorkpieceBox = 0,
  kWorkpieceCenter,
  kConveyor,
  kCylinderBody,
  kRodSweep,
  kProcessingHead,
  kBox,
  kRingSensor,
  kInductiveSensor,
  kRodTip
};

constexpr uint64_t kCategoryWorkpieceBox = 1ull << 0;
constexpr uint64_t kCategoryWorkpieceCenter = 1ull << 1;
constexpr uint64_t kCategoryConveyor = 1ull << 2;
constexpr uint64_t kCategoryCylinderBody = 1ull << 3;
constexpr uint64_t kCategoryRodSweep = 1ull << 4;
constexpr uint64_t kCategoryProcessingHead = 1ull << 5;
constexpr uint64_t kCategoryBox = 1ull << 6;
constexpr uint64_t kCategoryRingSensor = 1ull << 7;
constexpr uint64_t kCategoryInductiveSensor = 1ull << 8;
constexpr uint64_t kCategoryRodTip = 1ull << 9;

struct ShapeEntry {
  uint64_t key = 0;
  int component_index = -1;
  ComponentType type = ComponentType::PLC;
  ShapeRole role = ShapeRole::kWorkpieceBox;
  uint64_t category = 0;
  b2BodyId body = b2_nullBodyId;
  b2ShapeId shape = b2_nullShapeId;
  b2Vec2 center = {0.0f, 0.0f};
  float half_w = 0.0f;
  float half_h = 0.0f;
  float radius = 0.0f;
  int last_seen = 0;
  bool is_circle = false;
};

struct Box2dContext {
  const Application* owner = nullptr;
  b2WorldId world = b2_nullWorldId;
  int frame = 0;
  std::vector<ShapeEntry> entries;
  std::unordered_map<uint64_t, size_t> lookup;
};

Box2dContext& GetBox2dContext() {
  static Box2dContext context;
  return context;
}

std::mutex& GetBox2dMutex() {
  static std::mutex mutex;
  return mutex;
}

uint64_t MakeKey(int instance_id, ShapeRole role) {
  return (static_cast<uint64_t>(static_cast<uint32_t>(instance_id)) << 8) |
         static_cast<uint64_t>(role);
}

void* EncodeUserData(size_t index) {
  return reinterpret_cast<void*>(static_cast<uintptr_t>(index + 1u));
}

size_t DecodeUserData(void* user_data) {
  if (!user_data) {
    return static_cast<size_t>(-1);
  }
  return static_cast<size_t>(reinterpret_cast<uintptr_t>(user_data)) - 1u;
}

float ClampHalfExtent(float value) {
  return (value < kMinHalfExtent) ? kMinHalfExtent : value;
}

b2Vec2 ToB2(ImVec2 value) {
  return {value.x * kBox2dScale, value.y * kBox2dScale};
}

b2AABB ToB2Aabb(const Aabb& box) {
  b2AABB out;
  out.lowerBound = {box.min_x * kBox2dScale, box.min_y * kBox2dScale};
  out.upperBound = {box.max_x * kBox2dScale, box.max_y * kBox2dScale};
  return out;
}

bool EnsureWorld(Box2dContext& ctx, const Application* owner) {
  if (ctx.owner != owner) {
    if (b2World_IsValid(ctx.world)) {
      b2DestroyWorld(ctx.world);
    }
    ctx.entries.clear();
    ctx.lookup.clear();
    ctx.world = b2_nullWorldId;
    ctx.frame = 0;
    ctx.owner = owner;
  }

  if (!b2World_IsValid(ctx.world)) {
    b2WorldDef def = b2DefaultWorldDef();
    def.gravity = {0.0f, 0.0f};
    def.enableSleep = false;
    def.enableContinuous = true;
    ctx.world = b2CreateWorld(&def);
  }

  return b2World_IsValid(ctx.world);
}

void ReserveEntries(Box2dContext& ctx, size_t count) {
  if (ctx.entries.capacity() < count) {
    ctx.entries.reserve(count);
  }
  if (ctx.lookup.bucket_count() < count) {
    ctx.lookup.reserve(count);
  }
}

void CreateBoxEntry(Box2dContext& ctx,
                    uint64_t key,
                    int component_index,
                    ComponentType type,
                    ShapeRole role,
                    uint64_t category,
                    b2Vec2 center,
                    float half_w,
                    float half_h) {
  b2BodyDef body_def = b2DefaultBodyDef();
  body_def.type = b2_kinematicBody;
  body_def.position = center;
  b2BodyId body = b2CreateBody(ctx.world, &body_def);

  b2ShapeDef shape_def = b2DefaultShapeDef();
  shape_def.isSensor = true;
  shape_def.filter.categoryBits = category;
  shape_def.filter.maskBits = UINT64_MAX;

  b2Polygon polygon = b2MakeBox(half_w, half_h);
  b2ShapeId shape = b2CreatePolygonShape(body, &shape_def, &polygon);

  ShapeEntry entry;
  entry.key = key;
  entry.component_index = component_index;
  entry.type = type;
  entry.role = role;
  entry.category = category;
  entry.body = body;
  entry.shape = shape;
  entry.center = center;
  entry.half_w = half_w;
  entry.half_h = half_h;
  entry.radius = 0.0f;
  entry.last_seen = ctx.frame;
  entry.is_circle = false;

  ctx.entries.push_back(entry);
  size_t index = ctx.entries.size() - 1u;
  ctx.lookup[key] = index;
  b2Shape_SetUserData(shape, EncodeUserData(index));
}

void CreateCircleEntry(Box2dContext& ctx,
                       uint64_t key,
                       int component_index,
                       ComponentType type,
                       ShapeRole role,
                       uint64_t category,
                       b2Vec2 center,
                       float radius) {
  b2BodyDef body_def = b2DefaultBodyDef();
  body_def.type = b2_kinematicBody;
  body_def.position = center;
  b2BodyId body = b2CreateBody(ctx.world, &body_def);

  b2ShapeDef shape_def = b2DefaultShapeDef();
  shape_def.isSensor = true;
  shape_def.filter.categoryBits = category;
  shape_def.filter.maskBits = UINT64_MAX;

  b2Circle circle;
  circle.center = {0.0f, 0.0f};
  circle.radius = radius;
  b2ShapeId shape = b2CreateCircleShape(body, &shape_def, &circle);

  ShapeEntry entry;
  entry.key = key;
  entry.component_index = component_index;
  entry.type = type;
  entry.role = role;
  entry.category = category;
  entry.body = body;
  entry.shape = shape;
  entry.center = center;
  entry.half_w = 0.0f;
  entry.half_h = 0.0f;
  entry.radius = radius;
  entry.last_seen = ctx.frame;
  entry.is_circle = true;

  ctx.entries.push_back(entry);
  size_t index = ctx.entries.size() - 1u;
  ctx.lookup[key] = index;
  b2Shape_SetUserData(shape, EncodeUserData(index));
}

void SyncBoxEntry(Box2dContext& ctx,
                  int instance_id,
                  int component_index,
                  ComponentType type,
                  ShapeRole role,
                  uint64_t category,
                  const Aabb& box) {
  uint64_t key = MakeKey(instance_id, role);
  float half_w =
      ClampHalfExtent((box.max_x - box.min_x) * 0.5f * kBox2dScale);
  float half_h =
      ClampHalfExtent((box.max_y - box.min_y) * 0.5f * kBox2dScale);
  b2Vec2 center = {(box.min_x + box.max_x) * 0.5f * kBox2dScale,
                   (box.min_y + box.max_y) * 0.5f * kBox2dScale};

  auto it = ctx.lookup.find(key);
  if (it == ctx.lookup.end()) {
    CreateBoxEntry(ctx, key, component_index, type, role, category, center,
                   half_w, half_h);
    return;
  }

  ShapeEntry& entry = ctx.entries[it->second];
  entry.component_index = component_index;
  entry.type = type;
  entry.last_seen = ctx.frame;

  if (entry.category != category) {
    b2Filter filter = b2Shape_GetFilter(entry.shape);
    filter.categoryBits = category;
    filter.maskBits = UINT64_MAX;
    b2Shape_SetFilter(entry.shape, filter);
    entry.category = category;
  }

  if (std::abs(entry.center.x - center.x) > 1e-4f ||
      std::abs(entry.center.y - center.y) > 1e-4f) {
    b2Body_SetTransform(entry.body, center, b2MakeRot(0.0f));
    entry.center = center;
  }

  if (std::abs(entry.half_w - half_w) > 1e-4f ||
      std::abs(entry.half_h - half_h) > 1e-4f) {
    b2Polygon polygon = b2MakeBox(half_w, half_h);
    b2Shape_SetPolygon(entry.shape, &polygon);
    entry.half_w = half_w;
    entry.half_h = half_h;
  }

  b2Shape_SetUserData(entry.shape, EncodeUserData(it->second));
}

void SyncCircleEntry(Box2dContext& ctx,
                     int instance_id,
                     int component_index,
                     ComponentType type,
                     ShapeRole role,
                     uint64_t category,
                     ImVec2 center,
                     float radius) {
  uint64_t key = MakeKey(instance_id, role);
  float scaled_radius = ClampHalfExtent(radius * kBox2dScale);
  b2Vec2 scaled_center = ToB2(center);

  auto it = ctx.lookup.find(key);
  if (it == ctx.lookup.end()) {
    CreateCircleEntry(ctx, key, component_index, type, role, category,
                      scaled_center, scaled_radius);
    return;
  }

  ShapeEntry& entry = ctx.entries[it->second];
  entry.component_index = component_index;
  entry.type = type;
  entry.last_seen = ctx.frame;

  if (entry.category != category) {
    b2Filter filter = b2Shape_GetFilter(entry.shape);
    filter.categoryBits = category;
    filter.maskBits = UINT64_MAX;
    b2Shape_SetFilter(entry.shape, filter);
    entry.category = category;
  }

  if (std::abs(entry.center.x - scaled_center.x) > 1e-4f ||
      std::abs(entry.center.y - scaled_center.y) > 1e-4f) {
    b2Body_SetTransform(entry.body, scaled_center, b2MakeRot(0.0f));
    entry.center = scaled_center;
  }

  if (std::abs(entry.radius - scaled_radius) > 1e-4f) {
    b2Circle circle;
    circle.center = {0.0f, 0.0f};
    circle.radius = scaled_radius;
    b2Shape_SetCircle(entry.shape, &circle);
    entry.radius = scaled_radius;
  }

  b2Shape_SetUserData(entry.shape, EncodeUserData(it->second));
}

void PruneEntries(Box2dContext& ctx) {
  for (size_t i = 0; i < ctx.entries.size();) {
    if (ctx.entries[i].last_seen == ctx.frame) {
      ++i;
      continue;
    }

    if (b2Body_IsValid(ctx.entries[i].body)) {
      b2DestroyBody(ctx.entries[i].body);
    }
    ctx.lookup.erase(ctx.entries[i].key);

    size_t last = ctx.entries.size() - 1u;
    if (i != last) {
      ctx.entries[i] = ctx.entries[last];
      ctx.lookup[ctx.entries[i].key] = i;
      b2Shape_SetUserData(ctx.entries[i].shape, EncodeUserData(i));
    }
    ctx.entries.pop_back();
  }
}

void SortUnique(std::vector<int>* values) {
  if (!values || values->empty()) {
    return;
  }
  std::sort(values->begin(), values->end());
  values->erase(std::unique(values->begin(), values->end()), values->end());
}

struct IndexQueryContext {
  Box2dContext* ctx = nullptr;
  uint64_t category_mask = 0;
  std::vector<int>* indices = nullptr;
};

bool CollectIndices(b2ShapeId shapeId, void* context) {
  auto* query = static_cast<IndexQueryContext*>(context);
  if (!query || !query->ctx || !query->indices) {
    return true;
  }
  size_t entry_index = DecodeUserData(b2Shape_GetUserData(shapeId));
  if (entry_index >= query->ctx->entries.size()) {
    return true;
  }
  const ShapeEntry& entry = query->ctx->entries[entry_index];
  if ((entry.category & query->category_mask) == 0) {
    return true;
  }
  query->indices->push_back(entry.component_index);
  return true;
}

void QueryOverlaps(Box2dContext& ctx,
                   const Aabb& box,
                   uint64_t category_mask,
                   std::vector<int>* out) {
  if (!out) {
    return;
  }
  out->clear();
  if (category_mask == 0 || !b2World_IsValid(ctx.world)) {
    return;
  }

  b2QueryFilter filter = b2DefaultQueryFilter();
  filter.categoryBits = 1;
  filter.maskBits = category_mask;

  IndexQueryContext query;
  query.ctx = &ctx;
  query.category_mask = category_mask;
  query.indices = out;

  b2World_OverlapAABB(ctx.world, ToB2Aabb(box), filter, CollectIndices,
                      &query);
  SortUnique(out);
}

struct MaxIndexQueryContext {
  Box2dContext* ctx = nullptr;
  uint64_t category_mask = 0;
  int best_index = -1;
};

bool CollectMaxIndex(b2ShapeId shapeId, void* context) {
  auto* query = static_cast<MaxIndexQueryContext*>(context);
  if (!query || !query->ctx) {
    return true;
  }
  size_t entry_index = DecodeUserData(b2Shape_GetUserData(shapeId));
  if (entry_index >= query->ctx->entries.size()) {
    return true;
  }
  const ShapeEntry& entry = query->ctx->entries[entry_index];
  if ((entry.category & query->category_mask) == 0) {
    return true;
  }
  if (entry.component_index > query->best_index) {
    query->best_index = entry.component_index;
  }
  return true;
}

int QueryMaxIndex(Box2dContext& ctx, const Aabb& box, uint64_t category_mask) {
  if (category_mask == 0 || !b2World_IsValid(ctx.world)) {
    return -1;
  }

  b2QueryFilter filter = b2DefaultQueryFilter();
  filter.categoryBits = 1;
  filter.maskBits = category_mask;

  MaxIndexQueryContext query;
  query.ctx = &ctx;
  query.category_mask = category_mask;
  query.best_index = -1;

  b2World_OverlapAABB(ctx.world, ToB2Aabb(box), filter, CollectMaxIndex,
                      &query);
  return query.best_index;
}

bool BuildBeamProxy(ImVec2 center,
                    ImVec2 forward,
                    float range,
                    float half_width,
                    b2ShapeProxy* out_proxy) {
  if (!out_proxy || range <= 0.0f) {
    return false;
  }
  float len = std::sqrt(forward.x * forward.x + forward.y * forward.y);
  if (len < 1e-4f) {
    return false;
  }
  ImVec2 dir = {forward.x / len, forward.y / len};
  ImVec2 right = {-dir.y, dir.x};
  ImVec2 start = center;
  ImVec2 end = {center.x + dir.x * range, center.y + dir.y * range};

  ImVec2 p0 = {start.x - right.x * half_width,
               start.y - right.y * half_width};
  ImVec2 p1 = {start.x + right.x * half_width,
               start.y + right.y * half_width};
  ImVec2 p2 = {end.x + right.x * half_width, end.y + right.y * half_width};
  ImVec2 p3 = {end.x - right.x * half_width, end.y - right.y * half_width};

  b2Vec2 points[4] = {ToB2(p0), ToB2(p1), ToB2(p2), ToB2(p3)};
  *out_proxy = b2MakeProxy(points, 4, 0.0f);
  return true;
}

struct BeamQueryContext {
  Box2dContext* ctx = nullptr;
  int target_workpiece_index = -1;
  bool hit_workpiece = false;
  bool hit_rod_tip = false;
};

bool BeamQueryCallback(b2ShapeId shapeId, void* context) {
  auto* query = static_cast<BeamQueryContext*>(context);
  if (!query || !query->ctx) {
    return true;
  }
  size_t entry_index = DecodeUserData(b2Shape_GetUserData(shapeId));
  if (entry_index >= query->ctx->entries.size()) {
    return true;
  }
  const ShapeEntry& entry = query->ctx->entries[entry_index];

  if (entry.category == kCategoryWorkpieceCenter) {
    if (query->target_workpiece_index < 0 ||
        entry.component_index == query->target_workpiece_index) {
      query->hit_workpiece = true;
    }
  } else if (entry.category == kCategoryRodTip) {
    query->hit_rod_tip = true;
  }

  if (query->target_workpiece_index >= 0) {
    return !query->hit_workpiece;
  }
  return !(query->hit_workpiece && query->hit_rod_tip);
}

bool BeamOverlaps(Box2dContext& ctx,
                  ImVec2 center,
                  ImVec2 forward,
                  float range,
                  float half_width,
                  int target_workpiece_index,
                  bool* out_workpiece,
                  bool* out_rod_tip) {
  if (out_workpiece) {
    *out_workpiece = false;
  }
  if (out_rod_tip) {
    *out_rod_tip = false;
  }
  if (!b2World_IsValid(ctx.world)) {
    return false;
  }

  b2ShapeProxy proxy;
  if (!BuildBeamProxy(center, forward, range, half_width, &proxy)) {
    return false;
  }

  b2QueryFilter filter = b2DefaultQueryFilter();
  filter.categoryBits = 1;
  filter.maskBits = kCategoryWorkpieceCenter | kCategoryRodTip;

  BeamQueryContext query;
  query.ctx = &ctx;
  query.target_workpiece_index = target_workpiece_index;

  b2World_OverlapShape(ctx.world, &proxy, filter, BeamQueryCallback, &query);

  if (out_workpiece) {
    *out_workpiece = query.hit_workpiece;
  }
  if (out_rod_tip) {
    *out_rod_tip = query.hit_rod_tip;
  }
  return true;
}
}  // namespace

bool Application::UpdateWorkpieceInteractionsBox2d(float delta_time,
                                                   bool warmup_only) {
  std::lock_guard<std::mutex> lock(GetBox2dMutex());
  Box2dContext& ctx = GetBox2dContext();
  if (!EnsureWorld(ctx, this)) {
    return false;
  }
  ctx.frame++;

  const float kSnapDistance = 10.0f;
  const float kConveyorSpeed = 60.0f;
  const int kWorkpieceSubsteps = 4;

  std::vector<int> workpiece_indices;
  std::vector<int> conveyor_cache_index(placed_components_.size(), -1);
  std::vector<int> box_cache_index(placed_components_.size(), -1);
  std::vector<int> sensor_cache_index(placed_components_.size(), -1);

  struct SensorCache {
    int index = -1;
    ComponentType type = ComponentType::PLC;
    Aabb box{};
    ImVec2 center{};
    ImVec2 forward{};
    ImVec2 ring_center{};
    std::string snap_key;
  };

  struct ConveyorCache {
    int index = -1;
    Aabb box{};
    ImVec2 dir{};
  };

  struct CylinderCache {
    int index = -1;
    Aabb box{};
    float velocity = 0.0f;
    ImVec2 rod_tip{};
    ImVec2 rod_dir{};
    ImVec2 rod_vel{};
    Aabb rod_swept{};
    std::string snap_key;
  };

  struct ProcCylinderCache {
    int index = -1;
    Aabb head_box{};
    float center_x = 0.0f;
    float center_y = 0.0f;
    bool is_down = false;
    bool motor_on = false;
    std::string ready_key;
    std::string snap_key;
  };

  struct BoxCache {
    int index = -1;
    Aabb box{};
  };

  std::vector<SensorCache> sensor_caches;
  std::vector<ConveyorCache> conveyor_caches;
  std::vector<CylinderCache> cylinder_caches;
  std::vector<ProcCylinderCache> proc_cylinder_caches;
  std::vector<BoxCache> box_caches;

  workpiece_indices.reserve(placed_components_.size());
  sensor_caches.reserve(placed_components_.size());
  conveyor_caches.reserve(placed_components_.size());
  cylinder_caches.reserve(placed_components_.size());
  proc_cylinder_caches.reserve(placed_components_.size());
  box_caches.reserve(placed_components_.size());

  const int substeps = kWorkpieceSubsteps;
  const float step_dt =
      substeps > 0 ? delta_time / static_cast<float>(substeps) : 0.0f;

  for (size_t i = 0; i < placed_components_.size(); ++i) {
    auto& comp = placed_components_[i];
    if (IsWorkpiece(comp.type)) {
      workpiece_indices.push_back(static_cast<int>(i));
      continue;
    }

    switch (comp.type) {
      case ComponentType::CONVEYOR: {
        bool active =
            comp.internalStates.count(state_keys::kMotorActive) &&
            comp.internalStates.at(state_keys::kMotorActive) > 0.5f;
        if (!active) {
          break;
        }
        ConveyorCache cache;
        cache.index = static_cast<int>(i);
        cache.box = GetAabb(comp);
        cache.dir = LocalDirToWorld(comp, ImVec2(1.0f, 0.0f));
        conveyor_cache_index[i] = static_cast<int>(conveyor_caches.size());
        conveyor_caches.push_back(cache);
        break;
      }
      case ComponentType::CYLINDER: {
        CylinderCache cache;
        cache.index = static_cast<int>(i);
        cache.box = GetAabb(comp);
        cache.velocity =
            comp.internalStates.count(state_keys::kVelocity)
                ? comp.internalStates.at(state_keys::kVelocity)
                : 0.0f;
        cache.rod_tip = GetCylinderRodTipWorld(comp);
        cache.rod_dir = LocalDirToWorld(comp, ImVec2(-1.0f, 0.0f));
        cache.rod_vel =
            ImVec2(cache.rod_dir.x * cache.velocity,
                   cache.rod_dir.y * cache.velocity);
        ImVec2 sweep(cache.rod_vel.x * step_dt, cache.rod_vel.y * step_dt);
        const float kRodHitLeft = 12.0f;
        const float kRodHitRight = 20.0f;
        float rod_radius = std::max(kRodHitLeft, kRodHitRight);
        float rod_min_x =
            std::min(cache.rod_tip.x, cache.rod_tip.x + sweep.x) -
            rod_radius;
        float rod_max_x =
            std::max(cache.rod_tip.x, cache.rod_tip.x + sweep.x) +
            rod_radius;
        float rod_min_y =
            std::min(cache.rod_tip.y, cache.rod_tip.y + sweep.y) -
            rod_radius;
        float rod_max_y =
            std::max(cache.rod_tip.y, cache.rod_tip.y + sweep.y) +
            rod_radius;
        cache.rod_swept = {rod_min_x, rod_min_y, rod_max_x, rod_max_y};
        cache.snap_key =
            std::string("snap_lock_cyl_") + std::to_string(comp.instanceId);
        cylinder_caches.push_back(cache);
        break;
      }
      case ComponentType::PROCESSING_CYLINDER: {
        constexpr float kProcDrillUp = 30.0f;
        constexpr float kProcDrillDown = 15.0f;
        constexpr float kProcEpsilon = 0.1f;
        ProcCylinderCache cache;
        cache.index = static_cast<int>(i);
        Aabb cyl_box = GetAabb(comp);
        float pos_val =
            comp.internalStates.count(state_keys::kPosition)
                ? comp.internalStates.at(state_keys::kPosition)
                : kProcDrillUp;
        cache.is_down = pos_val <= (kProcDrillDown + kProcEpsilon);
        cache.motor_on =
            comp.internalStates.count(state_keys::kMotorOn) &&
            comp.internalStates.at(state_keys::kMotorOn) > 0.5f;
        cache.center_x = cyl_box.min_x + 60.0f;
        cache.center_y = cyl_box.min_y + 90.0f;
        float head_radius = 30.0f;
        cache.head_box.min_x = cache.center_x - head_radius;
        cache.head_box.max_x = cache.center_x + head_radius;
        cache.head_box.min_y = cache.center_y - head_radius;
        cache.head_box.max_y = cache.center_y + head_radius;
        cache.ready_key =
            std::string("proc_ready_") + std::to_string(comp.instanceId);
        cache.snap_key =
            std::string("snap_lock_proc_") + std::to_string(comp.instanceId);
        proc_cylinder_caches.push_back(cache);
        break;
      }
      case ComponentType::BOX: {
        BoxCache cache;
        cache.index = static_cast<int>(i);
        cache.box = GetAabb(comp);
        box_cache_index[i] = static_cast<int>(box_caches.size());
        box_caches.push_back(cache);
        break;
      }
      case ComponentType::RING_SENSOR:
      case ComponentType::INDUCTIVE_SENSOR: {
        comp.internalStates[state_keys::kIsDetected] = 0.0f;
        SensorCache cache;
        cache.index = static_cast<int>(i);
        cache.type = comp.type;
        cache.box = GetAabb(comp);
        if (comp.type == ComponentType::RING_SENSOR) {
          cache.ring_center =
              LocalToWorld(comp, ImVec2(kRingCenterX, kRingCenterY));
          cache.snap_key =
              std::string("snap_lock_ring_") +
              std::to_string(comp.instanceId);
        } else {
          cache.center = GetSensorCenterWorld(comp);
          cache.forward = LocalDirToWorld(comp, ImVec2(0.0f, -1.0f));
        }
        sensor_cache_index[i] = static_cast<int>(sensor_caches.size());
        sensor_caches.push_back(cache);
        break;
      }
      default:
        break;
    }
  }

  size_t shape_estimate = placed_components_.size() * 4u + 16u;
  ReserveEntries(ctx, shape_estimate);

  for (int workpiece_index : workpiece_indices) {
    const auto& workpiece = placed_components_[workpiece_index];
    Aabb workpiece_box = GetAabb(workpiece);
    SyncBoxEntry(ctx, workpiece.instanceId, workpiece_index, workpiece.type,
                 ShapeRole::kWorkpieceBox, kCategoryWorkpieceBox,
                 workpiece_box);
    ImVec2 center(workpiece.position.x + workpiece.size.width * 0.5f,
                  workpiece.position.y + workpiece.size.height * 0.5f);
    SyncCircleEntry(ctx, workpiece.instanceId, workpiece_index, workpiece.type,
                    ShapeRole::kWorkpieceCenter,
                    kCategoryWorkpieceCenter, center,
                    kWorkpieceCenterRadius);
  }

  for (const auto& conveyor : conveyor_caches) {
    const auto& comp = placed_components_[conveyor.index];
    SyncBoxEntry(ctx, comp.instanceId, conveyor.index, comp.type,
                 ShapeRole::kConveyor, kCategoryConveyor, conveyor.box);
  }

  for (const auto& cylinder : cylinder_caches) {
    const auto& comp = placed_components_[cylinder.index];
    SyncBoxEntry(ctx, comp.instanceId, cylinder.index, comp.type,
                 ShapeRole::kCylinderBody, kCategoryCylinderBody,
                 cylinder.box);
    SyncBoxEntry(ctx, comp.instanceId, cylinder.index, comp.type,
                 ShapeRole::kRodSweep, kCategoryRodSweep, cylinder.rod_swept);
    SyncCircleEntry(ctx, comp.instanceId, cylinder.index, comp.type,
                    ShapeRole::kRodTip, kCategoryRodTip, cylinder.rod_tip,
                    kRodTipRadius);
  }

  for (const auto& proc : proc_cylinder_caches) {
    const auto& comp = placed_components_[proc.index];
    SyncBoxEntry(ctx, comp.instanceId, proc.index, comp.type,
                 ShapeRole::kProcessingHead, kCategoryProcessingHead,
                 proc.head_box);
  }

  for (const auto& box : box_caches) {
    const auto& comp = placed_components_[box.index];
    SyncBoxEntry(ctx, comp.instanceId, box.index, comp.type, ShapeRole::kBox,
                 kCategoryBox, box.box);
  }

  for (const auto& sensor : sensor_caches) {
    const auto& comp = placed_components_[sensor.index];
    uint64_t category = sensor.type == ComponentType::RING_SENSOR
                            ? kCategoryRingSensor
                            : kCategoryInductiveSensor;
    ShapeRole role = sensor.type == ComponentType::RING_SENSOR
                         ? ShapeRole::kRingSensor
                         : ShapeRole::kInductiveSensor;
    SyncBoxEntry(ctx, comp.instanceId, sensor.index, comp.type, role, category,
                 sensor.box);
  }

  PruneEntries(ctx);

  if (warmup_only || workpiece_indices.empty()) {
    return true;
  }

  auto is_in_view = [&](const Aabb& box) -> bool {
    if (!physics_lod_state_.has_view) {
      return true;
    }
    float zoom = physics_lod_state_.zoom;
    if (zoom < 0.05f) {
      zoom = 0.05f;
    }
    float margin = 300.0f / zoom;
    if (box.max_x < physics_lod_state_.view_min_x - margin ||
        box.min_x > physics_lod_state_.view_max_x + margin ||
        box.max_y < physics_lod_state_.view_min_y - margin ||
        box.min_y > physics_lod_state_.view_max_y + margin) {
      return false;
    }
    return true;
  };

  const float kSleepVelocity = 0.05f;
  const uint64_t kSleepCategories =
      kCategoryConveyor | kCategoryCylinderBody | kCategoryRodSweep |
      kCategoryProcessingHead | kCategoryBox | kCategoryRingSensor |
      kCategoryInductiveSensor;

  std::vector<uint8_t> workpiece_sleep(workpiece_indices.size(), 0);
  std::vector<int> sleep_hits;
  sleep_hits.reserve(8);

  for (size_t i = 0; i < workpiece_indices.size(); ++i) {
    int workpiece_index = workpiece_indices[i];
    auto& workpiece = placed_components_[workpiece_index];
    bool manual_drag =
        workpiece.internalStates.count(state_keys::kIsManualDrag) &&
        workpiece.internalStates.at(state_keys::kIsManualDrag) > 0.5f;
    if (manual_drag) {
      continue;
    }
    bool stuck_box =
        workpiece.internalStates.count(state_keys::kIsStuckBox) &&
        workpiece.internalStates.at(state_keys::kIsStuckBox) > 0.5f;
    if (stuck_box) {
      continue;
    }
    float vel_x = workpiece.internalStates.count(state_keys::kVelX)
                      ? workpiece.internalStates.at(state_keys::kVelX)
                      : 0.0f;
    float vel_y = workpiece.internalStates.count(state_keys::kVelY)
                      ? workpiece.internalStates.at(state_keys::kVelY)
                      : 0.0f;
    if (std::abs(vel_x) > kSleepVelocity || std::abs(vel_y) > kSleepVelocity) {
      continue;
    }
    Aabb workpiece_box = GetAabb(workpiece);
    if (is_in_view(workpiece_box)) {
      continue;
    }
    Aabb detection_box = GetWorkpieceDetectionAabb(workpiece);
    QueryOverlaps(ctx, detection_box, kSleepCategories, &sleep_hits);
    if (!sleep_hits.empty()) {
      continue;
    }
    workpiece_sleep[i] = 1;
    if (workpiece.internalStates.count(state_keys::kVelX)) {
      workpiece.internalStates[state_keys::kVelX] = 0.0f;
    }
    if (workpiece.internalStates.count(state_keys::kVelY)) {
      workpiece.internalStates[state_keys::kVelY] = 0.0f;
    }
    if (workpiece.internalStates.count(state_keys::kIsContacted)) {
      workpiece.internalStates[state_keys::kIsContacted] = 0.0f;
    }
  }

  auto update_workpiece_shapes = [&](const PlacedComponent& workpiece,
                                     int index) {
    Aabb workpiece_box = GetAabb(workpiece);
    SyncBoxEntry(ctx, workpiece.instanceId, index, workpiece.type,
                 ShapeRole::kWorkpieceBox, kCategoryWorkpieceBox,
                 workpiece_box);
    ImVec2 center(workpiece.position.x + workpiece.size.width * 0.5f,
                  workpiece.position.y + workpiece.size.height * 0.5f);
    SyncCircleEntry(ctx, workpiece.instanceId, index, workpiece.type,
                    ShapeRole::kWorkpieceCenter,
                    kCategoryWorkpieceCenter, center,
                    kWorkpieceCenterRadius);
  };

  std::vector<int> sensor_detection_hits;
  std::vector<int> sensor_body_hits;
  sensor_detection_hits.reserve(16);
  sensor_body_hits.reserve(16);

  auto apply_sensor_detection = [&](PlacedComponent& workpiece,
                                    int workpiece_index,
                                    bool allow_snap) {
    Aabb workpiece_detection = GetWorkpieceDetectionAabb(workpiece);
    Aabb workpiece_body = GetAabb(workpiece);
    QueryOverlaps(ctx, workpiece_detection,
                  kCategoryRingSensor | kCategoryInductiveSensor,
                  &sensor_detection_hits);
    if (sensor_detection_hits.empty()) {
      return;
    }
    QueryOverlaps(ctx, workpiece_body,
                  kCategoryRingSensor | kCategoryInductiveSensor,
                  &sensor_body_hits);

    for (int sensor_index : sensor_detection_hits) {
      int cache_idx = sensor_cache_index[sensor_index];
      if (cache_idx < 0) {
        continue;
      }
      const auto& cache = sensor_caches[cache_idx];
      auto& sensor = placed_components_[sensor_index];
      const bool is_ring = cache.type == ComponentType::RING_SENSOR;
      const bool is_inductive = cache.type == ComponentType::INDUCTIVE_SENSOR;
      bool overlaps_body =
          std::binary_search(sensor_body_hits.begin(), sensor_body_hits.end(),
                             sensor_index);

      if (is_ring) {
        const std::string& snap_key = cache.snap_key;
        if (!overlaps_body) {
          workpiece.internalStates[snap_key] = 0.0f;
        }
        ImVec2 ring_center = cache.ring_center;
        ImVec2 work_center = {
            workpiece.position.x + workpiece.size.width * 0.5f,
            workpiece.position.y + workpiece.size.height * 0.5f};
        float dx = work_center.x - ring_center.x;
        float dy = work_center.y - ring_center.y;
        if (dx * dx + dy * dy >
            kRingDetectRadius * kRingDetectRadius) {
          continue;
        }
        sensor.internalStates[state_keys::kIsDetected] = 1.0f;
        if (allow_snap) {
          bool snap_locked =
              workpiece.internalStates.count(snap_key) &&
              workpiece.internalStates.at(snap_key) > 0.5f;
          if (!snap_locked) {
            float delta_x =
                (workpiece.position.x + workpiece.size.width * 0.5f) -
                ring_center.x;
            float delta_y =
                (workpiece.position.y + workpiece.size.height * 0.5f) -
                ring_center.y;
            if (std::abs(delta_x) <= kSnapDistance &&
                std::abs(delta_y) <= kSnapDistance) {
              workpiece.position.x =
                  ring_center.x - workpiece.size.width * 0.5f;
              workpiece.position.y =
                  ring_center.y - workpiece.size.height * 0.5f;
              workpiece.internalStates[state_keys::kVelX] = 0.0f;
              workpiece.internalStates[state_keys::kVelY] = 0.0f;
              workpiece.internalStates[snap_key] = 1.0f;
            }
          }
        }
        continue;
      }

      if (is_inductive) {
        bool is_metal =
            workpiece.type == ComponentType::WORKPIECE_METAL ||
            (workpiece.internalStates.count(state_keys::kIsMetal) &&
             workpiece.internalStates.at(state_keys::kIsMetal) > 0.5f);
        if (!is_metal) {
          continue;
        }
        const float kDetectionRange = 75.0f;
        const float kBeamHalfWidth = 15.0f;
        bool hit_workpiece = false;
        BeamOverlaps(ctx, cache.center, cache.forward, kDetectionRange,
                     kBeamHalfWidth, workpiece_index, &hit_workpiece, nullptr);
        if (!hit_workpiece) {
          continue;
        }
        sensor.internalStates[state_keys::kIsDetected] = 1.0f;
      }
    }
  };

  std::vector<int> cylinder_body_hits;
  std::vector<int> rod_sweep_hits;
  std::vector<int> processing_hits;
  cylinder_body_hits.reserve(16);
  rod_sweep_hits.reserve(16);
  processing_hits.reserve(8);

  for (int step = 0; step < substeps; ++step) {
    for (size_t workpiece_order = 0; workpiece_order < workpiece_indices.size();
         ++workpiece_order) {
      if (workpiece_sleep[workpiece_order]) {
        continue;
      }
      int workpiece_index = workpiece_indices[workpiece_order];
      auto& workpiece = placed_components_[workpiece_index];
      update_workpiece_shapes(workpiece, workpiece_index);

      bool manual_drag =
          workpiece.internalStates.count(state_keys::kIsManualDrag) &&
          workpiece.internalStates.at(state_keys::kIsManualDrag) > 0.5f;
      if (manual_drag) {
        apply_sensor_detection(workpiece, workpiece_index, false);
        update_workpiece_shapes(workpiece, workpiece_index);
        continue;
      }

      bool stuck_box =
          workpiece.internalStates.count(state_keys::kIsStuckBox) &&
          workpiece.internalStates.at(state_keys::kIsStuckBox) > 0.5f;
      if (stuck_box) {
        workpiece.internalStates[state_keys::kVelX] = 0.0f;
        workpiece.internalStates[state_keys::kVelY] = 0.0f;
        update_workpiece_shapes(workpiece, workpiece_index);
        continue;
      }

      workpiece.internalStates[state_keys::kIsContacted] = 0.0f;
      float vel_x = workpiece.internalStates.count(state_keys::kVelX)
                        ? workpiece.internalStates.at(state_keys::kVelX)
                        : 0.0f;
      float vel_y = workpiece.internalStates.count(state_keys::kVelY)
                        ? workpiece.internalStates.at(state_keys::kVelY)
                        : 0.0f;

      Aabb workpiece_box = GetAabb(workpiece);
      bool on_conveyor = false;
      bool pushed_by_cylinder = false;

      int conveyor_index =
          QueryMaxIndex(ctx, workpiece_box, kCategoryConveyor);
      if (conveyor_index >= 0) {
        int cache_idx = conveyor_cache_index[conveyor_index];
        if (cache_idx >= 0) {
          const auto& conveyor = conveyor_caches[cache_idx];
          on_conveyor = true;
          vel_x = conveyor.dir.x * kConveyorSpeed;
          vel_y = conveyor.dir.y * kConveyorSpeed;
        }
      }

      if (!on_conveyor) {
        vel_x = 0.0f;
        vel_y = 0.0f;
      }

      QueryOverlaps(ctx, workpiece_box, kCategoryCylinderBody,
                    &cylinder_body_hits);
      QueryOverlaps(ctx, workpiece_box, kCategoryRodSweep, &rod_sweep_hits);

      for (const auto& cylinder : cylinder_caches) {
        float velocity = cylinder.velocity;
        const std::string& snap_key = cylinder.snap_key;
        const float kSnapReleaseVelocity = 0.5f;
        bool body_overlap =
            std::binary_search(cylinder_body_hits.begin(),
                               cylinder_body_hits.end(), cylinder.index);
        bool rod_overlap =
            std::binary_search(rod_sweep_hits.begin(), rod_sweep_hits.end(),
                               cylinder.index);

        if (std::abs(velocity) <= kSnapReleaseVelocity) {
          workpiece.internalStates[snap_key] = 0.0f;
          continue;
        }
        if (!body_overlap) {
          workpiece.internalStates[snap_key] = 0.0f;
        }
        if (!rod_overlap) {
          continue;
        }

        workpiece.internalStates[state_keys::kIsContacted] = 1.0f;
        ImVec2 workpiece_center(
            workpiece.position.x + workpiece.size.width * 0.5f,
            workpiece.position.y + workpiece.size.height * 0.5f);
        ImVec2 rod_tip = cylinder.rod_tip;
        ImVec2 rod_dir = cylinder.rod_dir;
        ImVec2 rod_vel = cylinder.rod_vel;
        ImVec2 to_work(workpiece_center.x - rod_tip.x,
                       workpiece_center.y - rod_tip.y);
        ImVec2 rod_perp(-rod_dir.y, rod_dir.x);
        float lateral = Dot(to_work, rod_perp);
        bool snap_locked =
            workpiece.internalStates.count(snap_key) &&
            workpiece.internalStates.at(snap_key) > 0.5f;
        bool snapped_to_cylinder = false;
        if (!snap_locked && std::abs(lateral) <= kSnapDistance) {
          workpiece_center.x -= rod_perp.x * lateral;
          workpiece_center.y -= rod_perp.y * lateral;
          workpiece.position.x =
              workpiece_center.x - workpiece.size.width * 0.5f;
          workpiece.position.y =
              workpiece_center.y - workpiece.size.height * 0.5f;
          to_work.x = workpiece_center.x - rod_tip.x;
          to_work.y = workpiece_center.y - rod_tip.y;
          vel_x = 0.0f;
          vel_y = 0.0f;
          snapped_to_cylinder = true;
          workpiece.internalStates[snap_key] = 1.0f;
        }
        const float kContactGap = 1.0f;
        if (!snapped_to_cylinder && std::abs(velocity) > 1.0f) {
          float proj = Dot(to_work, rod_dir);
          float sign = (velocity >= 0.0f) ? 1.0f : -1.0f;
          float half_extent =
              0.5f * (std::abs(rod_dir.x) * workpiece.size.width +
                      std::abs(rod_dir.y) * workpiece.size.height);
          float desired_offset = sign * (half_extent + kContactGap);
          bool in_front = (sign > 0.0f) ? (proj >= 0.0f) : (proj <= 0.0f);
          if (in_front) {
            if ((sign > 0.0f && proj < desired_offset) ||
                (sign < 0.0f && proj > desired_offset)) {
              workpiece_center.x = rod_tip.x + rod_dir.x * desired_offset;
              workpiece_center.y = rod_tip.y + rod_dir.y * desired_offset;
              workpiece.position.x =
                  workpiece_center.x - workpiece.size.width * 0.5f;
              workpiece.position.y =
                  workpiece_center.y - workpiece.size.height * 0.5f;
            }
            vel_x = rod_vel.x;
            vel_y = rod_vel.y;
            pushed_by_cylinder = true;
          }
        }
      }

      if (pushed_by_cylinder) {
        on_conveyor = false;
      }

      for (const auto& proc : proc_cylinder_caches) {
        bool is_down = proc.is_down;
        bool motor_on = proc.motor_on;
        const std::string& ready_key = proc.ready_key;
        const std::string& snap_key = proc.snap_key;
        bool ready = workpiece.internalStates.count(ready_key) &&
                     workpiece.internalStates.at(ready_key) > 0.5f;
        bool snap_locked = workpiece.internalStates.count(snap_key) &&
                           workpiece.internalStates.at(snap_key) > 0.5f;

        Aabb proc_workpiece_box = GetAabb(workpiece);
        QueryOverlaps(ctx, proc_workpiece_box, kCategoryProcessingHead,
                      &processing_hits);
        bool overlapping =
            std::binary_search(processing_hits.begin(), processing_hits.end(),
                               proc.index);

        if (overlapping && !snap_locked) {
          workpiece.internalStates[state_keys::kIsContacted] = 1.0f;
          workpiece.position.x = proc.center_x - workpiece.size.width * 0.5f;
          workpiece.position.y =
              proc.center_y - workpiece.size.height * 0.5f;
          vel_x = 0.0f;
          vel_y = 0.0f;
          ready = true;
          workpiece.internalStates[snap_key] = 1.0f;
        } else if (overlapping) {
          workpiece.internalStates[state_keys::kIsContacted] = 1.0f;
          ready = true;
        } else if (!overlapping) {
          ready = false;
        }

        if (ready && is_down) {
          if (motor_on) {
            workpiece.internalStates[state_keys::kIsProcessed] = 1.0f;
            ready = false;
          } else {
            ready = false;
          }
        }

        workpiece.internalStates[ready_key] = ready ? 1.0f : 0.0f;
      }

      int last_box_index = QueryMaxIndex(ctx, workpiece_box, kCategoryBox);
      if (last_box_index >= 0) {
        int cache_idx = box_cache_index[last_box_index];
        if (cache_idx >= 0) {
          const auto& box_cache = box_caches[cache_idx];
          workpiece.internalStates[state_keys::kIsStuckBox] = 1.0f;
          workpiece.position.x =
              GetCenterX(box_cache.box) - workpiece.size.width * 0.5f;
          workpiece.position.y =
              GetCenterY(box_cache.box) - workpiece.size.height * 0.5f;
          vel_x = 0.0f;
          vel_y = 0.0f;
          workpiece_box = GetAabb(workpiece);
        }
      }

      workpiece.position.x += vel_x * step_dt;
      workpiece.position.y += vel_y * step_dt;
      workpiece.internalStates[state_keys::kVelX] = vel_x;
      workpiece.internalStates[state_keys::kVelY] = vel_y;

      update_workpiece_shapes(workpiece, workpiece_index);
      apply_sensor_detection(workpiece, workpiece_index, true);
      update_workpiece_shapes(workpiece, workpiece_index);
    }
  }

  return true;
}

bool Application::UpdateSensorsBox2d() {
  std::lock_guard<std::mutex> lock(GetBox2dMutex());
  Box2dContext& ctx = GetBox2dContext();
  if (!EnsureWorld(ctx, this)) {
    return false;
  }

  size_t shape_estimate = placed_components_.size() * 2u + 16u;
  ReserveEntries(ctx, shape_estimate);

  for (size_t i = 0; i < placed_components_.size(); ++i) {
    const auto& comp = placed_components_[i];
    if (IsWorkpiece(comp.type)) {
      ImVec2 center(comp.position.x + comp.size.width * 0.5f,
                    comp.position.y + comp.size.height * 0.5f);
      SyncCircleEntry(ctx, comp.instanceId, static_cast<int>(i), comp.type,
                      ShapeRole::kWorkpieceCenter,
                      kCategoryWorkpieceCenter, center,
                      kWorkpieceCenterRadius);
    } else if (comp.type == ComponentType::CYLINDER) {
      ImVec2 rod_tip = GetCylinderRodTipWorld(comp);
      SyncCircleEntry(ctx, comp.instanceId, static_cast<int>(i), comp.type,
                      ShapeRole::kRodTip, kCategoryRodTip, rod_tip,
                      kRodTipRadius);
    }
  }

  for (auto& sensor : placed_components_) {
    if (sensor.type != ComponentType::LIMIT_SWITCH &&
        sensor.type != ComponentType::SENSOR) {
      continue;
    }

    float detection_range =
        (sensor.type == ComponentType::LIMIT_SWITCH) ? 100.0f : 75.0f;
    float beam_half_width =
        (sensor.type == ComponentType::LIMIT_SWITCH) ? 10.0f : 15.0f;
    ImVec2 sensor_center =
        (sensor.type == ComponentType::LIMIT_SWITCH)
            ? GetLimitSwitchCenterWorld(sensor)
            : GetSensorCenterWorld(sensor);
    ImVec2 sensor_forward = LocalDirToWorld(sensor, ImVec2(0.0f, -1.0f));

    bool hit_workpiece = false;
    bool hit_rod_tip = false;
    BeamOverlaps(ctx, sensor_center, sensor_forward, detection_range,
                 beam_half_width, -1, &hit_workpiece, &hit_rod_tip);

    if (sensor.type == ComponentType::LIMIT_SWITCH) {
      bool is_pressed_manual =
          sensor.internalStates.count(state_keys::kIsPressedManual) &&
          sensor.internalStates.at(state_keys::kIsPressedManual) > 0.5f;
      sensor.internalStates[state_keys::kIsPressed] =
          (hit_workpiece || hit_rod_tip || is_pressed_manual) ? 1.0f : 0.0f;
    } else {
      sensor.internalStates[state_keys::kIsDetected] =
          (hit_workpiece || hit_rod_tip) ? 1.0f : 0.0f;
    }
  }

  return true;
}

}  // namespace plc
