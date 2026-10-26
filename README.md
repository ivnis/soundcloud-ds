# SoundCloud DS

**English** | [Русский](README.ru.md)

A SoundCloud homebrew player for Nintendo DS, with a C++/BlocksDS client and a
Python/FastAPI + FFmpeg bridge. Search and control playback on the touchscreen;
the bridge handles upstream HTTPS, audio conversion, artwork, and recommendations.

```text
SoundCloud -> Python bridge / FFmpeg -> HTTP PCM -> Nintendo DS / Maxmod
```

The current prototype has been tested in melonDS. **Physical DS Fat validation
is still pending.** This is an independent project, not affiliated with
SoundCloud or Nintendo.

## Features

- Public track search, four results per page, and an English/Russian keyboard.
- Touch playback controls, five-second skips, and drag-to-seek progress.
- Background search while music plays; scrolling titles in the mini-player.
- Server-prepared 64x64 artwork, a matching UI palette, and audio-reactive visuals.
- Automatic playback of related tracks, skipping recently played IDs.
- Separate Emulator/Console server profiles and Wi-Fi settings.
- Wi-Fi password entry for WEP and DSi-mode WPA2-Personal;
  see [hardware restrictions and setup](docs/WIFI.md).
- Shoulder-button volume with a temporary on-screen indicator.

The interface is English. Russian is supported for text input and track metadata.
Accounts, private libraries, playlists, and stereo are not implemented.

## Quick start

### Release downloads

Get [v0.2.0](https://github.com/ivnis/soundcloud-ds/releases/tag/v0.2.0):

- [Nintendo DS ROM](https://github.com/ivnis/soundcloud-ds/releases/download/v0.2.0/soundcloud-ds-v0.2.0.nds)
- [Standalone server ZIP](https://github.com/ivnis/soundcloud-ds/releases/download/v0.2.0/soundcloud-ds-server-v0.2.0.zip)

Download both, run the bridge, and load the ROM. The server ZIP contains its own
English/Russian setup guides and a Windows launcher. Source-build instructions
follow below. Check SHA256SUMS.txt on the release page to verify downloads.

Source-build requirements: Python 3.13, FFmpeg, a BlocksDS toolchain or Docker, and melonDS
or a compatible flashcart. SDKs, emulator binaries, and firmware are not bundled.

```sh
git clone https://github.com/ivnis/soundcloud-ds.git
cd soundcloud-ds
```

### 1. Start the bridge

With Docker Compose:

```sh
docker compose up -d --build
curl http://127.0.0.1:8000/healthz
```

The default source is a ten-minute 440 Hz test tone. To enable SoundCloud, copy
`.env.example` to `.env`, set `SC_SOURCE=soundcloud`, and restart the service:

```sh
docker compose up -d --force-recreate
```

`SC_TRACK_URL` selects the default public track; the DS can also search for
other tracks. No personal SoundCloud token is needed. The adapter uses the
unofficial `soundcloud-v2` package, so upstream changes and regional availability
can affect playback. Restricted/preview-only sources are filtered.

For a local Windows bridge:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r server/requirements-dev.txt
.\tools\start-local.ps1 -Source soundcloud -ServerOnly
```

This helper uses port **8765** on loopback and supplies FFmpeg via imageio-ffmpeg.
Run `./tools/stop-local.ps1` to stop it. Docker instead exposes port **8000**.

For a native Linux bridge, install FFmpeg, create a virtual environment, install
`server/requirements.txt`, and run a single worker:

```sh
SC_SOURCE=soundcloud SC_TRACK_URL=https://soundcloud.com/forss/flickermood \
  python -m uvicorn server.app:app --host 0.0.0.0 --port 8000 --workers 1
```

### 2. Build the ROM

From a BlocksDS/Wonderful Toolchain shell:

```sh
make -C client
```

Or from the repository root with Docker (POSIX shell):

```sh
docker run --rm -v "$PWD:/work" -w /work/client \
  --entrypoint make skylyrac/blocksds:slim-latest
```

Output: `client/soundcloud_ds.nds`. On Windows, `tools/build.ps1` accepts
`-MakeExecutable`, `-ShellExecutable`, `-WonderfulRoot`, and `-BlocksDSRoot`
if these tools are not already configured. The CI workflow also builds the ROM
and uploads it as an Actions artifact.

### 3. Connect the DS client

Load the ROM in melonDS with DS networking enabled, connect to `melonAP`, and
open the gear in the top-right corner of the touchscreen.

- **Emulator:** `10.64.0.1:8765` is the preset for melonDS indirect networking
  with the Windows local helper. Set port `8000` when using the Docker bridge.
- **Console:** enter the LAN/VPS IPv4 or DNS hostname and port of your bridge.
  `127.0.0.1` points to the DS itself and is not the PC's address.
- Tap the address to edit it; use **Test connection** to check the bridge.

The Console profile initially uses `192.168.1.100:8000`, a placeholder that must
be replaced. Optional build defaults are in `client/include/config.local.example.h`;
copy it to ignored `config.local.h` to override the Console default.
Profiles persist on a working FAT/DLDI filesystem, or last for the session
without one. Changing the server or Wi-Fi stops the current stream.

To use the Windows double-click launchers, place `melonDS.exe` and its required
files under `tools/melonds`, build the ROM, and install the Python dependencies.
Then use `Start SoundCloud.cmd` or `Start Test Tone.cmd`. Existing emulator
key mappings are preserved. No emulator/SDK download is performed by these helpers.

## Controls

Button labels refer to **Nintendo DS buttons**, not PC key names. Configure the
keyboard mapping in melonDS.

| Control | Action |
| --- | --- |
| A | Play/pause; play a selected search result |
| B | Stop; back/cancel in settings and text input |
| X | Switch Search/Player; delete a character in text input |
| Y | Open search; submit text in the keyboard |
| L / R | Decrease/increase volume |
| Left / Right | Player: skip five seconds; Search: change page |
| Up / Down | Select a search result; Player: previous/next search result |
| SELECT | Toggle buffer diagnostics |
| START | Exit the application |

Touch the progress bar and release to seek. Pause is preserved. Autoplay starts
a recommendation after a natural track ending; Stop and a manual seek exactly
to the end do not start another track. Empty recommendations leave playback
finished. Previous/Next buttons navigate the current search results.

## Bridge and hardware limits

Audio is mono, signed 16-bit little-endian PCM at 22,050 Hz (44,100 bytes/s).
The DS keeps six seconds of history and up to eight seconds ahead; startup
normally waits for two seconds. Distant seeks may require server rebuffering.
Artwork is a 4,624-byte indexed packet, decoded to RGB555 on the DS.

Use one bridge worker: one audio stream is allowed at a time. The DS-to-bridge
connection is plain HTTP without authentication. Use a trusted network or limit
access to your bridge; this prototype is intended for personal use.
DS Fat/Lite requires an open or WEP 2.4 GHz network; it cannot use WPA/WPA2.
v0.2.0 supports manual WEP keys and WPA2-Personal on a DSi
actually running in native DSi mode. This is not in the v0.1.0 ROM and physical
encrypted-network association remains unverified. See [Wi-Fi support](docs/WIFI.md).
v0.2.0 accepts IPv4 or DNS names over **HTTP**.
Use `bridge.example.com:8000` or `http://bridge.example.com`; without a port,
HTTP defaults to 80. DNS runs in one shared cooperative worker with a bounded
cache. HTTPS/TLS and IPv6 endpoint syntax are not implemented. The published
v0.1.0 ROM still accepts numeric IPv4 only.

## Development

On Linux with Python 3.13, FFmpeg, and g++:

```sh
python -m pip install -r server/requirements-dev.txt
python -m pytest -q
g++ -std=c++17 -Wall -Wextra -Werror -Iclient/include \
  tests/client_core.cpp client/source/{http,search,keyboard,endpoint,artwork,ui}.cpp \
  -o /tmp/soundcloud-ds-tests
/tmp/soundcloud-ds-tests
```

On Windows, after installing `server/requirements-dev.txt` and g++:

```powershell
.\tools\check.ps1
```

Current checks cover 65 server tests plus native C++ HTTP, buffering, Unicode,
keyboard, endpoint/profile, artwork, geometry, and recommendation-queue checks.
They do not establish real-hardware audio timing. GitHub Actions runs these
checks and the ARM build. Fonts and the icon are bundled with generated headers;
ordinary builds do not require asset-generation tools.

[Contributing](CONTRIBUTING.md) · [Third-party notices](THIRD_PARTY_NOTICES.md)

## Documentation

- [API and architecture](docs/API.md) (English).
- [Publishing checklist](docs/PUBLISHING.md) (English).
- [Cloudflare Tunnel without Microsoft Store](docs/CLOUDFLARE.md) (English/Russian;
  includes HTTP hostname setup for v0.2.0 ROMs).
- [Русская документация и подробная настройка](README.ru.md).
- Detailed development notes in Russian: [UI/settings/artwork](docs/UI_ARTWORK_SETTINGS.md),
  [search](docs/SEARCH_UI.md), [seeking](docs/SEEK_AND_ARTWORK.md),
  [validation history](docs/VALIDATION.md).

## License and author

**GPL-3.0-only** — see [LICENSE](LICENSE). Bundled fonts/icons retain their
respective licenses in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

By [ivnis](https://github.com/ivnis) · [Telegram](https://t.me/lovelyperv).
