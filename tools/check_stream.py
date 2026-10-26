"""Read a complete bridge stream, checking framing, sample count and duration.

This drains as fast as possible; it is not a realtime DS performance test.
"""
import argparse
import json
import socket
import struct
import time
import urllib.error
import urllib.request

from server.config import BYTES_PER_SECOND


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("url", help="For example http://127.0.0.1:8000/v1/audio")
    args = parser.parse_args()
    start = time.monotonic()
    with urllib.request.urlopen(args.url, timeout=70) as response:
        assert response.headers["X-Audio-Rate"] == "22050", "Unexpected sample rate"
        assert response.headers["X-Audio-Channels"] == "1", "Unexpected channels"
        codec = response.headers["X-Audio-Format"]
        assert codec in ("s16le", "ima4-ds-v1"), "Unexpected sample format"
        start_ms = int(response.headers.get("X-Audio-Start-Ms", "0"))
        expected_ms = int(response.headers["X-Track-Duration-Ms"]) - start_ms
        count = 0
        samples = 0
        pending = bytearray()
        while block := response.read(16384):
            count += len(block)
            if codec == "ima4-ds-v1":
                pending.extend(block)
                at = 0
                while len(pending) - at >= 6:
                    _, index, reserved, size = struct.unpack_from("<hBBH", pending, at)
                    assert index <= 88 and reserved == 0 and 1 <= size <= 512, "Invalid ADPCM block"
                    length = 6 + size // 2
                    if len(pending) - at < length:
                        break
                    at += length
                    samples += size
                del pending[:at]
    if codec == "ima4-ds-v1" and pending:
        raise SystemExit("Truncated ADPCM block")
    if not count or (codec == "s16le" and count % 2):
        raise SystemExit(f"Invalid PCM length: {count}")
    duration_ms = (samples * 2 if codec == "ima4-ds-v1" else count) * 1000 / BYTES_PER_SECOND
    if abs(duration_ms - expected_ms) > 2000:
        raise SystemExit(f"Duration mismatch: {duration_ms:.0f} vs {expected_ms} ms")
    print(f"OK: {count} wire bytes ({codec}), {duration_ms / 1000:.3f}s audio, "
          f"drained in {time.monotonic() - start:.2f}s")


if __name__ == "__main__":
    try:
        main()
    except urllib.error.HTTPError as exc:
        try:
            detail = json.load(exc).get("detail", exc.reason)
        except (ValueError, AttributeError):
            detail = exc.reason
        raise SystemExit(f"Bridge HTTP {exc.code}: {detail}") from None
    except (urllib.error.URLError, socket.timeout) as exc:
        raise SystemExit(f"Bridge connection failed: {exc}") from None
