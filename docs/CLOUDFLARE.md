# Cloudflare Tunnel without Microsoft Store

**English** | [Русский](CLOUDFLARE.ru.md)

The project uses Cloudflare's portable Windows x64 executable; Microsoft Store,
winget, a Cloudflare account, and a domain are not required for a Quick Tunnel.

1. Start the local bridge on port 8765, for example with Start SoundCloud.cmd.
2. Double-click **Start Cloudflare Tunnel.cmd** in the repository root.
3. Wait for `Registered tunnel connection`, then check the printed URL followed
   by `/healthz`. The banner can print a URL before the connection is ready.
4. Keep the window open. Ctrl+C or **Stop Cloudflare Tunnel.cmd** stops the
   tunnel; it does not stop the local bridge.

The first launch downloads cloudflared 2026.10.3 from the official Cloudflare
GitHub release and verifies its pinned SHA-256. The executable, URL, session
record, and logs are kept in ignored `tools/.cache/cloudflared`.

For a different local bridge port or transport:

```powershell
.\tools\start-cloudflare.ps1 -Port 8000 -Protocol quic
```

QUIC is the default because HTTP/2 failed its edge TLS handshake in the tested
network, while QUIC connected successfully. No global VPN/DNS/firewall settings
are changed. Cloudflare normally requires outbound port 7844 (UDP for QUIC,
TCP for HTTP/2). A stopped/restarted Quick Tunnel gets a different hostname.
The service is public to anyone who has its URL and has no uptime guarantee.

## Nintendo DS compatibility

The tunnel's public HTTP and HTTPS health routes were both verified.
**v0.2.0 ROMs support HTTP hostnames**. In Settings, enter:

```text
your-tunnel.trycloudflare.com:80
```

Use the actual hostname printed by cloudflared. `http://` is optional; `https://`
is not supported by the ROM. The host is resolved to IPv4 in a cooperative
task and remains in the HTTP Host header, which selects the correct tunnel.
Do not substitute a Cloudflare edge IP. After restarting a Quick Tunnel, update
the hostname in the ROM because the temporary address changes.

In melonDS, the development ROM successfully tested `/healthz`, played a real
track, and fetched artwork/recommendations through the tunnel. Association and
playback from physical DS hardware through this tunnel remain unverified.
The released v0.1.0 ROM predates hostname support and needs a newer build.

References: [Cloudflare downloads](https://developers.cloudflare.com/tunnel/downloads/),
[Quick Tunnels](https://developers.cloudflare.com/tunnel/get-started/quick-tunnels/),
[required edge connectivity](https://developers.cloudflare.com/tunnel/configuration/).
