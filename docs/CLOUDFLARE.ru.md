# Cloudflare Tunnel без Microsoft Store

[English](CLOUDFLARE.md) | **Русский**

Используется официальный portable cloudflared для Windows x64. Для Quick Tunnel
не нужны Microsoft Store, winget, аккаунт Cloudflare или собственный домен.

1. Запустить локальный bridge на порту 8765, например через Start SoundCloud.cmd.
2. Дважды нажать **Start Cloudflare Tunnel.cmd** в корне проекта.
3. Дождаться `Registered tunnel connection` и проверить адрес с `/healthz`.
   Баннер может напечатать URL раньше, чем подключение заработает.
4. Оставить окно открытым. Ctrl+C либо **Stop Cloudflare Tunnel.cmd** остановит
   туннель; локальный bridge продолжит работать.

При первом запуске скачивается cloudflared 2026.10.3 из официального GitHub-релиза
Cloudflare с проверкой SHA-256. Бинарник, URL, запись процесса и логи находятся
в игнорируемой папке `tools/.cache/cloudflared`.

Другой порт или транспорт:

```powershell
.\tools\start-cloudflare.ps1 -Port 8000 -Protocol quic
```

По умолчанию выбран QUIC: в проверенной сети HTTP/2 давал ошибку TLS до Edge,
а QUIC подключился. Глобальные настройки VPN, DNS и firewall не изменяются.
Для Edge нужен исходящий порт 7844: UDP для QUIC, TCP для HTTP/2.
При новом запуске адрес меняется. Сервис доступен всем, у кого есть ссылка;
Quick Tunnel не гарантирует постоянную доступность.

## Совместимость с DS

Публичный `/healthz` проверен по HTTP и HTTPS. **ROM v0.2.0
поддерживает домены по HTTP**. В Settings нужно указать:

```text
your-tunnel.trycloudflare.com:80
```

Заменить пример настоящим доменом из вывода cloudflared. Префикс `http://`
необязателен; `https://` ROM не поддерживает. Домен разрешается в IPv4 отдельной
кооперативной задачей и сохраняется в HTTP Host, по которому выбирается туннель.
Подставлять вместо него IP узла Cloudflare не нужно. При перезапуске Quick Tunnel
обновить домен в ROM: временный адрес меняется.

В melonDS проверены `/healthz`, реальное воспроизведение, обложка и рекомендации
через этот туннель. На физической DS соединение и поток через Cloudflare пока
не проверены. Релизный ROM v0.1.0 ещё не содержит поддержки доменов.

Источники: [скачивание cloudflared](https://developers.cloudflare.com/tunnel/downloads/),
[Quick Tunnels](https://developers.cloudflare.com/tunnel/get-started/quick-tunnels/),
[подключение к Edge](https://developers.cloudflare.com/tunnel/configuration/).
