# Capture and framing

## Capture setup

- Wireshark: capture on the active interface, filter by game process IP (`ip.addr == <server>`). Resolve server IP from netstat while the game runs: `netstat -ano | findstr <pid>`.
- TLS traffic: set `SSLKEYLOGFILE=<path>` env globally (System Properties → Environment Variables) before launching the client. Works when the client uses schannel/OpenSSL and honors the env — read the pcap in Wireshark with the keylog under Preferences → Protocols → TLS. If stream stays encrypted, the client pins or ignores env → hook the client's TLS read/write, see `frida-instrumentation`.
- UDP: Wireshark has no stream reassembly for raw UDP — use "Follow → UDP stream" for flow order, or write the relay from `proxy-craft-and-replay.md` and log both directions yourself.
- Loopback/local server: use RawCap or npcap's loopback adapter for 127.0.0.1 traffic.

## Reliability-layer header maps

### ENet
Header: `peerID(u16)` + `sentTime(u16)` + command block. Host-sent packets have `peerID = 0xFFFF`. Flag bits in peer ID: `0x8000` compressed, `0x4000` sent-time present. Commands (u8 + u16 peer id inside block): 1 connect, 2 verify connect, 3 acknowledge, 4 disconnect, 5 ping, **6 send reliable**, **7 send unreliable**, 8 send fragment, 9 send unsequenced. Payload starts after command struct (reliable: `reliableSequenceNumber(u16)` then data).

### KCP
Segment header (24 bytes, little-endian): `conv(u32)` `cmd(u8)` `frg(u8)` `wnd(u16)` `ts(u32)` `sn(u32)` `una(u32)` `len(u32)`. cmd: `0x51` PUSH, `0x52` ACK, `0x53` WASK, `0x54` WINS. Payload follows `len` bytes. Some wrappers prefix their own magic/conv byte before KCP — if the first 24 bytes don't parse, check for a 1–4 byte wrapper prefix.

### RakNet
First byte = message ID. System IDs live below `0x93`; custom game payloads start at `ID_USER_PACKET_ENUM`. Offline datagrams (ping/handshake) carry a 16-byte magic constant — spot them by identical leading 16 bytes across different-length packets. Reliability layer headers include sequence/receipt numbers per reliability category (unreliable/reliable/sequenced) — expect 3 bytes of ordering metadata before payload on many IDs.

### GameNetworkingSockets / Steam datagram
Look for the connection-id / packet-number crypto header (encrypted after handshake) — teardown requires hooking the client library, not traffic analysis.

## Framing decode

Order of tests on a reassembled byte stream:
1. Fixed-size datagrams? (UDP with constant packet length per message class)
2. Length prefix? Check first 2/4 bytes against total packet length for both endiannesses; also check for a 1-byte type + 2-byte length layout (very common).
3. Delimiter? `\n` in JSON-ish streams, `\x00` in older C structs.
4. Reliability header first, payload framing after (ENet/KCP/RakNet — strip their headers before framing tests).

## Serialization identification

- **Protobuf:** field tags are varints; decode first byte as `(field<<3)|wire_type`; wire types seen: 0 varint, 1 64-bit, 2 length-delimited, 5 32-bit. Valid protobufs decode into small field numbers (1–30) with consistent wire types per field across packets.
- **MessagePack:** `0x80–0x8F` fixmap, `0x90–0x9F` fixarray, `0xA0–0xBF` fixstr, `0x00–0x7F` positive fixint, `0xC0–0xC3` nil/bool.
- **JSON/JSON5:** obviously printable, `{`/`[` leading.
- **FlatBuffers:** leading u32 = vtable offset (small even number), lots of little u32 offsets.
- **Custom C structs:** same field offsets across packets of the same ID; strings as fixed char arrays.

Identify per packet-ID: cluster packets by leading bytes, decode one representative, verify constants hold across the cluster.
