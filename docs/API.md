# API and architecture

[English README](../README.md) | [Русский README](../README.ru.md)

The bridge owns upstream SoundCloud HTTPS requests and FFmpeg processes.
The DS uses nonblocking HTTP/1.0 sockets and Maxmod manual audio updates.
Only numeric IPv4 endpoints are accepted by the DS UI.

## Endpoints

| Method and path | Result |
| --- | --- |
| `GET /healthz` | `status`, configured `source`, and audio `busy` flag |
| `GET /v1/search?q=lofi&offset=0&limit=4` | At most four track summaries and pagination offsets |
| `GET /v1/related?track_id=ID&limit=12` | At most 12 related summaries, independent of search |
| `GET /v1/track?track_id=ID` | Metadata and PCM format, without upstream URLs |
| `GET /v1/audio?track_id=ID&start_ms=0` | Raw PCM from the requested position |
| `GET /v1/artwork?track_id=ID` | DS-native artwork packet; 204 if absent |

Omitting `track_id` uses the bridge's configured default track. IDs contain
1–20 decimal digits. Search queries contain 1–80 characters after trimming;
offset is 0–5000 and search limit is 1–4. Related limit is 1–12.

Search and related responses use this shape:

```json
{
  "tracks": [
    {"id": "42", "title": "Example", "artist": "Artist", "duration_ms": 9000}
  ],
  "offset": 0,
  "next_offset": 4,
  "has_more": true
}
```

Filtering unavailable/preview tracks can produce fewer than four results;
`next_offset` advances over upstream positions, not the number retained.
Related responses use offset/next_offset zero and has_more false. The client
keeps recommendations separate from current search results and remembers
24 recent IDs to avoid immediate autoplay repeats.

## Audio

Format: signed 16-bit little-endian mono PCM, 22,050 Hz, no WAV header.
The body supports Content-Length, chunked framing, or completion by connection
close; incomplete responses are rejected. Metadata comes from these headers:

```text
X-Audio-Rate: 22050
X-Audio-Channels: 1
X-Audio-Format: s16le
X-Audio-Start-Ms: 0
X-Track-Duration-Ms: 9000
X-Track-Title: percent-encoded UTF-8
X-Track-Artist: percent-encoded UTF-8
```

One stream is allowed per bridge process. A single worker is required.
Source lookup is serialized and bounded; background artwork processing has
its own gate. Disconnects close FFmpeg and release the audio slot.

Common statuses: 409 for another active audio stream, 416 for an out-of-range
seek, 422 for invalid query parameters, 502 for source/FFmpeg failure, 503 for
a busy lookup, and 504 for audio startup timeout. Artwork failure returns 404.

## Artwork

The server crops/resizes artwork to 64x64, quantizes it to 256 colors, and
selects an accent plus readable on-accent color. The 4,624-byte packet is:

| Bytes | Contents |
| --- | --- |
| 16 | Little-endian `4s6H`: `SCAR`, version 1, width 64, height 64, accent RGB555, on-accent RGB555, palette size 256 |
| 512 | 256 little-endian RGB555 palette entries |
| 4096 | One palette index per pixel |

Decoded artwork takes 8 KiB on the DS. The server caches at most 128 processed
images for one hour; signed audio source metadata is cached separately for
120 seconds. No audio files or artwork are bundled with the repository.

## Configuration

Bridge settings: `SC_SOURCE` (`test` or `soundcloud`), `SC_TRACK_URL`,
`SC_TEST_SECONDS` (1–3600), and `FFMPEG_BIN`. See `.env.example` for Compose
and `server/.env.example` for native process settings. Uvicorn's port is set
at launch; the Windows helper uses 8765 and Compose uses 8000.

The DS stores both endpoint profiles in `/soundcloud-ds.cfg` when FAT/DLDI is
available. Without a filesystem they last for the session. L/R volume feedback
lasts about 1.5 seconds. Artwork and the visualizer are rendered locally from
the prepared packet and actual audio samples, without JPEG decoding or FFT.
