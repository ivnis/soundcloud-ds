"""One FFmpeg process per active HTTP stream, with bounded reads and cleanup."""
import asyncio
from contextlib import suppress
from urllib.parse import urlparse

from .config import RATE, Settings
from .source import Track


class AudioError(Exception):
    pass


class AudioJob:
    def __init__(self, process, fallback_args=None):
        self.process = process
        self._closed = False
        self._stderr_task = asyncio.create_task(self._drain_stderr())
        self._fallback_args = fallback_args
        self._produced = False
        self.seek_fallback_used = False

    async def _drain_stderr(self):
        # Drain continuously to prevent a full stderr pipe stalling the audio.
        while await self.process.stderr.read(4096):
            pass

    @staticmethod
    def _command(settings, track, start_ms, output_seek=False):
        args = [settings.ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin"]
        if track.test:
            args += ["-f", "lavfi", "-i", f"sine=frequency=440:sample_rate={RATE}"]
            if start_ms:
                args += ["-ss", str(start_ms / 1000)]
        else:
            if start_ms and not output_seek:
                args += ["-ss", str(start_ms / 1000)]
            if urlparse(track.input_url).path.endswith(".m3u8"):
                args += ["-seg_max_retry", "3"]
            args += ["-rw_timeout", "15000000", "-i", track.input_url]
            if start_ms and output_seek:
                args += ["-ss", str(start_ms / 1000)]
        args += ["-t", str((track.duration_ms - start_ms) / 1000), "-vn", "-sn", "-dn",
                 "-ac", "1", "-ar", str(RATE), "-c:a", "pcm_s16le", "-f", "s16le", "pipe:1"]
        return args

    @staticmethod
    async def _launch(args):
        try:
            return await asyncio.create_subprocess_exec(
                *args, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE,
                limit=32768,
            )
        except OSError as exc:
            raise AudioError("FFmpeg could not be started") from exc

    @classmethod
    async def open(cls, settings: Settings, track: Track, start_ms: int = 0):
        if not 0 <= start_ms < track.duration_ms:
            raise AudioError("Seek position is outside the track")
        args = cls._command(settings, track, start_ms)
        fallback = cls._command(settings, track, start_ms, output_seek=True) if start_ms and not track.test else None
        return cls(await cls._launch(args), fallback)

    async def _fallback(self):
        self.seek_fallback_used = True
        args, self._fallback_args = self._fallback_args, None
        await self._finish_process()
        self.process = await self._launch(args)
        self._stderr_task = asyncio.create_task(self._drain_stderr())

    async def read(self):
        try:
            block = await asyncio.wait_for(self.process.stdout.read(8192),
                                           timeout=8 if self._fallback_args and not self._produced else 30)
        except TimeoutError as exc:
            if self._fallback_args and not self._produced:
                await self._fallback()
                return await self.read()
            raise AudioError("Audio source timed out") from exc
        if not block:
            code = await self.process.wait()
            if self._fallback_args and not self._produced:
                # Some HLS/fMP4 sources accept input seek but emit no samples.
                # Output seek decodes/discards the prefix and is more compatible.
                await self._fallback()
                return await self.read()
            if code:
                raise AudioError("Audio conversion failed")
        else:
            self._produced = True
            self._fallback_args = None
        return block

    async def _finish_process(self):
        if self.process.returncode is None:
            with suppress(ProcessLookupError):
                self.process.terminate()
            try:
                await asyncio.wait_for(self.process.wait(), timeout=3)
            except TimeoutError:
                with suppress(ProcessLookupError):
                    self.process.kill()
                await self.process.wait()
        await self._stderr_task

    async def close(self):
        if self._closed:
            return
        self._closed = True
        await self._finish_process()
