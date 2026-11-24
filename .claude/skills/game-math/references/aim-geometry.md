# Aim geometry

## FOV circle — pixel space vs angular space

Pixel-space check (what the user sees, cheapest):

```cpp
// language: C++, file: fov_check.hpp
// radius_px scales with vertical FOV; center = screen center
bool in_fov(const Vec2& crosshair_px, const Vec2& target_px, float fov_deg, int screen_h) {
    float dist = sqrtf((target_px.x-crosshair_px.x)*(target_px.x-crosshair_px.x)
                     + (target_px.y-crosshair_px.y)*(target_px.y-crosshair_px.y));
    float radius_px = (fov_deg / 180.0f) * screen_h;   // linear approx, fine for fov<=90
    return dist <= radius_px;
}
```

Angular check (needed for prediction and for engines without matrices):
```
view_dir   = (cos(pitch)*cos(yaw), sin(pitch), cos(pitch)*sin(yaw))   // engine-order dependent
target_dir = normalize(bone_pos - eye_pos)
angle      = acos(dot(view_dir, target_dir))          // radians; clamp dot to [-1,1]
```
Convert desired FOV once: `radius_px = tan(fov_rad/2) / tan(vfov_rad/2) * (screen_h/2)` — exact tangent-linear mapping; the linear approx above drifts >90° FOV.

Closest-target selection: minimize angle (angular) or pixel distance (pixel space) — they rank differently off-center; pixel-space matches human expectation.

## Frame-rate-independent smoothing

Fixed-per-frame lerp (`x += (t-x)*0.3`) changes with FPS. Exponential time-constant form:

```cpp
// k: higher = snappier; dt = frame time seconds
float alpha = 1.0f - expf(-k * dt);
aim.x += (target.x - aim.x) * alpha;
aim.y += (target.y - aim.y) * alpha;
```

k calibration: k=8 → ~40% correction per 60fps frame, k=15 → aggressive, k=3 → floaty. Humanlike flicks need the two-phase model — see `behavioral-evasion` (saccade + corrective), not a single lerp.

## Projectile prediction

Iterative flight time (2–3 iterations converge for game velocities):

```cpp
// language: C++, file: predict.hpp
// solve t for moving target + projectile speed; gravity drop for ballistics
Vec3 predict(const Vec3& eye, const Vec3& tpos, const Vec3& tvel, float speed, float g) {
    float t = 0.0f;
    Vec3 aim = tpos;
    for (int i = 0; i < 3; ++i) {
        float dist = sqrtf((aim.x-eye.x)*(aim.x-eye.x) + (aim.y-eye.y)*(aim.y-eye.y) + (aim.z-eye.z)*(aim.z-eye.z));
        t = dist / speed;
        aim.x = tpos.x + tvel.x * t;
        aim.y = tpos.y + tvel.y * t - 0.5f * g * t * t;   // drop
        aim.z = tpos.z + tvel.z * t;
    }
    return aim;
}
```

Notes:
- `g` is engine-dependent and often not 9.8 — calibrate: log shot drop over known distance, solve `g` from `0.5*g*t²`.
- Hitscan targets: `pos + vel*t` only, no drop term, single iteration.
- Zoom/scoped projectiles with drag: engines vary (QuadraticDrag); if first-order misses at range, calibrate drag empirically — don't model what you haven't measured.

## Recoil control (RCS)

Recoil applies as view punch between shot index n-1 and n. Compensation:

```
punch_delta = punch_current - punch_previous     // per-shot kick, engine exposes punch/recoil index
compensation = -punch_delta * control_factor     // factor 1.0 = full, <1 leaves natural spread
apply before the shot lands (bullet eval frame), not after
```
Track `recoil_index`/shot count; sprays have per-index patterns (AK-style climb then horizontal drift) — table of per-index offsets beats pure delta feedback where the pattern is static per weapon.

## Visibility checks

Ray from eye to bone, per engine:
- Unity: `Physics.Raycast(eye, dir, out hit, dist)` — check hit collider is the target.
- Unreal: `LineTraceSingle` with `ECC_Visible` channel (or `TRACE_TRACE` complex) — simple vs complex collision matters for mesh-level accuracy.
- Source: `TraceRay` with `MASK_SHOT`, hitgroup filtering.
Kernel-external builds: visibility needs the engine's trace — either call it (internal) or approximate with occlusion volumes (external); guessing walls from bounding boxes looks wrong at doorframes.

## Target ordering

Priority vector, not single key: `score = w1*angle + w2*dist + w3*threat + w4*health_penalty` — tune weights per game; visibility gate first, FOV gate second, score third. Sorting the full entity list per frame is wasted work: early-out on FOV before any bone math (bones are the expensive call).
