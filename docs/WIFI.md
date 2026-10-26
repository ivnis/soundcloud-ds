# Wi-Fi passwords and hardware support

**English** | [Русский](WIFI.ru.md)

This describes v0.2.0. The older v0.1.0 ROM does
not include the new password keyboard or the DSi-mode initialization change.

| Hardware / execution mode | Supported networks |
| --- | --- |
| DS Fat / DS Lite | Open or WEP, legacy 2.4 GHz / 802.11b-compatible access point |
| DSi / DSi XL running in DS compatibility mode | Same restrictions as DS Fat/Lite |
| DSi / DSi XL running in native DSi mode | Open, WEP, and compatible WPA2-Personal/AES networks |
| melonDS indirect networking | Connect the emulated DS to open `melonAP`; the PC manages its real connection |

DS Fat/Lite cannot gain WPA/WPA2 support through a password field. For those
devices, keep your regular network protected and use a separate compatible
access point if needed. WPA3-only, WPA/TKIP, enterprise authentication, and
5 GHz-only networks are not supported by this implementation.

## Connect

1. Open Settings → Wi-Fi, or use the initial network selection screen.
2. Each row shows its security type. Unsupported networks have a red label.
   Selecting one opens a warning. **Try anyway** passes a diagnostic request
   to DSWifi; Cancel returns without connecting. This does not bypass hardware
   restrictions: DS Fat/Lite cannot acquire WPA/WPA2 support this way, and the
   library may reject or time out the request before association.
3. Select a supported encrypted network to open its password keyboard.
4. Enter the key, then tap **Connect** or press DS Y.
5. B or Back cancels. A wrong key or connection timeout returns to network selection.

Passwords are masked by default. Show/Hide toggles visibility, Shift changes
letter case, and the 123 → #+= → ABC key cycles through digits/symbol pages and
letters. All printable ASCII characters, including spaces, are available.
Leading/trailing spaces and case are preserved.

- WEP: 5 or 13 ASCII characters, or 10 or 26 hexadecimal digits. Hexadecimal
  keys are decoded into bytes before calling DSWifi.
- WPA2-Personal: an 8–63 character printable ASCII passphrase. A raw 64-digit
  PSK and non-ASCII passphrases are not accepted by this editor.

APs already stored in firmware still use WFC settings by default. On the
selection screen, DS Y opens manual key entry to override a saved key;
DS X connects using saved WFC settings. New passwords are not written to a
file or log. Temporary input/key buffers are wiped after submission/cancel;
the Wi-Fi library keeps whatever connection state it needs.

## Finding a phone hotspot

The list now shows its visible range and total count; scroll with Up/Down to
see more than five entries. Rescan or DS B restarts scanning. Reads are cached
as a snapshot, failed reads are skipped, and selection is preserved by BSSID
when the list changes. Compatible, named networks are shown first.

A blank SSID is labeled `Hidden XX:XX:XX` using the BSSID suffix. It is not
assumed to be your phone. Unhide the hotspot name, or use a saved WFC connection;
this UI does not guess a hidden SSID.

On a physical DS Fat, try a visible, simply named hotspot such as `POCO-DS`,
explicitly set to **2.4 GHz**, open security, and legacy compatibility if the
phone offers that option. Turn the hotspot off/on and rescan. If channel
selection is available, try 1, 6, or 11. Open security alone does not guarantee
DS compatibility: its radio needs legacy 802.11b rates, while a phone hotspot
may use newer-only rates. The phone's client Wi-Fi specifications do not prove
that its hotspot enables those legacy rates.

These changes improve discovery/UI diagnostics, but actual POCO M7 hotspot
association has not been verified on physical hardware.

## DSi mode and validation

Initialization requests `WIFI_ATTEMPT_DSI_MODE`; the library falls back to DS
mode on ordinary DS hardware. The generated ROM is DSi-enhanced, but it must
actually be launched in native DSi mode by a suitable loader. A flashcart
running in DS compatibility mode does not unlock WPA2 on a DSi.

The app also checks DSWifi's per-network compatibility flag, native DSi mode,
PSK authentication and AES group/pair ciphers, rather than assuming that every
network advertising WPA2 is usable. Native tests cover these restrictions, key
validation, hexadecimal decoding, secret wiping, and password keyboard coverage.
ARM builds and UI previews passed. melonDS in DS mode scanned `melonAP` and
connected successfully after the initialization change. Physical WPA2 association on DSi and physical
WEP association on DS are **not yet verified**.

References: [DSWifi internet guide](https://blocksds.skylyrac.net/dswifi/md_documentation_2internet__mode.html),
[Nintendo DS network compatibility](https://en-americas-support.nintendo.com/app/answers/detail/a_id/4669/).
