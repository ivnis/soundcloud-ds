"""Local-only bridge with a graceful stop-file control for the desktop launcher."""
import argparse
import asyncio
from pathlib import Path
import sys

import uvicorn

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from server.app import create_app
from server.config import Settings


async def run(args):
    import imageio_ffmpeg

    settings = Settings(source=args.source, track_url=args.track,
                        ffmpeg=imageio_ffmpeg.get_ffmpeg_exe())
    server = uvicorn.Server(uvicorn.Config(
        create_app(settings), host="127.0.0.1", port=8765, workers=1,
        timeout_graceful_shutdown=3,
    ))
    task = asyncio.create_task(server.serve())
    try:
        while not task.done():
            if args.stop_file.exists():
                server.should_exit = True
                break
            await asyncio.sleep(0.25)
        await task
    finally:
        if not task.done():
            server.should_exit = True
            await task


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", choices=["test", "soundcloud"], required=True)
    parser.add_argument("--track", default="https://soundcloud.com/forss/flickermood")
    parser.add_argument("--stop-file", type=Path, required=True)
    asyncio.run(run(parser.parse_args()))
