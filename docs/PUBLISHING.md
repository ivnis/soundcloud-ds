# Publishing to GitHub

[English README](../README.md) | [Русский README](../README.ru.md)

Target repository: `ivnis/soundcloud-ds`. The local default branch is `main`.
Project code is GPL-3.0-only; third-party notices must remain in the repository.

## Before the first push

- Run the tests and build described in README.md.
- Check `git status --short` and `git diff --cached --stat`.
- Confirm that `.env`, `config.local.h`, SDK caches, emulator binaries/configs,
  BIOS/firmware, ROMs, logs, recordings, and save files are not staged.
- Include both README languages, LICENSE, and THIRD_PARTY_NOTICES.md.

The initial source commit intentionally excludes the SDK and local emulator
setup. A fresh clone must follow the documented setup steps.

## Create the remote

While signed in as `ivnis`, create a repository named `soundcloud-ds` at
https://github.com/new. Choose its visibility and leave GitHub's generated
README, license, and .gitignore options unchecked: those files already exist
in the source commit.

Alternatively, with an authenticated GitHub CLI:

```sh
gh repo create ivnis/soundcloud-ds --public \
  --description "SoundCloud homebrew player for Nintendo DS with a Python streaming bridge"
```

Use `--private` instead if that is the intended visibility. Creating the
repository and pushing are separate from local preparation.

## Push

The prepared origin is `https://github.com/ivnis/soundcloud-ds.git`.
Git Credential Manager or GitHub CLI must have access to the account.

```sh
git remote -v
git push -u origin main
```

Do not force-push if the remote already has history. Fetch and review it first.
After the push, check the **Checks** Actions workflow: it runs server/native
tests and builds an NDS artifact. A successful local build does not establish
that the hosted workflow has run successfully.

For a future release, publish the `.nds` alongside a source tag and describe
which emulator/hardware checks were actually completed. Real DS Fat validation
and a long continuous autoplay session are still pending.
