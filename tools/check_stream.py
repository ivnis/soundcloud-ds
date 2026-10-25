"""Read a complete bridge stream, checking framing, PCM length and duration.

This drains as fast as possible; it is not a realtime DS performance test.
"""
import argparse
import json
import socket
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
        assert response.headers["X-Audio-Format"] == "s16le", "Unexpected sample format"
        start_ms = int(response.headers.get("X-Audio-Start-Ms", "0"))
        expected_ms = int(response.headers["X-Track-Duration-Ms"]) - start_ms
        count = 0
        while block := response.read(16384):
            count += len(block)
    if not count or count % 2:
        raise SystemExit(f"Invalid PCM length: {count}")
    duration_ms = count * 1000 / BYTES_PER_SECOND
    if abs(duration_ms - expected_ms) > 2000:
        raise SystemExit(f"Duration mismatch: {duration_ms:.0f} vs {expected_ms} ms")
    print(f"OK: {count} PCM bytes, {duration_ms / 1000:.3f}s audio, "
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
