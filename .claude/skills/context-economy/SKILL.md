---
name: context-economy
description: Use BY DEFAULT on every non-trivial task — any codebase work, multi-file reading, large files, broad exploration, debugging, implementation, or review — even when the user did not ask. Keep the main context small by delegating bulk reads to subagents, using targeted searches instead of full-file reads, and returning summaries instead of dumps. Prevents context-window bloat and token waste.
---

# Context Economy

## Overview

**Core principle:** The main context is the wheelhouse — navigation only. Cargo moves through subagents.

Every token of raw file content that enters the main context is paid for on every subsequent turn of the session. A 2000-line file read once costs that, plus interest. Most reads exist only to answer one question — get the answer, not the file.

## Rules

1. **Delegate the sweep.** Broad questions ("where is X handled?", "how does Y work here?", multi-directory exploration) go to an Explore/general-purpose subagent with a pointed question. The subagent reads everything and returns the conclusion. Never sweep directories into the main context yourself.
2. **Read narrow, not wide.** For single known files: `grep`/symbol search first, then read the specific line range (offset/limit). Full-file reads only for files that will be edited or are under ~200 lines.
3. **Search beats read.** A symbol lookup, a `git log -S`, an import graph answer — one command each. If the question fits in one grep, do not open the file.
4. **Summarize, don't paste.** Quoting source into an answer or a plan: quote the 3-5 load-bearing lines with `file:line` references, never the surrounding half-file. The reference is clickable; the paste is dead weight.
5. **One question, one subagent.** Do not re-read what a subagent already read; use its conclusion. Don't verify a subagent's search by repeating it yourself.
6. **Heavy extraction is a subagent job.** Parsing a big log, a long document, a JSON dump, or diffing large outputs — dispatch a subagent with the extraction question and receive the distilled result.
7. **Watch the re-read reflex.** Do not re-read a file you already read this session "to be sure" — the harness tracks file state; an Edit that would fail on stale content fails loudly anyway.

## When this does NOT apply

- The file is about to be edited — read it properly, in full, before cutting.
- The user explicitly asked to see the content — show it, that is the deliverable.
- The read is small and the round-trip of a subagent would cost more tokens than the read.

## Smell test

If the transcript shows more than ~5 full-file reads in the main context for one task, or a single read over 1000 lines that was used for one fact, the economy has failed — switch to delegation for the remainder.
