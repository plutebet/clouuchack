---
name: game-engine-teardown
description: Unity (Mono/IL2CPP) and Unreal internals — Il2CppDumper, GObjects/GNames, SDK gen, native RTTI. Engine reversing only; building → cheat-development.
---

# Game Engine Teardown

**Identify stack:** `GameAssembly.dll`→IL2CPP; `mono-2.0-bdwgc.dll`→Mono; `*-Win64-Shipping.exe`→Unreal.

**Unity Mono:** dnSpy directly on `Assembly-CSharp.dll` (full decompile); hook jitted code or `mono_*` exports; cache LocalPlayer from static field.

**Unity IL2CPP:** Il2CppDumper on `GameAssembly.dll` + `global-metadata.dat` (`<game>_Data/il2cpp_data/Metadata/`) → `script.json`+`il2cpp.h`+DummyDll. Offsets file-static — re-dump on patch or runtime-resolve (pattern scan).

**Unreal:** find **GObjects** + **GNames** (FNamePool) by pattern → **Dumper-7** SDK gen (UE4.x–5.x) or **UE4SS** for fastest recon. First offsets: GObjects, FName `AppendString`, `ProcessEvent`, UEngine `World`/`OwningGameInstance`, `LocalPlayers`.

**No engine symbols (native RTTI):** walk `.rdata` `??_R0` type descriptors (`".?AV<Name>@"`) → vftable xrefs = class anchors; ReClass.NET for carving.

Refs: atlas `topics/re-tools-cracking.md` §1, `uc-wiki-techniques.md` §2; skills `game-engine`, `reverse-engineering`.

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
