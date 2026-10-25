# Сервер SoundCloud DS — v0.1.0

[English](README.md) | **Русский**

Этот архив содержит **только сервер**. Файл `soundcloud-ds-v0.1.0.nds`
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
3. Установить Python 3.13, если его ещё нет.
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
