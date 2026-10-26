import asyncio
import os
import struct
import wave

import pytest
from fastapi.testclient import TestClient

from server.app import create_app
from server.audio import AudioJob, AudioError
from server.config import Settings, BYTES_PER_SECOND
from server.source import Source, Track
from tests.test_server import FakeJob


class CountingSource(Source):
    def __init__(self):
        super().__init__(Settings())
        self.calls = []

    def resolve(self, track_id=None):
        self.calls.append(track_id)
        return super().resolve()


def test_seek_offset_headers_and_short_lived_source_cache():
    source, offsets = CountingSource(), []

    async def factory(settings, track, start_ms):
        offsets.append(start_ms)
        return FakeJob()

    with TestClient(create_app(Settings(), source=source, job_factory=factory)) as client:
        for offset in [5000, 10000, 2000]:
            response = client.get(f"/v1/audio?track_id=42&start_ms={offset}")
            assert response.status_code == 200
            assert response.headers["x-audio-start-ms"] == str(offset)
            assert response.headers["x-track-duration-ms"] == "600000"
        assert source.calls == ["42"]
        assert offsets == [5000, 10000, 2000]
        assert not client.get("/healthz").json()["busy"]


def test_failed_cached_source_is_refreshed():
    source = CountingSource()
    attempts = []

    async def factory(*_):
        attempts.append(1)
        return FakeJob(fail=len(attempts) == 2)

    with TestClient(create_app(Settings(), source=source, job_factory=factory)) as client:
        assert client.get("/v1/audio?track_id=42").status_code == 200
        assert client.get("/v1/audio?track_id=42&start_ms=5000").status_code == 502
        assert client.get("/v1/audio?track_id=42&start_ms=5000").status_code == 200
        assert source.calls == ["42", "42"]


@pytest.mark.parametrize("position,status", [("-1", 422), ("x", 422), ("86400001", 422),
                                             ("600000", 416), ("700000", 416)])
def test_invalid_positions_do_not_start_ffmpeg(position, status):
    calls = []

    async def factory(*_):
        calls.append(1)
        return FakeJob()

    with TestClient(create_app(Settings(), job_factory=factory)) as client:
        assert client.get(f"/v1/audio?start_ms={position}").status_code == status
        assert not calls
        assert not client.get("/healthz").json()["busy"]


def test_generated_audio_seek_emits_only_remaining_duration():
    async def check():
        settings = Settings(test_seconds=3, ffmpeg=os.getenv("FFMPEG_BIN", "ffmpeg"))
        job = await AudioJob.open(settings, Source(settings).resolve(), 1500)
        size = 0
        try:
            while block := await job.read():
                size += len(block)
        finally:
            await job.close()
        assert size == BYTES_PER_SECOND * 3 // 2
        assert job.process.returncode == 0
        with pytest.raises(AudioError):
            await AudioJob.open(settings, Source(settings).resolve(), 3000)

    asyncio.run(check())


@pytest.mark.parametrize("start_ms", [500, 501, 731])
def test_input_seek_matches_known_pcm_samples(tmp_path, start_ms):
    # A deterministic, nonperiodic PCM fixture verifies the actual sample
    # position, rather than merely checking timestamps or a constant sine.
    path = tmp_path / "position.wav"
    rate, seconds = 22050, 2
    pcm = b"".join(struct.pack("<h", ((i * 37 + i // 97) % 20001) - 10000)
                   for i in range(rate * seconds))
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(rate)
        wav.writeframes(pcm)

    async def check():
        settings = Settings(ffmpeg=os.getenv("FFMPEG_BIN", "ffmpeg"))
        track = Track("position", "test", 2000, "", str(path))
        job = await AudioJob.open(settings, track, start_ms)
        blocks = []
        try:
            while block := await job.read():
                blocks.append(block)
        finally:
            await job.close()
        first_sample = (start_ms * rate + 500) // 1000
        assert b"".join(blocks) == pcm[first_sample * 2:]

    asyncio.run(check())


def test_empty_fast_seek_retries_sequentially(monkeypatch):
    class Pipe:
        def __init__(self, blocks):
            self.blocks = iter(blocks)

        async def read(self, _):
            return next(self.blocks, b"")

    class Process:
        returncode = 0

        def __init__(self, data):
            self.stdout = Pipe([data, b""])
            self.stderr = Pipe([b""])

        async def wait(self):
            return 0

    commands = []
    processes = [Process(b""), Process(b"\x01\x00" * 100)]

    async def launch(args):
        commands.append(args)
        return processes.pop(0)

    monkeypatch.setattr(AudioJob, "_launch", staticmethod(launch))

    async def check():
        track = Track("HLS", "test", 10000, "", "https://example.sndcdn.com/test.m3u8")
        job = await AudioJob.open(Settings(), track, 5000)
        assert await job.read() == b"\x01\x00" * 100
        assert await job.read() == b""
        assert job.seek_fallback_used
        await job.close()

    asyncio.run(check())
    assert len(commands) == 2
    assert commands[0].index("-ss") < commands[0].index("-i")
    assert commands[1].index("-ss") > commands[1].index("-i")
    assert "-seg_max_retry" in commands[0]
