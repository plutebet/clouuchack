# Proxy craft, replay, fuzzing

## TCP/WS relay (start here)

Minimal forwarder, no protocol awareness — byte-exact, preserves framing:

```python
# language: Python, file: tcp_relay.py
# usage: python tcp_relay.py <listen_port> <server_host> <server_port>
import socket, threading, sys

def pipe(src, dst, tag):
    try:
        while True:
            data = src.recv(65535)
            if not data: break
            log(tag, data)                # your tap: file/pcap/struct parse
            dst.sendall(data)
    except OSError: pass
    finally:
        src.close(); dst.close()

def handle(client, host, port):
    server = socket.create_connection((host, port))
    threading.Thread(target=pipe, args=(client, server, "C->S"), daemon=True).start()
    threading.Thread(target=pipe, args=(server, client, "S->C"), daemon=True).start()
    client.recv(1)  # hold open
```

Redirect the client to it via hosts file, client config, or a one-line patch of the server address constant (`crypto-identification` for obfuscated strings). For TLS targets terminate TLS in the relay with your own cert + pin-bypass in the client (`frida-instrumentation`).

## UDP relay

Same shape, no accept/recv loop differences that matter: one bound socket facing the client, one send-socket facing the server, keep a mapping client-addr → server socket. Log `(direction, src, ts, bytes)` — timestamps matter later for heartbeat analysis. **Never reserialize**: forward frames untouched, parse only the copy you log.

## Decode pipeline

1. Log to per-flow files, both directions, with timestamps.
2. Cluster by leading bytes → candidate packet IDs.
3. For each ID: collect ≥50 samples, mark constant bytes (header/checksum) vs variable bytes (payload).
4. Attempt framing decode (`capture-and-framing.md`), then serialization ID.
5. Named-field map grows incrementally; keep it as a struct table with evidence (which session, which player action produced it).

## Packet diffing — field discovery

- Send identical in-game actions twice; diff the two outgoing packets: identical bytes = constant/stateless, differing = payload.
- Controlled input: move exactly +X then revert; find fields that track position deltas (floats that change by the same magnitude). Position/velocity fields reveal themselves under axis-isolated movement.
- Flip one field at a time on the wire (the relay is the place to mutate) and watch the server: accepted state change, silently ignored, or disconnect. The disconnect tells you a guard fired — find it (seq/checksum/HMAC, see below).

## Guards: seq, checksum, HMAC

| Symptom | Guard | Handling |
|---|---|---|
| Flip ignored entirely | checksum over payload | recompute in relay once algorithm found (CRC32 variants, adler, fletcher) |
| Replays rejected, fresh packets fine | sequence number | track + increment in relay |
| Flip → instant disconnect, checksum recomputed correctly | HMAC/signature | key needed → `crypto-identification`; or hook the signing function in the client and sign your forged packets through it |
| Session dies after N minutes through relay | heartbeat mismatch | forward heartbeats untouched; do not buffer them behind your parser |

Hooking the client's send path (`sendto`, WSASendTo, or the lib's own `ENetPacket_send`-alike) and letting forged packets originate from the client itself bypasses most guard work — use when the guard is expensive to reverse.

## Replay and fuzz discipline

- Replay one direction at a time; full-session replay desyncs state and proves nothing.
- Fuzz payload fields only after framing is solid — pre-framing fuzz noise is all dropped at the framing layer and teaches you nothing.
- Keep a per-experiment log: packet ID, mutation, server observable. The map is the deliverable, not the packets.

## Encryption layer

If payload entropy is uniformly high: handshake hunt (DH/AES key exchange — hook the client's key-derivation instead of breaking the exchange), or static key (same ciphertext at same offset across sessions = static/position-dependent XOR — `crypto-identification`). Client-side key hooks beat traffic-side cryptanalysis every time the client must hold the plaintext key: it does, by definition.
