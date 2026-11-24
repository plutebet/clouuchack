---
name: memory-bank
description: Use to persist project knowledge across sessions — maintains a memory-bank/ archive (MEMORY.md) with decisions, named builds, offsets, environment quirks. Triggers at project start, after shipping a build or artifact, when the user says "remember this" / "запомни" / "занеси в банк", or at session start whenever memory-bank/ exists in the workspace.
---

# Memory Bank

## Overview

**Core principle:** Sessions die; the bank doesn't. The conversation is working memory — the bank is the chart archive. Anything worth referencing next week goes in the bank: named, dated, one line.

## Structure

`memory-bank/` in the project root:

- **MEMORY.md** — the living chart, target under ~200 lines:
  - **Active context** — current task, where it stands, next step.
  - **Decisions** — what was chosen and why, one line each, dated. (`2026-09-27: indirect syscalls over direct — ETW TI hooks ntdll stubs.`)
  - **Artifacts** — named builds, one line each. Names are indexes: `crypter-v2 (2026-09-20) — rolling-xor stack strings, syscall unhooking.`
  - **Environment** — tool versions, paths, build commands, quirks that cost time once.
  - **Reefs (mistakes)** — approaches that failed, wrong offsets, traps hit — one line each, so they are never re-tried.
  - **Open questions** — parked threads with enough context to resume cold.
- **archive/** — overflow. Old detail moves here (one file per topic) when MEMORY.md bloats; MEMORY.md points to it.

## Protocol

- **Session start:** if `memory-bank/MEMORY.md` exists, read it first — it is short by design. Resume from Active context, not from zero.
- **Write at checkpoints, not continuously:** decision made, artifact shipped, environment changed, session ending. Each write also refreshes Active context.
- **Facts, not narrative.** `offset 0x40, win32u, build 22631` — not "we spent an hour finding it." The bank is consulted, not read.
- **Log reefs the same session:** when an approach fails or costs real time, write the reef entry immediately — the bank exists so the same mistake is made at most once.
- **Name everything.** A build without a name cannot be referenced; names accumulate into voyage history.
- **Date every entry.** Freshness is part of the fact — a 2024 driver offset is a different fact from a 2026 one.

## Boundaries

- No secrets, keys, or credentials in the bank.
- Do not duplicate what git/code already records — the bank holds the why and the map, not the diff.
- If the user's current ask contradicts the bank, the ask wins; update the bank after.
