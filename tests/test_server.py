import asyncio
import socket
import subprocess
import sys
import time
import os
from pathlib import Path

import httpx
import pytest
from fastapi.testclient import TestClient

from server.app import create_app
from server.config import BYTES_PER_SECOND, Settings
from server.source import Source, SourceError


class FakeJob:
    def __init__(self, fail=False):
        self.blocks = iter([b"\x01\x00" * 32, b"\x02\x00" * 32, b""])
        self.closed = 0
        self.fail = fail

    async def read(self):
        from server.audio import AudioError
        if self.fail:
            raise AudioError("Test failure")
        return next(self.blocks)

    async def close(self):
        self.closed += 1


def test_metadata_does_not_expose_upstream_url():
    with TestClient(create_app(Settings())) as client:
        response = client.get("/v1/track")
        assert response.status_code == 200
        assert response.json()["format"] == "s16le"
        assert response.json()["sample_rate"] == 22050
        assert "input_url" not in response.json()


def test_stream_headers_and_repeated_cleanup():
    jobs = []

    async def factory(*_):
        job = FakeJob()
        jobs.append(job)
        return job

    with TestClient(create_app(Settings(), job_factory=factory)) as client:
        for _ in range(3):
            response = client.get("/v1/audio")
            assert response.status_code == 200
            assert len(response.content) == 128
            assert response.headers["x-audio-rate"] == "22050"
            assert response.headers["cache-control"] == "no-store"
            assert client.get("/healthz").json()["busy"] is False
    assert all(job.closed == 1 for job in jobs)


def test_initial_audio_failure_releases_stream_slot():
    async def factory(*_):
        return FakeJob(fail=True)

    with TestClient(create_app(Settings(), job_factory=factory)) as client:
        for _ in range(2):
            assert client.get("/v1/audio").status_code == 502
            assert client.get("/healthz").json()["busy"] is False


def test_source_failure_is_a_useful_http_error():
    class BrokenSource:
        def resolve(self):
            raise SourceError("Track is unavailable for streaming")

    with TestClient(create_app(Settings(), source=BrokenSource())) as client:
        assert client.get("/v1/track").status_code == 502
        assert client.get("/v1/audio").status_code == 502
        assert client.get("/healthz").json()["busy"] is False


def test_startup_timeout_releases_slot():
    class TimeoutJob(FakeJob):
        async def read(self):
            raise TimeoutError()

    job = TimeoutJob()

    async def factory(*_):
        return job

    with TestClient(create_app(Settings(), job_factory=factory)) as client:
        assert client.get("/v1/audio").status_code == 504
        assert client.get("/healthz").json()["busy"] is False
    assert job.closed == 1


def test_failure_after_first_block_releases_slot():
    from server.audio import AudioError

    class BrokenJob(FakeJob):
        async def read(self):
            block = await super().read()
            if block.startswith(b"\x02"):
                raise AudioError("Source lost")
            return block

    job = BrokenJob()

    async def factory(*_):
        return job

    with TestClient(create_app(Settings(), job_factory=factory)) as client:
        with pytest.raises(AudioError):
            client.get("/v1/audio")
        assert client.get("/healthz").json()["busy"] is False
    assert job.closed == 1


def test_config_rejects_arbitrary_source_url(monkeypatch):
    monkeypatch.setenv("SC_SOURCE", "soundcloud")
    monkeypatch.setenv("SC_TRACK_URL", "https://localhost/private")
    with pytest.raises(ValueError):
        Settings.from_env()


@pytest.fixture(scope="module")
def live_server():
    # A real TCP server exercises HTTP/1.0 close framing and cancellation;
    # in-process ASGI transports buffer the whole body and cannot test these.
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    env = dict(os.environ, SC_SOURCE="test", SC_TEST_SECONDS="600")
    process = subprocess.Popen(
        [sys.executable, "-m", "uvicorn", "server.app:app", "--host", "127.0.0.1",
         "--port", str(port), "--workers", "1", "--log-level", "error"],
        cwd=Path(__file__).resolve().parents[1], env=env,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    base = f"http://127.0.0.1:{port}"
    try:
        for _ in range(100):
            if process.poll() is not None:
                pytest.fail("Test bridge exited during startup")
            try:
                if httpx.get(base + "/healthz", timeout=0.2).status_code == 200:
                    break
            except httpx.HTTPError:
                pass
            time.sleep(0.05)
        else:
            pytest.fail("Test bridge did not start")
        yield base, port
    finally:
        process.terminate()
        process.wait(timeout=10)


def wait_free(base):
    for _ in range(100):
        if not httpx.get(base + "/healthz").json()["busy"]:
            return
        time.sleep(0.05)
    pytest.fail("Audio process/stream slot was not cleaned up on disconnect")


def test_http10_real_audio_and_disconnect_cleanup(live_server):
    base, port = live_server
    with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
        sock.sendall(b"GET /v1/audio HTTP/1.0\r\nHost: localhost\r\n\r\n")
        data = b""
        while b"\r\n\r\n" not in data:
            data += sock.recv(4096)
        headers, pcm = data.split(b"\r\n\r\n", 1)
        assert b"200 OK" in headers
        assert b"transfer-encoding" not in headers.lower()
        while len(pcm) < BYTES_PER_SECOND * 2:
            block = sock.recv(8192)
            assert block
            pcm += block
        assert any(pcm)
        search = httpx.get(base + "/v1/search?q=test")
        assert search.status_code == 200
        assert search.json()["tracks"][0]["id"] == "0"
        assert sock.recv(2048)  # Searching did not interrupt the ongoing PCM stream.
        assert httpx.get(base + "/v1/audio").status_code == 409
    wait_free(base)


def test_http11_real_audio(live_server):
    base, _ = live_server
    wait_free(base)
    with httpx.stream("GET", base + "/v1/audio", timeout=10) as response:
        assert response.status_code == 200
        assert response.headers["transfer-encoding"] == "chunked"
        assert len(next(response.iter_bytes())) > 0
    wait_free(base)


def test_same_console_replaces_stream_without_tcp_disconnect(live_server):
    base, port = live_server
    wait_free(base)
    session = "0123456789abcdef"
    with socket.create_connection(("127.0.0.1", port), timeout=5) as old:
        old.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
        old.sendall((f"GET /v1/audio?codec=ima4-ds-v1&session={session}&request_id=1 HTTP/1.0\r\n"
                     "Host: localhost\r\n\r\n").encode())
        data = b""
        while b"\r\n\r\n" not in data:
            data += old.recv(4096)
        assert b"200 OK" in data
        # Keep the old TCP connection OPEN, as a buffering proxy can do.
        before = time.monotonic()
        with socket.socket() as new:
            new.settimeout(10)
            new.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
            new.connect(("127.0.0.1", port))
            new.sendall((f"GET /v1/audio?codec=ima4-ds-v1&session={session}&request_id=2 HTTP/1.0\r\n"
                         "Host: localhost\r\n\r\n").encode())
            data = b""
            while b"\r\n\r\n" not in data:
                data += new.recv(4096)
            assert b"200 OK" in data
            assert time.monotonic() - before < 5
            assert b"lookup;dur=" in data
            assert httpx.get(base + "/healthz").json()["busy"]
            # A late cleanup from stream 1 must not unlock stream 2.
            assert httpx.get(base + "/v1/audio").status_code == 409
            assert httpx.get(base + f"/v1/audio?session={session}&request_id=1").status_code == 409
            assert httpx.get(base + "/v1/audio?session=fedcba9876543210&request_id=1").status_code == 409
    wait_free(base)


@pytest.mark.parametrize("query", ["session=0123456789abcdef", "request_id=1",
                                   "session=x&request_id=1", "session=0123456789abcdef&request_id=0"])
def test_invalid_stream_session_does_not_claim_slot(query):
    with TestClient(create_app(Settings())) as client:
        assert client.get("/v1/audio?" + query).status_code == 422
        assert not client.get("/healthz").json()["busy"]


def test_switch_cancels_previous_startup_and_closes_each_job_once():
    async def check():
        ready = asyncio.Event()
        jobs = []

        class WaitingJob(FakeJob):
            async def read(self):
                ready.set()
                await asyncio.Event().wait()

        async def factory(*_):
            job = WaitingJob() if not jobs else FakeJob()
            jobs.append(job)
            return job

        app = create_app(Settings(), job_factory=factory)
        async with httpx.AsyncClient(transport=httpx.ASGITransport(app=app), base_url="http://test") as client:
            old = asyncio.create_task(client.get("/v1/audio?session=0123456789abcdef&request_id=1"))
            await asyncio.wait_for(ready.wait(), 2)
            new = await asyncio.wait_for(client.get("/v1/audio?session=0123456789abcdef&request_id=2"), 2)
            assert new.status_code == 200 and len(new.content) == 128
            assert (await asyncio.wait_for(old, 2)).status_code == 409
            assert not (await client.get("/healthz")).json()["busy"]
        assert len(jobs) == 2 and all(job.closed == 1 for job in jobs)

    asyncio.run(check())


def test_ffmpeg_finite_pcm_length():
    from server.audio import AudioJob

    async def check():
        settings = Settings(test_seconds=1, ffmpeg=os.getenv("FFMPEG_BIN", "ffmpeg"))
        job = await AudioJob.open(settings, Source(settings).resolve())
        try:
            blocks = []
            while block := await job.read():
                blocks.append(block)
            assert sum(map(len, blocks)) == BYTES_PER_SECOND
        finally:
            await job.close()
        assert job.process.returncode == 0

    asyncio.run(check())


def test_ffmpeg_close_with_full_output_pipe_does_not_wait_for_kill_timeout():
    from server.audio import AudioJob

    async def check():
        settings = Settings(test_seconds=600, ffmpeg=os.getenv("FFMPEG_BIN", "ffmpeg"))
        job = await AudioJob.open(settings, Source(settings).resolve())
        await job.read()
        # Leave stdout unread so the asyncio pipe reaches its high-water mark.
        await asyncio.sleep(0.3)
        before = time.monotonic()
        await asyncio.wait_for(job.close(), 2)
        assert time.monotonic() - before < 2
        assert job.process.returncode is not None

    asyncio.run(check())
