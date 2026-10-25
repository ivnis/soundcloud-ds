from types import SimpleNamespace as NS

import pytest
from curl_cffi import requests

from server.config import Settings
from server.source import Source, SourceError


def make_source(policy="MONETIZE", snipped=False):
    track = NS(title="Example", user=NS(username="Artist"), duration=3000,
               permalink_url="https://soundcloud.com/artist/track", streamable=True,
               policy=policy, track_authorization="test",
               media=NS(transcodings=[NS(snipped=snipped,
                        format=NS(protocol="hls"),
                        url="https://api-v2.soundcloud.com/media/example")]))
    source = Source(Settings(source="soundcloud", track_url=track.permalink_url))
    source._client = NS(resolve=lambda _: track, client_id="example", _session=NS(timeout=0))
    return source


@pytest.mark.parametrize("host", ["cf-media.sndcdn.com", "playback.media-streaming.soundcloud.cloud"])
def test_monetized_full_stream_is_playable(monkeypatch, host):
    monkeypatch.setattr(requests, "get", lambda *a, **kw: NS(
        raise_for_status=lambda: None,
        json=lambda: {"url": f"https://{host}/test/playlist.m3u8"}))
    track = make_source().resolve()
    assert track.title == "Example"
    assert track.input_url.endswith(".m3u8")


@pytest.mark.parametrize("policy,snipped", [("BLOCK", False), ("SNIP", True), ("ALLOW", True)])
def test_restricted_and_preview_tracks_are_rejected(policy, snipped):
    with pytest.raises(SourceError):
        make_source(policy, snipped).resolve()


def test_unexpected_upstream_host_is_rejected(monkeypatch):
    monkeypatch.setattr(requests, "get", lambda *a, **kw: NS(
        raise_for_status=lambda: None, json=lambda: {"url": "https://localhost/private"}))
    with pytest.raises(SourceError):
        make_source().resolve()


def test_stale_transcoding_falls_back_to_next(monkeypatch):
    source = make_source()
    track = source._client.resolve("")
    track.media.transcodings *= 2
    calls = []

    def get(*args, **kwargs):
        calls.append(1)
        if len(calls) == 1:
            raise requests.RequestsError("Stale transcoding")
        return NS(raise_for_status=lambda: None,
                  json=lambda: {"url": "https://cf-media.sndcdn.com/test.mp3"})

    monkeypatch.setattr(requests, "get", get)
    assert source.resolve().input_url.endswith(".mp3")
    assert len(calls) == 2


def test_search_is_bounded_and_filters_previews():
    source = make_source()
    template = source._client.resolve("")
    calls, yielded = [], []

    def tracks(q, **kwargs):
        calls.append((q, kwargs))
        for i in range(10000):
            yielded.append(i)
            track = NS(**vars(template))
            track.id = i + 1
            if i == 1:
                track.policy = "SNIP"
            yield track

    source._client.search_tracks = tracks
    result = source.search("artist", offset=12, limit=3)
    assert len(yielded) == 4
    assert [t.id for t in result["tracks"]] == ["1", "3"]
    assert result["has_more"] is True and result["next_offset"] == 15
    assert calls == [("artist", {"limit": 4, "offset": "12"})]


def test_resolve_selected_id_refreshes_source(monkeypatch):
    source = make_source()
    selected = []
    template = source._client.resolve("")

    def get_track(track_id):
        selected.append(track_id)
        return template

    source._client.get_track = get_track
    monkeypatch.setattr(requests, "get", lambda *args, **kwargs: NS(
        raise_for_status=lambda: None, json=lambda: {"url": "https://cf-media.sndcdn.com/live.mp3"}))
    assert source.resolve("12345").title == "Example"
    assert selected == [12345]
