#include <nds.h>
#include <dswifi9.h>
#include <maxmod9.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "config.h"
#include "keyboard.h"
#include "request.h"
#include "seek.h"
#include "search.h"
#include "text.h"
#include "ui.h"
#include "artwork.h"
#include "endpoint.h"
#include "autoplay.h"
#include "layout.h"

static SeekBuffer<kBufferBytes, kHistoryBytes, kAheadBytes> audio;
static NetRequest audioRequest(true), searchRequest(false);
static NetRequest artworkRequest(false), settingsRequest(false), relatedRequest(false);
static SearchResults recommendations;
static char relatedBody[16384]{}, recentTracks[24][24]{};
static unsigned relatedSize=0, relatedAt=0, relatedRetries=0, recentCount=0, recentCursor=0;
static bool relatedPending=false, relatedBusy=false, autoAdvance=false;
static uint8_t visualTarget[32]{}, visualLevels[32]{};
static void pollRelated();
static Artwork artwork;
static uint8_t artworkBody[kArtworkBytes]{};
static unsigned artworkSize = 0, artworkAt = 0, artworkRetries = 0;
static bool artworkPending = false, artworkAttempted = false;
static char settingsBody[512]{}, settingsMessage[96]{}, serverLabel[40]{}, serverDraft[64]{}, networkName[34]{};
static unsigned settingsSize = 0;
static bool settingsBusy = false, settingsError = false, editingServer = false, editorError = false, wifiBackToSettings = false;
static Page settingsReturn = Page::Browse;
static int settingSelected = 0;
static SearchResults results, incomingResults;
static Canvas topCanvas, bottomCanvas;
static uint16_t *topVram, *bottomVram;
static bool paintPending = false;
static UiModel model;
static unsigned frame = 0, wifiStarted = 0, playedSamples = 0, underruns = 0, drainFrame = 0;
static bool streamOpen = false, paused = false, buffering = true, receivedEOF = false, metadataRead = false;
static int volume = 96, selectedAP = 0, selected = 0, firstResult = 0;
static unsigned volumeChangedAt = 0;
static bool volumeTouched = false;
static char title[481]{}, artist[321]{}, trackId[24]{}, message[128]{};
static char query[193]{}, queryBackup[193]{}, submitted[193]{}, searchMessage[128]{};
static char searchBody[16384]{};
static unsigned searchSize = 0, searchOffset = 0, searchStart = 0, searchAt = 0;
static unsigned audioStart = 0, audioAt = 0;
static bool audioPending = false, searchPending = false, searching = false, searched = false;
static bool russianKeyboard = false, numberKeyboard = false, diagnostics = false;
static int keyboardSelected = 0;
static Page page = Page::Browse;
enum class Wifi { Scan, Joining, Ready };
enum class Playback { Stopped, Loading, Active, Finished, Error };
static Wifi wifi = Wifi::Scan;
static Playback playback = Playback::Stopped;
static unsigned trackDurationMs = 0, streamStartMs = 0, previewMs = 0;
static unsigned forwardTargetSamples = 0;
static bool scrubbing = false, seeking = false;

static void resetArtwork() {
    artworkRequest.cancel(); artwork.clear(); artworkSize = artworkRetries = 0;
    artworkPending = artworkAttempted = false;
}

static unsigned positionMs() {
    return static_cast<unsigned>(static_cast<uint64_t>(playedSamples) * 1000 / kSampleRate);
}

static void stopAudio(bool resetTime = true) {
    if (streamOpen) mmStreamClose();
    streamOpen = false;
    audioRequest.cancel();
    audioPending = false;
    scrubbing = seeking = false;
    autoAdvance = false;
    forwardTargetSamples = 0;
    audio.clear();
    paused = false; buffering = true; receivedEOF = false;
    metadataRead = false; drainFrame = 0;
    if (resetTime) { playedSamples = 0; streamStartMs = 0; }
    playback = Playback::Stopped;
}

static void failAudio(const char* error) {
    std::snprintf(message, sizeof(message), "%s", error);
    streamStartMs = positionMs();
    bool keepPaused = paused;
    stopAudio(false);
    paused = keepPaused;
    playback = Playback::Error;
}

static mm_word audioCallback(mm_word samples, mm_addr destination, mm_stream_formats) {
    auto* out = static_cast<uint8_t*>(destination);
    std::size_t bytes = samples * 2;
    std::memset(out, 0, bytes);
    std::memset(visualTarget, 0, sizeof(visualTarget));
    if (paused) return samples;
    if (buffering) {
        if (audio.size() >= kPrefillBytes || receivedEOF) buffering = false;
        else return samples;
    }
    if (audio.size() < bytes && !receivedEOF) { buffering = true; ++underruns; return samples; }
    bytes = std::min(bytes, audio.size() & ~std::size_t(1));
    audio.read(out, bytes);
    playedSamples += bytes / 2;
    const auto* pcm = reinterpret_cast<const int16_t*>(out);
    unsigned count = bytes / 2;
    for (unsigned i=0;i<count;i+=4) {
        int sample=pcm[i]; unsigned magnitude=sample<0?-sample:sample;
        unsigned band=i*32/count;
        visualTarget[band]=std::max(visualTarget[band],static_cast<uint8_t>(std::min(255u,magnitude/64)));
    }
    return samples;
}

static void openAudio() {
    mm_stream stream{};
    stream.sampling_rate = kSampleRate;
    stream.buffer_length = 2048;
    stream.callback = audioCallback;
    stream.format = MM_STREAM_16BIT_MONO;
    stream.timer = MM_TIMER0;
    stream.manual = true;
    mmStreamOpen(&stream);
    mmStreamVolume(volume);
    streamOpen = true;
}

static bool audioSink(void*, const uint8_t* bytes, std::size_t count) { return audio.write(bytes, count) == count; }
static bool searchSink(void*, const uint8_t* bytes, std::size_t count) {
    if (searchSize + count >= sizeof(searchBody)) return false;
    std::memcpy(searchBody + searchSize, bytes, count);
    searchSize += count;
    searchBody[searchSize] = 0;
    return true;
}

static bool artworkSink(void*, const uint8_t* bytes, std::size_t count) {
    if (artworkSize + count > sizeof(artworkBody)) return false;
    std::memcpy(artworkBody + artworkSize, bytes, count); artworkSize += count; return true;
}
static bool settingsSink(void*, const uint8_t* bytes, std::size_t count) {
    if (settingsSize + count >= sizeof(settingsBody)) return false;
    std::memcpy(settingsBody + settingsSize, bytes, count); settingsSize += count; settingsBody[settingsSize] = 0; return true;
}

static void playTrack(const TrackItem* item = nullptr) {
    stopAudio();
    relatedRequest.cancel(); relatedPending=relatedBusy=false; relatedRetries=0;
    recommendations=SearchResults{};
    if (item) {
        if (std::strcmp(trackId, item->id) != 0) resetArtwork();
        std::snprintf(trackId, sizeof(trackId), "%s", item->id);
        std::snprintf(title, sizeof(title), "%s", item->title);
        std::snprintf(artist, sizeof(artist), "%s", item->artist);
        model.duration = item->durationMs / 1000;
        trackDurationMs = item->durationMs;
    }
    if (*trackId) {
        bool seen=false;
        for(unsigned i=0;i<recentCount;++i) if(std::strcmp(recentTracks[i],trackId)==0) seen=true;
        if(!seen){std::snprintf(recentTracks[recentCursor],24,"%s",trackId);recentCursor=(recentCursor+1)%24;recentCount=std::min(24u,recentCount+1);}
    }
    relatedPending=true; relatedAt=frame+120;
    underruns = 0;
    message[0] = 0;
    playback = Playback::Loading;
    audioPending = true;
    audioStart = frame;
    // Give the old HTTP stream a chance to close before requesting another.
    audioAt = frame + 15;
}

static void seekTo(unsigned targetMs) {
    if (!*title || !trackDurationMs) return;
    targetMs = std::min(targetMs, trackDurationMs);
    if (targetMs == positionMs() && playback != Playback::Error) return;
    unsigned samples = static_cast<unsigned>(static_cast<uint64_t>(targetMs) * kSampleRate / 1000);
    bool keepPaused = paused || playback == Playback::Stopped || playback == Playback::Finished;
    if (targetMs == trackDurationMs) {
        stopAudio(false);
        playedSamples = samples; streamStartMs = targetMs;
        playback = Playback::Finished;
        return;
    }
    if (metadataRead && !audioPending && (playback == Playback::Active || playback == Playback::Finished || forwardTargetSamples) && audio.seekSamples(samples)) {
        if (streamOpen) mmStreamClose();
        streamOpen = false;
        playedSamples = samples;
        paused = keepPaused;
        buffering = audio.size() < kPrefillBytes && !receivedEOF;
        drainFrame = 0;
        forwardTargetSamples = 0; seeking = false; playback = Playback::Active;
        return;
    }
    if (metadataRead && audioRequest.active() && samples > audio.positionSamples() &&
        samples - audio.positionSamples() <= 30 * kSampleRate) {
        // Reuse the running stream for nearby forward jumps, draining its
        // prefix at network speed instead of restarting HLS on the server.
        if (streamOpen) mmStreamClose();
        streamOpen = false;
        paused = keepPaused;
        forwardTargetSamples = samples;
        playedSamples = samples;
        playback = Playback::Loading; seeking = true; drainFrame = 0;
        return;
    }
    stopAudio(false);
    audio.clear(samples);
    playedSamples = samples; streamStartMs = targetMs;
    paused = keepPaused;
    playback = Playback::Loading;
    seeking = true; audioPending = true;
    audioStart = frame; audioAt = frame + 15;
    message[0] = 0;
}

static void skipSeconds(int seconds) {
    seekTo(seekStepMs(positionMs(), seconds * 1000, trackDurationMs));
}

static void togglePlayback() {
    if (playback == Playback::Active) paused = !paused;
    else if (playback == Playback::Error && streamStartMs && streamStartMs < trackDurationMs) seekTo(streamStartMs);
    else if (playback != Playback::Loading) playTrack();
}

static void changeVolume(int amount) {
    volume = std::max(0, std::min(127, volume + amount));
    volumeTouched = true; volumeChangedAt = frame;
    if (streamOpen) mmStreamVolume(volume);
}

static void stepTrack(int direction) {
    if (!results.count) return;
    int current = -1;
    for (unsigned i = 0; i < results.count; ++i) if (std::strcmp(results.tracks[i].id, trackId) == 0) current = i;
    int target = current < 0 ? (direction > 0 ? 0 : static_cast<int>(results.count) - 1) : current + direction;
    if (target < 0 || static_cast<unsigned>(target) >= results.count) return;
    selected = target;
    firstResult = selected < firstResult ? selected : selected >= firstResult + 4 ? selected - 3 : firstResult;
    playTrack(&results.tracks[selected]);
}

static void pollAudio() {
    if (audioPending && frame >= audioAt) {
        char path[128];
        std::snprintf(path, sizeof(path), "/v1/audio?start_ms=%u%s%s", streamStartMs, *trackId ? "&track_id=" : "", trackId);
        audioPending = false;
        audioRequest.begin(path, frame);
    }
    audioRequest.poll(frame, static_cast<unsigned>(audio.free()), audioSink, nullptr);
    if (audioRequest.state() == NetRequest::State::Error) {
        int code = audioRequest.response().statusCode();
        if ((code == 409 || code == 503) && frame - audioStart < 60 * 38) {
            audioRequest.cancel(); audioPending = true; audioAt = frame + 60;
        } else {
            failAudio(code == 502 || code == 504 ? "Couldn't load this track. A to retry." : audioRequest.error());
        }
        return;
    }
    if (!metadataRead && (audioRequest.active() || audioRequest.state() == NetRequest::State::Done) &&
        audioRequest.response().headersDone()) {
        const auto& response = audioRequest.response();
        if (response.startMs() != streamStartMs) { failAudio("Server returned a different position."); return; }
        std::snprintf(title, sizeof(title), "%s", response.title());
        std::snprintf(artist, sizeof(artist), "%s", response.artist());
        model.duration = response.durationMs() / 1000;
        trackDurationMs = response.durationMs();
        metadataRead = true;
        if (!artwork.valid && !artworkAttempted) { artworkPending = true; artworkAt = frame + 30; }
        seeking = false;
        playback = Playback::Active;
    }
    if (audioRequest.state() == NetRequest::State::Done) {
        receivedEOF = true;
        audioRequest.cancel();
    }
    if (forwardTargetSamples) {
        if (audio.advanceToSample(forwardTargetSamples)) {
            playedSamples = forwardTargetSamples;
            forwardTargetSamples = 0; seeking = false; playback = Playback::Active;
            buffering = audio.size() < kPrefillBytes && !receivedEOF;
        } else if (receivedEOF) {
            playedSamples = audio.positionSamples();
            forwardTargetSamples = 0; seeking = false; playback = Playback::Finished;
        } else return;
    }
    if (playback == Playback::Active && !streamOpen && !paused &&
        (audio.size() >= kPrefillBytes || (receivedEOF && audio.size()))) openAudio();
    if (streamOpen) mmStreamUpdate();
    if (receivedEOF && !audio.size() && !paused && streamOpen) {
        if (!drainFrame) drainFrame = frame;
        if (frame - drainFrame >= 12) { mmStreamClose(); streamOpen = false; playback = Playback::Finished; autoAdvance = true; }
    }
}

static void openKeyboard() {
    editingServer = editorError = false;
    std::snprintf(queryBackup, sizeof(queryBackup), "%s", query);
    page = Page::Keyboard;
    keyboardSelected = 0;
}

static void pollArtwork() {
    if (artworkPending && frame >= artworkAt && metadataRead && !searching &&
        playback != Playback::Loading && (audio.size() >= kPrefillBytes || receivedEOF)) {
        char path[96]; std::snprintf(path, sizeof(path), "/v1/artwork%s%s", *trackId ? "?track_id=" : "", trackId);
        artworkSize = 0; artworkPending = false; artworkAttempted = true;
        artworkRequest.begin(path, frame);
    }
    if (playback == Playback::Active && !paused && audio.size() < kPrefillBytes && !receivedEOF) return;
    unsigned room = sizeof(artworkBody) - artworkSize;
    artworkRequest.poll(frame, std::min(room ? room : 1u, 1024u), artworkSink, nullptr);
    if (artworkRequest.state() == NetRequest::State::Done) {
        parseArtwork(artworkBody, artworkSize, artwork); artworkRequest.cancel();
    } else if (artworkRequest.state() == NetRequest::State::Error) {
        int code = artworkRequest.response().statusCode();
        if (code != 204 && code != 404 && artworkRetries++ < 2) { artworkPending = true; artworkAt = frame + 120; }
        artworkRequest.cancel();
    }
}

static bool relatedSink(void*,const uint8_t* bytes,std::size_t count) {
    if(relatedSize+count>=sizeof(relatedBody))return false;
    std::memcpy(relatedBody+relatedSize,bytes,count);relatedSize+=count;relatedBody[relatedSize]=0;return true;
}
static void pollRelated() {
    if(relatedPending && frame>=relatedAt && metadataRead && !searching &&
       playback!=Playback::Loading && (audio.size()>=kPrefillBytes || receivedEOF)) {
        char path[96];std::snprintf(path,sizeof(path),"/v1/related%s%s",*trackId?"?track_id=":"",trackId);
        relatedSize=0;relatedPending=false;relatedBusy=true;relatedRequest.begin(path,frame);
    }
    if(playback==Playback::Active && !paused && audio.size()<kPrefillBytes && !receivedEOF)return;
    if(relatedBusy){
        relatedRequest.poll(frame,std::min(4096u,static_cast<unsigned>(sizeof(relatedBody)-relatedSize-1)),relatedSink,nullptr);
        if(relatedRequest.state()==NetRequest::State::Done){
            if(!parseSearch(relatedBody,relatedSize,recommendations))recommendations=SearchResults{};
            relatedBusy=false;relatedRequest.cancel();
        }else if(relatedRequest.state()==NetRequest::State::Error || relatedSize+1>=sizeof(relatedBody)){
            relatedBusy=false;relatedRequest.cancel();
            if(relatedRetries++<2){relatedPending=true;relatedAt=frame+180;}
        }
    }
    if(autoAdvance && playback==Playback::Finished && !relatedBusy && !relatedPending){
        autoAdvance=false;
        int index=nextRecommendation(recommendations,trackId,recentTracks,recentCount);
        if(index>=0){TrackItem next=recommendations.tracks[index];playTrack(&next);return;}
        std::snprintf(message,sizeof(message),"No new recommendations. Choose another track.");
    }
}

static void openSettings() {
    if (page != Page::Settings) settingsReturn = page == Page::Player ? Page::Player : Page::Browse;
    scrubbing = false; page = Page::Settings; settingSelected = 0;
}
static void openServerEditor() {
    editingServer = true; editorError = false; keyboardSelected = 0;
    formatEndpoint(currentEndpoint(), serverDraft, sizeof(serverDraft)); page = Page::Keyboard;
}
static bool applyServer(const char* text) {
    ServerEndpoint endpoint{};
    if (!parseEndpoint(text, endpoint)) { editorError = true; return false; }
    bool changed = std::strcmp(endpoint.host, currentEndpoint().host) != 0 || endpoint.port != currentEndpoint().port;
    if (changed) {
        stopAudio(); resetArtwork(); searchRequest.cancel(); settingsRequest.cancel(); relatedRequest.cancel(); settingsBusy = relatedPending = relatedBusy = false;
        recentCount=recentCursor=0;
        searching = searchPending = searched = false; results = SearchResults{};
        title[0] = artist[0] = trackId[0] = query[0] = submitted[0] = searchMessage[0] = 0;
        trackDurationMs = model.duration = 0; selected = firstResult = 0;
        setEndpoint(endpoint);
    }
    bool saved = savePreferences();
    std::snprintf(settingsMessage, sizeof(settingsMessage), "%s", saved ? "Server saved" : "Applied for this session");
    settingsError = false; editingServer = editorError = false; page = Page::Settings;
    return true;
}
static void changeConnectionMode(bool emulator) {
    if(emulatorMode()==emulator)return;
    selectEmulatorMode(emulator);
    ServerEndpoint target=currentEndpoint();
    selectEmulatorMode(!emulator);
    bool changed=std::strcmp(target.host,currentEndpoint().host)!=0 || target.port!=currentEndpoint().port;
    selectEmulatorMode(emulator);
    if(changed){
        stopAudio();resetArtwork();searchRequest.cancel();settingsRequest.cancel();relatedRequest.cancel();
        settingsBusy=relatedPending=relatedBusy=false;
        searching=searchPending=searched=false;results=SearchResults{};recommendations=SearchResults{};
        title[0]=artist[0]=trackId[0]=query[0]=submitted[0]=searchMessage[0]=0;
        trackDurationMs=model.duration=0;selected=firstResult=0;recentCount=recentCursor=0;
    }
    bool saved=savePreferences();
    std::snprintf(settingsMessage,sizeof(settingsMessage),"%s",saved?"Connection profile saved":"Profile applied for this session");
    settingsError=false;
}
static void testServer() {
    if (wifi != Wifi::Ready) {
        settingsError = true; std::snprintf(settingsMessage, sizeof(settingsMessage), "Connect to Wi-Fi first"); return;
    }
    settingsRequest.cancel(); settingsSize = 0; settingsBusy = true; settingsError = false;
    settingsMessage[0] = 0; settingsRequest.begin("/healthz", frame);
}
static void pollSettings() {
    if (!settingsBusy) return;
    settingsRequest.poll(frame, sizeof(settingsBody) - settingsSize - 1, settingsSink, nullptr);
    if (settingsRequest.state() == NetRequest::State::Done) {
        settingsError = !parseHealth(settingsBody, settingsSize);
        std::snprintf(settingsMessage, sizeof(settingsMessage), "%s", settingsError ? "Not a compatible server" : "Server is ready");
        settingsBusy = false; settingsRequest.cancel();
    } else if (settingsRequest.state() == NetRequest::State::Error || settingsSize + 1 >= sizeof(settingsBody)) {
        settingsBusy = false; settingsError = true; settingsRequest.cancel();
        std::snprintf(settingsMessage, sizeof(settingsMessage), "Server could not be reached");
    }
}

static void submitSearch(unsigned offset = 0) {
    char* start = query;
    while (*start == ' ') ++start;
    if (start != query) std::memmove(query, start, std::strlen(start) + 1);
    std::size_t end = std::strlen(query);
    while (end && query[end - 1] == ' ') query[--end] = 0;
    if (!*query) { std::snprintf(searchMessage, sizeof(searchMessage), "Enter a track or artist."); return; }
    std::snprintf(submitted, sizeof(submitted), "%s", query);
    searchRequest.cancel();
    searching = true; searchPending = true; searchAt = frame; searchStart = frame;
    searchOffset = offset;
    searchMessage[0] = 0;
    searchSize = 0;
    page = Page::Browse;
}

static void pageResults(int direction) {
    if (searching) return;
    if (direction < 0 && results.offset) submitSearch(results.offset >= kSearchPageSize ? results.offset - kSearchPageSize : 0);
    else if (direction > 0 && results.hasMore && results.nextOffset <= 5000) submitSearch(results.nextOffset);
}

static void pollSearch() {
    if (searchPending && frame >= searchAt) {
        char encoded[580], path[768];
        if (!urlEncode(submitted, encoded, sizeof(encoded))) {
            searching = searchPending = false;
            std::snprintf(searchMessage, sizeof(searchMessage), "Search query is too long."); return;
        }
        std::snprintf(path, sizeof(path), "/v1/search?q=%s&offset=%u&limit=%u", encoded, searchOffset, kSearchPageSize);
        searchSize = 0;
        searchRequest.begin(path, frame);
        searchPending = false;
    }
    unsigned budget = sizeof(searchBody) - searchSize - 1;
    searchRequest.poll(frame, std::min(budget, 4096u), searchSink, nullptr);
    if (searchRequest.state() == NetRequest::State::Error) {
        int status = searchRequest.response().statusCode();
        if (status == 503 && frame - searchStart < 60 * 35) {
            searchRequest.cancel(); searchPending = true; searchAt = frame + 60; return;
        }
        searching = false;
        std::snprintf(searchMessage, sizeof(searchMessage), "Search couldn't finish. Try again.");
        searchRequest.cancel();
    } else if (searchRequest.state() == NetRequest::State::Done) {
        searching = false; searched = true;
        if (parseSearch(searchBody, searchSize, incomingResults)) {
            results = incomingResults; selected = firstResult = 0; searchMessage[0] = 0;
        } else std::snprintf(searchMessage, sizeof(searchMessage), "Search response wasn't understood.");
        searchRequest.cancel();
    } else if (searching && !budget) {
        searching = false;
        searchRequest.cancel();
        std::snprintf(searchMessage, sizeof(searchMessage), "Too many results. Try another query.");
    }
}

static void scanWifi() {
    stopAudio(); searchRequest.cancel(); searching = searchPending = false;
    artworkRequest.cancel(); artworkPending = false;
    if (!artwork.valid) artworkAttempted=false;
    relatedRequest.cancel(); relatedBusy=false; relatedPending=true; relatedAt=frame+120;
    settingsRequest.cancel(); settingsBusy = false;
    Wifi_DisconnectAP(); Wifi_ScanMode(); selectedAP = 0; wifi = Wifi::Scan;
    std::snprintf(message, sizeof(message), "Open or saved WEP networks");
}

static void connectWifi(bool saved = false) {
    if (saved) { Wifi_AutoConnect(); std::snprintf(networkName, sizeof(networkName), "Saved network"); }
    else {
        Wifi_AccessPoint ap{};
        if (Wifi_GetAPData(selectedAP, &ap) != 0) return;
        std::snprintf(networkName, sizeof(networkName), "%.32s", ap.ssid);
        if (!(ap.flags & WFLAG_APDATA_COMPATIBLE)) {
            std::snprintf(message, sizeof(message), "Choose an open or WEP network"); return;
        }
        if (ap.flags & WFLAG_APDATA_CONFIG_IN_WFC) Wifi_ConnectWfcAP(&ap);
        else if (ap.security_type == AP_SECURITY_OPEN) {
            Wifi_SetIP(0, 0, 0, 0, 0); Wifi_ConnectSecureAP(&ap, nullptr, 0);
        } else { std::snprintf(message, sizeof(message), "Save the WEP key in WFC first"); return; }
    }
    wifi = Wifi::Joining; wifiStarted = frame;
}

static void cancelKeyboard() {
    if (editingServer) { editingServer = editorError = false; page = Page::Settings; }
    else { std::snprintf(query, sizeof(query), "%s", queryBackup); page = Page::Browse; }
}
static void keyboardKey(int index) {
    KeyboardKey keys[48]; unsigned count = makeKeyboard(keys, russianKeyboard, numberKeyboard, editingServer);
    if (index < 0 || static_cast<unsigned>(index) >= count) return;
    keyboardSelected = index;
    char* text = editingServer ? serverDraft : query;
    std::size_t capacity = editingServer ? sizeof(serverDraft) : sizeof(query);
    const auto& key = keys[index];
    editorError = false;
    switch (key.action) {
        case KeyAction::Letter: appendText(text, capacity, key.label); break;
        case KeyAction::Space: appendText(text, capacity, " "); break;
        case KeyAction::Backspace: eraseCodepoint(text); break;
        case KeyAction::Language: russianKeyboard = !russianKeyboard; keyboardSelected = 0; break;
        case KeyAction::Numbers: numberKeyboard = !numberKeyboard; keyboardSelected = 0; break;
        case KeyAction::Search: if (editingServer) applyServer(serverDraft); else submitSearch(); break;
    }
}
static void keyboardInput(unsigned keys, unsigned repeat) {
    KeyboardKey keyboard[48]; unsigned count = makeKeyboard(keyboard, russianKeyboard, numberKeyboard, editingServer);
    if (repeat & KEY_LEFT) keyboardSelected = keyboardMove(keyboard, count, keyboardSelected, -1, 0);
    if (repeat & KEY_RIGHT) keyboardSelected = keyboardMove(keyboard, count, keyboardSelected, 1, 0);
    if (repeat & KEY_UP) keyboardSelected = keyboardMove(keyboard, count, keyboardSelected, 0, -1);
    if (repeat & KEY_DOWN) keyboardSelected = keyboardMove(keyboard, count, keyboardSelected, 0, 1);
    if (keys & KEY_B) cancelKeyboard();
    else if (keys & KEY_X) { eraseCodepoint(editingServer ? serverDraft : query); editorError = false; }
    else if (keys & KEY_Y) { if (editingServer) applyServer(serverDraft); else submitSearch(); }
    else if (keys & KEY_A) keyboardKey(keyboardSelected);
    if (keys & KEY_TOUCH) {
        touchPosition touch{}; touchRead(&touch);
        if (touch.py < 27 && touch.px > 200) cancelKeyboard();
        else keyboardKey(keyboardHit(keyboard, count, touch.px, touch.py));
    }
}
static void settingsAction(int item) {
    if (item == 0) openServerEditor();
    else if (item == 1) changeConnectionMode(!emulatorMode());
    else if (item == 2) { wifiBackToSettings = true; scanWifi(); page = Page::Browse; }
    else testServer();
}
static void settingsInput(unsigned keys, unsigned repeat) {
    if (repeat & KEY_UP) settingSelected = std::max(0, settingSelected - 1);
    if (repeat & KEY_DOWN) settingSelected = std::min(3, settingSelected + 1);
    if (keys & (KEY_B | KEY_X)) { page = settingsReturn; return; }
    if (keys & KEY_Y) { openServerEditor(); return; }
    if (keys & KEY_A) settingsAction(settingSelected);
    if (!(keys & KEY_TOUCH)) return;
    touchPosition touch{}; touchRead(&touch);
    if (touch.py < 27 && touch.px < 64) page = settingsReturn;
    else if (touch.py >= Layout::serverTop && touch.py < Layout::serverTop+Layout::serverHeight) settingsAction(0);
    else if (touch.py >= Layout::modeTop && touch.py < Layout::modeTop+Layout::modeHeight) changeConnectionMode(touch.px < 128);
    else if (touch.py >= Layout::wifiTop && touch.py < Layout::wifiTop+Layout::wifiHeight) settingsAction(2);
    else if (touch.py >= Layout::testTop && touch.py < Layout::testTop+Layout::testHeight) settingsAction(3);
}

static void browseInput(unsigned keys, unsigned repeat) {
    if (!searching && results.count) {
        if (repeat & KEY_UP) selected = std::max(0, selected - 1);
        if (repeat & KEY_DOWN) selected = std::min(static_cast<int>(results.count) - 1, selected + 1);
        if (selected < firstResult) firstResult = selected;
        if (selected >= firstResult + 4) firstResult = selected - 3;
    }
    if (keys & KEY_LEFT) pageResults(-1);
    if (keys & KEY_RIGHT) pageResults(1);
    if (keys & KEY_A) {
        if (!searching && *searchMessage) submitSearch(searchOffset);
        else if (results.count && !searching) playTrack(&results.tracks[selected]);
        else if (!searched && !searching) togglePlayback();
        else if (!searching) openKeyboard();
    }
    if (!(keys & KEY_TOUCH)) return;
    touchPosition touch{}; touchRead(&touch);
    if (touch.py < 27) {
        if (touch.px >= 104) page = Page::Player;
    } else if (touch.py >= Layout::searchTop && touch.py < Layout::searchTop+Layout::searchHeight) openKeyboard();
    else if (touch.py >= Layout::miniTop) { if (touch.px >= 224) stopAudio(); else togglePlayback(); }
    else if (!searched && !searching && touch.py >= Layout::resultsTop && touch.py < Layout::miniTop) {
        openKeyboard();
    } else if (touch.py >= Layout::pagesTop && touch.py < Layout::pagesTop+Layout::pagesHeight) {
        if (touch.px < 58) pageResults(-1);
        if (touch.px >= 198) pageResults(1);
    } else if (!searching && !*searchMessage && touch.py >= Layout::resultsTop && touch.py < Layout::resultsTop+kSearchPageSize*Layout::rowStride) {
        if ((touch.py-Layout::resultsTop)%Layout::rowStride >= Layout::rowHeight) return;
        int target = firstResult + (touch.py-Layout::resultsTop)/Layout::rowStride;
        if (target >= 0 && static_cast<unsigned>(target) < results.count) { selected = target; playTrack(&results.tracks[selected]); }
    }
}

static void playerInput(unsigned keys, unsigned, unsigned held) {
    if (keys & KEY_A) togglePlayback();
    if (keys & KEY_UP) stepTrack(-1);
    if (keys & KEY_DOWN) stepTrack(1);
    if (keys & KEY_LEFT) skipSeconds(-5);
    if (keys & KEY_RIGHT) skipSeconds(5);
    if (scrubbing || !(held & KEY_TOUCH) || !(keys & KEY_TOUCH)) return;
    touchPosition touch{}; touchRead(&touch);
    if (touch.py < 27) { if (touch.px < 104) page = Page::Browse; }
    else if (touch.py >= Layout::seekTop && touch.py <= Layout::seekBottom && trackDurationMs) {
        scrubbing = true; previewMs = seekTouchMs(touch.px, trackDurationMs);
    } else if (touch.py >= Layout::transportTop && touch.py < Layout::transportBottom) {
        if (touch.px < 80) skipSeconds(-5);
        else if (touch.px > 176) skipSeconds(5);
        else togglePlayback();
    } else if (touch.py >= Layout::trackTop && touch.py < Layout::trackTop+Layout::trackHeight) {
        stepTrack(touch.px < 128 ? -1 : 1);
    }
}

static void input(unsigned keys, unsigned repeat, unsigned held) {
    if (scrubbing) {
        if (wifi != Wifi::Ready || page != Page::Player) scrubbing = false;
        else if (held & KEY_TOUCH) {
            touchPosition touch{}; touchRead(&touch);
            previewMs = seekTouchMs(touch.px, trackDurationMs);
        } else { unsigned target = previewMs; scrubbing = false; seekTo(target); }
    }
    if (wifi == Wifi::Ready) {
        if (repeat & KEY_L) changeVolume(-4);
        if (repeat & KEY_R) changeVolume(4);
    }
    if (page == Page::Keyboard) { keyboardInput(keys, repeat); return; }
    if (page == Page::Settings) { settingsInput(keys, repeat); return; }
    if (keys & KEY_TOUCH) {
        touchPosition touch{}; touchRead(&touch);
        if (touch.py < 27 && touch.px >= 208) { openSettings(); return; }
    }
    if (wifi != Wifi::Ready && (keys & KEY_SELECT)) { openSettings(); return; }
    if (wifi == Wifi::Scan) {
        int count = Wifi_GetNumAP();
        if (repeat & KEY_UP) --selectedAP;
        if (repeat & KEY_DOWN) ++selectedAP;
        selectedAP = std::max(0, std::min(std::max(0, count - 1), selectedAP));
        if ((keys & KEY_A) && count) connectWifi();
        else if (keys & KEY_X) connectWifi(true);
        else if (keys & KEY_TOUCH) {
            touchPosition touch{}; touchRead(&touch);
            int first = std::max(0, selectedAP - 4);
            int chosen = first + (static_cast<int>(touch.py) - 48) / 23;
            if (touch.py >= 48 && touch.py < 163 && chosen < count) { selectedAP = chosen; connectWifi(); }
        }
        return;
    }
    if (wifi == Wifi::Joining) {
        if (keys & KEY_B) scanWifi();
        else if (keys & KEY_TOUCH) { touchPosition touch{}; touchRead(&touch); if (touch.py >= 146 && touch.py < 170) scanWifi(); }
        return;
    }
    if (keys & KEY_SELECT) diagnostics = !diagnostics;
    if (keys & KEY_B) stopAudio();
    if (keys & KEY_Y) { openKeyboard(); return; }
    if (keys & KEY_X) { scrubbing = false; page = page == Page::Browse ? Page::Player : Page::Browse; return; }
    if (page == Page::Browse) browseInput(keys, repeat);
    else playerInput(keys, repeat, held);
}

static void render() {
    model.wifiPhase = static_cast<int>(wifi);
    model.page = page; model.message = message; model.title = title; model.artist = artist;
    model.trackId = trackId; model.query = query; model.searchMessage = searchMessage;
    model.editingServer = editingServer; model.editorError = editorError;
    if (editingServer) model.query = serverDraft;
    formatEndpoint(currentEndpoint(), serverLabel, sizeof(serverLabel));
    model.emulatorMode=emulatorMode();model.showVolume=volumeTouched && frame-volumeChangedAt<90;
    model.visualLevels=visualLevels;
    for(unsigned i=0;i<32;++i){
        unsigned target=playback==Playback::Active && !paused && !buffering?visualTarget[i]:0;
        visualLevels[i]=static_cast<uint8_t>((visualLevels[i]*2+target)/3);
    }
    model.server = serverLabel; model.networkName = networkName; model.settingSelected = settingSelected;
    model.settingsMessage = settingsMessage; model.settingsBusy = settingsBusy; model.settingsError = settingsError;
    model.cover = artwork.valid ? artwork.pixels : nullptr; model.accent = artwork.accent; model.onAccent = artwork.ink;
    model.results = &results; model.searching = searching; model.searched = searched;
    model.selected = selected; model.first = firstResult; model.volume = volume;
    model.elapsedMs = scrubbing ? previewMs : positionMs();
    model.durationMs = trackDurationMs;
    model.elapsed = model.elapsedMs / 1000;
    model.bufferPercent = std::min(100u, static_cast<unsigned>(audio.size() * 100 / kAheadBytes));
    model.scrubbing = scrubbing;
    model.underruns = underruns; model.frame = frame; model.titleFrame = frame-audioStart; model.russianKeyboard = russianKeyboard;
    model.numberKeyboard = numberKeyboard; model.keyboardSelected = keyboardSelected; model.diagnostics = diagnostics;
    model.status = playback == Playback::Loading ? (seeking ? PlayStatus::Seeking : PlayStatus::Connecting) : playback == Playback::Error ? PlayStatus::Error :
        playback == Playback::Finished ? PlayStatus::Finished : playback == Playback::Stopped ? (*title ? PlayStatus::Stopped : PlayStatus::Ready) :
        paused ? PlayStatus::Paused : !streamOpen || buffering ? PlayStatus::Buffering : PlayStatus::Playing;
    if (wifi == Wifi::Scan) {
        int count = Wifi_GetNumAP(), first = std::max(0, selectedAP - 4);
        model.networkCount = std::max(0, std::min(5, count - first)); model.networkSelected = selectedAP - first;
        for (unsigned i = 0; i < model.networkCount; ++i) {
            Wifi_AccessPoint ap{}; Wifi_GetAPData(first + i, &ap);
            std::snprintf(model.networks[i], sizeof(model.networks[i]), "%.32s", ap.ssid);
        }
    }
    drawUi(topCanvas, bottomCanvas, model);
    paintPending = true;
}

int main() {
    defaultExceptionHandler();
    videoSetMode(MODE_5_2D); videoSetModeSub(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG); vramSetBankC(VRAM_C_SUB_BG);
    topVram = bgGetGfxPtr(bgInit(2, BgType_Bmp16, BgSize_B16_256x256, 0, 0));
    bottomVram = bgGetGfxPtr(bgInitSub(2, BgType_Bmp16, BgSize_B16_256x256, 0, 0));
    lcdMainOnTop();
    mm_ds_system system{}; system.fifo_channel = FIFO_MAXMOD;
    if (!mmInit(&system) || !Wifi_InitDefault(INIT_ONLY | WIFI_DS_MODE_ONLY)) {
        model.status = PlayStatus::Error; model.message = "Initialization failed. Restart the app."; model.results = &results;
        drawUi(topCanvas, bottomCanvas, model);
        DC_FlushRange(&topCanvas, sizeof(topCanvas)); DC_FlushRange(&bottomCanvas, sizeof(bottomCanvas));
        dmaCopy(topCanvas.pixels, topVram, sizeof(topCanvas.pixels)); dmaCopy(bottomCanvas.pixels, bottomVram, sizeof(bottomCanvas.pixels));
        do { cothread_yield_irq(IRQ_VBLANK); scanKeys(); } while (!(keysDown() & KEY_START));
        return 1;
    }
    keysSetRepeat(20, 5);
    loadPreferences();
    Wifi_EnableWifi(); Wifi_ScanMode();
    std::snprintf(message, sizeof(message), "Open or saved WEP networks");
    render();
    while (true) {
        cothread_yield_irq(IRQ_VBLANK);
        if (paintPending) {
            DC_FlushRange(topCanvas.pixels, sizeof(topCanvas.pixels)); DC_FlushRange(bottomCanvas.pixels, sizeof(bottomCanvas.pixels));
            dmaCopy(topCanvas.pixels, topVram, sizeof(topCanvas.pixels)); dmaCopy(bottomCanvas.pixels, bottomVram, sizeof(bottomCanvas.pixels));
            paintPending = false;
        }
        ++frame; scanKeys();
        unsigned keys = keysDown(), repeat = keysDownRepeat(), held = keysHeld();
        if (keys & KEY_START) break;
        input(keys, repeat, held);
        if (wifi == Wifi::Joining) {
            int status = Wifi_AssocStatus();
            if (status == ASSOCSTATUS_ASSOCIATED) {
                wifi = Wifi::Ready; message[0] = 0;
                if (wifiBackToSettings) { page = Page::Settings; wifiBackToSettings = false; }
            }
            else if (status == ASSOCSTATUS_CANNOTCONNECT || frame - wifiStarted > 60 * 30) {
                scanWifi(); std::snprintf(message, sizeof(message), "Couldn't connect. Try another network.");
            }
        } else if (wifi == Wifi::Ready) {
            if (Wifi_AssocStatus() != ASSOCSTATUS_ASSOCIATED) {
                scanWifi(); std::snprintf(message, sizeof(message), "Wi-Fi disconnected. Choose a network.");
            } else { pollAudio(); pollSearch(); pollSettings(); pollArtwork(); pollRelated(); }
        }
        if (frame % 6 == 0 || keys) render();
    }
    stopAudio(); searchRequest.cancel(); artworkRequest.cancel(); settingsRequest.cancel(); relatedRequest.cancel(); Wifi_DisconnectAP(); Wifi_DisableWifi();
    return 0;
}
