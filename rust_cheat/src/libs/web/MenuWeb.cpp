#include "MenuWeb.h"

#include <atomic>
#include <cstdio>

namespace webmenu {
namespace {

HWND g_game_hwnd = nullptr;
bool g_open = false;
std::atomic<bool> g_binding_key{false};

} // namespace

void Init(HWND gameHwnd) {
  g_game_hwnd = gameHwnd;
  printf("[menu] WebView2 bridge init (game hwnd %p)\n", (void *)gameHwnd);
}

void Shutdown() {
  g_open = false;
  g_game_hwnd = nullptr;
}

bool SetOpen(bool open) {
  g_open = open;
  return true;
}

void Tick() {}

void Notify(const char *msg, int durationMs) {
  (void)durationMs;
  if (msg)
    printf("[menu] %s\n", msg);
}

bool BindingKey() { return g_binding_key.load(std::memory_order_relaxed); }

} // namespace webmenu
