import math
import os
import struct
import subprocess

import pytest
from fastapi.testclient import TestClient

from server.adpcm import Encoder
from server.app import create_app
from server.config import Settings
from tests.test_server import FakeJob


def test_reference_vector_and_fragmented_pcm():
    pcm = struct.pack("<5h", 0, 1000, -1000, 30000, -30000)
    expected = bytes.fromhex("000000000500f7f7")
    assert Encoder().feed(pcm, final=True) == expected
    for split in range(len(pcm) + 1):
        encoder = Encoder()
        assert encoder.feed(pcm[:split]) + encoder.feed(pcm[split:], final=True) == expected
    with pytest.raises(ValueError):
        Encoder().feed(b"x", final=True)


@pytest.mark.parametrize("count", [0, 1, 2, 511, 512, 513, 1025, 22050])
def test_blocks_ignore_upstream_read_boundaries(count):
    pcm = struct.pack(f"<{count}h", *((i * 997) % 65536 - 32768 for i in range(count)))
    expected = Encoder().feed(pcm, final=True)
    encoder = Encoder()
    fragmented = b"".join(encoder.feed(pcm[i:i+137]) for i in range(0, len(pcm), 137))
    assert fragmented + encoder.feed(b"", final=True) == expected
    assert not encoder.pending


def test_matches_independent_ffmpeg_ima_decoder():
    # 505 samples form exactly one standard 256-byte mono IMA WAV block.
    samples = [int(20000 * math.sin(i * 0.13)) for i in range(505)]
    pcm = struct.pack("<505h", *samples)
    encoded = Encoder().feed(pcm, final=True)
    block = encoded[:4] + encoded[6:]
    assert len(block) == 256
    fmt = struct.pack("<HHIIHHHH", 0x11, 1, 22050, 11178, 256, 4, 2, 505)
    chunks = b"fmt " + struct.pack("<I", len(fmt)) + fmt
    chunks += b"fact" + struct.pack("<II", 4, 505)
    chunks += b"data" + struct.pack("<I", len(block)) + block
    wav = b"RIFF" + struct.pack("<I", 4 + len(chunks)) + b"WAVE" + chunks
    result = subprocess.run([os.getenv("FFMPEG_BIN", "ffmpeg"), "-v", "error",
                             "-i", "pipe:0", "-f", "s16le", "pipe:1"],
                            input=wav, capture_output=True, check=True).stdout
    decoded = struct.unpack("<505h", result)
    # IMA is lossy; verify useful fidelity, rather than promising PCM quality.
    noise = sum((a-b)**2 for a,b in zip(samples[40:], decoded[40:]))
    signal = sum(a*a for a in samples[40:])
    assert 10 * math.log10(signal/noise) > 25
    # FFmpeg uses the equivalent multiply form with slightly different rounding.
    assert decoded[:5] == (0, 13, 43, 106, 242)


def test_stream_negotiation_cleanup_and_seek():
    jobs = []

    async def factory(*_):
        job = FakeJob()
        jobs.append(job)
        return job

    with TestClient(create_app(Settings(), job_factory=factory)) as client:
        for offset in (0, 5000):
            response = client.get(f"/v1/audio?codec=ima4-ds-v1&start_ms={offset}")
            assert response.status_code == 200
            assert response.headers["x-audio-format"] == "ima4-ds-v1"
            assert response.headers["x-audio-start-ms"] == str(offset)
            assert struct.unpack_from("<H", response.content, 4)[0] == 64
            assert len(response.content) == 38  # exact final sample count, no padding
            assert not client.get("/healthz").json()["busy"]
        assert client.get("/v1/audio?codec=unknown").status_code == 422
        assert len(client.get("/v1/audio").content) == 128  # old ROM compatibility
    assert all(job.closed == 1 for job in jobs)


def test_real_ffmpeg_duration_and_compression():
    settings = Settings(test_seconds=1, ffmpeg=os.getenv("FFMPEG_BIN", "ffmpeg"))
    with TestClient(create_app(settings)) as client:
        compressed = client.get("/v1/audio?codec=ima4-ds-v1").content
        pcm = client.get("/v1/audio").content
    assert len(pcm) == 44100
    assert len(compressed) < len(pcm) * 0.26
    at = count = 0
    while at < len(compressed):
        predictor, index, reserved, samples = struct.unpack_from("<hBBH", compressed, at)
        assert 0 <= index <= 88 and reserved == 0 and 1 <= samples <= 512
        count += samples
        at += 6 + samples // 2
    assert at == len(compressed) and count == 22050
