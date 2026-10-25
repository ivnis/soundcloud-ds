#include "keyboard.h"
#include "text.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

unsigned makeKeyboard(KeyboardKey* keys, bool russian, bool numbers, bool server) {
    if (server) {
        const char* rows[] = {"123.", "456:", "7890"};
        unsigned count = 0;
        for (int row = 0; row < 3; ++row) for (int col = 0; col < 4; ++col) {
            keys[count] = KeyboardKey{8 + col * 60, 62 + row * 23, 57, 21, {}, KeyAction::Letter};
            keys[count++].label[0] = rows[row][col];
        }
        keys[count++] = KeyboardKey{8, 133, 240, 23, "Delete", KeyAction::Backspace};
        keys[count++] = KeyboardKey{8, 164, 240, 23, "Apply server", KeyAction::Search};
        return count;
    }
    const char* latin[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
    const char* cyrillic[] = {"йцукенгшщзх", "фывапролджэ", "ячсмитьбюё"};
    const char* digits[] = {"1234567890", "-_.@&+!?/", "()'\":,;#"};
    unsigned count = 0;
    for (int row = 0; row < 3; ++row) {
        const char* text = numbers ? digits[row] : russian ? cyrillic[row] : latin[row];
        unsigned letters = codepointCount(text);
        int width = 238 / letters;
        int left = (256 - width * letters) / 2;
        for (unsigned col = 0; col < letters; ++col) {
            auto& key = keys[count++];
            key = KeyboardKey{left + static_cast<int>(col) * width, 62 + row * 23, width - 2, 21, {}, KeyAction::Letter};
            const char* start = text;
            nextCodepoint(text);
            auto length = static_cast<std::size_t>(text - start);
            std::memcpy(key.label, start, length);
            key.label[length] = 0;
        }
    }
    keys[count++] = KeyboardKey{8, 133, 42, 23, "", KeyAction::Language};
    std::snprintf(keys[count - 1].label, 12, "%s", russian ? "RU" : "EN");
    keys[count++] = KeyboardKey{53, 133, 39, 23, "", KeyAction::Numbers};
    std::snprintf(keys[count - 1].label, 12, "%s", numbers ? "ABC" : "123");
    keys[count++] = KeyboardKey{95, 133, 97, 23, "space", KeyAction::Space};
    keys[count++] = KeyboardKey{195, 133, 53, 23, "delete", KeyAction::Backspace};
    keys[count++] = KeyboardKey{8, 164, 240, 23, "Search", KeyAction::Search};
    return count;
}

int keyboardHit(const KeyboardKey* keys, unsigned count, int x, int y) {
    for (unsigned i = 0; i < count; ++i) {
        const auto& k = keys[i];
        if (x >= k.x && x < k.x + k.w && y >= k.y && y < k.y + k.h) return i;
    }
    return -1;
}

int keyboardMove(const KeyboardKey* keys, unsigned count, int current, int dx, int dy) {
    if (current < 0 || static_cast<unsigned>(current) >= count) return 0;
    int best = current, score = 1000000;
    const auto& from = keys[current];
    for (unsigned i = 0; i < count; ++i) {
        int x = keys[i].x + keys[i].w / 2 - from.x - from.w / 2;
        int y = keys[i].y + keys[i].h / 2 - from.y - from.h / 2;
        if ((dx && x * dx <= 0) || (dy && y * dy <= 0)) continue;
        int distance = dx ? std::abs(x) + std::abs(y) * 5 : std::abs(y) + std::abs(x) * 3;
        if (distance < score) { score = distance; best = i; }
    }
    return best;
}
