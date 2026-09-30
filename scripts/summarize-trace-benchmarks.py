#!/usr/bin/env python3
"""Generate a deterministic Markdown summary from trace benchmark CSVs."""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path
import statistics
import sys


def read_csv(path: Path, expected: list[str]) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != expected:
            raise ValueError(
                f"{path.name}: unexpected CSV schema\n"
                f"expected: {','.join(expected)}\n"
                f"actual:   {','.join(reader.fieldnames or [])}"
            )
        rows = list(reader)
        if not rows:
            raise ValueError(f"{path.name}: CSV contains no data rows")
        return rows


def fmt(value: float) -> str:
    if value >= 100.0:
        return f"{value:.2f}"
    return f"{value:.3f}"


def markdown_table(headers: list[str], rows: list[list[str]]) -> list[str]:
    return [
        "| " + " | ".join(headers) + " |",
        "| " + " | ".join("---" for _ in headers) + " |",
        *("| " + " | ".join(row) + " |" for row in rows),
    ]


def summarize_trace(rows: list[dict[str, str]]) -> list[str]:
    grouped: dict[str, list[float]] = defaultdict(list)
    checksums: dict[str, set[str]] = defaultdict(set)
    for row in rows:
        grouped[row["mode"]].append(float(row["ns_per_evaluation"]))
        checksums[row["mode"]].add(row["checksum"])

    medians = {mode: statistics.median(values) for mode, values in grouped.items()}
    baseline = medians.get("baseline")
    if baseline is None:
        raise ValueError("trace.csv: missing baseline mode")

    table_rows: list[list[str]] = []
    for mode in grouped:
        values = grouped[mode]
        checksum = checksums[mode]
        if len(checksum) != 1:
            raise ValueError(f"trace.csv: inconsistent checksum for {mode}")
        median = medians[mode]
        table_rows.append([
            mode,
            str(len(values)),
            fmt(median),
            fmt(min(values)),
            fmt(max(values)),
            f"{median / baseline:.3f}x",
        ])

    return markdown_table(
        ["Mode", "Trials", "Median ns/eval", "Min", "Max", "vs baseline"],
        table_rows,
    )


def summarize_cost_encoding(rows: list[dict[str, str]]) -> list[str]:
    table_rows = [
        [
            row["cost_model"],
            fmt(float(row["ns_per_event"])),
            fmt(float(row["bytes_per_event"])),
            row["event_count"],
        ]
        for row in rows
    ]
    return markdown_table(
        ["Cost model", "ns/event", "bytes/event", "Events"],
        table_rows,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("trace_csv", type=Path)
    parser.add_argument("cost_encoding_csv", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    trace = read_csv(
        args.trace_csv,
        ["trial", "mode", "ns_per_evaluation", "checksum"],
    )
    cost_encoding = read_csv(
        args.cost_encoding_csv,
        ["cost_model", "ns_per_event", "bytes_per_event", "event_count"],
    )

    lines = [
        "# Trace microbenchmark summary",
        "",
        "Performance values are diagnostic measurements, not pass/fail thresholds.",
        "Ratios are computed from per-mode trial medians on this runner.",
        "",
        "## End-to-end tracing",
        "",
        *summarize_trace(trace),
        "",
        "## Binary cost encoding",
        "",
        *summarize_cost_encoding(cost_encoding),
        "",
    ]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        sys.exit(2)
