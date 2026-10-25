import pytest
from fastapi.testclient import TestClient

from server.app import create_app
from server.config import Settings
from server.source import Source, SearchTrack, SourceError
from tests.test_server import FakeJob


class SearchSource(Source):
    def __init__(self):
        super().__init__(Settings())
        self.calls = []

    def search(self, q, offset, limit):
        self.calls.append((q, offset, limit))
        return {"tracks": [SearchTrack("42", "Музыка 🎵", "Artist", 9000)],
                "offset": offset, "next_offset": offset + limit, "has_more": True}

    def resolve(self, track_id=None):
        self.calls.append(track_id)
        return super().resolve()


def test_search_and_play_selected_track():
    source = SearchSource()

    async def factory(*_):
        return FakeJob()

    with TestClient(create_app(Settings(), source=source, job_factory=factory)) as client:
        response = client.get("/v1/search", params={"q": "  музыка  ", "offset": 4})
        assert response.status_code == 200
        result = response.json()
        assert result["tracks"][0] == {"id": "42", "title": "Музыка 🎵", "artist": "Artist", "duration_ms": 9000}
        assert result["has_more"] and result["next_offset"] == 8
        assert source.calls == [("музыка", 4, 4)]
        assert client.get("/v1/audio?track_id=42").status_code == 200
        assert source.calls[-1] == "42"
        assert client.get("/v1/track?track_id=42").status_code == 200


@pytest.mark.parametrize("path", [
    "/v1/search?q=", "/v1/search?q=%20%20", "/v1/search?q=x&limit=5",
    "/v1/search?q=x&offset=-1", "/v1/search?q=x&offset=5001",
    "/v1/audio?track_id=https://localhost", "/v1/audio?track_id=-1",
    "/v1/audio?track_id=123456789012345678901", "/v1/track?track_id=abc",
])
def test_invalid_search_or_track_id_never_reaches_source(path):
    source = SearchSource()
    with TestClient(create_app(Settings(), source=source)) as client:
        assert client.get(path).status_code == 422
        assert not source.calls
        assert not client.get("/healthz").json()["busy"]


def test_search_source_error_is_recoverable():
    class BrokenSource(SearchSource):
        def search(self, *_):
            raise SourceError("Search failed")

    with TestClient(create_app(Settings(), source=BrokenSource())) as client:
        assert client.get("/v1/search?q=x").status_code == 502
        assert not client.get("/healthz").json()["busy"]
