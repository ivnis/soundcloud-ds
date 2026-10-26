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
from .adpcm import Encoder
from .config import RATE, Settings
from .source import Source, SourceError
from .artwork import ArtworkStore, ArtworkError

log = logging.getLogger("uvicorn.error")


def create_app(settings=None, source=None, job_factory=None, artwork_store=None):
    settings = settings or Settings.from_env()
    source = source or Source(settings)
    job_factory = job_factory or AudioJob.open
    active = asyncio.Lock()
    handoff = asyncio.Lock()
    active_control = None
    latest_requests = OrderedDict()
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
        data.update(sample_rate=RATE, channels=1, format="s16le",
                    supported_formats=["s16le", "ima4-ds-v1"])
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
                    start_ms: int = Query(default=0, ge=0, le=86400000),
                    codec: str = Query(default="s16le", pattern=r"^(s16le|ima4-ds-v1)$"),
                    session: str | None = Query(default=None, pattern=r"^[0-9a-f]{16}$"),
                    request_id: int | None = Query(default=None, ge=1, le=4294967295)):
        nonlocal active_control
        if (session is None) != (request_id is None):
            raise HTTPException(422, "Session and request_id must be supplied together")
        # Serialize stream replacement instead of rejecting a reconnect that
        # arrives while the previous FFmpeg job is being stopped. The lock is
        # held only for the bounded handoff/cleanup path below.
        async with handoff:
            if session and request_id <= latest_requests.get(session, 0):
                raise HTTPException(409, "This audio request was superseded")
            if active.locked():
                if not session or active_control is None or active_control["session"] != session:
                    raise HTTPException(409, "Another audio stream is active")
                # TCP disconnects can arrive late through a proxy. Explicitly
                # stop the previous stream from this ROM before claiming its slot.
                await active_control["stop"]()
            await active.acquire()
            control = {"session": session, "stop": None}
            active_control = control
            if session:
                latest_requests[session] = request_id
                latest_requests.move_to_end(session)
                while len(latest_requests) > 64:
                    latest_requests.popitem(last=False)
        job = None
        cleanup_task = None
        startup = None
        stopped = asyncio.Event()
        started = time.monotonic()
        lookup_ms = first_pcm_ms = 0
        wire_bytes = pcm_bytes = 0
        max_source_gap_ms = 0

        async def do_cleanup():
            nonlocal active_control
            try:
                if job is not None:
                    await job.close()
            finally:
                jobs.discard(job)
                # A late finally from the replaced generator must never release
                # the new stream's lock. All callers await the same cleanup task.
                if active_control is control:
                    active_control = None
                    active.release()
                log.info("audio_end track=%s start_ms=%d codec=%s elapsed_ms=%d pcm_bytes=%d wire_bytes=%d max_source_gap_ms=%d replaced=%s",
                         track_id or "default", start_ms, codec, (time.monotonic()-started)*1000,
                         pcm_bytes, wire_bytes, max_source_gap_ms, stopped.is_set())

        async def cleanup():
            nonlocal cleanup_task
            if cleanup_task is None:
                cleanup_task = asyncio.create_task(do_cleanup())
            await asyncio.shield(cleanup_task)

        async def stop():
            stopped.set()
            if startup is not None and not startup.done():
                startup.cancel()
                await asyncio.gather(startup, return_exceptions=True)
            await cleanup()

        control["stop"] = stop

        try:
            async def prepare():
                nonlocal job, lookup_ms, first_pcm_ms
                before = time.monotonic()
                track = await resolve(track_id)
                lookup_ms = int((time.monotonic()-before)*1000)
                if start_ms >= track.duration_ms:
                    raise HTTPException(416, "Seek position is outside the track")
                if await request.is_disconnected():
                    raise asyncio.CancelledError()
                job = await job_factory(settings, track, start_ms)
                jobs.add(job)
                before = time.monotonic()
                first = await asyncio.wait_for(job.read(), timeout=35 if start_ms else 10)
                first_pcm_ms = int((time.monotonic()-before)*1000)
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
            if stopped.is_set() and isinstance(exc, asyncio.CancelledError):
                return Response(status_code=409)
            if isinstance(exc, AudioError):
                track_cache.pop(track_id or "default", None)
                raise HTTPException(502, str(exc)) from exc
            if isinstance(exc, TimeoutError):
                track_cache.pop(track_id or "default", None)
                raise HTTPException(504, "Audio startup timed out; retry later") from exc
            raise

        async def generate():
            nonlocal wire_bytes, pcm_bytes, max_source_gap_ms
            try:
                if stopped.is_set():
                    return
                encoder = Encoder() if codec == "ima4-ds-v1" else None
                pcm_bytes += len(first)
                encoded = encoder.feed(first) if encoder else first
                if encoded:
                    wire_bytes += len(encoded)
                    yield encoded
                    await asyncio.sleep(0)
                while not stopped.is_set():
                    before = time.monotonic()
                    block = await job.read()
                    max_source_gap_ms = max(max_source_gap_ms, int((time.monotonic()-before)*1000))
                    if stopped.is_set() or not block:
                        break
                    pcm_bytes += len(block)
                    encoded = encoder.feed(block) if encoder else block
                    if encoded:
                        wire_bytes += len(encoded)
                        yield encoded
                        # Buffered subprocess reads/ASGI sends may both complete
                        # synchronously. Let stop/search requests run promptly.
                        await asyncio.sleep(0)
                if encoder and not stopped.is_set():
                    tail = encoder.feed(b"", final=True)
                    if tail:
                        wire_bytes += len(tail)
                        yield tail
            except AudioError:
                if stopped.is_set():
                    return
                track_cache.pop(track_id or "default", None)
                log.warning("Audio source interrupted")
                # Abort rather than send a successful HTTP chunked terminator.
                raise
            finally:
                await asyncio.shield(cleanup())

        headers = {
            "Cache-Control": "no-store", "X-Accel-Buffering": "no",
            "X-Audio-Rate": str(RATE), "X-Audio-Channels": "1", "X-Audio-Format": codec,
            "X-Track-Title": quote(track.title[:120], safe=""),
            "X-Track-Artist": quote(track.artist[:80], safe=""),
            "X-Track-Duration-Ms": str(track.duration_ms),
            "X-Audio-Start-Ms": str(start_ms),
            "Server-Timing": f"lookup;dur={lookup_ms}, first_pcm;dur={first_pcm_ms}",
        }
        log.info("audio_start track=%s start_ms=%d codec=%s lookup_ms=%d first_pcm_ms=%d total_ms=%d",
                 track_id or "default", start_ms, codec, lookup_ms, first_pcm_ms,
                 (time.monotonic()-started)*1000)
        return StreamingResponse(generate(), media_type="application/octet-stream",
                                 headers=headers, background=BackgroundTask(cleanup))

    return app


app = create_app()
