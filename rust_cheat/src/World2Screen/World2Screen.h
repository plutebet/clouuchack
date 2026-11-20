
// ================================================================================
// Source archive verified by @imguisource_bot
// Channels:
// - ImGui Source (t.me/ImGui_Source_FIXNEY)
// - shit src 🧨 (t.me/il2cpp_src)
// ================================================================================

#pragma once
#include <chrono>
#include "Math/Math.h"

class W2S {
public:
  static Matrix4x4 *FindViewMatrix() { return &g_ViewMatrix; }
  static bool PrepareMatrix();
  static bool WorldToScreen(const Vector3 &worldPos, Vector2 &screenPos);
  static void ResetCache() { g_CachedMatrixAddr = 0; }

  // Cheap, non-destructive address refresh used by the updater. Reads the
  // camera chain and latches a new native matrix pointer if it is structurally
  // valid; on failure it keeps the current address and never runs the expensive
  // scan/probe.
  static bool TryRefreshMatrixAddress();

  // Thread-safe projection for worker threads (e.g. the aim worker). Reads the
  // published matrix through its seqlock and projects, so it never touches the
  // render thread's g_ViewMatrix / g_CameraPos (no torn reads either way).
  static bool ProjectPublished(const Vector3 &worldPos, Vector2 &screenPos);
  // Thread-safe read of the published camera position (the distance origin).
  static bool GetPublishedCameraPos(Vector3 &out);

  // Background updater: reads the view*projection matrix + camera position
  // through the driver on its own thread and publishes them. The render
  // thread's PrepareMatrix() then only copies the published values, so it
  // never blocks on the driver's shared-memory mutex.
  static void StartUpdater();
  static void StopUpdater();
  static uint64_t PublishedUs();

  static float g_ScreenW;
  static float g_ScreenH;

  // Native Camera.position (the local player's eye in world space). Refreshed
  // with the view matrix; used as a distance/range origin when the local
  // player object is not present in the entity snapshot.
  static Vector3 GetCameraPos() { return g_CameraPos; }
  static bool HaveCameraPos() { return g_HaveCameraPos; }

private:
  static uintptr_t g_CachedMatrixAddr;
  static Matrix4x4 g_ViewMatrix;
  static Vector3 g_CameraPos;
  static bool g_HaveCameraPos;
  static std::chrono::steady_clock::time_point g_LastUpdateTime;

  static constexpr int UPDATE_INTERVAL_MS = 0;
  static constexpr size_t MATRIX_SIZE = sizeof(Matrix4x4);

  static bool UpdateMatrixAddress();
  static bool ReadMatrixFromMemory();
  static void UpdaterTick();
};
