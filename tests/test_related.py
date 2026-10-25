from types import SimpleNamespace as NS
import asyncio

import httpx
import pytest
from fastapi.testclient import TestClient

from server.app import create_app
from server.config import Settings
from server.source import Source, SourceError, SearchTrack
from tests.test_source import make_source


def test_related_is_bounded_filters_seed_duplicates_and_previews():
    source = make_source()
    template = source._client.resolve("")
    consumed = []

    def related(track_id, **kwargs):
        assert track_id == 42 and kwargs == {"limit": 6}
        for key, policy in [(42, "ALLOW"), (7, "ALLOW"), (7, "ALLOW"),
                            (8, "SNIP"), (9, "ALLOW"), (10, "ALLOW"), (11, "ALLOW")]:
            consumed.append(key)
            yield NS(**{**vars(template), "id": key, "policy": policy})

    source._client.get_track_related = related
    result = source.related("42", limit=5)
    assert len(consumed) == 6
    assert [track.id for track in result["tracks"]] == ["7", "9", "10"]
    assert not result["has_more"]


def test_default_track_recommendations_resolve_the_seed():
    source = make_source()
    source._client.resolve("").id = 42
    calls = []
    source._client.get_track_related = lambda key, **kw: calls.append(key) or iter(())
    assert source.related(None)["tracks"] == []
    assert calls == [42]


class RelatedSource(Source):
    def __init__(self):
        super().__init__(Settings())
        self.calls = []

    def related(self, key, limit):
        self.calls.append((key, limit))
        return {"tracks": [SearchTrack("7", "Next track", "Artist", 3000)],
                "offset": 0, "next_offset": 0, "has_more": False}


def test_related_api_uses_same_bounded_client_packet():
    source = RelatedSource()
    with TestClient(create_app(Settings(), source=source)) as client:
        result = client.get("/v1/related?track_id=42&limit=3")
        assert result.status_code == 200
        assert result.json()["tracks"][0]["id"] == "7"
        assert source.calls == [("42", 3)]
        assert client.get("/v1/related").status_code == 200
        assert source.calls[-1] == (None, 12)


@pytest.mark.parametrize("query", ["track_id=abc", "track_id=-1", "track_id=42&limit=13", "limit=0"])
def test_related_rejects_invalid_queries(query):
    source = RelatedSource()
    with TestClient(create_app(Settings(), source=source)) as client:
        assert client.get("/v1/related?" + query).status_code == 422
        assert source.calls == []


def test_related_failure_does_not_take_audio_slot():
    class Broken(RelatedSource):
        def related(self, *_):
            raise SourceError("Recommendations unavailable")

    with TestClient(create_app(Settings(), source=Broken())) as client:
        assert client.get("/v1/related?track_id=42").status_code == 502
        assert not client.get("/healthz").json()["busy"]


def test_recommendations_do_not_interrupt_an_active_audio_stream():
    async def run():
        started, release = asyncio.Event(), asyncio.Event()

        class Job:
            reads = 0

            async def read(self):
                self.reads += 1
                if self.reads == 1:
                    return b"\x00\x00" * 32
                started.set()
                await release.wait()
                return b""

            async def close(self):
                pass

        async def factory(*_):
            return Job()

        app = create_app(Settings(), source=RelatedSource(), job_factory=factory)
        async with httpx.AsyncClient(transport=httpx.ASGITransport(app=app), base_url="http://test") as client:
            task = asyncio.create_task(client.get("/v1/audio?track_id=42"))
            try:
                await asyncio.wait_for(started.wait(), 3)
                assert (await client.get("/v1/related?track_id=42")).status_code == 200
                assert (await client.get("/healthz")).json()["busy"]
            finally:
                release.set()
                assert (await task).status_code == 200

    asyncio.run(run())
