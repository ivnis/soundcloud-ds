# Audio transport optimization

**English** | [Русский](AUDIO.ru.md)

These changes are in the development build after v0.2.0. Update the bridge and
ROM together. The published v0.2.0 assets still use PCM audio.

The bridge converts upstream audio with FFmpeg to 22,050 Hz mono, then encodes
IMA ADPCM in small blocks. The ROM decodes the stream to its existing PCM seek
buffer. Playback, visuals, volume, and local five-second skips use decoded samples.

| Transport | Audio bytes per second, excluding HTTP/TCP |
| --- | --- |
| Previous 16-bit PCM | 44,100 |
| IMA ADPCM, 512-sample blocks | about 11,284 |

This reduces audio traffic by about 74.4%, without lowering sample rate.
ADPCM is lossy: it introduces quantization noise. It is inexpensive to decode
with integer arithmetic, has no codec library dependency on the DS, and does
not wait for the complete track to be converted. Actual hardware throughput
and listening quality must still be tested on a DS.

The three-second prefill is 132,300 decoded bytes, requiring roughly 33.8 KB
on the network. The six-second history/20-second lookahead stay in PCM for
instant local seeking. Requests outside this window restart encoding at the
requested sample position. Sample counts avoid padding at the track end.
The client uses the same three-second target after an underrun. The large ahead
window absorbs jitter while playback continues. It cannot compensate for an
average connection speed below the encoded audio rate.

Artwork remains exactly 64x64, 256 colors, 4,624 wire bytes. It can start after
0.25 seconds of decoded audio has arrived, rather than waiting for two seconds.
Recommendations start after 12 seconds are buffered, playback has started,
and the cover request has finished, so they do not compete with startup.

## TCP receive window

The pinned DSWifi/lwIP defaults to `TCP_WND = 4 * 1460`, only 5,840 bytes.
A window of that size is a throughput limit on a high-latency path: at 500 ms
RTT the theoretical window/RTT ceiling is only 11,680 bytes/s, close to the
11,284 bytes/s needed by this codec. Packet loss or higher RTT can empty an
audio buffer even when a desktop with a larger TCP window downloads quickly.
This is a plausible contributor to the reported repeated pauses; the actual
DS-to-edge RTT/packet loss have not been measured.

The ROM Makefile builds a pinned ARM9 DSWifi library with a 10,220-byte receive
window (7 MSS), below the 12 KiB shared RX buffer with some headroom. At 500 ms
RTT its window-limited ceiling is about 20 KB/s. A larger window may leave too
little space for DSWifi's shared packet queue and increase packet loss.
the previous 32-MSS experiment while protecting the radio packet queue. It
does not diagnose or eliminate an independently stalled access point or tunnel.
The first build needs Git, Internet access, and the SDK's usual POSIX tools; sources/builds live in ignored
`tools/.cache/dswifi-stream`. Subsequent builds reuse the archive. Memory is
allocated as needed; a larger window does not preallocate the entire audio
track. The 20-second PCM window uses 1,146,600 bytes including six seconds of
history. The linked ROM must still be tested for stability on DS Fat.

Source: [pinned lwIP settings](https://github.com/blocksds/dswifi/blob/59d2636dd851ea26a4ea49f329321d3bfb002066/source/arm9/lwip/include/lwipopts.h).
DSWifi and lwIP licenses are retained in `client/assets/DSWIFI-COPYING*`.

### Reading after the buffer fills

Previously the PCM-space budget was recalculated on every frame. Once near
full, the window could allow reads of only 1–255 wire bytes. The new download
gate reads every frame as soon as it can accept a 2 KiB burst, with an 8 KiB
per-frame maximum. It stops only when less than about 9 KiB of decoded space
remains, protecting the PCM window from a partially received block. That is
under 0.21 seconds of audio. Actual socket reads can still return fewer bytes
when the network supplies fewer. The 20-second buffer fills as fast as the
network and ARM9 decoder allow, then resumes quickly as playback creates room.

The larger TCP window changed lwIP's default explicit window-update threshold
from 1,460 to 5,840 bytes (`min(TCP_WND/4, TCP_MSS*4)`). The build now pins this
threshold to one MSS (1,460 bytes), so freed receive space is advertised sooner.
The versioned preparation stamp applies this change to existing cached builds.
These address small-read overhead and receive-window update latency; whether
they resolve the physical console's reported slowdown remains to be tested.

## Compatibility and diagnostics

### Slow switches after v0.2.0

The earlier single-stream guard returned HTTP 409 until the previous TCP
connection was observed to disconnect. A proxy can delay that disconnect;
the ROM then retried once per second for up to 38 seconds while showing
Buffering. Console requests in the local bridge log reproduced this 409/200
pattern. This is a confirmed cause of delayed track changes, not proof that
every pause has the same cause.

The development ROM now sends a per-boot, MAC-hashed `session` identifier and
increasing `request_id`. A newer request from the same session cancels the
old startup/FFmpeg job and awaits cleanup before starting. Older request IDs
are rejected so delayed requests cannot replace a newer selection. Other
sessions and old ROMs retain the single-stream 409 behavior. Identifiers are
not authentication. Restarting the ROM creates a new session.

`Server-Timing` reports `lookup` and `first_pcm` durations in milliseconds.
Server logs contain `audio_start` (lookup, initial conversion, total startup)
and `audio_end` (decoded/wire bytes, largest upstream read delay, replacement).
These separate SoundCloud startup stalls from transfer delays. A proxy can
still buffer after the server has emitted data; server timings alone cannot
measure what the DS received.

A second reproduced switch delay was FFmpeg shutdown with a full stdout pipe:
waiting for process exit without draining stdout hit the three-second kill
timeout on Windows and left pipe transports open. Cleanup now terminates and
drains stdout under a read lock while waiting for exit. The regression test
requires cleanup in under two seconds with the output pipe deliberately full.

Press SELECT to toggle the existing diagnostic overlay: buffered seconds, recent
wire KB/s, underrun count (`G`) and automatic reconnect count (`R`). The rate is sampled over approximately half a second;
it naturally falls when a full audio buffer applies backpressure. Compare the
rate while the buffer is low with the codec's 11.3 KB/s requirement.

### Silent TCP stalls

An associated Wi-Fi link does not prove its TCP stream is still delivering data.
The previous client waited up to 65 seconds for the general network timeout.
The audio request is polled every frame while Wi-Fi is connected, including
startup buffering, underruns and pause. Buffer size alone never restarts the
request. Playback waits for three seconds after an underrun; explicit socket
failures and truncated responses can still trigger recovery, with increasing
retry delays capped at one second. 32 KiB of useful transfer resets the delay.

Recovery resumes from the END of retained decoded audio, not from the beginning
of the song. It retains playback position, history, cover, volume and pause state.
The millisecond seek rounds down; the small duplicated PCM prefix is discarded
before appending, accounting for FFmpeg's nearest-sample seek rounding. Session ownership
lets the bridge replace the old request even if its disconnect is delayed.

Transport recovery reopens TCP without flushing the cached DNS address. It does
not reassociate the radio or retain Wi-Fi passwords. If the Wi-Fi driver/link
itself cannot transfer packets, audio remains buffered while retries continue;
a manual Wi-Fi reconnect may still be necessary. Physical DS testing is required
to distinguish this from a proxy or TCP stall. Variable speed alone is normal
when the buffer fills.

### Transport compatibility

New ROMs request `/v1/audio?codec=ima4-ds-v1`. A new bridge retains the default
`s16le` response for old ROMs and desktop tools. An old bridge may ignore the
codec query and return PCM; the new ROM accepts either format from the header.
Metadata includes `supported_formats`. No TLS, global network, or tunnel
configuration changes are required.

Check an entire compressed stream:

```sh
python -m tools.check_stream 'http://127.0.0.1:8000/v1/audio?codec=ima4-ds-v1'
```

The checker validates HTTP completion, block framing, and exact sample counts
against the remaining duration. It downloads as fast as possible; it does not
measure the DS radio. Compare direct LAN access with the tunnel on the same
console to isolate the extra network path. Source lookup and upstream startup
can still delay the first sound even when the compressed stream is small.

## Wire format: `ima4-ds-v1`

`X-Audio-Rate: 22050`, `X-Audio-Channels: 1`, and
`X-Audio-Format: ima4-ds-v1` describe the decoded format/transport.
HTTP chunk boundaries have no relation to codec block boundaries.

Each independent block has a six-byte little-endian header (`<hBBH`): signed
initial predictor, step-table index 0..88, reserved byte zero, and sample count
1..512. The predictor is the first output sample. Remaining samples use standard
4-bit IMA codes, low nibble first, with an unused high nibble for an odd number
of codes. Payload length is `sample_count // 2` bytes. The index is carried
between encoder blocks but repeated in every header; the predictor resets to
the actual first sample. A final short block preserves exact duration.

The decoder uses fixed buffers, validates headers and incomplete blocks, and
reserves enough PCM space for a pending block before reading more network data.
Server tests include an independent FFmpeg IMA WAV decode for fidelity; native
tests cover reference samples, malformed blocks, HTTP chunks and every split
of the reference response.

Algorithm reference: [IMA ADPCM recommended practices](https://www.cs.columbia.edu/~hgs/audio/dvi/IMA_ADPCM.pdf).
