# SoundCloud DS bridge — v0.1.0

**English** | [Русский](README.ru.md)

This archive contains the **server only**. Download
`soundcloud-ds-v0.1.0.nds` separately from the same GitHub release and load it
in melonDS or on a compatible flashcart. The server handles SoundCloud lookup,
audio conversion, artwork, and related tracks. It is not a desktop music player.

## Docker / Linux VPS

Requires Docker and Docker Compose. On Windows, install and start Docker Desktop
before running the command below. You do **not** need Python or the Python
setup/start scripts when using Docker.

1. Extract the entire **server ZIP** with **Extract All**.
2. Open the extracted folder containing **compose.yaml**. This is the server
   folder, not the Downloads folder, the ZIP itself, or the inner `server` folder.
3. On Windows, click File Explorer's address bar, type `powershell`, and press
   Enter. The terminal opens in that folder; there is no need to type `cd`.
   On macOS/Linux, open a terminal in that folder. If changing folders manually,
   use `cd "path/to/extracted-folder"`, replacing the example with your real path.
4. Copy `.env.example` to `.env` and set `SC_SOURCE=soundcloud` for music
   (`test` is the default ten-minute test tone).
5. Run:

```sh
docker compose up -d --build
curl http://127.0.0.1:8000/healthz
```

The bridge listens on port **8000**. To stop it: `docker compose down`.
After editing `.env`: `docker compose up -d --force-recreate`.

If you see **no configuration file provided**, the terminal is in the wrong
folder: check that `compose.yaml` is present. If **docker is not recognized**,
install Docker Desktop and reopen the terminal. If Docker reports that it cannot
connect to the daemon, start Docker Desktop and wait for the engine to be ready.
Use `docker compose logs -f bridge` to inspect bridge errors.

## Windows

1. Download the **server ZIP** from the release's Downloads/Assets section.
   This is separate from the `.nds` and GitHub's automatic **Source code** ZIP.
2. Right-click the server ZIP and choose **Extract All**. Open the extracted
   folder; do not run scripts while browsing inside the compressed ZIP.
3. Install Python 3.13 if it is not installed already.
4. Double-click **Start Server.cmd**. On first launch it creates `.venv` and
   installs dependencies automatically; leave the window open while it runs.

No `cd` command or manually opened terminal is required. **Setup Server.cmd**
can also be used separately to install or repair dependencies.

For manual setup, open the extracted folder in File Explorer, click its address
bar, type `powershell`, and press Enter. That opens a terminal in the correct
folder, so you do not need to type `cd`. Then run:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements-windows.txt
.\start-server.ps1
```

After this one-time setup, `Start Server.cmd` starts SoundCloud mode.
FFmpeg is supplied by imageio-ffmpeg; no separate FFmpeg installation is needed.
The default Windows address is **127.0.0.1:8765**. Press Ctrl+C to stop.

For the independent test tone:

```powershell
.\start-server.ps1 -Source test
```

For a physical DS on your LAN, bind to the LAN interface instead:

```powershell
.\start-server.ps1 -BindAddress 0.0.0.0 -Port 8000
```

Allow that port through your firewall for the intended network. The scripts
do not change firewall rules or install an emulator.

## Native Linux

Install Python 3.13 and FFmpeg, then from the extracted folder:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r server/requirements.txt
SC_SOURCE=soundcloud SC_TRACK_URL=https://soundcloud.com/forss/flickermood \
  python -m uvicorn server.app:app --host 0.0.0.0 --port 8000 --workers 1
```

## Connect the ROM

Open **Settings** (gear on the touchscreen):

- **Emulator:** `10.64.0.1:8765` for the Windows launcher with melonDS indirect
  networking. Use port `8000` for the Docker bridge.
- **Console:** enter the real numeric LAN/VPS IPv4 and bridge port. The initial
  `192.168.1.100:8000` value is a placeholder.
- Tap the address to edit it; use **Test connection** before searching.

Keep exactly **one worker**. One audio stream is supported per server process.
The DS-to-bridge connection is plain HTTP without authentication, so limit it
to your intended network. The upstream SoundCloud adapter is unofficial and
can be affected by service changes or regional restrictions.

This first release was tested in melonDS; physical DS Fat and long continuous
autoplay validation are pending. ROM, SDK, emulator, firmware, audio recordings,
and Python packages are not bundled here.

## License and source

GPL-3.0-only — see [LICENSE](../../LICENSE). Dependencies retain their own licenses.
Source and build instructions: https://github.com/ivnis/soundcloud-ds/tree/v0.1.0

Author: [ivnis](https://github.com/ivnis) · https://t.me/lovelyperv
