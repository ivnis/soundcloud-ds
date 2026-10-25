#pragma once
#include <cstddef>

enum class KeyAction { Letter, Language, Numbers, Space, Backspace, Search };
struct KeyboardKey {
    int x, y, w, h;
    char label[16];
    KeyAction action;
};
unsigned makeKeyboard(KeyboardKey* keys, bool russian, bool numbers, bool server = false);
int keyboardHit(const KeyboardKey* keys, unsigned count, int x, int y);
int keyboardMove(const KeyboardKey* keys, unsigned count, int current, int dx, int dy);
