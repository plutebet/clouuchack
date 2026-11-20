#pragma once

#include <windows.h>

namespace webmenu {

void Init(HWND gameHwnd);
void Shutdown();
bool SetOpen(bool open);
void Tick();
void Notify(const char *msg, int durationMs = 2600);
bool BindingKey();

} // namespace webmenu
