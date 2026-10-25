"""Keep the unofficial SoundCloud integration outside the wire protocol."""
from dataclasses import dataclass
from urllib.parse import urlparse
import threading
import time
from itertools import islice

from .config import Settings


class SourceError(Exception):
    pass


@dataclass(frozen=True)
class Track:
    title: str
    artist: str
    duration_ms: int
    permalink: str
    input_url: str
    test: bool = False
    artwork_url: str = ""


@dataclass(frozen=True)
class SearchTrack:
    id: str
    title: str
    artist: str
    duration_ms: int


class Source:
    def __init__(self, settings: Settings):
        self.settings = settings
        self._client = None
        self._lock = threading.Lock()

    @staticmethod
    def _remaining(deadline):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise SourceError("SoundCloud lookup timed out; retry later")
        return remaining

    def _discover_client_id(self, wrapper, requests, deadline):
        # The wrapper's public generate_client_id() has no timeout parameter.
        # Reuse its pinned-version patterns with a total discovery deadline.
        with requests.Session(impersonate="chrome") as session:
            response = session.get("https://soundcloud.com", timeout=min(6, self._remaining(deadline)))
            response.raise_for_status()
            scripts = wrapper._ASSETS_SCRIPTS_REGEX.findall(response.text)
            for script in scripts:
                try:
                    response = session.get(script, timeout=min(4, self._remaining(deadline)))
                    response.raise_for_status()
                except requests.RequestsError:
                    continue
                match = wrapper._CLIENT_ID_REGEX.search(response.text)
                if match:
                    return match.group(1)
        raise SourceError("Could not discover SoundCloud client ID; retry later")

    def _get_client(self, deadline):
        from soundcloud import SoundCloud
        from curl_cffi import requests
        if self._client is None:
            client_id = self._discover_client_id(SoundCloud, requests, min(deadline, time.monotonic() + 15))
            self._client = SoundCloud(client_id=client_id)
        self._client._session.timeout = min(8, self._remaining(deadline))
        return self._client

    @staticmethod
    def _playable(track):
        return (track is not None and getattr(track, "streamable", False)
                and getattr(track, "policy", "ALLOW") in {"ALLOW", "MONETIZE"}
                and any(not t.snipped for t in track.media.transcodings))

    def search(self, query: str, offset=0, limit=4):
        if self.settings.source == "test":
            items = [SearchTrack("0", "Network test tone (440 Hz)", "SoundCloud DS",
                                 self.settings.test_seconds * 1000)] if offset == 0 else []
            return {"tracks": items, "has_more": False, "offset": offset, "next_offset": offset}
        with self._lock:
            deadline = time.monotonic() + 25
            try:
                client = self._get_client(deadline)
                # Limit iteration explicitly: the library otherwise fetches all pages.
                raw = list(islice(client.search_tracks(query, limit=limit + 1,
                                                       offset=str(offset)), limit + 1))
                self._remaining(deadline)
                tracks = [SearchTrack(str(t.id), t.title[:120], t.user.username[:80], t.duration)
                          for t in raw[:limit] if self._playable(t)]
                return {"tracks": tracks, "has_more": len(raw) > limit,
                        "offset": offset, "next_offset": offset + limit}
            except SourceError:
                raise
            except Exception as exc:
                self._client = None
                raise SourceError("SoundCloud search failed; retry later") from exc

    def related(self, track_id: str | None, limit=12):
        if self.settings.source == "test":
            return {"tracks": [], "has_more": False, "offset": 0, "next_offset": 0}
        with self._lock:
            deadline = time.monotonic() + 25
            try:
                client = self._get_client(deadline)
                if track_id is None:
                    track_id = str(client.resolve(self.settings.track_url).id)
                raw = list(islice(client.get_track_related(int(track_id), limit=limit + 1), limit + 1))
                self._remaining(deadline)
                seen = {str(track_id)}
                tracks = []
                for item in raw:
                    key = str(item.id)
                    if key in seen or not self._playable(item):
                        continue
                    seen.add(key)
                    tracks.append(SearchTrack(key, item.title[:120], item.user.username[:80], item.duration))
                    if len(tracks) == limit:
                        break
                return {"tracks": tracks, "has_more": False, "offset": 0, "next_offset": 0}
            except SourceError:
                raise
            except Exception as exc:
                self._client = None
                raise SourceError("SoundCloud recommendations failed; retry later") from exc

    def resolve(self, track_id=None) -> Track:
        if self.settings.source == "test":
            return Track("Network test tone (440 Hz)", "SoundCloud DS",
                         self.settings.test_seconds * 1000, "", "", True)
        # The wrapper is synchronous. Called via asyncio.to_thread by the API.
        with self._lock:
            deadline = time.monotonic() + 25
            try:
                from curl_cffi import requests
                client = self._get_client(deadline)
                track = client.get_track(int(track_id)) if track_id is not None else client.resolve(self.settings.track_url)
                if track is None or not getattr(track, "streamable", False):
                    raise SourceError("Track is unavailable for streaming")
                if getattr(track, "policy", "ALLOW") not in {"ALLOW", "MONETIZE"}:
                    raise SourceError("Track is restricted or preview-only")
                candidates = [t for t in track.media.transcodings if not t.snipped]
                candidates.sort(key=lambda t: t.format.protocol != "progressive")
                if not candidates:
                    raise SourceError("No full-length audio source is available")
                for transcoding in candidates:
                    host = urlparse(transcoding.url)
                    if host.scheme != "https" or host.hostname != "api-v2.soundcloud.com":
                        continue
                    params = {"client_id": self._client.client_id,
                              "track_authorization": track.track_authorization}
                    try:
                        response = requests.get(transcoding.url, params=params,
                                                impersonate="chrome", timeout=min(8, self._remaining(deadline)))
                        response.raise_for_status()
                        audio = response.json().get("url", "")
                    except (requests.RequestsError, ValueError):
                        # A listed transcoding may be stale while another is live.
                        continue
                    parsed = urlparse(audio)
                    if parsed.scheme != "https" or not parsed.hostname or not (
                        parsed.hostname.endswith(".sndcdn.com") or
                        parsed.hostname.endswith(".soundcloud.com") or
                        parsed.hostname.endswith(".soundcloud.cloud")
                    ):
                        continue
                    return Track(track.title, track.user.username, track.duration,
                                 track.permalink_url, audio,
                                 artwork_url=getattr(track, "artwork_url", None) or "")
                raise SourceError("No supported audio source is available")
            except SourceError:
                raise
            except Exception as exc:
                self._client = None
                # Do not expose signed URLs or upstream cookies in API errors.
                raise SourceError("SoundCloud lookup failed; retry later") from exc
