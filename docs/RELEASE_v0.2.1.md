# SoundCloud DS v0.2.1

This update improves audio delivery on slow and high-latency connections.

## Highlights

- Adds IMA ADPCM audio streaming, reducing audio payload size by about 74% while keeping 22,050 Hz mono output.
- Keeps downloading audio while playback is active and adds recovery for interrupted streams.
- Adds per-ROM stream identifiers so reconnects and track changes can replace the previous request cleanly.
- Tunes the Nintendo DS Wi-Fi TCP receive window and update threshold for the shared packet buffer.
- Adds buffer, receive-rate, underrun, and reconnect diagnostics on the player screen.

## Upgrade

Install the v0.2.1 ROM and use the matching v0.2.1 server bundle. The v0.2.0 ROM uses uncompressed PCM and does not send the stream identifiers used for reliable replacement and recovery.

For Cloudflare Quick Tunnel, enter the current hostname with port `80`, for example `your-name.trycloudflare.com:80`. The temporary hostname changes when the tunnel restarts.

## Validation

- 86 server tests and the native C++ client tests pass.
- The development ROM was exercised in melonDS with live SoundCloud search, playback, artwork, and related tracks.
- The public HTTP tunnel was checked for health, search, and audio responses.
- Physical Nintendo DS/3DS playback still needs device-side confirmation.
