---
name: game-math
description: Cheat math — view matrices, world-to-screen projection, bone-space transforms, FOV geometry, aim smoothing, projectile prediction, recoil compensation. Use for any w2s bug, matrix hunt, FOV-circle check, or aimbot math question. For code skeletons use cheat-development; for where offsets/matrix live per engine use game-engine-teardown.
---

# Game math

**The one model behind everything:** world → view → projection → clip space → NDC (divide by w) → screen (affine map + Y flip). Every w2s bug is one of: wrong matrix convention (row vs column major), reading translation from the wrong triple, missing the Y flip, or dividing when w is behind camera.

**Convention triage (decide first, once per engine):**
- DirectX / most Windows titles: row vectors — `clip = pos * M`, translation lives in M[3][0..2] (elements 12,13,14 when read as flat[16]), Y axis down in NDC, screen origin top-left.
- OpenGL/GLM-style: column vectors — `clip = M * pos`, translation in M[0..2][3] (flat elements 3,7,11).
- Sanity probe: project a known world position (own feet = player origin), compare with the entity's on-screen position. Wrong convention = w2s flips or mirrors — reverse-engineer the convention from the flip pattern, don't guess.

**w2s, complete:**
```
clip  = VP * [pos, 1]
if (clip.w <= 0.1)  skip               // behind camera — check w, not z
ndc    = clip.xyz / clip.w
screen = ((ndc.x + 1) * 0.5 * width, (1 - ndc.y) * 0.5 * height)
```

**Matrix hunt in memory:** scan for 16 contiguous floats updated per-frame; diff-scan while moving in world (translation triple tracks your movement, projection quadrant is static); validate by projecting known entities. Engine locations: Unity `Camera.worldToCameraMatrix`/`projectionMatrix` (or `localToWorldMatrix` per object); Unreal `FMinimalViewInfo`/`PlayerCameraManager` + bone `SpaceBases`; Source `WorldToScreenMatrix()`.

**Bones:** world position = root transform ∘ local bone matrix. Unity gives `Transform.position` per bone directly (humanoid or generic); Unreal stores `FTransform` (quat + translation + scale) per bone — decompose quaternion to matrix before composing. The classic bug: taking element 12,13,14 for translation in a convention where it's 3,7,11.

Refs in `references/`:
- matrices-and-w2s.md — full convention tables, matrix hunt workflow, per-engine specifics, quaternion decomposition
- aim-geometry.md — FOV circles in pixel vs angular space, frame-rate-independent smoothing, projectile prediction iteration, RCS math, visibility traces

Route: `cheat-development` (code skeletons, hooks), `game-engine-teardown` (offsets, SDK layout), atlas `topics/internal-hooks.md` for where the math plugs into render hooks.

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
