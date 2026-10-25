# SoundCloud icon

Source: https://github.com/simple-icons/simple-icons/blob/develop/icons/soundcloud.svg

Bundled SVG from Simple Icons, distributed under CC0 1.0 Universal; see
[SOUNDCLOUD-ICON-LICENSE.md](SOUNDCLOUD-ICON-LICENSE.md).

`tools/generate_soundcloud_icon.py` renders the bundled SVG at 8x resolution,
then generates a 36x18 coverage mask with Pillow and resvg-py 0.5.0.
The checked-in `client/include/soundcloud_icon.h` is used directly by the DS
build. No SVG renderer is required on the console or for a regular build.
Coverage is composited using the active theme accent, preserving the shape
without a hardcoded background color.
