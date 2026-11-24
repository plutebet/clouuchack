# unity6-bypass — trigger rework

## What changed

New files (header-only, like the rest of the project), plus one assembly object:

| file | purpose |
|---|---|
| `src/game/map_bounds.hpp` | AABB of all 23 PKM2 maps, generated from the assets |
| `src/game/map_world.hpp` | PKM2 loader, BVH, segment/triangle raycast, map auto-detection |
| `src/game/maps_embedded.hpp` | table of the blobs linked into the binary |
| `src/game/maps_embedded.S` | 23 `.incbin` directives for `assets/maps/*.pkm` |
| `assets/maps/*.pkm` | map collider set (Standoff 2 1.1.0, Android APK+OBB) |

The maps are **linked into the executable** (46 MB of `.rodata`, only the pages
of the map actually used become resident), so nothing has to be pushed next to
the binary. `assets/maps` is needed only at build time; `--maps <dir>` still
overrides the embedded set at runtime.

Rewritten:

* `src/func/triggerbot.hpp` — donor (larpsense) architecture: hit-volume ray,
  nearest-wins, teammate blocks, delay, press/release, damage feedback.
  The visibility gate is now the map raycast, not `szi.mow`/`IsVisible`:
  * `szi.mow` is `None / InFrustumOnly / Always`, i.e. "inside the camera
    frustum" — true for an enemy behind a wall who is simply on screen. The old
    check (`mow == 0 -> invisible`) let every visible-but-occluded enemy pass.
  * the damage "confirmation" used to be permanent; it is now valid for 2.5 s.
  * without a map (and with `Strict` off) every flag must agree: `mow != 0`,
    `IsVisible`, fresh bones, and bones that are still moving.
  * the ray must come from the **native** game camera; synthetic
    `main-bor` / `vqn-bor` / `ios-transform` matrices are rejected.
  * a map is dropped after 3 shots that did no damage on a path the map called
    clear, then re-detected.

Patched:

* `src/game/../ui/cfg.hpp` — `hold_ms`, `strict`, `block_teammates`,
  `min_distance`, `max_distance`.
* `src/func/visuals.cpp` — teammate blocker list, feed ground points to the map
  tracker, trigger uses the pre-fallback camera matrix, ESP fully off no longer
  disables the trigger.
* `src/ui/menu.cpp` — Trigger tab: Delay, Hold, Wall Check, Strict, Team Block,
  map status line.
* `src/main.cpp` — `--maps <dir>`, tracker start/stop, touch watchdog.
* `src/other/touch_inject.hpp` — tap watchdog (a press can never stick down).

## Build

Quick way: put `build-ndk.bat` in the project root and run it. It picks the
newest NDK under `%LOCALAPPDATA%\Android\Sdk\ndk`, verifies `includes\` and
`assets\maps`, builds, and prints the output path.

The archive you got contains `jni/` and `src/`. `jni/Android.mk` also compiles
`../includes/draw/...` (Android_draw, ImGui and the backends), which are **not**
part of this archive — put the `includes/` folder of your full project next to
`jni/` and `src/` first, otherwise the build stops on the first missing file.

Layout:

```
unity6-bypass/
  jni/Android.mk  jni/Application.mk  jni/Android_rise.mk
  src/                 (includes src/game/maps_embedded.S)
  includes/            <- from your full project
  assets/maps/*.pkm    <- only for the build, baked into the binary
```

The `.S` uses `.incbin "assets/maps/<Name>.pkm"`, i.e. paths relative to the
ndk-build working directory — **run ndk-build from the project root**.
The `rise` binary does not link the blob object (`Android_rise.mk` is unchanged
and `RISE_BUILD` empties the table), so it stays small.

Requirements: Android NDK r26+ (r27 for the 16 KB page-size flags already in
`Android.mk`), `APP_ABI := arm64-v8a`, C++17 (already in `Application.mk`).

```bat
:: from the project root
set NDK=C:\Users\kosoj\AppData\Local\Android\Sdk\ndk\27.2.12479018
"%NDK%\ndk-build.cmd" NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=jni/Android.mk NDK_APPLICATION_MK=jni/Application.mk -j8
```

Output: `libs/arm64-v8a/eeeww` (module name from `Android.mk`), ~46 MB because
the map set is inside it.
The `rise` binary is `jni/Android_rise.mk` → `libs/arm64-v8a/rise`.

Maps are compiled in, so nothing else needs to be prepared. If you change the
asset set, re-generate the two lists (same names, same order):

```bash
ls assets/maps/*.pkm | sed 's|.*/||; s|\.pkm$||'
```

## Run

```bat
adb push libs\arm64-v8a\eeeww /data/local/tmp/sf_render
adb shell chmod 755 /data/local/tmp/sf_render
adb shell su -c "/data/local/tmp/sf_render"
```

Root is required for `process_vm_readv` on the game and for the SurfaceFlinger
overlay; without it the overlay starts but reads nothing.

Options: `--maps <dir>`, `--no-launch`, `--diag`, `--skip-build-check`
(diagnostics only — it bypasses the libunity build-id check).
Log: `/data/local/tmp/.sf_render.log`. Lines that matter for the trigger:

```
[MAP] loaded Sandstone: 59244 triangles (49805 bullet layer), ... 
[MAP] picked Sandstone: 5/6 ground probes, 7 candidates, 2 attempts
[TRIGGER] hold: strict: waiting for a map collider (assets/maps)
[TRIGGER] hold: wall at 8.42 m before id=0x... (bone=8)
[TRIGGER] fire #12 id=0x... hp=78 bone=10 dist=24.3 map=1
[TRIGGER] no damage id=0x... hp=78 streak=1 blacklist=400ms
```

If `[MAP]` never says `picked`, the ground probe disagrees with the assets —
check that the `.pkm` files are the ones from `assets/maps/manifest.json`
(Standoff 2 1.1.0, versionCode 103982).

## Not verified

No NDK on the machine that produced this patch: the code was **not compiled**
and never ran on a device. The map containers, triangle counts and bounding
boxes were verified against the files themselves (two independent parsers), but
syntax errors and the live map auto-detection are unproven.
