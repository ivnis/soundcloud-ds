# v0.2.0 — Domains, Wi-Fi improvements, and easier server setup

This release includes all changes since v0.1.0 in both downloadable packages.
Update the ROM and server ZIP together; source changes alone do not update
files already downloaded from an older release.

## Downloads

- **soundcloud-ds-v0.2.0.nds** — freshly compiled Nintendo DS client.
- **soundcloud-ds-server-v0.2.0.zip** — standalone bridge, Docker Compose,
  first-run Windows setup, Cloudflare Tunnel launchers, and English/Russian guides.
- **SHA256SUMS.txt** — checksums for both packages.

No local .env/config overrides, temporary tunnel addresses, SDK/emulator
binaries, firmware, saved Wi-Fi credentials, or music files are included.

## ROM changes

- HTTP server addresses now accept **DNS hostnames** or IPv4, with an optional
  port (default 80). Original hostnames are preserved in the HTTP Host header,
  making Cloudflare Quick Tunnel routing possible.
- One cooperative DNS worker serves all requests; cached results and generation
  checks keep lookup work out of the main playback loop.
- Server keyboard supports names, punctuation, long addresses, and scrolling.
  Existing saved endpoint profiles remain readable.
- Manual **WEP key** entry, and **WPA2-Personal/AES** entry in native DSi mode.
  Passwords are masked, with Show/Hide, Shift, and full printable ASCII symbols.
- Improved Wi-Fi scans: stable selection by BSSID, skipped failed reads,
  explicit Hidden labels, network totals, scrolling, and Rescan.
- Unsupported networks show a warning and an optional **Try anyway** diagnostic
  request. This does not add unsupported radio/encryption capabilities.

## Server/setup changes

- Windows **Start Server.cmd** creates the environment and installs dependencies
  on first run; **Setup Server.cmd** installs or repairs them separately.
- Docker instructions explain how to open a terminal in the extracted folder.
  Docker users do not need Python installed on the host.
- **Start/Stop Cloudflare Tunnel.cmd** are now inside the server ZIP. They use
  the official portable cloudflared executable with SHA-256 verification;
  Microsoft Store and winget are not required. QUIC is the default transport.
- Updated primary English documentation with Russian counterparts.

## Connect through Cloudflare

Run the bridge and Start Cloudflare Tunnel.cmd, wait for
`Registered tunnel connection`, then enter the generated hostname in ROM Settings:

```text
your-tunnel.trycloudflare.com:80
```

Use your actual generated hostname. The ROM uses **HTTP**, without TLS;
do not paste an `https://` URL. Quick Tunnel addresses change after restart
and are public to anyone with the link.

## Limits and validation

**DS Fat/Lite cannot use WPA/WPA2 or 5 GHz networks.** It still requires a
compatible open/WEP 2.4 GHz access point with legacy rates. WPA2 entry requires
native DSi mode and a compatible Personal/AES network; DS compatibility mode
on a DSi does not unlock it. WPA3, WPA/TKIP, enterprise authentication, HTTPS,
IPv6 literals, accounts, private libraries, playlists, and stereo are not included.

65 server tests, native C++ checks, and an ARM build pass. In melonDS, hostname
resolution, health checks, real music playback, artwork, and recommendations
were verified through a Cloudflare Tunnel. Physical DS/DSi encrypted-network
association and playback through Cloudflare remain unverified.

Code: GPL-3.0-only. Font/icon licenses remain in the source notices.

<details>
<summary>Русский</summary>

Релиз включает все изменения после v0.1.0. Скачайте отдельно
**soundcloud-ds-v0.2.0.nds** и **soundcloud-ds-server-v0.2.0.zip**.
Обновить нужно оба пакета; старые архивы автоматически не меняются.

В ROM добавлены домены по HTTP, кооперативный DNS, длинные адреса, ввод ключей
Wi-Fi, исправленный список сетей, Rescan и диагностическое Try anyway.
DS Fat/Lite по-прежнему не поддерживает WPA/WPA2 аппаратно; для WPA2-Personal/AES
нужен нативный DSi-режим. Пустые SSID подписаны Hidden с окончанием BSSID.

В серверном ZIP есть автоматическая установка зависимостей для Windows,
Setup Server.cmd, уточнённые Docker-инструкции и запуск/остановка Cloudflare
без Microsoft Store. При Docker Python на хосте не нужен; при обычном запуске
Windows нужен установленный Python 3.13.

Для Cloudflare указать в Settings настоящий `your-tunnel.trycloudflare.com:80`,
без `https://`. При перезапуске туннеля адрес меняется. Контрольные суммы —
в SHA256SUMS.txt. Внешний доступ и поток проверены в melonDS; на физической
DS Fat/DSi соответствующие проверки ещё не выполнены.

</details>
