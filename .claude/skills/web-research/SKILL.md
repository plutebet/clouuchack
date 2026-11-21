---
name: web-research
description: Use whenever a question needs the internet — current events, releases, versions, pricing, news, docs, error messages, unfamiliar APIs/libraries, "which is better" comparisons, or anything possibly changed after training data. Also use to verify any claim the user will act on. Covers search strategy, source selection, cross-checking, and cited answers. Triggers even when the user does not say "search" or "google". Trigger it also whenever you are unsure, do not understand something, or are about to guess — search first, look at how other people did it, avoid making a new mistake.
---

# Web Research

## Overview

**Core principle:** If the answer has a timestamp, the network is the primary source. If memory is uncertain, the network arbitrates. Stale memory is a reef — chart from live water.

## When to go online (any one suffices)

- Question contains or implies "latest", "current", "now", a year ≥ training cutoff, or a version/release/price/news.
- An unfamiliar tool, API, or error message — one search usually beats guessing.
- About to improvise because something is not understood — search how other people solved the same problem first; a known working answer beats a freshly invented mistake.
- A comparison or recommendation question ("which is better", "what should I use") — recency changes the answer.
- A local claim (doc, README, comment) contradicts expected behavior and may be outdated.
- A library/API/build error mentions a version — check the changelog or issue tracker before guessing.
- About to state a fact the user will act on and memory is not certain — verify, don't vibe.
- Verifying a disputed claim: find the primary source (spec, RFC, official changelog, repo), not a blog paraphrase of it.

## Search strategy — what separates research from a lucky first hit

1. **Decompose before searching.** Split the question into the 1-3 concrete facts that answer it. Search each fact, not the full sentence the user typed.
2. **Query craft.** Write queries the way an expert types into a search bar: drop filler words, keep the distinctive terms, add the discriminating detail (exact error text, version number, model name). Technical questions: prefer English queries — the source corpus is larger; use the local language when the answer lives in local sources (regional pricing, local news, RU-language forums).
3. **Two rounds minimum for non-trivial questions.** Round 1: broad discovery — learn the domain's vocabulary and which hosts are authoritative. Round 2: targeted — site filters (`allowed_domains`, `blocked_domains`), exact error strings, version numbers. If round 1 results are weak, reformulate — different phrasing, different language, different site scope — instead of settling for the first page.
4. **Tool selection.**
   - `WebSearch` — discovery, news, finding the authoritative host. US-only results: for non-English sources, search on the site itself via `web_reader`.
   - `WebFetch` — pointed extraction from a known page. The prompt must name the exact fact(s) wanted; it runs on a small model, vague prompts return dumps.
   - `web_reader` (`mcp__web_reader__webReader`) — full pages, JS-heavy/SPA content, docs sites whose snippets truncate, and pages that need image retention.
   - Independent sub-questions: fire the calls in parallel in one block.
5. **Source hierarchy.** Official docs/changelogs/repos/specs > source code > reputable engineering blogs > aggregators > forums. A GitHub release note outranks a blog about the release note. Check dates — a 2021 blog post about a 2026 tool is noise.
6. **Cross-check load-bearing facts.** Anything the user will act on: two independent sources, or one primary source. If sources conflict, resolve by authority and date, and say which won and why.
7. **Synthesize, don't relay.** The deliverable is the answer, not a tour of search results. Three sources agreeing: cite the best one, not all three. Sources conflicting: that is the finding — report it.

## Answer discipline

- Lead with the answer; support after. Match depth to the question.
- Distinguish verified (cited) from inferred or from-memory. Mark uncertainty plainly — "unverified" beats confident wrong.
- End with a `Sources:` list of markdown links actually used. No citation, no claim.

## Token discipline

- One search per question first; fan out to parallel searches only when the question genuinely splits into independent sub-questions.
- `WebFetch` prompt names the exact fact wanted — vague prompts return dumps.
- Same URL within 15 min = cache hit — refetch is free, re-searching is not.
- Never paste whole pages into the answer.

## Boundaries

- Authenticated/private targets: use the session's own tools (gh, MCP), never the public fetchers.
- A redirect to another host: follow once with the returned URL, don't loop.
- If the network fails twice, say so plainly and mark the answer as unverified — never paper over with memory presented as live fact.
