#include "autoplay.h"
#include "wifi_key.h"
#include "wifi_scan.h"
#include "http.h"
#include "ring.h"
#include "search.h"
#include "text.h"
#include "keyboard.h"
#include "seek.h"
#include "endpoint.h"
#include "artwork.h"
#include "ui.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static bool sink(void* context, const uint8_t* data, std::size_t count) {
    auto& out = *static_cast<std::vector<uint8_t>*>(context);
    out.insert(out.end(), data, data + count);
    return true;
}

static const std::string headers = "HTTP/1.1 200 OK\r\n"
    "X-Audio-Rate: 22050\r\nX-Audio-Channels: 1\r\nX-Audio-Format: s16le\r\n"
    "X-Track-Title: Hello%20DS%21\r\nX-Track-Artist: Tester\r\n";

static void parseEveryBoundary(const std::string& wire, const std::string& expected) {
    for (std::size_t split = 0; split <= wire.size(); ++split) {
        HttpAudio parser;
        std::vector<uint8_t> out;
        assert(parser.feed(reinterpret_cast<const uint8_t*>(wire.data()), split, sink, &out));
        assert(parser.feed(reinterpret_cast<const uint8_t*>(wire.data() + split), wire.size() - split, sink, &out));
        assert(parser.eof());
        assert(std::string(out.begin(), out.end()) == expected);
        assert(std::strcmp(parser.title(), "Hello DS!") == 0);
    }
}

int main() {
    Ring<8> ring;
    uint8_t input[] = {0,1,2,3,4,5,6,7,8,9}, output[10]{};
    assert(ring.write(input, 6) == 6);
    assert(ring.read(output, 4) == 4);
    assert(std::memcmp(output, input, 4) == 0);
    assert(ring.write(input + 6, 4) == 4); // wrap
    assert(ring.read(output, 10) == 6);
    assert(std::memcmp(output, input + 4, 6) == 0);
    assert(ring.write(input, 10) == 8); // full != empty
    assert(ring.free() == 0);
    ring.clear();
    assert(ring.size() == 0 && ring.free() == 8);

    SeekBuffer<20, 8, 12> window;
    uint8_t sequence[40], sample[16]{};
    for (unsigned i = 0; i < sizeof(sequence); ++i) sequence[i] = i;
    assert(window.write(sequence, 20) == 12);
    assert(window.read(sample, 8) == 8 && std::memcmp(sample, sequence, 8) == 0);
    assert(window.write(sequence + 12, 8) == 8);
    assert(window.read(sample, 8) == 8 && std::memcmp(sample, sequence + 8, 8) == 0);
    assert(window.firstSample() == 4);
    assert(window.seekSamples(5));
    assert(window.read(sample, 4) == 4 && std::memcmp(sample, sequence + 10, 4) == 0);
    assert(!window.seekSamples(3));
    assert(window.write(sequence + 20, 6) == 6); // write wraps the physical ring
    assert(window.seekSamples(12));
    assert(window.read(sample, 2) == 2 && std::memcmp(sample, sequence + 24, 2) == 0);
    assert(!window.seekSamples(14));
    window.clear(5000);
    assert(window.positionSamples() == 5000 && window.firstSample() == 5000);
    assert(window.write(sequence, 12) == 12);
    assert(window.read(sample, 5) == 4); // never split a 16-bit PCM sample
    assert(window.seekSamples(5000));
    assert(window.read(sample, 4) == 4 && std::memcmp(sample, sequence, 4) == 0);
    window.clear();
    assert(window.write(sequence, 12) == 12);
    assert(!window.advanceToSample(15));
    assert(window.positionSamples() == 6 && window.free() == 12);
    assert(window.write(sequence + 12, 12) == 12);
    assert(!window.advanceToSample(15));
    assert(window.write(sequence + 24, 12) == 12);
    assert(window.advanceToSample(15));
    assert(window.read(sample, 4) == 4 && std::memcmp(sample, sequence + 30, 4) == 0);
    assert(seekStepMs(1000, -5000, 30000) == 0);
    assert(seekStepMs(28000, 5000, 30000) == 30000);
    assert(seekStepMs(10000, -5000, 30000) == 5000);
    assert(seekTouchMs(-10, 30000) == 0 && seekTouchMs(300, 30000) == 30000);
    assert(seekTouchMs(128, 30000) == 15000);
    assert(seekTouchMs(240, 86400000) == 86400000);
    ServerEndpoint endpoint{};
    assert(parseEndpoint(" http://192.168.001.010:8000/ ", endpoint));
    char address[40]; formatEndpoint(endpoint, address, sizeof(address));
    assert(std::strcmp(address, "192.168.1.10:8000") == 0);
    for (const char* invalid : {"https://1.2.3.4:80", "host:80", "1.2.3:80", "256.1.1.1:80",
                               "1.2.3.4:0", "1.2.3.4:65536", "1.2.3.4:80/path", "1.2.3.4:", ""})
        assert(!parseEndpoint(invalid, endpoint));
    assert(emulatorMode() && std::strcmp(currentEndpoint().host,"10.64.0.1")==0 && currentEndpoint().port==8765);
    auto previous = currentEndpoint();
    assert(parseEndpoint("10.64.0.1:8765", endpoint));
    setEndpoint(endpoint); assert(currentEndpoint().port == 8765); setEndpoint(previous);
    assert(parseHealth("{\"status\": \"ok\", \"busy\":false}", 30));
    const std::string badHealth = "{\"status\":\"wrong\"}";
    assert(!parseHealth(badHealth.data(), badHealth.size()));

    uint8_t image[kArtworkBytes]{};
    std::memcpy(image, "SCAR", 4);
    auto word = [&](unsigned offset, unsigned value) { image[offset] = value & 255; image[offset + 1] = value >> 8; };
    word(4,1); word(6,64); word(8,64); word(10,0x83ff); word(12,0x8000); word(14,256);
    word(16,0x801f); word(18,0xfc00);
    image[528 + 100] = 1;
    static Artwork art;
    assert(parseArtwork(image, sizeof(image), art));
    assert(art.valid && art.accent == 0x83ff && art.pixels[0] == 0x801f && art.pixels[100] == 0xfc00);
    assert(!parseArtwork(image, sizeof(image) - 1, art) && !art.valid);
    image[4] = 2; assert(!parseArtwork(image, sizeof(image), art)); image[4] = 1;

    static Canvas canvas;
    canvas.clear(0);
    canvas.centerText(20, 30, 100, 24, "Next", 0xffff);
    int minX=256,maxX=-1,minY=192,maxY=-1;
    for(int y=0;y<192;++y)for(int x=0;x<256;++x)if(canvas.pixels[y*256+x]){
        minX=std::min(minX,x);maxX=std::max(maxX,x);minY=std::min(minY,y);maxY=std::max(maxY,y);
    }
    assert(std::abs((minX+maxX)-139)<=1 && std::abs((minY+maxY)-83)<=1);
    for (bool pause : {false,true}) {
        canvas.clear(0); canvas.play(128,92,0xffff,pause,18);
        double mass=0,sumX=0,sumY=0;
        for(int y=0;y<192;++y)for(int x=0;x<256;++x)if(canvas.pixels[y*256+x]){++mass;sumX+=x;sumY+=y;}
        assert(mass && std::abs(sumX/mass-128)<1.5 && std::abs(sumY/mass-92)<1.0);
    }

    parseEveryBoundary(headers + "\r\nabcdefgh", "abcdefgh");
    parseEveryBoundary(headers + "Content-Length: 8\r\n\r\nabcdefgh", "abcdefgh");
    parseEveryBoundary(headers + "Transfer-Encoding: chunked\r\n\r\n"
        "3;foo=bar\r\nabc\r\n5\r\ndefgh\r\n0\r\nX-Foo: bar\r\n\r\n", "abcdefgh");

    for (const auto& wire : std::vector<std::string>{
        "HTTP/1.1 409 Conflict\r\n\r\n",
        headers + "Content-Length: -1\r\n\r\n",
        headers + "Content-Length: 8\r\nTransfer-Encoding: chunked\r\n\r\n",
        headers + "Content-Encoding: gzip\r\n\r\n",
        headers + "Content-Length: 999999999999999999999999\r\n\r\n",
        headers + "Transfer-Encoding: chunked\r\n\r\nxyz\r\n",
        headers + "Transfer-Encoding: chunked\r\n\r\n2\r\nabxx",
        headers + "X-Big: " + std::string(3000, 'a') + "\r\n\r\n",
    }) {
        HttpAudio parser;
        std::vector<uint8_t> out;
        assert(!parser.feed(reinterpret_cast<const uint8_t*>(wire.data()), wire.size(), sink, &out));
        assert(parser.error());
    }

    for (const auto& wire : std::vector<std::string>{
        headers + "Content-Length: 20\r\n\r\nabc",
        headers + "Transfer-Encoding: chunked\r\n\r\n2\r\nab\r\n",
        headers + "X-Track-Duration-Ms: 10000\r\n\r\nab",
        headers + "\r\nabc", // incomplete 16-bit sample
        headers + "\r\n", // no PCM
    }) {
        HttpAudio parser;
        std::vector<uint8_t> out;
        assert(parser.feed(reinterpret_cast<const uint8_t*>(wire.data()), wire.size(), sink, &out));
        assert(!parser.eof());
    }

    // Thousands of small chunks must not be treated as oversized headers.
    HttpAudio parser;
    std::vector<uint8_t> out;
    std::string wire = headers + "Transfer-Encoding: chunked\r\n\r\n";
    for (int i = 0; i < 20000; ++i) wire += "2\r\nab\r\n";
    wire += "0\r\n\r\n";
    for (unsigned char c : wire) assert(parser.feed(&c, 1, sink, &out));
    assert(parser.eof());
    assert(out.size() == 40000);
    HttpAudio jsonHttp(false);
    out.clear();
    std::string jsonWire = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\n{} ";
    for (unsigned char c : jsonWire) assert(jsonHttp.feed(&c, 1, sink, &out));
    assert(jsonHttp.eof() && jsonHttp.done());
    HttpAudio seekHttp;
    std::vector<uint8_t> seekOut;
    std::string seekWire = headers + "X-Track-Duration-Ms: 12000\r\nX-Audio-Start-Ms: 11000\r\n\r\n" + std::string(44100, '\0');
    assert(seekHttp.feed(reinterpret_cast<const uint8_t*>(seekWire.data()), seekWire.size(), sink, &seekOut));
    assert(seekHttp.eof()); // One second remains, rather than twelve seconds.
    assert(seekHttp.startMs() == 11000 && seekHttp.durationMs() == 12000);
    HttpAudio invalidSeek;
    seekWire = headers + "X-Track-Duration-Ms: 12000\r\nX-Audio-Start-Ms: 13000\r\n\r\nab";
    assert(invalidSeek.feed(reinterpret_cast<const uint8_t*>(seekWire.data()), seekWire.size(), sink, &seekOut));
    assert(!invalidSeek.eof());

    SearchResults results;
    std::string json = R"({"tracks":[{"id":"42","title":"\u041c\u0443\u0437\u044b\u043a\u0430 \ud83c\udfb5","artist":"A \"quote\"","duration_ms":123000}],"has_more":true,"offset":12,"next_offset":24})";
    assert(parseSearch(json.data(), json.size(), results));
    assert(results.count == 1 && results.hasMore && results.nextOffset == 24);
    assert(std::strcmp(results.tracks[0].title, u8"Музыка 🎵") == 0);
    assert(std::strcmp(results.tracks[0].artist, "A \"quote\"") == 0);
    for (std::size_t cut = 0; cut < json.size(); ++cut) assert(!parseSearch(json.data(), cut, results));
    for (const auto& invalid : std::vector<std::string>{
        R"({"tracks":[{"id":"http://evil","title":"x"}]})",
        R"({"tracks":[{"id":"42","title":"\ud800"}]})",
        R"({"tracks":[],"offset":999999999999999999999999})",
        R"({"tracks":[],"has_more":tru})",
        R"({"tracks":[]} garbage)",
    }) assert(!parseSearch(invalid.data(), invalid.size(), results));
    assert(parseSearch("{\"tracks\":[]}", 13, results) && results.count == 0);

    // Related queue skips the seed and recently played tracks without wrapping.
    SearchResults related;
    related.count=4;
    for(unsigned i=0;i<4;++i)std::snprintf(related.tracks[i].id,24,"%u",i+1);
    char recent[2][24]={"2","3"};
    assert(nextRecommendation(related,"1",recent,2)==3);
    assert(nextRecommendation(related,"4",recent,2)==0);
    related.count=3;
    assert(nextRecommendation(related,"1",recent,2)==-1);
    related.count=0;
    assert(nextRecommendation(related,"1",recent,2)==-1);

    selectEmulatorMode(true);
    ServerEndpoint emulator=currentEndpoint();
    selectEmulatorMode(false);
    ServerEndpoint console{};
    assert(parseEndpoint("192.168.7.15:8765",console));setEndpoint(console);
    selectEmulatorMode(true);
    assert(std::strcmp(currentEndpoint().host,emulator.host)==0);
    selectEmulatorMode(false);
    assert(std::strcmp(currentEndpoint().host,"192.168.7.15")==0);
    selectEmulatorMode(true);

    char query[193] = "";
    assert(appendText(query, sizeof(query), u8"музыка 🎵"));
    assert(codepointCount(query) == 8);
    eraseCodepoint(query);
    assert(std::strcmp(query, u8"музыка ") == 0);
    char encoded[580];
    assert(urlEncode(u8"a&б +", encoded, sizeof(encoded)));
    assert(std::strcmp(encoded, "a%26%D0%B1%20%2B") == 0);
    assert(!urlEncode("&", encoded, 3));
    KeyboardKey keyboard[48];
    for (bool russian : {false, true}) for (bool numbers : {false, true}) {
        unsigned count = makeKeyboard(keyboard, russian, numbers);
        assert(count <= 48);
        for (unsigned i = 0; i < count; ++i) {
            auto& key = keyboard[i];
            assert(keyboardHit(keyboard, count, key.x + key.w / 2, key.y + key.h / 2) == static_cast<int>(i));
            int next = keyboardMove(keyboard, count, i, 1, 0);
            assert(next >= 0 && static_cast<unsigned>(next) < count);
        }
        assert(keyboardHit(keyboard, count, 0, 0) == -1);
    }
    unsigned serverKeys = makeKeyboard(keyboard, false, false, true);
    assert(serverKeys == 14 && std::strcmp(keyboard[13].label,"Apply server") == 0);
    for(unsigned i=0;i<serverKeys;++i)assert(keyboardHit(keyboard,serverKeys,keyboard[i].x+3,keyboard[i].y+3)==static_cast<int>(i));
    WifiKey wifiKey;
    const uint8_t scanBssid[6]={0,1,2,0xa3,0xb4,0xc5};
    char scanName[34];
    formatWifiName(scanName,sizeof(scanName),"",0,scanBssid);
    assert(std::strcmp(scanName,"Hidden A3:B4:C5")==0);
    formatWifiName(scanName,sizeof(scanName),"POCO-DS",7,scanBssid);
    assert(std::strcmp(scanName,"POCO-DS")==0);
    formatWifiName(scanName,sizeof(scanName),"bad\nname",8,scanBssid);
    assert(std::strcmp(scanName,"bad?name")==0);
    char clipped[4];formatWifiName(clipped,sizeof(clipped),"POCO-DS",7,scanBssid);
    assert(std::strcmp(clipped,"POC")==0);
    char empty[1]={'x'};formatWifiName(empty,sizeof(empty),"",0,scanBssid);assert(empty[0]==0);
    formatWifiName(nullptr,0,"",0,scanBssid);
    assert(supportsWifiNetwork(WifiSecurity::Open,true,false,false,false));
    assert(supportsWifiNetwork(WifiSecurity::Wep,true,false,false,false));
    assert(!supportsWifiNetwork(WifiSecurity::Wpa2,true,false,true,true)); // DS mode.
    assert(supportsWifiNetwork(WifiSecurity::Wpa2,true,true,true,true));
    assert(!supportsWifiNetwork(WifiSecurity::Wpa2,true,true,false,true)); // Enterprise.
    assert(!supportsWifiNetwork(WifiSecurity::Wpa2,true,true,true,false)); // TKIP.
    assert(!supportsWifiNetwork(WifiSecurity::Wpa2,false,true,true,true));
    assert(!supportsWifiNetwork(WifiSecurity::Unsupported,true,true,true,true));
    assert(parseWifiKey(WifiSecurity::Wep,"6162636465",wifiKey));
    assert(wifiKey.length==5 && std::memcmp(wifiKey.bytes,"abcde",5)==0);
    assert(parseWifiKey(WifiSecurity::Wep,"ABCDEF0123456789abcdef0123",wifiKey) && wifiKey.length==13);
    assert(parseWifiKey(WifiSecurity::Wep,"abcde",wifiKey) && wifiKey.length==5);
    assert(parseWifiKey(WifiSecurity::Wep,"ThirteenChars",wifiKey) && wifiKey.length==13);
    for(const char* invalid:{"", "four", "123456", "zzzzzzzzzz", "616263646x"}){
        assert(!parseWifiKey(WifiSecurity::Wep,invalid,wifiKey));
        assert(wifiKey.length==0);for(auto byte:wifiKey.bytes)assert(byte==0);
    }
    assert(parseWifiKey(WifiSecurity::Wpa2," Case$ensitive! ",wifiKey));
    assert(wifiKey.length==16 && std::memcmp(wifiKey.bytes," Case$ensitive! ",16)==0);
    std::string longest(63,'a'),tooLong(64,'a');
    assert(parseWifiKey(WifiSecurity::Wpa2,longest.c_str(),wifiKey) && wifiKey.length==63);
    assert(!parseWifiKey(WifiSecurity::Wpa2,tooLong.c_str(),wifiKey));
    assert(!parseWifiKey(WifiSecurity::Wpa2,"short",wifiKey));
    assert(!parseWifiKey(WifiSecurity::Wpa2,"nonascii\xc3\xa9",wifiKey));
    assert(!parseWifiKey(WifiSecurity::Unsupported,"aPassword",wifiKey));
    assert(parseWifiKey(WifiSecurity::Open,"",wifiKey));
    assert(!parseWifiKey(WifiSecurity::Open,"password",wifiKey));
    assert(parseWifiKey(WifiSecurity::Wpa2,"aPassword",wifiKey));
    wipeWifiSecret(&wifiKey,sizeof(wifiKey));
    assert(wifiKey.length==0);for(auto byte:wifiKey.bytes)assert(byte==0);
    bool printable[127]{};printable[32]=true; // Space has its own key.
    for(unsigned mode=0;mode<3;++mode)for(bool shifted:{false,true}){
        unsigned count=makeKeyboard(keyboard,false,mode!=0,false,true,shifted,mode==2);
        assert(count<=48 && std::strcmp(keyboard[count-1].label,"Connect")==0);
        for(unsigned i=0;i<count;++i){
            assert(keyboardHit(keyboard,count,keyboard[i].x+2,keyboard[i].y+2)==static_cast<int>(i));
            if(keyboard[i].action==KeyAction::Letter){
                unsigned c=static_cast<unsigned char>(keyboard[i].label[0]);
                assert(c>=33 && c<=126 && keyboard[i].label[1]==0);printable[c]=true;
            }
        }
    }
    for(unsigned c=32;c<127;++c)assert(printable[c]);
    std::cout << "Client ring and HTTP tests passed\n";
}
