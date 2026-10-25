"""Environment-only server configuration; no user credentials are needed."""
from dataclasses import dataclass
import os
from urllib.parse import urlparse

RATE = 22050
BYTES_PER_SECOND = RATE * 2


@dataclass(frozen=True)
class Settings:
    source: str = "test"
    track_url: str = ""
    test_seconds: int = 600
    ffmpeg: str = "ffmpeg"

    @classmethod
    def from_env(cls):
        result = cls(
            source=os.getenv("SC_SOURCE", "test"),
            track_url=os.getenv("SC_TRACK_URL", ""),
            test_seconds=int(os.getenv("SC_TEST_SECONDS", "600")),
            ffmpeg=os.getenv("FFMPEG_BIN", "ffmpeg"),
        )
        if result.source not in {"test", "soundcloud"}:
            raise ValueError("SC_SOURCE must be test or soundcloud")
        if not 1 <= result.test_seconds <= 3600:
            raise ValueError("SC_TEST_SECONDS must be 1..3600")
        if result.source == "soundcloud":
            url = urlparse(result.track_url)
            if url.scheme != "https" or url.hostname not in {"soundcloud.com", "www.soundcloud.com"}:
                raise ValueError("SC_TRACK_URL must be a canonical https://soundcloud.com/artist/track URL")
        return result
