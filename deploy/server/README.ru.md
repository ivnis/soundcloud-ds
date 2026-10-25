# Сервер SoundCloud DS — v0.1.0

[English](README.md) | **Русский**

Этот архив содержит **только сервер**. Файл `soundcloud-ds-v0.1.0.nds`
скачивается отдельно из того же релиза и запускается в melonDS или с flashcart.
Сервер обрабатывает SoundCloud, конвертирует аудио, готовит обложки и рекомендации.

## Docker / VPS

Нужны Docker и Docker Compose. Полностью распаковать архив, перейти в его папку,
скопировать `.env.example` в `.env`, указать `SC_SOURCE=soundcloud` и запустить:

```sh
docker compose up -d --build
curl http://127.0.0.1:8000/healthz
```

Порт — **8000**. По умолчанию `SC_SOURCE=test` выдаёт тестовый тон на десять минут.
Остановка: `docker compose down`. После изменения `.env`:
`docker compose up -d --force-recreate`.

## Windows

Установить Python 3.13. В PowerShell из папки распакованного архива:

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
- **Console:** ввести реальный числовой IPv4 своего ПК/VPS и порт сервера;
  начальное `192.168.1.100:8000` — пример.
- Нажать адрес для редактирования и проверить **Test connection**.

Запускать ровно **один worker**; одновременно поддерживается один аудиопоток.
DS подключается по HTTP без авторизации: ограничить доступ своей сетью.
Неофициальный адаптер SoundCloud зависит от изменений сервиса и региона.

Первая версия проверена в melonDS. Физическая DS Fat и длительное непрерывное
автопроигрывание пока не проверены. SDK, эмулятор, firmware, записи музыки
и пакеты Python в архив не включены.

## Лицензия и исходники

GPL-3.0-only — [LICENSE](../../LICENSE); зависимости сохраняют свои лицензии.
Исходники: https://github.com/ivnis/soundcloud-ds/tree/v0.1.0

Автор: [ivnis](https://github.com/ivnis) · https://t.me/lovelyperv
