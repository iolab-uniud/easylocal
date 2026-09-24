#!/usr/bin/env python3
"""Generate a deterministic Markdown summary from neighborhood benchmark CSVs."""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path
import statistics
import sys


EXPECTED_HEADERS = {
    "metadata.csv": ["key", "value"],
    "traversal.csv": [
        "domain", "workload", "variant", "neighborhood_size", "trial",
        "repetitions", "measured_moves", "ns_per_move", "checksum",
    ],
    "search.csv": [
        "domain", "algorithm", "variant", "trial", "repetitions",
        "evaluations_per_run", "measured_evaluations", "allocations_per_run",
        "allocated_bytes_per_run", "ns_per_evaluation", "ns_per_run",
        "termination", "checksum",
    ],
}


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        expected = EXPECTED_HEADERS[path.name]
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


def escape(text: str) -> str:
    return text.replace("|", "\\|").replace("\n", " ")


def format_number(value: float) -> str:
    if value >= 1000.0:
        return f"{value:.1f}"
    if value >= 100.0:
        return f"{value:.2f}"
    return f"{value:.3f}"


def ratio_text(value: float, baseline: float | None) -> str:
    if baseline is None or baseline == 0.0:
        return "n/a"
    return f"{value / baseline:.3f}x"


def markdown_table(headers: list[str], rows: list[list[str]]) -> list[str]:
    lines = [
        "| " + " | ".join(headers) + " |",
        "| " + " | ".join("---" for _ in headers) + " |",
    ]
    lines.extend(
        "| " + " | ".join(escape(cell) for cell in row) + " |"
        for row in rows
    )
    return lines


def summarize_traversal(rows: list[dict[str, str]]) -> list[str]:
    groups: dict[tuple[str, str, str], list[float]] = defaultdict(list)
    sizes: dict[tuple[str, str], str] = {}
    for row in rows:
        key = (row["domain"], row["workload"], row["variant"])
        groups[key].append(float(row["ns_per_move"]))
        sizes[(row["domain"], row["workload"])] = row["neighborhood_size"]

    medians = {key: statistics.median(values) for key, values in groups.items()}
    table_rows: list[list[str]] = []
    for key in sorted(groups):
        domain, workload, variant = key
        values = groups[key]
        median = medians[key]
        baseline = medians.get((domain, workload, "cursor"))
        table_rows.append([
            domain,
            workload,
            variant,
            sizes[(domain, workload)],
            str(len(values)),
            format_number(median),
            format_number(min(values)),
            format_number(max(values)),
            ratio_text(median, baseline),
        ])

    return markdown_table(
        [
            "Domain", "Workload", "Variant", "Moves/scan", "Trials",
            "Median ns/move", "Min", "Max", "vs cursor",
        ],
        table_rows,
    )


def summarize_search(rows: list[dict[str, str]]) -> list[str]:
    groups: dict[tuple[str, str, str], list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        groups[(row["domain"], row["algorithm"], row["variant"])].append(row)

    medians = {
        key: statistics.median(float(row["ns_per_evaluation"]) for row in values)
        for key, values in groups.items()
    }

    table_rows: list[list[str]] = []
    for key in sorted(groups):
        domain, algorithm, variant = key
        values = groups[key]
        ns_eval = medians[key]
        ns_run = statistics.median(float(row["ns_per_run"]) for row in values)
        baseline = medians.get((domain, algorithm, "raw-cursor"))
        table_rows.append([
            domain,
            algorithm,
            variant,
            str(len(values)),
            format_number(ns_eval),
            format_number(ns_run),
            "/".join(sorted({row["evaluations_per_run"] for row in values})),
            "/".join(sorted({row["allocations_per_run"] for row in values})),
            "/".join(sorted({row["allocated_bytes_per_run"] for row in values})),
            "/".join(sorted({row["termination"] for row in values})),
            ratio_text(ns_eval, baseline),
        ])

    return markdown_table(
        [
            "Domain", "Algorithm", "Variant", "Trials", "Median ns/eval",
            "Median ns/run", "Eval/run", "Alloc/run", "Bytes/run",
            "Termination", "vs raw cursor",
        ],
        table_rows,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("results_dir", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    results_dir = args.results_dir
    output = args.output or results_dir / "summary.md"
    data = {name: read_csv(results_dir / name) for name in EXPECTED_HEADERS}

    raw_metadata = data["metadata.csv"]
    metadata = {row["key"]: row["value"] for row in raw_metadata}
    if len(metadata) != len(raw_metadata):
        raise ValueError("metadata.csv: duplicate metadata key")
    if metadata.get("schema_version") != "1":
        raise ValueError(
            "metadata.csv: unsupported schema_version "
            f"{metadata.get('schema_version', 'missing')}"
        )

    metadata_keys = [
        "toolchain", "os", "os_description", "os_release", "architecture",
        "runner_os", "runner_arch", "hardware_model", "logical_cpus",
        "compiler_version", "compiler_target", "stdlib", "stdlib_version",
        "cxxflags", "cmake", "ninja", "generator", "build_type",
        "cpp_standard", "sdkroot", "xcode", "target_work", "trials", "seed",
        "git_commit", "git_dirty",
    ]
    metadata_rows = [[key, metadata.get(key, "unknown")] for key in metadata_keys]

    lines = [
        "# Neighborhood traversal benchmark summary",
        "",
        "Performance values are diagnostic measurements, not pass/fail thresholds.",
        "Ratios are computed from per-case trial medians.",
        "",
        "## Run metadata",
        "",
        *markdown_table(["Key", "Value"], metadata_rows),
        "",
        "## Deterministic traversal",
        "",
        *summarize_traversal(data["traversal.csv"]),
        "",
        "## Runner-level search",
        "",
        *summarize_search(data["search.csv"]),
        "",
    ]

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        sys.exit(2)
