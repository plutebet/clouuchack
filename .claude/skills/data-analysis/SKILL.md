---
name: data-analysis
description: Use when working with data files — CSV, JSON, logs, Excel exports, SQL dumps — for profiling, cleaning, statistics, aggregation, or charts. Triggers on "analyze", "statistics", "how many", "distribution", "average", "сколько", "посчитай", or any question answerable from a dataset. Compute the answer; never estimate what can be computed.
---

# Data Analysis

## Overview

**Core principle:** Never eyeball a dataset, never estimate what you can compute. Profile first, compute second, visualize when the shape matters.

## Pipeline

1. **Profile before anything.** One pass: rows/cols, dtypes, null counts, unique counts, min/max, head. Pandas `info()`+`describe()`, `csvstat`, or one `duckdb` query. The profile usually changes the plan.
2. **Clean deliberately.** Every mutation logged in the output: rows dropped (n and why), types coerced, dates assumed. Silent cleaning is lying with extra steps.
3. **Compute with the right tool.**
   - Tabular, fits in RAM: pandas.
   - JSON: `jq` for extraction, pandas for analysis.
   - Logs: `grep`/`awk` under ~100 MB; `duckdb` or chunked pandas above.
   - Never load 10 GB into pandas — chunk it, sample for shape, aggregate early.
4. **Visualize when shape matters.** matplotlib, save PNG to disk and reference the path — never dump base64 into chat. One chart, labeled axes, units in the title.
5. **Report with denominators.** `23% of 4,101 rows (943)` — never a bare percentage. State assumptions (timezone, encoding, filters) next to the numbers they affect.

## Rules

- Dates: pin format and timezone explicitly. Naive datetimes are the #1 silent error in this lane.
- The script is the deliverable for any non-trivial transform — rerunnable beats remembered.
- Sample first for shape (`.sample(10_000)`); full compute only for the final number.
- Report what was measured, not what it implies — distributions, not conclusions; the captain draws the inference.

## Environment (Windows)

- `py -3` launcher; check `pandas`/`duckdb` availability, `pip install` into the active env when missing.
- Cyrillic CSVs from Windows tools: try `utf-8`, fall back to `cp1251`.
