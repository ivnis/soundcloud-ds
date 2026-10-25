# v0.1.0 — First public release

The first public SoundCloud DS prototype: touchscreen search and playback,
server-prepared artwork, a matching theme, and automatic related tracks.

## Downloads

- **soundcloud-ds-v0.1.0.nds** — the compiled Nintendo DS client.
- **soundcloud-ds-server-v0.1.0.zip** — the standalone Python bridge, Docker
  Compose files, Windows launcher, GPLv3 license, and English/Russian setup guides.
- **SHA256SUMS.txt** — checksums for both downloads.

You need **both the client and a running bridge**. The ROM connects to your
server; it does not stream directly from SoundCloud.

Windows server setup: extract the server ZIP, install Python 3.13, create
`.venv`, install `requirements-windows.txt`, and run `Start Server.cmd`.
Docker/Linux setup is described in the ZIP's README. Python packages and FFmpeg
are installed separately; they are not embedded in the ZIP.

In the ROM's Settings, use `10.64.0.1:8765` for the Windows bridge with melonDS
indirect networking. Use port `8000` for Docker. For physical hardware, enter
your server's real LAN/VPS IPv4 and port.

## Included

- Four search results per page and an English/Russian touchscreen keyboard.
- Play/pause, five-second skips, drag-to-seek, and shoulder-button volume.
- Artwork-derived UI colors, audio-reactive visuals, and scrolling mini-player titles.
- Independent server profiles for Emulator/Console and Wi-Fi settings.
- Autoplay recommendations with a recent-track history.

## Validation and limits

65 server tests, native C++ checks, and a clean ARM build passed locally.
Emulator playback and an automatic transition between test tracks were checked.
Physical DS Fat and long continuous real-track autoplay remain unverified.
The SoundCloud adapter is unofficial; upstream changes, region restrictions,
and occasional lookup/seek failures remain possible.

One audio stream per server process; use one worker. The DS connection is
unencrypted HTTP without authentication and requires numeric IPv4.
No accounts, private libraries, playlists, or stereo in this version.

Code: **GPL-3.0-only**. Bundled fonts/icons retain their documented licenses.

<details>
<summary>Русский</summary>

Первый публичный релиз прототипа SoundCloud DS.

Скачайте отдельно **soundcloud-ds-v0.1.0.nds** и
**soundcloud-ds-server-v0.1.0.zip**. ROM запускается в melonDS или с flashcart;
сервер нужно распаковать и запустить на ПК/VPS. В архиве есть инструкции
на русском и английском, запуск для Windows и Docker Compose.

Для Windows нужны Python 3.13 и установка `requirements-windows.txt` в `.venv`.
После этого запустите `Start Server.cmd`. Адрес в Settings для melonDS —
`10.64.0.1:8765`; при Docker замените порт на `8000`.
Для настоящей DS укажите реальный IPv4 своего сервера.

Есть поиск по четыре результата, EN/RU-клавиатура, перемотка, обложки,
акцентная тема, визуализация и автопереход по рекомендациям.
На физической DS Fat и в длительном непрерывном автопроигрывании версия пока
не проверена. Для проверки скачанных файлов приложен `SHA256SUMS.txt`.

</details>
