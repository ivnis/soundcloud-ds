#pragma once
#include <cstdint>
#include "keyboard.h"
#include "search.h"

enum class Page { Browse, Keyboard, Player, Settings, WifiWarning };
enum class PlayStatus { Ready, Connecting, Seeking, Buffering, Playing, Paused, Stopped, Finished, Error };

struct UiModel {
    Page page = Page::Browse;
    PlayStatus status = PlayStatus::Ready;
    int wifiPhase = 0; // scan / joining / connected
    char networks[5][34]{};
    char networkSecurity[5][9]{};
    bool networkCompatible[5]{};
    unsigned networkCount = 0, networkSelected = 0;
    unsigned networkTotal=0, networkFirst=0;
    const char* message = "";
    const char* title = "";
    const char* artist = "";
    const char* trackId = "";
    const char* query = "";
    const char* searchMessage = "";
    const SearchResults* results = nullptr;
    bool searching = false, searched = false;
    int selected = 0, first = 0, volume = 96, keyboardSelected = 0;
    unsigned elapsed = 0, duration = 0, bufferPercent = 0, underruns = 0, frame = 0;
    unsigned titleFrame = 0;
    unsigned serverFrame = 0;
    unsigned elapsedMs = 0, durationMs = 0;
    bool scrubbing = false;
    bool russianKeyboard = false, numberKeyboard = false, diagnostics = false;
    bool editingServer = false, editorError = false, settingsBusy = false, settingsError = false;
    bool editingWifi=false, passwordVisible=false, passwordShifted=false, passwordExtras=false;
    const char* passwordLabel="Wi-Fi password";
    const char* wifiWarning="";
    const char* server = "";
    const char* networkName = "";
    const char* settingsMessage = "";
    int settingSelected = 0;
    bool emulatorMode = true, showVolume = false;
    const uint8_t* visualLevels = nullptr;
    const uint16_t* cover = nullptr;
    uint16_t accent = 0x8000 | 31 | (13 << 5) | (5 << 10);
    uint16_t onAccent = 0x8000 | 1 | (2 << 5) | (3 << 10);
};

struct alignas(32) Canvas {
    uint16_t pixels[256 * 192]{};
    void clear(uint16_t color);
    void rect(int x, int y, int w, int h, uint16_t color);
    void round(int x, int y, int w, int h, uint16_t color);
    void line(int x1, int y1, int x2, int y2, uint16_t color);
    void circle(int x, int y, int radius, uint16_t color);
    void text(int x, int y, const char* text, uint16_t color, int width = 248, int lines = 1, int scale = 1);
    void centerText(int x, int y, int w, int h, const char* text, uint16_t color, int lines = 1);
    void play(int cx, int cy, uint16_t color, bool paused, int height = 10);
};

void drawUi(Canvas& top, Canvas& bottom, const UiModel& model);
