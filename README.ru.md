# SoundCloud DS

[English](README.md) | **Русский**

Homebrew-плеер для **Nintendo DS Fat**: C++/BlocksDS на консоли и Python/FastAPI + FFmpeg на VPS. Первичная отладка — в melonDS, окончательная проверка — на железе.

```text
SoundCloud → soundcloud-v2 → FFmpeg на VPS → TCP/HTTP → буфер DS → Maxmod
```

Плеер ищет публичные треки SoundCloud прямо на DS, показывает результаты страницами и воспроизводит выбранный трек. Верхний экран — обложка, сведения о треке и визуализация; нижний — поиск, клавиатура EN/RU, прогресс и сенсорный плеер. Основной язык меню — английский. Поиск работает во время воспроизведения; названия поддерживают латиницу и кириллицу.

Поддерживаются шаги ±5 секунд, сенсорная перемотка, обложки 64×64 и акцентные цвета из картинки, фоновая аудиовизуализация и автопереход по рекомендациям текущего трека. Шестерёнка открывает выбор сервера и Wi-Fi; громкость меняется плечевыми L/R. [Настройки и обложки](docs/UI_ARTWORK_SETTINGS.md), [поиск и плеер](docs/SEARCH_UI.md), [перемотка](docs/SEEK_AND_ARTWORK.md). Аккаунт, плейлисты и стерео остаются следующими этапами.

Для нового клона сначала установить зависимости, собрать ROM и добавить melonDS в `tools/melonds`. Затем можно использовать `Start SoundCloud.cmd` или `Start Test Tone.cmd`. Подробная подготовка описана в [основной инструкции](README.md#quick-start).

## Быстрый запуск сервера в Windows

Готовые файлы [релиза v0.1.0](https://github.com/ivnis/soundcloud-ds/releases/tag/v0.1.0)
скачиваются отдельно: **soundcloud-ds-v0.1.0.nds** — клиент и
**soundcloud-ds-server-v0.1.0.zip** — сервер. В серверном архиве есть собственные
инструкции на русском и английском, запуск для Windows и Docker Compose.
Нужны оба файла и работающий bridge. Контрольные суммы — в SHA256SUMS.txt релиза.

Из корня проекта, Python 3.13:

```powershell
python -m venv .venv
.\.venv\Scripts\python -m pip install -r server/requirements-dev.txt
$env:FFMPEG_BIN = & .\.venv\Scripts\python -c "import imageio_ffmpeg; print(imageio_ffmpeg.get_ffmpeg_exe())"
.\.venv\Scripts\python -m uvicorn server.app:app --host 0.0.0.0 --port 8000 --workers 1
```

По умолчанию сервер выдаёт тестовый тон 440 Гц длительностью десять минут. Метаданные: `http://127.0.0.1:8000/v1/track`, состояние: `/healthz`, PCM: `/v1/audio`. `imageio-ffmpeg` нужен только для удобной локальной проверки в Windows; на VPS используется системный FFmpeg.

Чтобы включить SoundCloud, остановить сервер, задать переменные и запустить его снова:

```powershell
$env:SC_SOURCE = "soundcloud"
$env:SC_TRACK_URL = "https://soundcloud.com/artist/track"
.\.venv\Scripts\python -m uvicorn server.app:app --host 0.0.0.0 --port 8000 --workers 1
```

Сначала проверить выбранный трек отдельно:

```powershell
.\.venv\Scripts\python -m tools.probe_soundcloud https://soundcloud.com/artist/track
```

Проба разрешает ссылку и декодирует две секунды. Проверка полного трека выполняется отдельно. Библиотека [soundcloud-v2](https://7x11x13.xyz/soundcloud.py/soundcloud.html) использует неофициальный API; доступность зависит от изменений SoundCloud и региона VPS. Личный токен не требуется. Preview и заблокированные треки отклоняются.

Проверить весь поток работающего сервера:

```powershell
.\.venv\Scripts\python -m tools.check_stream http://127.0.0.1:8000/v1/audio
```

Эта проверка читает аудио до конца с максимальной скоростью и сверяет длину; она не заменяет десятиминутный тест воспроизведения на консоли.

## VPS

На Linux с Docker Compose, из корня проекта:

```sh
docker compose up -d --build
curl http://127.0.0.1:8000/healthz
docker compose logs -f bridge
```

Для SoundCloud создать корневой `.env`:

```dotenv
SC_SOURCE=soundcloud
SC_TRACK_URL=https://soundcloud.com/artist/track
```

После изменения конфигурации — `docker compose up -d --force-recreate`. Шаблон настроек находится в `server/.env.example`; `FFMPEG_BIN` в контейнере уже настроен через системный PATH. Одновременно разрешён один поток, дополнительные подключения получают HTTP 409. Нужен ровно один worker без `--reload`.

DS подключается к порту 8000 по HTTP; настроить firewall VPS на доступ с используемой сети. Это персональный прототип без аутентификации и шифрования участка DS ↔ VPS. Сервер не принимает произвольные URL от клиента и не передаёт ему подписанные ссылки SoundCloud. FFmpeg завершается после stop, отключения клиента или остановки сервиса.

## Сборка `.nds`

Установить [BlocksDS и Wonderful Toolchain](https://blocksds.skylyrac.net/docs/setup/windows/) либо использовать официальный контейнер. Начальный адрес можно задать в `client/include/config.local.h` по примеру `config.local.example.h`. После запуска он редактируется в Settings без пересборки. Для настоящей DS нужен числовой IPv4 VPS или LAN-адрес ПК, а не `127.0.0.1`.

Из Wonderful Toolchain Shell:

```sh
cd soundcloud-ds/client
make
```

Результат: `client/soundcloud_ds.nds`. Выбран ARM7 core `arm7_dswifi_maxmod.elf`, включающий и сеть, и звук.

Из PowerShell с установленными SDK, make и sh в PATH: `./tools/build.ps1`. Пути к make/sh и SDK также можно передать параметрами скрипта. SDK, эмулятор и локальные настройки не входят в репозиторий.

Альтернатива, PowerShell с Docker из корня проекта:

```powershell
docker run --rm --mount "type=bind,source=$($PWD.Path),target=/work" --workdir /work/client --entrypoint make skylyrac/blocksds:slim-latest
```

## melonDS и DS Fat

1. В melonDS включить DS mode и online networking. Начать с indirect mode; используется эмулируемая точка доступа `melonAP`. Нужна актуальная сборка с исправлением отключения WiFi примерно через 67 секунд при выключенном power saving.
2. Запустить `.nds`, выбрать `melonAP` кнопками вверх/вниз и A. X подключает сохранённые WFC-настройки. Если точка не появилась, проверить настройки WiFi эмулятора.
3. После подключения нажать строку поиска или DS Y, ввести запрос и нажать Search. Нажатие по результату запускает трек. DS X переключает Search/Player, DS B останавливает. Плечевые L/R меняют громкость; крестовина влево/вправо листает страницы Search или перематывает Player, вверх/вниз на Player переключает треки. Шестерёнка открывает сервер/Wi-Fi, START выходит в загрузчик.
4. На настоящей DS Fat запускать через flashcart. Нужна открытая либо WEP-точка 2,4 ГГц; WEP-пароль заранее сохраняется в WFC-настройках. WPA/WPA2 на DS Fat не поддерживается.

Для PCM-проверки на ПК (с современным FFmpeg):

```sh
ffplay -f s16le -ar 22050 -ac 1 http://127.0.0.1:8000/v1/audio
```

## Формат и поведение

- PCM signed 16-bit little-endian, 22 050 Гц, моно: 44 100 байт/с, около 353 кбит/с без накладных расходов.
- Аудиоокно 617 400 байт: шесть секунд истории и до восьми секунд впереди. Старт и повторная буферизация после 88 200 байт. Короткий завершённый трек запускается без ожидания двух секунд. Близкая перемотка использует сохранённое аудио или текущее соединение, далёкая — серверный seek.
- При underrun выводится тишина, позиция не двигается до заполнения буфера. При паузе данные сохраняются, заполнение ограничивается буфером и TCP backpressure. Возможна небольшая задержка управления из-за внутреннего аудиобуфера Maxmod.
- Сокеты неблокирующие; DNS отсутствует в основном цикле. Maxmod работает в manual mode: сетевой код и callback используют буфер последовательно в одном потоке.
- Поддерживаются HTTP Content-Length, chunked и тело до закрытия соединения. Консоль запрашивает HTTP/1.0, чтобы обычно получать поток без chunked framing.
- Обрыв с незавершённым chunk/Content-Length и преждевременный конец относительно длительности дают ошибку. Для завершения по закрытию соединения допустима разница до двух секунд из-за длительности исходного аудио.
- Bitmap-интерфейс использует Gohufont с латиницей и кириллицей. Основной язык меню — английский; EN/RU на клавиатуре меняет язык ввода.

## API

| Запрос | Ответ |
|---|---|
| `GET /healthz` | JSON: `status`, `source`, `busy` |
| `GET /v1/search?q=lofi&offset=0&limit=4` | JSON: `tracks`, `offset`, `next_offset`, `has_more` |
| `GET /v1/track[?track_id=ID]` | JSON: `title`, `artist`, `duration_ms`, `permalink`, `test`, `sample_rate`, `channels`, `format` |
| `GET /v1/audio[?track_id=ID&start_ms=30000]` | `application/octet-stream`, PCM с выбранной позиции, без WAV-заголовка |
| `GET /v1/related[?track_id=ID]` | До 12 рекомендаций для автопроигрывания, отдельные от поиска |
| `GET /v1/artwork[?track_id=ID]` | Палитра и пиксели 64×64, 4624 байта; 204 при отсутствии картинки |

Аудиоответ содержит `X-Audio-Rate: 22050`, `X-Audio-Channels: 1`, `X-Audio-Format: s16le`, `X-Track-Duration-Ms`, `X-Audio-Start-Ms`, а также `X-Track-Title` и `X-Track-Artist` в percent-encoded UTF-8. Клиент получает метаданные прямо из заголовков потока. HTTP 409 означает занятый поток, 502 — ошибку источника/FFmpeg, 503 — занятый lookup, 416 — позицию за пределами трека. Недавние подписанные источники кешируются на 120 секунд; ошибка запуска удаляет запись.

Lookup SoundCloud ограничен общим бюджетом 25 секунд, ожидание первого PCM-блока — ещё 10 секунд, для seek с HLS fallback — до 35 секунд (HTTP 504 при превышении). Адаптер использует шаблоны извлечения client ID и curl-сессию закреплённой версии `soundcloud-v2`, чтобы ограничивать сетевые запросы; при обновлении библиотеки нужны тесты адаптера и живая проба.

## Проверки

```powershell
.\tools\check.ps1
```

Либо Linux с установленными FFmpeg и g++:

```sh
python -m pip install -r server/requirements-dev.txt
python -m pytest -q
g++ -std=c++17 -Wall -Wextra -Werror -Iclient/include tests/client_core.cpp client/source/http.cpp client/source/search.cpp client/source/keyboard.cpp client/source/endpoint.cpp client/source/artwork.cpp client/source/ui.cpp -o /tmp/soundcloud-ds-core-tests
/tmp/soundcloud-ds-core-tests
```

Автоматические тесты проверяют HTTP API, реальный PCM от FFmpeg, HTTP/1.0 и HTTP/1.1, ограничение одновременных потоков, очистку после отключения, ошибки источника, кольцевой буфер и разбиение HTTP на произвольные TCP-пакеты.

Ручная приёмка: десять минут тестового потока без underrun, полный SoundCloud-трек, пауза/продолжение, stop/restart, недоступный источник, отключение сети, затем те же проверки на DS Fat. Автоматические тесты не подтверждают аудиотайминги и пропускную способность настоящей консоли.

Фактически выполненные проверки и оставшаяся приёмка записаны в [docs/VALIDATION.md](docs/VALIDATION.md).


## Wi-Fi в разрабатываемой версии

В разрабатываемой версии после v0.1.0 добавлен ручной ввод WEP-ключей и
WPA2-Personal на DSi в нативном DSi-режиме. DS Fat/Lite аппаратно не поддерживает
WPA/WPA2. [Пароли и ограничения Wi-Fi](docs/WIFI.ru.md).

## Лицензия

Код проекта: **GPL-3.0-only**, [LICENSE](LICENSE). Сторонние ресурсы сохраняют свои лицензии: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Автор: [ivnis](https://github.com/ivnis). Независимый homebrew-проект, не связанный с SoundCloud или Nintendo.
