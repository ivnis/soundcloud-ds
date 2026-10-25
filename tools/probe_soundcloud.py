"""Resolve a public track and decode 2 seconds without exposing signed URLs.

Run from the repository root:
  python -m tools.probe_soundcloud https://soundcloud.com/artist/track
"""
import asyncio
import os
import sys
from dataclasses import replace

from server.audio import AudioError, AudioJob
from server.config import BYTES_PER_SECOND, Settings
from server.source import Source, SourceError


async def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python -m tools.probe_soundcloud TRACK_URL")
    os.environ.update(SC_SOURCE="soundcloud", SC_TRACK_URL=sys.argv[1])
    settings = Settings.from_env()
    track = await asyncio.to_thread(Source(settings).resolve)
    print(f"Resolved: {track.artist} - {track.title} ({track.duration_ms} ms)")
    job = await AudioJob.open(settings, replace(track, duration_ms=2000))
    try:
        count = 0
        while block := await job.read():
            count += len(block)
        if count != BYTES_PER_SECOND * 2:
            raise RuntimeError(f"Expected 88200 PCM bytes, got {count}")
        print("SoundCloud -> FFmpeg -> PCM: OK (2 seconds)")
    finally:
        await job.close()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except (SourceError, AudioError) as exc:
        raise SystemExit(str(exc)) from None
