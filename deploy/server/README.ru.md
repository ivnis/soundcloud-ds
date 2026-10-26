# Сервер SoundCloud DS — v0.2.0

[English](README.md) | **Русский**

Этот архив содержит **только сервер**. Файл `soundcloud-ds-v0.2.0.nds`
скачивается отдельно из того же релиза и запускается в melonDS или с flashcart.
Сервер обрабатывает SoundCloud, конвертирует аудио, готовит обложки и рекомендации.

## Docker / VPS

Нужны Docker и Docker Compose. В Windows сначала установить и запустить Docker
Desktop. Python и скрипты установки/запуска Python при этом способе **не нужны**.

1. Полностью распаковать **серверный ZIP** через **Извлечь всё**.
2. Открыть папку, где лежит **compose.yaml**. Это не Downloads, не сам ZIP
   и не вложенная папка `server`.
3. В Windows нажать адресную строку Проводника, ввести `powershell` и нажать
   Enter. Терминал уже откроется в нужной папке; команда `cd` не требуется.
   В macOS/Linux открыть терминал в этой папке. При ручном переходе использовать
   `cd "путь/к/распакованной-папке"`, заменив пример настоящим путём.
4. Скопировать `.env.example` в `.env` и указать `SC_SOURCE=soundcloud`.
5. Запустить:

```sh
docker compose up -d --build
curl http://127.0.0.1:8000/healthz
```

Порт — **8000**. По умолчанию `SC_SOURCE=test` выдаёт тестовый тон на десять минут.
Остановка: `docker compose down`. После изменения `.env`:
`docker compose up -d --force-recreate`.

Ошибка **no configuration file provided** означает, что терминал открыт не в
папке с `compose.yaml`. Если команда **docker не найдена**, установить Docker
Desktop и заново открыть терминал. Если нет подключения к daemon, запустить
Docker Desktop и дождаться готовности engine. Логи: `docker compose logs -f bridge`.

## Windows

1. Скачать **серверный ZIP** из Assets релиза. Это отдельный архив, не `.nds`
   и не автоматически создаваемый GitHub архив **Source code**.
2. Нажать по ZIP правой кнопкой → **Извлечь всё**. Открыть распакованную папку;
   не запускать скрипты прямо внутри ZIP.
3. Установить **Python 3.13**, если его ещё нет: на [официальной странице Windows](https://www.python.org/downloads/windows/) найти Python 3.13, скачать **Windows installer (64-bit)** и запустить. При установке включить **Add python.exe to PATH**.
4. Дважды нажать **Start Server.cmd**. При первом запуске он сам создаст `.venv`
   и установит зависимости. Окно сервера оставить открытым.

Команда `cd` и ручной запуск терминала не нужны. **Setup Server.cmd** позволяет
отдельно установить или восстановить зависимости.

Для ручной установки открыть распакованную папку в Проводнике, нажать адресную
строку, ввести `powershell` и нажать Enter. Терминал уже будет в нужной папке;
вводить `cd` не требуется. Затем выполнить:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements-windows.txt
.\start-server.ps1
```

После первой установки зависимостей можно запускать **Start Server.cmd**.
FFmpeg поставляется через imageio-ffmpeg. Адрес по умолчанию —
**127.0.0.1:8765**. Остановка — Ctrl+C.

Тестовый тон: `./start-server.ps1 -Source test`.
Для настоящей DS в локальной сети:
`./start-server.ps1 -BindAddress 0.0.0.0 -Port 8000`.
Разрешить выбранный порт в своём firewall; скрипты не меняют правила автоматически.

## Нативный Linux

Установить Python 3.13 и FFmpeg, затем выполнить из папки архива:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r server/requirements.txt
SC_SOURCE=soundcloud SC_TRACK_URL=https://soundcloud.com/forss/flickermood \
  python -m uvicorn server.app:app --host 0.0.0.0 --port 8000 --workers 1
```

## Подключение DS

В ROM открыть шестерёнку **Settings**:

- **Emulator:** `10.64.0.1:8765` для Windows-скрипта и indirect networking melonDS.
  При запуске через Docker заменить порт на `8000`.
- **Console:** ввести IPv4 или домен своего сервера и порт;
  начальное `192.168.1.100:8000` — пример.
- Нажать адрес для редактирования и проверить **Test connection**.

ROM v0.2.0 принимает домены по HTTP, например `bridge.example.com:8000`.
Без порта используется 80. HTTPS/TLS в ROM не реализован.
DS Fat/Lite по-прежнему нужна совместимая открытая/WEP-сеть 2,4 ГГц со старыми
скоростями; WPA/WPA2 аппаратно недоступны. WPA2-Personal/AES требует нативного
DSi-режима. Try anyway — диагностика, а не разблокировка возможностей железа.

Запускать ровно **один worker**; одновременно поддерживается один аудиопоток.
DS подключается по HTTP без авторизации: ограничить доступ своей сетью.
Неофициальный адаптер SoundCloud зависит от изменений сервиса и региона.

Первая версия проверена в melonDS. Физическая DS Fat и длительное непрерывное
автопроигрывание пока не проверены. SDK, эмулятор, firmware, записи музыки
и пакеты Python в архив не включены.

## Cloudflare Tunnel в Windows без Microsoft Store

1. Запустить bridge и оставить его работающим.
2. Дважды нажать **Start Cloudflare Tunnel.cmd** в распакованной папке сервера.
   Он скачает официальный portable cloudflared и проверит SHA-256.
3. Дождаться `Registered tunnel connection` и скопировать выданный домен.
4. В Settings ROM указать свой `your-tunnel.trycloudflare.com:80`, заменив
   пример настоящим доменом. Используется HTTP; `https://` вводить не нужно.
5. Оставить окно туннеля открытым. Ctrl+C либо **Stop Cloudflare Tunnel.cmd**
   остановит только туннель. При новом запуске адрес изменится.

По умолчанию используется порт Windows-bridge 8765. Для Docker на порту 8000
открыть PowerShell в папке сервера и выполнить:

```powershell
.\tools\start-cloudflare.ps1 -Port 8000
```

Транспорт по умолчанию — QUIC. Для временного Quick Tunnel не нужны аккаунт,
домен, Store или winget. Сервис доступен всем, у кого есть ссылка.
Бинарник, сессия и логи сохраняются в `tools/.cache`.
Это временный доступ без гарантии постоянной работы.

## Лицензия и исходники

GPL-3.0-only — [LICENSE](../../LICENSE); зависимости сохраняют свои лицензии.
Исходники: https://github.com/ivnis/soundcloud-ds/tree/v0.2.0

Автор: [ivnis](https://github.com/ivnis) · https://t.me/lovelyperv
