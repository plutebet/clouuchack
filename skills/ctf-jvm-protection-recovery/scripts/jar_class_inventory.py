#!/usr/bin/env python3
"""Read-only structural inventory for a JAR used in authorized CTF analysis."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import zipfile
from pathlib import Path

OPAQUE = re.compile(r"^[0-9a-fA-F]{32}$")


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("jar", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    failures: list[dict[str, object]] = []
    versions: dict[str, int] = {}
    class_count = 0
    opaque_entries = 0
    total_uncompressed = 0
    with zipfile.ZipFile(args.jar) as archive:
        infos = archive.infolist()
        names = [info.filename for info in infos]
        duplicate_count = len(names) - len(set(names))
        for info in infos:
            total_uncompressed += info.file_size
            if OPAQUE.fullmatch(info.filename):
                opaque_entries += 1
            if not info.filename.endswith(".class") or info.is_dir():
                continue
            class_count += 1
            with archive.open(info) as stream:
                header = stream.read(8)
            if len(header) != 8 or header[:4] != b"\xca\xfe\xba\xbe":
                failures.append({"entry": info.filename, "error": "invalid class header"})
                continue
            minor, major = struct.unpack(">HH", header[4:])
            key = f"{major}.{minor}"
            versions[key] = versions.get(key, 0) + 1
        crc_failure = archive.testzip()

    report = {
        "jar": str(args.jar.resolve()),
        "size": args.jar.stat().st_size,
        "sha256": file_sha256(args.jar),
        "entries": len(names),
        "duplicate_entries": duplicate_count,
        "class_entries": class_count,
        "opaque_32hex_entries": opaque_entries,
        "uncompressed_bytes": total_uncompressed,
        "class_versions": versions,
        "first_crc_failure": crc_failure,
        "class_header_failures": failures,
    }
    output = json.dumps(report, indent=2, ensure_ascii=False)
    print(output)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(output + "\n", encoding="utf-8")
    return 1 if duplicate_count or crc_failure or failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
