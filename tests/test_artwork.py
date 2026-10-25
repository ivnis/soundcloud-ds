import asyncio
from dataclasses import replace
from io import BytesIO
import struct

import httpx
from PIL import Image
import pytest
from fastapi.testclient import TestClient

from server.app import create_app
from server.artwork import (ArtworkStore, ArtworkError, PACKET_SIZE, allowed_url,
                            encode_artwork, contrast, unpack_rgb555)
from server.config import Settings
from server.source import Source


def image_bytes(color=(38, 94, 205), mode="RGB"):
    image = Image.new(mode, (120, 80), color)
    output = BytesIO()
    image.save(output, format="PNG")
    return output.getvalue()


@pytest.mark.parametrize("color", [(38, 94, 205), (0, 0, 0), (255, 255, 255), (110, 110, 110), (190, 35, 65)])
def test_packet_palette_and_accent_contrast(color):
    payload = encode_artwork(image_bytes(color))
    assert len(payload) == PACKET_SIZE == 4624
    magic, version, width, height, accent, foreground, count = struct.unpack("<4s6H", payload[:16])
    assert (magic, version, width, height, count) == (b"SCAR", 1, 64, 64, 256)
    assert contrast(unpack_rgb555(accent), (15, 18, 26)) >= 3
    assert contrast(unpack_rgb555(accent), unpack_rgb555(foreground)) >= 4.5
    palette = struct.unpack("<256H", payload[16:528])
    assert all(color & 0x8000 for color in palette)
    assert len(payload[528:]) == 4096


def test_transparency_and_bad_images():
    assert len(encode_artwork(image_bytes((255, 0, 0, 0), "RGBA"))) == PACKET_SIZE
    with pytest.raises(ArtworkError):
        encode_artwork(b"not an image")
    huge = Image.new("RGB", (2001, 2000))
    output = BytesIO()
    huge.save(output, format="PNG")
    with pytest.raises(ArtworkError):
        encode_artwork(output.getvalue())


@pytest.mark.parametrize("url", ["http://i1.sndcdn.com/a.jpg", "https://localhost/a.jpg",
                                  "https://evil-sndcdn.com/a.jpg", "file:///tmp/image.png",
                                  "https://i1.sndcdn.com:bad/a.jpg", "https://user@i1.sndcdn.com/a.jpg"])
def test_only_expected_https_hosts_are_fetched(url):
    assert not allowed_url(url)
    calls = []
    store = ArtworkStore(fetcher=lambda url: calls.append(url))
    with pytest.raises(ArtworkError):
        store.get(url)
    assert not calls


def test_lru_cache_is_bounded_and_reuses_processed_image():
    calls = []

    def fetch(url):
        calls.append(url)
        return image_bytes()

    store = ArtworkStore(fetcher=fetch, max_entries=2)
    first = store.get("https://i1.sndcdn.com/1.jpg")
    assert first == store.get("https://i1.sndcdn.com/1.jpg")
    store.get("https://i1.sndcdn.com/2.jpg")
    store.get("https://i1.sndcdn.com/3.jpg")
    assert len(store.cache) == 2 and len(calls) == 3
    store.get("https://i1.sndcdn.com/1.jpg")
    assert len(calls) == 4


class ArtSource(Source):
    def resolve(self, track_id=None):
        return replace(super().resolve(track_id), artwork_url="https://i1.sndcdn.com/cover.jpg")


def test_artwork_api_and_no_signed_audio_url_in_metadata():
    store = ArtworkStore(fetcher=lambda _: image_bytes())
    with TestClient(create_app(Settings(), source=ArtSource(Settings()), artwork_store=store)) as client:
        image = client.get("/v1/artwork?track_id=42")
        assert image.status_code == 200 and len(image.content) == PACKET_SIZE
        assert image.headers["content-length"] == str(PACKET_SIZE)
        data = client.get("/v1/track?track_id=42").json()
        assert data["has_artwork"]
        assert "input_url" not in data and "artwork_url" not in data


def test_missing_or_broken_artwork_does_not_break_player():
    with TestClient(create_app(Settings())) as client:
        assert client.get("/v1/artwork").status_code == 204
    store = ArtworkStore(fetcher=lambda _: b"broken")
    with TestClient(create_app(Settings(), source=ArtSource(Settings()), artwork_store=store)) as client:
        assert client.get("/v1/artwork").status_code == 404
        assert not client.get("/healthz").json()["busy"]


def test_artwork_processing_does_not_hold_audio_slot():
    async def check():
        playing, finish = asyncio.Event(), asyncio.Event()

        class Job:
            calls = 0

            async def read(self):
                self.calls += 1
                if self.calls == 1:
                    return b"\x01\x00" * 64
                playing.set()
                await finish.wait()
                return b""

            async def close(self):
                pass

        async def factory(*_):
            return Job()

        app = create_app(Settings(), source=ArtSource(Settings()), job_factory=factory,
                         artwork_store=ArtworkStore(fetcher=lambda _: image_bytes()))
        async with app.router.lifespan_context(app):
            async with httpx.AsyncClient(transport=httpx.ASGITransport(app=app), base_url="http://bridge") as client:
                audio = asyncio.create_task(client.get("/v1/audio?track_id=42"))
                await asyncio.wait_for(playing.wait(), timeout=2)
                cover = await client.get("/v1/artwork?track_id=42")
                assert cover.status_code == 200 and not audio.done()
                finish.set()
                assert (await audio).status_code == 200

    asyncio.run(check())
