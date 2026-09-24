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
    "allocations.csv": [
        "domain", "variant", "allocations", "allocated_bytes", "moves", "checksum"
    ],
    "benchmark.csv": [
        "domain", "workload", "variant", "neighborhood_size", "trial",
        "repetitions", "measured_moves", "ns_per_move", "checksum"
    ],
    "runner-benchmark.csv": [
        "domain", "algorithm", "variant", "trial", "repetitions",
        "evaluations_per_run", "measured_evaluations", "allocations_per_run",
        "allocated_bytes_per_run", "ns_per_evaluation", "ns_per_run",
        "termination", "checksum"
    ],
    "first-improvement-diagnostic.csv": [
        "variant", "trial", "repetitions", "evaluations_per_run",
        "measured_evaluations", "ns_per_evaluation", "ns_per_run",
        "termination", "checksum"
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


def ratio_text(value: float, baseline: float) -> str:
    if baseline == 0.0:
        return "n/a"
    return f"{value / baseline:.3f}x"


def markdown_table(headers: list[str], rows: list[list[str]]) -> list[str]:
    lines = [
        "| " + " | ".join(headers) + " |",
        "| " + " | ".join("---" for _ in headers) + " |",
    ]
    lines.extend("| " + " | ".join(escape(cell) for cell in row) + " |" for row in rows)
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
        baseline = medians.get((domain, workload, "cursor"))
        median = medians[key]
        table_rows.append([
            domain,
            workload,
            variant,
            sizes[(domain, workload)],
            str(len(values)),
            format_number(median),
            format_number(min(values)),
            format_number(max(values)),
            ratio_text(median, baseline) if baseline is not None else "n/a",
        ])

    return markdown_table(
        [
            "Domain", "Workload", "Variant", "Moves/scan", "Trials",
            "Median ns/move", "Min", "Max", "vs cursor"
        ],
        table_rows,
    )


def summarize_allocations(rows: list[dict[str, str]]) -> list[str]:
    table_rows = [
        [
            row["domain"], row["variant"], row["allocations"],
            row["allocated_bytes"], row["moves"]
        ]
        for row in sorted(rows, key=lambda r: (r["domain"], r["variant"]))
    ]
    return markdown_table(
        ["Domain", "Variant", "Allocations", "Allocated bytes", "Moves"],
        table_rows,
    )


def summarize_runner(rows: list[dict[str, str]]) -> list[str]:
    groups: dict[tuple[str, str, str], list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        groups[(row["domain"], row["algorithm"], row["variant"])].append(row)

    medians: dict[tuple[str, str, str], float] = {}
    for key, values in groups.items():
        medians[key] = statistics.median(float(row["ns_per_evaluation"]) for row in values)

    table_rows: list[list[str]] = []
    for key in sorted(groups):
        domain, algorithm, variant = key
        values = groups[key]
        ns_eval = medians[key]
        ns_run = statistics.median(float(row["ns_per_run"]) for row in values)
        allocations = {row["allocations_per_run"] for row in values}
        bytes_ = {row["allocated_bytes_per_run"] for row in values}
        evaluations = {row["evaluations_per_run"] for row in values}
        terminations = {row["termination"] for row in values}
        baseline = medians.get((domain, algorithm, "raw-cursor"))
        table_rows.append([
            domain,
            algorithm,
            variant,
            str(len(values)),
            format_number(ns_eval),
            format_number(ns_run),
            "/".join(sorted(evaluations)),
            "/".join(sorted(allocations)),
            "/".join(sorted(bytes_)),
            "/".join(sorted(terminations)),
            ratio_text(ns_eval, baseline) if baseline is not None else "n/a",
        ])

    return markdown_table(
        [
            "Domain", "Algorithm", "Variant", "Trials", "Median ns/eval",
            "Median ns/run", "Eval/run", "Alloc/run", "Bytes/run",
            "Termination", "vs raw cursor"
        ],
        table_rows,
    )


def summarize_first_improvement_diagnostic(
    rows: list[dict[str, str]],
) -> list[str]:
    groups: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        groups[row["variant"]].append(row)

    medians = {
        variant: statistics.median(
            float(row["ns_per_evaluation"]) for row in values
        )
        for variant, values in groups.items()
    }
    baseline = medians.get("raw-cursor")

    preferred_order = [
        "raw-cursor",
        "range-current",
        "range-reference",
        "explicit-iterator",
        "range-deferred-accept",
        "range-reference-deferred",
    ]
    ordered_variants = [v for v in preferred_order if v in groups]
    ordered_variants.extend(sorted(set(groups) - set(ordered_variants)))

    table_rows: list[list[str]] = []
    for variant in ordered_variants:
        values = groups[variant]
        ns_eval_values = [float(row["ns_per_evaluation"]) for row in values]
        ns_run_values = [float(row["ns_per_run"]) for row in values]
        evaluations = {row["evaluations_per_run"] for row in values}
        terminations = {row["termination"] for row in values}
        median = statistics.median(ns_eval_values)
        table_rows.append([
            variant,
            str(len(values)),
            format_number(median),
            format_number(min(ns_eval_values)),
            format_number(max(ns_eval_values)),
            format_number(statistics.median(ns_run_values)),
            "/".join(sorted(evaluations)),
            "/".join(sorted(terminations)),
            ratio_text(median, baseline) if baseline is not None else "n/a",
        ])

    return markdown_table(
        [
            "Variant", "Trials", "Median ns/eval", "Min", "Max",
            "Median ns/run", "Eval/run", "Termination", "vs raw cursor"
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
    metadata_rows_raw = data["metadata.csv"]
    metadata = {row["key"]: row["value"] for row in metadata_rows_raw}
    if len(metadata) != len(metadata_rows_raw):
        raise ValueError("metadata.csv: duplicate metadata key")
    if metadata.get("schema_version") != "1":
        raise ValueError(
            f"metadata.csv: unsupported schema_version "
            f"{metadata.get('schema_version', 'missing')}"
        )

    metadata_keys = [
        "toolchain", "os", "os_description", "os_release", "architecture",
        "runner_os", "runner_arch", "hardware_model", "logical_cpus",
        "compiler_version",
        "compiler_target", "stdlib", "stdlib_version", "cxxflags", "cmake",
        "ninja", "generator", "build_type", "cpp_standard", "sdkroot",
        "xcode", "target_moves", "runner_target_evaluations", "trials",
        "seed", "git_commit", "git_dirty"
    ]
    metadata_rows = [[key, metadata.get(key, "unknown")] for key in metadata_keys]

    lines = [
        "# Neighborhood authoring benchmark summary",
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
        *summarize_traversal(data["benchmark.csv"]),
        "",
        "## Traversal allocations",
        "",
        *summarize_allocations(data["allocations.csv"]),
        "",
        "## Runner-level search",
        "",
        *summarize_runner(data["runner-benchmark.csv"]),
        "",
        "## Assignment First Improvement diagnostic",
        "",
        "This diagnostic isolates the observed Assignment First Improvement runner",
        "anomaly without changing the framework API. `range-current` is the",
        "production algorithm; the other range variants change one loop/lifetime",
        "property at a time.",
        "",
        *summarize_first_improvement_diagnostic(
            data["first-improvement-diagnostic.csv"]
        ),
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
