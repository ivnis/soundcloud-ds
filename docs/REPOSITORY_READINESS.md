# Repository readiness

[English README](../README.md) | [Русский README](../README.ru.md)

Prepared for `ivnis/soundcloud-ds` on 2026-10-25.

This is a historical preparation record from before the first push. The project
has since been published; current downloads and setup are linked in README.md.

- Primary README is English, with reciprocal links to the Russian version.
- Original code is GPL-3.0-only; third-party asset licenses are bundled.
- Git branch is `main`, with origin `https://github.com/ivnis/soundcloud-ds.git`.
- Source/config templates are included. Virtual environments, local `.env`
  files, local endpoint overrides, SDK/emulator directories, builds, logs,
  firmware, and saves are ignored.
- A scan of staged sources found no matching private-key, GitHub-token, or
  AWS-access-key patterns. This is a bounded check, not a guarantee about
  arbitrary secrets.
- Relative Markdown links and staged whitespace checks passed.

## Verification

The staged source was exported into a separate directory using
`git checkout-index`, without `config.local.h`, SDK caches, or emulator files.
Using the installed external compiler/SDK and Python environment:

- All 65 server tests passed, with the Windows imageio-ffmpeg binary configured.
- Native C++ tests compiled and passed.
- A complete ARM build produced `client/soundcloud_ds.nds` using public defaults.
- The normal working copy also passed the checks and ARM build.

An initial concurrent clean-source check hit Windows virtual-memory limits;
all three clean-source checks passed when run sequentially. Existing warnings
remain: Starlette TestClient/httpx deprecation and an SDK GNU-stack linker note.

GitHub-hosted CI has not run yet. Physical DS Fat and long continuous autoplay
validation are pending. Preparation does not create or publish a GitHub
repository; see [publishing steps](PUBLISHING.md).
