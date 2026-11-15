---
name: reverser-firmware
description: Router / IoT firmware extraction — unpack nested filesystems, locate web server, find backdoor credentials.
---

# Firmware Extraction

1. Extract: `binwalk -eM firmware.bin`; find `squashfs-root`/`rootfs`.
2. Root: `/etc/passwd`, `/etc/shadow` (→ hashcat), init scripts, `bootargs.txt` (UART/JTAG flags).
3. Web server: find `httpd|lighttpd|nginx|boa`; `www/cgi-bin/` = highest-yield (per-endpoint binaries, direct `system()`).
4. Hardcoded creds: strings category secret/crypto on server binary; base64/hex near `strcmp`.
5. Auth bypass: magic-string compares (`"admin"`), debug paths (`/debug`, `/cgi-bin/shell`).
6. CVEs: busybox/kernel/dropbear/openssl versions → lookup.

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
