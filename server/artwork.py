"""Bounded artwork fetch/cache and a tiny DS-native paletted image format."""
from collections import OrderedDict
import colorsys
from io import BytesIO
import struct
import time
from urllib.parse import urljoin, urlparse

from PIL import Image, ImageOps, UnidentifiedImageError

SIZE = 64
PACKET_SIZE = 16 + 512 + SIZE * SIZE
BACKGROUND = (15, 18, 26)


class ArtworkError(Exception):
    pass


def rgb555(color):
    r, g, b = color
    return 0x8000 | (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)


def unpack_rgb555(value):
    return tuple(round(((value >> shift) & 31) * 255 / 31) for shift in (0, 5, 10))


def luminance(color):
    channels = [v / 255 for v in color]
    linear = [v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4 for v in channels]
    return .2126 * linear[0] + .7152 * linear[1] + .0722 * linear[2]


def contrast(a, b):
    first, second = sorted((luminance(a), luminance(b)))
    return (second + .05) / (first + .05)


def accent_colors(image):
    reduced = image.quantize(colors=16, dither=Image.Dither.NONE)
    palette = reduced.getpalette()
    candidates = []
    for count, index in reduced.getcolors():
        color = tuple(palette[index * 3:index * 3 + 3])
        _, saturation, value = colorsys.rgb_to_hsv(*(v / 255 for v in color))
        if count >= 40 and saturation >= .15 and value >= .18:
            candidates.append((count * (.35 + saturation), color))
    if candidates:
        color = max(candidates, key=lambda item: item[0])[1]
    else:
        color = image.resize((1, 1), Image.Resampling.BOX).getpixel((0, 0))
    # Re-evaluate after RGB555 quantization: the console displays these colors.
    color = tuple(color)
    for _ in range(20):
        if contrast(unpack_rgb555(rgb555(color)), BACKGROUND) >= 3.0:
            break
        color = tuple(min(255, round(channel * .85 + 255 * .15)) for channel in color)
    packed = rgb555(color)
    shown = unpack_rgb555(packed)
    foreground = (0, 0, 0) if contrast(shown, (0, 0, 0)) >= contrast(shown, (255, 255, 255)) else (255, 255, 255)
    return packed, rgb555(foreground)


def encode_artwork(data):
    try:
        with Image.open(BytesIO(data), formats=("JPEG", "PNG", "WEBP")) as original:
            if original.width * original.height > 4_000_000:
                raise ArtworkError("Artwork image is too large")
            image = ImageOps.exif_transpose(original).convert("RGBA")
            background = Image.new("RGBA", image.size, BACKGROUND + (255,))
            image = Image.alpha_composite(background, image).convert("RGB")
            image = ImageOps.fit(image, (SIZE, SIZE), Image.Resampling.LANCZOS)
    except (UnidentifiedImageError, OSError, ValueError, Image.DecompressionBombError) as exc:
        raise ArtworkError("Artwork could not be decoded") from exc
    accent, foreground = accent_colors(image)
    indexed = image.quantize(colors=256, method=Image.Quantize.MEDIANCUT,
                             dither=Image.Dither.FLOYDSTEINBERG)
    colors = indexed.getpalette()
    colors += [0] * (768 - len(colors))
    palette = [rgb555(tuple(colors[i:i + 3])) for i in range(0, 768, 3)]
    # magic, version, width, height, accent, on-accent, palette count
    header = struct.pack("<4s6H", b"SCAR", 1, SIZE, SIZE, accent, foreground, 256)
    return header + struct.pack("<256H", *palette) + indexed.tobytes()


def allowed_url(url):
    try:
        parsed = urlparse(url)
        return (parsed.scheme == "https" and parsed.hostname is not None
                and parsed.hostname.endswith(".sndcdn.com")
                and parsed.port in (None, 443) and parsed.username is None)
    except (ValueError, TypeError):
        return False


def fetch_artwork(url):
    from curl_cffi import requests
    deadline = time.monotonic() + 8
    try:
        with requests.Session(impersonate="chrome") as session:
            for _ in range(3):
                if not allowed_url(url):
                    raise ArtworkError("Unsupported artwork host")
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise ArtworkError("Artwork timed out")
                response = session.get(url, stream=True, allow_redirects=False, timeout=min(6, remaining))
                try:
                    if response.status_code in (301, 302, 303, 307, 308):
                        url = urljoin(url, response.headers.get("Location", ""))
                        continue
                    response.raise_for_status()
                    if int(response.headers.get("Content-Length", "0")) > 1_048_576:
                        raise ArtworkError("Artwork download is too large")
                    data = bytearray()
                    for chunk in response.iter_content(chunk_size=16384):
                        data.extend(chunk)
                        if len(data) > 1_048_576 or time.monotonic() > deadline:
                            raise ArtworkError("Artwork download exceeded its limit")
                    return bytes(data)
                finally:
                    response.close()
    except ArtworkError:
        raise
    except Exception as exc:
        raise ArtworkError("Artwork download failed") from exc
    raise ArtworkError("Too many artwork redirects")


class ArtworkStore:
    # Called behind the API's dedicated artwork gate, independently of audio.
    def __init__(self, fetcher=fetch_artwork, max_entries=128):
        self.fetcher = fetcher
        self.max_entries = max_entries
        self.cache = OrderedDict()

    def get(self, url):
        if not allowed_url(url):
            raise ArtworkError("Unsupported artwork host")
        cached = self.cache.get(url)
        if cached and time.monotonic() - cached[0] < 3600:
            self.cache.move_to_end(url)
            return cached[1]
        data = encode_artwork(self.fetcher(url))
        self.cache[url] = (time.monotonic(), data)
        self.cache.move_to_end(url)
        while len(self.cache) > self.max_entries:
            self.cache.popitem(last=False)
        return data
