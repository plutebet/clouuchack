# Matrices and world-to-screen

## Convention table

| Convention | Vector mult | Translation flat idx | NDC Y | Screen Y |
|---|---|---|---|---|
| DirectX (D3DX, most Win titles) | `v * M` (row vector) | 12, 13, 14 | -1 bottom | top-left origin, Y down |
| OpenGL / GLM storage | `M * v` (column vector) | 3, 7, 11 | -1 bottom | top-left origin in practice |
| Source engine `VMatrix` | `v * M` | 3, 7, 12? — `m[0][3]`, `m[1][3]`, `m[2][3]` | — | WorldToScreen handles flip |
| Unity (column-major storage) | `M * v` | 3, 7, 11 (M03, M13, M23) | — | `Camera.WorldToScreenPoint` Y up; flip for pixel coords |

Translation element is the fastest convention tell: move in-game along +X and diff-scan — the three floats that move together and linearly are the translation triple; their flat indices name the convention.

## Full w2s (C++)

```cpp
// language: C++, file: w2s.hpp, target: any (row-vector DirectX convention)
// VP = view-projection matrix, flat[16], row-major storage as games ship it
struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };

bool world_to_screen(const Vec3& pos, const float* m, int w, int h, Vec2& out) {
    float cx = m[0]*pos.x + m[1]*pos.y + m[2]*pos.z  + m[3];
    float cy = m[4]*pos.x + m[5]*pos.y + m[6]*pos.z  + m[7];
    float cw = m[12]*pos.x + m[13]*pos.y + m[14]*pos.z + m[15];
    if (cw < 0.1f) return false;                 // behind camera
    float ndc_x = cx / cw, ndc_y = cy / cw;
    out.x = (ndc_x + 1.0f) * 0.5f * w;
    out.y = (1.0f - ndc_y) * 0.5f * h;           // Y flip — NDC Y up, screen Y down
    return true;
}
```

Column-vector convention: transpose the multiply (`m[0], m[4], m[8], m[12]` per output component) — same gate on `w`.

## Matrix hunt workflow

1. Open the game, position player at a known world point (get coords from any readable source: HUD mod, map coordinate, or entity list).
2. Full memory scan for float quadruples: 16-float runs where exactly 3 consecutive floats change as you walk (translation) and 12 are stable per-frame (rotation/projection mixing in changes but bounded).
3. Freeze-test: write zeros to the suspect block → screen glitches black/skewed → confirmed view matrix, and the glitch pattern (full-screen skew vs entity-only) tells view vs per-object matrix.
4. Validate: project own player origin + a distant landmark; compare against entity list positions drawn via `cheat-development` overlay.

Projection-only path: if you have view matrix and projection matrix separately, `VP = P * V` (column) or `V * P` (row) — order flips with convention; wrong order is the #1 silent w2s failure after the w-gate.

## Per-engine specifics

- **Unity (Mono/IL2CPP):** `Camera.main.worldToCameraMatrix`, `projectionMatrix`; per-object `Transform.localToWorldMatrix`. IL2CPP: these are property getters — resolve via `game-engine-teardown` SDK, call as native functions. Screen Y from `WorldToScreenPoint` is **bottom-left origin** — flip for overlay drawing.
- **Unreal:** camera from `PlayerCameraManager` (`GetCameraCacheView`/`MinimalViewInfo`: Location, Rotation, FOV). Projection built from rotation — many UE externals skip matrices entirely: build VP from yaw/pitch/roll + FOV directly (composition from euler, standard UE axes: X forward, Y right, Z up, rotation order Yaw-Pitch-Roll applied as FRotationMatrix).
- **Source:** `CViewSetup`, `WorldToScreenMatrix()` on the engine client VMatrix (4x4, row-vector). Bones via `BoneAccessor`/`SetupBones` matrix array — position in `m[0][3], m[1][3], m[2][3]`.
- **Custom engines:** assume row-vector DirectX until disproven; UE-style euler composition is second most common.

## Quaternion decomposition (Unreal FTransform bones)

```cpp
// quat (x,y,z,w) + translation (x,y,z) + scale → rotation basis
// standard: M = T * R * S; translation is separate field, not in the matrix
float m00 = 1 - 2*(y*y + z*z), m01 = 2*(x*y - z*w),     m02 = 2*(x*z + y*w);
float m10 = 2*(x*y + z*w),     m11 = 1 - 2*(x*x + z*z), m12 = 2*(y*z - x*w);
float m20 = 2*(x*z - y*w),     m21 = 2*(y*z + x*w),     m22 = 1 - 2*(x*x + y*y);
```
Bone world pos = parent_world ∘ (local_rot, local_trans) — compose per hierarchy depth, or take the engine's pre-composed `SpaceBases` array when the SDK exposes it (cheaper, fewer mistakes).

## Validation checklist (w2s misbehaves)

1. Mirror-image left/right → translation triple read from wrong indices (convention wrong).
2. All points dead center → w-gate broken (dividing by w ≤ 0 or w near zero without gate).
3. Points cluster at bottom → missing Y flip.
4. Points lag one frame → matrix read mid-update; read once per frame after game update (see `cheat-development` timing).
5. Distance distortion near edges → projection aspect mismatch (using stale window size for screen w/h).
