import asyncio
from contextlib import asynccontextmanager
from dataclasses import asdict
import logging
import time
from collections import OrderedDict
from urllib.parse import quote

from fastapi import FastAPI, HTTPException, Query, Request
from starlette.responses import StreamingResponse, Response
from starlette.background import BackgroundTask

from .audio import AudioError, AudioJob
from .config import RATE, Settings
from .source import Source, SourceError
from .artwork import ArtworkStore, ArtworkError

log = logging.getLogger(__name__)


def create_app(settings=None, source=None, job_factory=None, artwork_store=None):
    settings = settings or Settings.from_env()
    source = source or Source(settings)
    job_factory = job_factory or AudioJob.open
    active = asyncio.Lock()
    lookup = asyncio.Lock()
    artwork_gate = asyncio.Lock()
    artwork_store = artwork_store or ArtworkStore()
    jobs = set()
    pending = set()
    # Keep short-lived signed sources for fast seek/restart of recent tracks.
    track_cache = OrderedDict()

    @asynccontextmanager
    async def lifespan(app):
        yield
        if pending:
            await asyncio.gather(*list(pending), return_exceptions=True)
        for job in list(jobs):
            await job.close()

    app = FastAPI(title="SoundCloud DS bridge", lifespan=lifespan)

    async def lookup_source(function, gate=None):
        gate = gate if gate is not None else lookup
        # Serialize wrapper access; no request can build an unbounded queue.
        if gate.locked():
            raise HTTPException(503, "Track lookup in progress; retry later")
        await gate.acquire()
        task = asyncio.create_task(asyncio.to_thread(function))
        pending.add(task)

        def finished(work):
            pending.discard(work)
            gate.release()
            if not work.cancelled():
                work.exception()  # Consume errors even if its HTTP caller disconnected.

        task.add_done_callback(finished)
        try:
            return await asyncio.shield(task)
        except SourceError as exc:
            raise HTTPException(502, str(exc)) from exc

    async def resolve(track_id=None):
        key = track_id or "default"
        cached = track_cache.get(key)
        if cached and time.monotonic() - cached[0] < 120:
            track_cache.move_to_end(key)
            return cached[1]
        result = await lookup_source(source.resolve if track_id is None else lambda: source.resolve(track_id))
        track_cache[key] = (time.monotonic(), result)
        track_cache.move_to_end(key)
        while len(track_cache) > 32:
            track_cache.popitem(last=False)
        return result

    @app.get("/v1/search")
    async def search(q: str = Query(min_length=1, max_length=80),
                     offset: int = Query(default=0, ge=0, le=5000),
                     limit: int = Query(default=4, ge=1, le=4)):
        query = q.strip()
        if not query:
            raise HTTPException(422, "Enter a search query")
        result = await lookup_source(lambda: source.search(query, offset, limit))
        return {**result, "tracks": [asdict(track) for track in result["tracks"]]}

    @app.get("/v1/related")
    async def related(track_id: str | None = Query(default=None, pattern=r"^[0-9]{1,20}$"),
                      limit: int = Query(default=12, ge=1, le=12)):
        result = await lookup_source(lambda: source.related(track_id, limit))
        return {**result, "tracks": [asdict(track) for track in result["tracks"]]}

    @app.get("/healthz")
    async def health():
        return {"status": "ok", "source": settings.source, "busy": active.locked()}

    @app.get("/v1/track")
    async def metadata(track_id: str | None = Query(default=None, pattern=r"^[0-9]{1,20}$")):
        track = await resolve(track_id)
        data = asdict(track)
        del data["input_url"]
        data.pop("artwork_url", None)
        data["has_artwork"] = bool(track.artwork_url)
        data.update(sample_rate=RATE, channels=1, format="s16le")
        return data

    @app.get("/v1/artwork")
    async def artwork(track_id: str | None = Query(default=None, pattern=r"^[0-9]{1,20}$")):
        track = await resolve(track_id)
        if not track.artwork_url:
            return Response(status_code=204)
        try:
            payload = await lookup_source(lambda: artwork_store.get(track.artwork_url), artwork_gate)
        except ArtworkError as exc:
            raise HTTPException(404, "Artwork is unavailable") from exc
        return Response(payload, media_type="application/octet-stream",
                        headers={"Cache-Control": "private, max-age=3600"})

    @app.get("/v1/audio")
    async def audio(request: Request,
                    track_id: str | None = Query(default=None, pattern=r"^[0-9]{1,20}$"),
                    start_ms: int = Query(default=0, ge=0, le=86400000)):
        if active.locked():
            raise HTTPException(409, "Another audio stream is active")
        await active.acquire()
        job = None
        cleaned = False
        startup = None

        async def cleanup():
            nonlocal cleaned
            if cleaned:
                return
            cleaned = True
            if job is not None:
                await job.close()
                jobs.discard(job)
            if active.locked():
                active.release()

        try:
            async def prepare():
                nonlocal job
                track = await resolve(track_id)
                if start_ms >= track.duration_ms:
                    raise HTTPException(416, "Seek position is outside the track")
                if await request.is_disconnected():
                    raise asyncio.CancelledError()
                job = await job_factory(settings, track, start_ms)
                jobs.add(job)
                first = await asyncio.wait_for(job.read(), timeout=35 if start_ms else 10)
                if not first:
                    raise AudioError("Audio source is empty")
                return track, first

            startup = asyncio.create_task(prepare())
            while not startup.done():
                await asyncio.wait({startup}, timeout=0.1)
                if not startup.done() and await request.is_disconnected():
                    startup.cancel()
                    await asyncio.gather(startup, return_exceptions=True)
                    await asyncio.shield(cleanup())
                    return Response(status_code=499)
            track, first = await startup
        except BaseException as exc:
            if startup is not None and not startup.done():
                startup.cancel()
                await asyncio.gather(startup, return_exceptions=True)
            await asyncio.shield(cleanup())
            if isinstance(exc, AudioError):
                track_cache.pop(track_id or "default", None)
                raise HTTPException(502, str(exc)) from exc
            if isinstance(exc, TimeoutError):
                track_cache.pop(track_id or "default", None)
                raise HTTPException(504, "Audio startup timed out; retry later") from exc
            raise

        async def generate():
            try:
                yield first
                while block := await job.read():
                    yield block
            except AudioError:
                track_cache.pop(track_id or "default", None)
                log.warning("Audio source interrupted")
                # Abort rather than send a successful HTTP chunked terminator.
                raise
            finally:
                await asyncio.shield(cleanup())

        headers = {
            "Cache-Control": "no-store", "X-Accel-Buffering": "no",
            "X-Audio-Rate": str(RATE), "X-Audio-Channels": "1", "X-Audio-Format": "s16le",
            "X-Track-Title": quote(track.title[:120], safe=""),
            "X-Track-Artist": quote(track.artist[:80], safe=""),
            "X-Track-Duration-Ms": str(track.duration_ms),
            "X-Audio-Start-Ms": str(start_ms),
        }
        return StreamingResponse(generate(), media_type="application/octet-stream",
                                 headers=headers, background=BackgroundTask(cleanup))

    return app


app = create_app()
