// Native render preview: preview_ui ARTWORK_PACKET OUTPUT_DIRECTORY
#include "ui.h"
#include "artwork.h"
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <cstdio>

static Canvas top, bottom;
static Artwork artwork;

void save(const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    out << "P6\n256 384\n255\n";
    for (const Canvas* canvas : {&top, &bottom}) for (uint16_t pixel : canvas->pixels) {
        char rgb[3] = {static_cast<char>((pixel & 31) * 255 / 31),
                       static_cast<char>(((pixel >> 5) & 31) * 255 / 31),
                       static_cast<char>(((pixel >> 10) & 31) * 255 / 31)};
        out.write(rgb, 3);
    }
}

int main(int argc, char** argv) {
    if (argc != 3) return 1;
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (!parseArtwork(bytes.data(), bytes.size(), artwork)) return 2;
    SearchResults results;
    results.count = 4;
    const char* titles[] = {"Lux Aeterna", "Flickermood", "Voca Nomen Tuum", "Journeyman"};
    for (unsigned i = 0; i < results.count; ++i) {
        std::snprintf(results.tracks[i].id, 24, "%u", i + 1);
        std::snprintf(results.tracks[i].title, 481, "%s", titles[i]);
        std::snprintf(results.tracks[i].artist, 321, "Forss");
        results.tracks[i].durationMs = 213000 + i * 60000;
    }
    UiModel model;
    model.results = &results; model.title = "Lux Aeterna"; model.artist = "Forss";
    model.status = PlayStatus::Playing; model.wifiPhase = 2; model.networkName = "melonAP";
    model.elapsed = 37; model.elapsedMs = 37000; model.duration = 370; model.durationMs = 370201;
    model.cover = artwork.pixels; model.accent = artwork.accent; model.onAccent = artwork.ink;
    uint8_t levels[32];for(unsigned i=0;i<32;++i)levels[i]=40+(i*57)%190;
    model.visualLevels=levels;
    model.server = "10.64.0.1:8765"; model.settingsMessage = "Server is ready";
    std::string folder = argv[2];
    for (Page page : {Page::Player, Page::Settings, Page::Browse, Page::Keyboard}) {
        model.page = page; model.query = page == Page::Keyboard ? model.server : "Forss";
        model.editingServer = page == Page::Keyboard; model.searched = true;
        drawUi(top, bottom, model);
        save(folder + "/" + (page == Page::Player ? "player" : page == Page::Settings ? "settings" : page == Page::Browse ? "search" : "server") + ".ppm");
    }
    model.page=Page::Browse;model.searched=false;model.editingServer=false;model.query="";
    model.title="";model.artist="";model.cover=nullptr;model.visualLevels=nullptr;model.status=PlayStatus::Ready;
    drawUi(top,bottom,model);save(folder+"/welcome.ppm");
    model.title="Lux Aeterna";model.artist="Forss";model.cover=artwork.pixels;model.visualLevels=levels;model.status=PlayStatus::Playing;model.searched=true;
    model.page=Page::Player;model.showVolume=true;
    drawUi(top,bottom,model);save(folder+"/volume.ppm");
    model.showVolume=false;model.page=Page::Browse;
    model.title="A long track title that scrolls inside the bottom mini player";
    model.titleFrame=0;drawUi(top,bottom,model);save(folder+"/marquee-start.ppm");
    model.titleFrame=330;drawUi(top,bottom,model);save(folder+"/marquee-end.ppm");
    model.page = Page::Player; model.status = PlayStatus::Paused;
    drawUi(top, bottom, model); save(folder + "/paused.ppm");
    model.page=Page::Browse;model.wifiPhase=0;model.networkCount=3;model.networkTotal=3;model.networkSelected=1;
    const char* networks[]={"Open network","Home network","Legacy network"};
    const char* security[]={"Open","WPA2","WEP"};
    for(unsigned i=0;i<3;++i){std::snprintf(model.networks[i],34,"%s",networks[i]);std::snprintf(model.networkSecurity[i],9,"%s",security[i]);model.networkCompatible[i]=i!=1;}
    model.message="WPA2 requires native DSi mode";
    drawUi(top,bottom,model);save(folder+"/wifi-scan.ppm");
    model.page=Page::Keyboard;model.editingServer=false;model.editingWifi=true;
    model.passwordLabel="WPA2 password";model.query="************";model.numberKeyboard=false;model.russianKeyboard=false;
    drawUi(top,bottom,model);save(folder+"/wifi-password.ppm");
    model.numberKeyboard=true;model.passwordExtras=true;
    drawUi(top,bottom,model);save(folder+"/wifi-symbols.ppm");

    model.page=Page::WifiWarning;model.networkName="POCO-DS";
    model.wifiWarning="DS Fat/Lite cannot use WPA/WPA2.\nThis test will not unlock support.";
    drawUi(top,bottom,model);save(folder+"/wifi-warning.ppm");

}
