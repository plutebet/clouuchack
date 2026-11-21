#pragma once
// ---------------------------------------------------------------------------
// Map collider world — the real wall check behind the trigger.
//
// A trigger that fires when the crosshair is on a hitbox needs to know whether
// anything solid stands between the eye and that hitbox. The managed game
// objects do not answer that: szi.mow is a frustum enum (None / InFrustumOnly /
// Always) and PlayerAnimationData.IsVisible is an animation-culling flag, so
// both stay "visible" for a player who is in the frame but behind a wall. That
// is exactly the failure the old trigger had.
//
// This file does what the PC donor does: ray-casts the eye -> hit point segment
// against the level's bullet-blocking triangles (Unity layer 14) out of the
// PKM2 collider set that ships next to the binary in assets/maps.
//
//   * container: "PKM2" + uint32 count, then 48-byte triangles
//     { float3 a; float3 b; float3 c; uint32 material, collider, layer; }
//     ("PKM1" files are the older 40-byte variant without material/collider.)
//   * acceleration: median-split BVH over triangle centroids, leaf <= 8
//   * query: segment/box slab test + Moller-Trumbore triangle test, early out
//     on the first blocking hit
//
// The map itself is identified at round start by a ground probe: a player's
// feet sit 0.4 m above the floor of the map he is actually on (the donor's
// map_alignment_matches rule). Candidates come from the AABB table in
// map_bounds.hpp, the probe picks between them, and the same probe is repeated
// every few seconds so a round change or a teleport cannot keep a stale map.
// ---------------------------------------------------------------------------

#include "map_bounds.hpp"
#include "maps_embedded.hpp"
#include "trigger_params.hpp"
#include "../other/diag.hpp"
#include "../other/vector3.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>
#include <sys/resource.h>
#include <sys/syscall.h>

namespace mapworld {

// Unity layer of the meshes the game's bullets collide with. Layer 15 is
// render-only dynamic occluders and must not block a shot.
inline constexpr uint32_t kBulletLayer = trigger_params::kBulletLayer;
inline constexpr uint32_t kUnknownMaterial = 0xffffffffu;

struct Tri {
    float v[9];            // a.xyz, b.xyz, c.xyz
    uint32_t material;
    uint32_t collider;
    uint32_t layer;
};
static_assert(sizeof(Tri) == 48, "PKM triangles are 48 bytes");

struct Bounds {
    float lo[3];
    float hi[3];
};

class World {
public:
    // Nodes are public so the free-function slab test can take one; nothing
    // outside this header is supposed to touch the acceleration structure.
    struct Node {
        float lo[3];
        float hi[3];
        uint32_t first;
        uint32_t count;     // 0 = internal node
        uint32_t left;
        uint32_t right;
    };

    bool load(const char* path) {
        clear();
        std::FILE* file = std::fopen(path, "rb");
        if (!file) {
            diag::log("MAP", "cannot open %s errno=%d", path, errno);
            return false;
        }
        std::fseek(file, 0, SEEK_END);
        const long size = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);
        if (size < 8 || size > 96L * 1024L * 1024L) {
            diag::log("MAP", "%s has an implausible size (%ld bytes)", path, size);
            std::fclose(file);
            return false;
        }
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        const size_t got = std::fread(bytes.data(), 1, bytes.size(), file);
        std::fclose(file);
        if (got != bytes.size()) {
            diag::log("MAP", "%s short read (%zu of %zu)", path, got, bytes.size());
            return false;
        }
        const char* slash = std::strrchr(path, '/');
        name_ = slash ? std::string(slash + 1) : std::string(path);
        if (name_.size() > 4) name_.resize(name_.size() - 4);
        return load_bytes(bytes.data(), bytes.size());
    }

    // Blob linked into the executable (maps_embedded.S). No file system, no
    // asset directory: the binary alone is enough.
    bool load_memory(const uint8_t* data, size_t size, const char* name) {
        clear();
        name_ = name && *name ? std::string(name) : std::string("?");
        return load_bytes(data, size);
    }

    bool load_bytes(const uint8_t* data, size_t size) {
        if (!data || size < 8) return false;
        const bool modern = std::memcmp(data, "PKM2", 4) == 0;
        if (!modern && std::memcmp(data, "PKM1", 4) != 0) {
            diag::log("MAP", "%s: not a PKM container", name_.empty() ? "?" : name_.c_str());
            return false;
        }
        const size_t stride = modern ? 48u : 40u;
        uint32_t count = 0;
        std::memcpy(&count, data + 4, sizeof(count));
        if (count == 0 || count > 2000000u || size != 8 + size_t(count) * stride) {
            diag::log("MAP", "%s: header count=%u does not match the file length (%zu)",
                      name_.empty() ? "?" : name_.c_str(), count, size);
            return false;
        }

        tris_.resize(size_t(count));
        for (int k = 0; k < 3; ++k) { box_.lo[k] = 1e30f; box_.hi[k] = -1e30f; }
        for (uint32_t i = 0; i < count; ++i) {
            const uint8_t* source = data + 8 + size_t(i) * stride;
            Tri& tri = tris_[size_t(i)];
            std::memcpy(tri.v, source, 36);
            if (modern) {
                std::memcpy(&tri.material, source + 36, 4);
                std::memcpy(&tri.collider, source + 40, 4);
                std::memcpy(&tri.layer, source + 44, 4);
            } else {
                std::memcpy(&tri.layer, source + 36, 4);
                tri.material = kUnknownMaterial;
                tri.collider = 0;
            }
            for (int k = 0; k < 9; ++k) {
                if (!std::isfinite(tri.v[k]) || std::fabs(tri.v[k]) > 100000.f) {
                    diag::log("MAP", "%s: non-finite vertex in triangle %u", name_.c_str(), i);
                    clear();
                    return false;
                }
            }
            for (int k = 0; k < 9; ++k) {
                const int axis = k % 3;
                box_.lo[axis] = std::min(box_.lo[axis], tri.v[k]);
                box_.hi[axis] = std::max(box_.hi[axis], tri.v[k]);
            }
        }
        build();
        size_t bullets = 0;
        for (const Tri& tri : tris_) if (tri.layer == kBulletLayer) ++bullets;
        diag::log("MAP", "loaded %s: %zu triangles (%zu bullet layer), %zu nodes, bbox (%.1f %.1f %.1f)-(%.1f %.1f %.1f)",
                  name_.c_str(), tris_.size(), bullets, nodes_.size(),
                  box_.lo[0], box_.lo[1], box_.lo[2], box_.hi[0], box_.hi[1], box_.hi[2]);
        return ready();
    }

    void clear() {
        tris_.clear();
        order_.clear();
        nodes_.clear();
        root_ = 0;
        box_ = Bounds{};
        name_.clear();
    }

    bool ready() const { return !tris_.empty() && !nodes_.empty(); }
    const std::string& name() const { return name_; }
    size_t triangles() const { return tris_.size(); }
    Bounds bounds() const { return box_; }

    // True when a layer-14 triangle lies strictly between the two points.
    // hit_distance (optional) receives the distance to the nearest such face.
    bool blocked(Vector3 from, Vector3 to, float* hit_distance = nullptr) const {
        if (nodes_.empty() || root_ >= nodes_.size()) return false;
        const float delta[3] = {to.x - from.x, to.y - from.y, to.z - from.z};
        const float length = std::sqrt(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
        if (!(length > 0.02f) || !std::isfinite(length)) return false;
        const float dir[3] = {delta[0] / length, delta[1] / length, delta[2] / length};
        const float origin[3] = {from.x, from.y, from.z};

        float best = length;
        uint32_t stack[64];
        int top = 0;
        stack[top++] = root_;
        while (top > 0) {
            const Node& node = nodes_[size_t(stack[--top])];
            if (!slab(origin, dir, node, best)) continue;
            if (node.count == 0) {
                if (top + 2 <= int(sizeof(stack) / sizeof(stack[0]))) {
                    stack[top++] = node.left;
                    stack[top++] = node.right;
                }
                continue;
            }
            for (uint32_t i = node.first; i < node.first + node.count; ++i) {
                const Tri& tri = tris_[size_t(order_[size_t(i)])];
                if (tri.layer != kBulletLayer) continue;
                const float distance = triangle_hit(origin, dir, tri, best);
                if (distance > 0.f && distance < best) best = distance;
            }
        }
        if (best >= length - 1e-4f) return false;
        if (hit_distance) *hit_distance = best;
        return true;
    }

private:
    void build() {
        order_.resize(tris_.size());
        for (size_t i = 0; i < order_.size(); ++i) order_[i] = uint32_t(i);
        nodes_.clear();
        nodes_.reserve(tris_.size() / 4 + 16);
        // build_node() appends the node it fills, so the first call creates
        // node 0 and returns 0. The traversal must start there: starting at a
        // hardcoded 0 while the root lives at index 1 (the old layout) made
        // every ray test return "clear", which is why the wall check never
        // once fired and the ground probe never identified a map.
        root_ = build_node(0, uint32_t(tris_.size()));
    }

    uint32_t build_node(uint32_t first, uint32_t count) {
        const uint32_t index = uint32_t(nodes_.size());
        nodes_.push_back(Node{});
        float lo[3] = {1e30f, 1e30f, 1e30f};
        float hi[3] = {-1e30f, -1e30f, -1e30f};
        for (uint32_t i = first; i < first + count; ++i) {
            const Tri& tri = tris_[size_t(order_[size_t(i)])];
            for (int k = 0; k < 9; ++k) {
                const int axis = k % 3;
                lo[axis] = std::min(lo[axis], tri.v[k]);
                hi[axis] = std::max(hi[axis], tri.v[k]);
            }
        }
        {
            Node& node = nodes_[size_t(index)];
            for (int k = 0; k < 3; ++k) { node.lo[k] = lo[k]; node.hi[k] = hi[k]; }
            node.first = first;
            node.count = count;
            node.left = 0;
            node.right = 0;
        }
        if (count <= 8) return index;

        int axis = 0;
        float extent = hi[0] - lo[0];
        if (hi[1] - lo[1] > extent) { axis = 1; extent = hi[1] - lo[1]; }
        if (hi[2] - lo[2] > extent) { axis = 2; }

        const uint32_t middle = first + count / 2;
        std::nth_element(order_.begin() + first, order_.begin() + middle, order_.begin() + first + count,
                         [this, axis](uint32_t l, uint32_t r) {
                             const Tri& a = tris_[size_t(l)];
                             const Tri& b = tris_[size_t(r)];
                             const float ca = a.v[axis] + a.v[axis + 3] + a.v[axis + 6];
                             const float cb = b.v[axis] + b.v[axis + 3] + b.v[axis + 6];
                             return ca < cb;
                         });
        const uint32_t left = build_node(first, middle - first);
        const uint32_t right = build_node(middle, first + count - middle);
        // nodes_ may have reallocated while recursing: index again.
        nodes_[size_t(index)].count = 0;
        nodes_[size_t(index)].left = left;
        nodes_[size_t(index)].right = right;
        return index;
    }

    static bool slab(const float origin[3], const float dir[3], const Node& node, float end) {
        float begin = 0.f;
        for (int axis = 0; axis < 3; ++axis) {
            const float a = origin[axis];
            const float v = dir[axis];
            const float l = node.lo[axis];
            const float h = node.hi[axis];
            if (std::fabs(v) < 1e-9f) {
                if (a < l || a > h) return false;
                continue;
            }
            float t1 = (l - a) / v;
            float t2 = (h - a) / v;
            if (t1 > t2) { const float swap = t1; t1 = t2; t2 = swap; }
            if (t1 > begin) begin = t1;
            if (t2 < end) end = t2;
            if (begin > end) return false;
        }
        return true;
    }

    static float triangle_hit(const float origin[3], const float dir[3], const Tri& tri, float max_distance) {
        const float* a = tri.v;
        const float* b = tri.v + 3;
        const float* c = tri.v + 6;
        const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const float e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const float p[3] = {dir[1] * e2[2] - dir[2] * e2[1],
                            dir[2] * e2[0] - dir[0] * e2[2],
                            dir[0] * e2[1] - dir[1] * e2[0]};
        const float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
        if (std::fabs(det) < 1e-8f) return 0.f;
        const float inv = 1.f / det;
        const float s[3] = {origin[0] - a[0], origin[1] - a[1], origin[2] - a[2]};
        const float u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv;
        if (u < -1e-5f || u > 1.00001f) return 0.f;
        const float q[3] = {s[1] * e1[2] - s[2] * e1[1],
                            s[2] * e1[0] - s[0] * e1[2],
                            s[0] * e1[1] - s[1] * e1[0]};
        const float v = (dir[0] * q[0] + dir[1] * q[1] + dir[2] * q[2]) * inv;
        if (v < -1e-5f || u + v > 1.00001f) return 0.f;
        const float distance = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
        if (distance <= 1e-4f || distance >= max_distance - 1e-4f) return 0.f;
        return distance;
    }

    std::vector<Tri> tris_;
    std::vector<uint32_t> order_;
    std::vector<Node> nodes_;
    uint32_t root_{0};
    Bounds box_{};
    std::string name_;
};

// ---------------------------------------------------------------------------
// Tracker: keeps one map world loaded for the current round.
//
// The render thread feeds it the ground points it already has (the local
// player's feet plus every enemy's feet, i.e. free data) every so often; the
// worker thread picks the map that explains those points and publishes it.
// Nothing in the worker touches game memory.
// ---------------------------------------------------------------------------
class Tracker {
public:
    static constexpr int kMaxPoints = 12;

    void start(const char* directory) {
        if (g_run.load()) return;
        directory_ = directory && *directory ? directory : "assets/maps";
        g_run.store(true);
        g_thread = std::thread([this] { worker_loop(); });
        diag::log("MAP", "tracker started, assets=%s", directory_.c_str());
    }

    void stop() {
        if (!g_run.load()) return;
        g_run.store(false);
        if (g_thread.joinable()) g_thread.join();
        std::lock_guard<std::mutex> lock(mutex_);
        world_.reset();
        for (int i = 0; i < kMapCount; ++i) {
            cache_[i].reset();
            cache_state_[i] = 0;
            drops_[i].store(0);
            votes_[i] = 0;
        }
        published_name_.clear();
        status_ = "map: stopped";
    }

    // Something about the loaded map does not hold (shots through walls that
    // the map called clear did no damage). Drop it and look again — but a map
    // that keeps failing this way is not coming back: after kMaxDropsPerMap
    // rounds of pick -> drop -> pick it is left out, and the log says so.
    void invalidate(const char* reason) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!world_) return;
        const std::string dropped = published_name_;
        for (int i = 0; i < kMapCount; ++i) {
            if (dropped == kMaps[i].name) {
                const unsigned n = ++drops_[i];
                if (n >= unsigned(trigger_params::kMaxDropsPerMap))
                    diag::log("MAP", "%s dropped %u times, not offering it again",
                              kMaps[i].name, n);
                break;
            }
        }
        diag::log("MAP", "dropping %s: %s", dropped.c_str(), reason ? reason : "unspecified");
        world_.reset();
        published_name_.clear();
        status_ = "map: re-checking";
        ++epoch_;
    }

    // Render thread, cheap. `points[0]` is the local player's feet.
    void update(const Vector3* points, int count, bool new_round, long long now_ms) {
        if (count <= 0) return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (new_round) ++epoch_;
        if (!new_round && now_ms - last_update_ms_ < 500) return;
        last_update_ms_ = now_ms;
        request_count_ = std::min(count, kMaxPoints);
        for (int i = 0; i < request_count_; ++i) {
            request_points_[i][0] = points[i].x;
            request_points_[i][1] = points[i].y;
            request_points_[i][2] = points[i].z;
        }
        // The published map has to contain the player who is using it.
        if (world_) {
            const Vector3 feet = points[0];
            if (!inside(published_, feet, 8.f)) {
                diag::log("MAP", "left %s (feet %.1f %.1f %.1f), re-detecting",
                          published_name_.c_str(), feet.x, feet.y, feet.z);
                world_.reset();
                published_name_.clear();
                status_ = "map: searching";
                ++epoch_;
            }
        }
    }

    bool ready() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return world_ != nullptr;
    }

    bool blocked(Vector3 eye, Vector3 point, float* distance = nullptr) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!world_) return false;
        return world_->blocked(eye, point, distance);
    }

    std::string status() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_;
    }

    std::string map_name() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return published_name_;
    }

private:
    static bool inside(const Bounds& box, const Vector3& point, float margin) {
        return point.x >= box.lo[0] - margin && point.x <= box.hi[0] + margin &&
               point.y >= box.lo[1] - margin && point.y <= box.hi[1] + margin &&
               point.z >= box.lo[2] - margin && point.z <= box.hi[2] + margin;
    }

    // Result of the ground probe for one candidate map.
    struct ProbeStats {
        int matched{0};        // points whose feet land on this map's floor
        int examined{0};       // points that were not zeroed out
        int local_matched{0};  // points[0] (the local player) matched
        float local_delta{0.f};// local feet height above that floor
        float residual{1e30f}; // mean |feet - floor| over the matched points
    };

    static int index_of(const std::string& name) {
        for (int i = 0; i < kMapCount; ++i)
            if (name == kMaps[i].name) return i;
        return -1;
    }

    static ProbeStats probe(const World& world, const float (*points)[3], int count) {
        ProbeStats stats;
        float sum = 0.f;
        for (int i = 0; i < count; ++i) {
            const Vector3 feet{points[i][0], points[i][1], points[i][2]};
            if (feet.x == 0.f && feet.y == 0.f && feet.z == 0.f) continue;
            ++stats.examined;
            float distance = 0.f;
            const Vector3 from{feet.x, feet.y + trigger_params::kProbeUp, feet.z};
            const Vector3 to{feet.x, feet.y - trigger_params::kProbeDown, feet.z};
            if (!world.blocked(from, to, &distance)) continue;
            // distance runs from feet+up, so this is (feet.y - floor.y): how far
            // the feet sit above the floor this map claims.
            const float delta = distance - trigger_params::kProbeUp;
            if (i == 0) stats.local_delta = delta;
            if (delta < trigger_params::kFloorMin || delta > trigger_params::kFloorMax) continue;
            ++stats.matched;
            sum += std::fabs(delta);
            if (i == 0) stats.local_matched = 1;
        }
        if (stats.matched > 0) stats.residual = sum / float(stats.matched);
        return stats;
    }

    // Parsed worlds are kept for the process lifetime. Before this cache a
    // failed detection re-parsed four maps every six seconds; the 2026-11-21
    // session did 892 full parses of the same four assets in twenty minutes,
    // which is where the periodic hitches came from.
    std::shared_ptr<World> load_cached(int index) {
        if (index < 0 || index >= kMapCount) return nullptr;
        if (cache_state_[index] == 2) return nullptr;
        if (cache_state_[index] == 1) return cache_[index];
        auto world = std::make_shared<World>();
        const MapEntry& entry = kMaps[index];
        bool ok = false;
        const EmbeddedMap* blob = find_embedded(entry.name);
        if (blob && blob->data && blob->size)
            ok = world->load_memory(blob->data, blob->size, entry.name);
        if (!ok) {
            const std::string path = directory_ + "/" + entry.name + ".pkm";
            ok = world->load(path.c_str());
        }
        if (!ok) {
            cache_state_[index] = 2;
            return nullptr;
        }
        if (!trigger_params::kCacheMaps) return world;
        cache_state_[index] = 1;
        cache_[index] = world;
        return world;
    }

    void publish(std::shared_ptr<World> world, const char* reason) {
        std::lock_guard<std::mutex> lock(mutex_);
        last_scan_ms_ = diag::now_ms();
        if (!world) {
            world_.reset();
            published_name_.clear();
            status_ = "map: none";
        } else {
            published_ = world->bounds();
            published_name_ = world->name();
            status_ = "map: " + published_name_;
            world_ = std::move(world);
        }
        diag::log("MAP", "%s", reason);
    }

    void detect() {
        float points[kMaxPoints][3];
        int count = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            count = request_count_;
            for (int i = 0; i < count; ++i)
                for (int k = 0; k < 3; ++k) points[i][k] = request_points_[i][k];
        }
        if (count <= 0) return;
        const Vector3 local{points[0][0], points[0][1], points[0][2]};
        if (local.x == 0.f && local.y == 0.f && local.z == 0.f) return;

        int order[kMapCount];
        int candidates = 0;
        for (int i = 0; i < kMapCount; ++i)
            if (in_bounds(kMaps[i], local, trigger_params::kCandidateMargin)) order[candidates++] = i;
        if (candidates == 0) {
            publish(nullptr, "no map asset contains the local player");
            return;
        }
        std::sort(order, order + candidates, [](int a, int b) {
            return volume(kMaps[a]) < volume(kMaps[b]);
        });
        if (candidates > trigger_params::kMaxCandidates) candidates = trigger_params::kMaxCandidates;

        std::shared_ptr<World> best;
        ProbeStats best_stats{};
        float best_residual = 1e30f;
        int attempts = 0;
        int ignored = 0;
        // Per-map evidence for this pass.
        bool explains[kMapCount]{};
        ProbeStats stats_of[kMapCount]{};
        float residuals[kMapCount];
        for (int i = 0; i < kMapCount; ++i) residuals[i] = 1e30f;

        for (int i = 0; i < candidates; ++i) {
            const int index = order[i];
            if (drops_[index].load() >= unsigned(trigger_params::kMaxDropsPerMap)) { ++ignored; continue; }
            std::shared_ptr<World> world = load_cached(index);
            if (!world) continue;
            ++attempts;
            const ProbeStats stats = probe(*world, points, count);
            // The local player stands on the map he is playing: without a floor
            // under his own feet the candidate is out, full stop.
            if (!stats.local_matched) continue;
            stats_of[index] = stats;
            residuals[index] = stats.residual;
            explains[index] = stats.matched == stats.examined &&
                              stats.examined >= trigger_params::kMinPointsToConfirm &&
                              stats.residual <= trigger_params::kResidualMax;
            const bool better = !best || stats.matched > best_stats.matched ||
                                (stats.matched == best_stats.matched && stats.residual < best_residual);
            if (!better) continue;
            best = world;
            best_stats = stats;
            best_residual = stats.residual;
        }

        // Votes. A map that explains every ground point gains one vote per
        // pass, any other map loses all of its votes. Several maps overlap and
        // a flat foreign floor fits one frame exactly as well as the real one
        // (offline replay of this session's own feet: Pyramid fitted 20/20 in
        // one match, TrainingOutside 109/186 over the whole session), so a map
        // is published only when it has been the single explanation for
        // kConfirmVotes passes in a row. A single ground point (round start, no
        // enemy in view) is weak evidence and needs twice as many.
        int leader = -1;
        unsigned leader_votes = 0;
        int tied = 0;
        for (int i = 0; i < kMapCount; ++i) {
            votes_[i] = explains[i] ? (votes_[i] < 255 ? votes_[i] + 1 : 255) : 0;
            if (!votes_[i]) continue;
            if (votes_[i] > leader_votes) {
                leader_votes = votes_[i]; leader = i; tied = 1;
            } else if (votes_[i] == leader_votes) {
                ++tied;
                // Same amount of evidence: the tighter alignment wins.
                if (leader >= 0 && residuals[i] < residuals[leader]) leader = i;
            }
        }
        std::shared_ptr<World> confirmed;
        ProbeStats confirmed_stats{};
        if (leader >= 0) {
            const ProbeStats& stats = stats_of[leader];
            const unsigned need = stats.examined < trigger_params::kMinPointsToConfirm
                                      ? unsigned(trigger_params::kConfirmVotes) * 2u
                                      : unsigned(trigger_params::kConfirmVotes);
            const long long now = diag::now_ms();
            if (tied > 1) {
                if (!ambiguous_since_) ambiguous_since_ = now;
            } else {
                ambiguous_since_ = 0;
            }
            // Ambiguity is normal on a shared flat floor at round start. It must
            // not silence the trigger forever: after kAmbiguityTimeoutMs the
            // tightest fit is used and the menu says so. If it was the wrong
            // map the damage feedback drops it (three shots, no damage).
            const bool give_up_waiting = ambiguous_since_ && now - ambiguous_since_ > trigger_params::kAmbiguityTimeoutMs;
            if (leader_votes >= need && (tied <= 1 || give_up_waiting)) {
                confirmed = cache_[leader];
                confirmed_stats = stats;
                if (tied > 1)
                    diag::log("MAP", "%d maps explain the ground equally well, using the tightest fit %s (residual %.2f)",
                              tied, kMaps[leader].name, stats.residual);
            }
        } else {
            ambiguous_since_ = 0;
        }

        if (!confirmed) {
            // Nothing is confirmed yet: keep a map that still contains the
            // player (the wall check on it is no worse than nothing), and say
            // so in the log and in the menu.
            bool keep = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                keep = world_ != nullptr && inside(published_, local, 8.f);
                if (keep) last_scan_ms_ = diag::now_ms();
            }
            char residual_text[24];
            if (best) std::snprintf(residual_text, sizeof(residual_text), "%.2f", best_residual);
            else std::snprintf(residual_text, sizeof(residual_text), "n/a");
            char line[256];
            std::snprintf(line, sizeof(line),
                          "probe: %d candidates, %d probed%s, best %s (%.0f%% of points, residual %s) — not confirmed yet",
                          candidates, attempts, ignored ? " (some maps ignored)" : "",
                          best ? best->name().c_str() : "none",
                          best ? 100.f * float(best_stats.matched) / float(best_stats.examined ? best_stats.examined : 1) : 0.f,
                          residual_text);
            if (keep) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    status_ = "map: " + published_name_ + " (checking)";
                }
                diag::log("MAP", "%s, keeping %s", line, published_name_.c_str());
            } else {
                // publish() logs the same line, so this path must not log it
                // twice: the session log had every "probe:" line duplicated.
                publish(nullptr, line);
            }
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (world_ && published_name_ == confirmed->name()) {
                status_ = "map: " + published_name_;
                last_scan_ms_ = diag::now_ms();   // same map, nothing to publish
                return;
            }
        }
        char line[256];
        std::snprintf(line, sizeof(line),
                      "picked %s: %d/%d ground probes (feet %.2f m above the floor, residual %.2f), "
                      "%d candidates, %d probes",
                      confirmed->name().c_str(), confirmed_stats.matched, confirmed_stats.examined,
                      confirmed_stats.local_delta, confirmed_stats.residual, candidates, attempts);
        publish(confirmed, line);
    }

    void worker_loop() {
        ::setpriority(PRIO_PROCESS, static_cast<id_t>(::syscall(SYS_gettid)), 10);
        unsigned handled = 0;
        long long last_scan = 0;
        while (g_run.load()) {
            unsigned epoch = 0;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                epoch = epoch_;
            }
            const long long now = diag::now_ms();
            bool have_map = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                have_map = world_ != nullptr;
            }
            // A loaded map is only re-checked when the round changes or the
            // player leaves its bounds (both bump the epoch). While nothing is
            // loaded, retry on a slow timer instead of reloading maps forever.
            const bool due = (epoch != handled) || (!have_map && now - last_scan >= 6000);
            if (!due) { usleep(100000); continue; }
            handled = epoch;
            last_scan = now;
            detect();
        }
    }

    mutable std::mutex mutex_;
    std::shared_ptr<World> world_;
    // Worker-thread only: parsed worlds and their load state (0 = not tried,
    // 1 = cached, 2 = failed to load).
    std::shared_ptr<World> cache_[kMapCount];
    unsigned char cache_state_[kMapCount]{};
    // Written by the render thread (invalidate) and read by the worker.
    std::atomic<unsigned> drops_[kMapCount]{};
    // Worker-thread only: consecutive passes in which the map explained every
    // ground point.
    unsigned char votes_[kMapCount]{};
    long long ambiguous_since_{0};
    Bounds published_{};
    std::string published_name_;
    std::string status_{"map: waiting"};
    std::string directory_{"assets/maps"};
    float request_points_[kMaxPoints][3]{};
    int request_count_{0};
    long long last_update_ms_{0};
    long long last_scan_ms_{0};
    unsigned epoch_{1};
    std::thread g_thread;
    std::atomic<bool> g_run{false};
};

inline Tracker g_tracker;

} // namespace mapworld
