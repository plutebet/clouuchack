---
name: protocol-teardown
description: Game network-protocol reverse engineering — capture, transport and framing identification, encryption-layer discovery, MITM proxy craft, packet replay and fuzzing. Use when traffic is the target or the auth/telemetry boundary sits on the wire. For server authority logic use game-server-security; for behavioral telemetry use behavioral-evasion; for hooking pinned TLS use frida-instrumentation.
---

# Protocol teardown — game traffic

**Pipeline:** capture → transport ID → framing ID → encryption layer → key recovery (→ `crypto-identification`) → proxy/replay/craft. Do not skip transport ID: most wasted hours come from parsing UDP as if framing were TCP-stream semantics.

**Transport identification (Wireshark, first 5 minutes):**
- TCP stream with fixed port + TLS ClientHello = TLS-wrapped game protocol; check `SSLKEYLOGFILE` support (works if client uses OS/schannel/OpenSSL and reads env — try before hooking).
- UDP with fixed-size rhythmic packets = likely a game reliability layer, not raw UDP: check ENet/KCP/RakNet/GameNetworkingSockets headers before assuming custom.
- WebSocket upgrade in TCP stream → JSON/protobuf/binary frames inside.
- QUIC/DTLS = encrypted transport from packet one; jump straight to key hooks (client TLS stack) instead of traffic analysis.

**Framing identification:** length-prefix (endianness, offset), delimiter, fixed-size datagrams, or reliability-layer headers. ENet: 2-byte peer ID (host = `0xFFFF`) + command block, reliable payload = command 6, unreliable = 7. KCP: 24-byte segment header, `conv(u32) cmd(1) frg(1) wnd(u16) ts(4) sn(4) una(4) len(4)`, cmd bytes `0x51`–`0x54`. RakNet: leading 1-byte message ID, custom game payloads start at `ID_USER_PACKET_ENUM`. Protobuf in-band: varint field tags = `(field<<3)|wire_type`, wire types 0/1/2/5 dominate.

**Encryption-layer discovery:** entropy per segment (uniform high = payload encrypted, header plaintext = framing intact); fixed key = same plaintext/position → same ciphertext; session keys = handshake present (find the exchange, or hook key derivation in the client — often cheaper than breaking DH). Keys and ciphers go through `crypto-identification`.

**MITM craft:** for TCP/WS use mitmproxy; for UDP build a relay (listen socket per client, forward socket per server, no protocol awareness needed to start). Proxy must preserve framing byte-exact — reserialize nothing until framing is decoded. Checksums/HMAC/seq guards: flip one byte, watch which guard drops the packet, recompute or hook the guard.

Refs in `references/`:
- capture-and-framing.md — per-transport capture, header maps, framing decode
- proxy-craft-and-replay.md — TCP/UDP relay build, decode pipeline, packet diffing, replay/fuzz discipline

Route: `crypto-identification` (keys/ciphers), `frida-instrumentation` (pinned TLS, client-side dumps), `game-server-security` (what the server validates), atlas `topics/scene-forums-media.md` for vendor-specific protocol notes.

## Unknown → search, don't guess

Unfamiliar error, unknown API, version/offset drift, or anything this skill does not answer — do not improvise from memory. Search the web first (`web-research` skill / WebSearch) and look at how other people solved the same problem: official docs, GitHub issues, forum threads, existing tooling. Record what you learn in memory-bank so the same mistake is never made twice.
