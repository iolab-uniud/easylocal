#!/usr/bin/env python3
"""Decode ELTR binary traces (trace::binary_recorder, async_binary_recorder).

    scripts/eltr.py run.eltrace                      # JSON Lines on stdout
    scripts/eltr.py run.eltrace --cost f64           # a floating-point cost
    scripts/eltr.py run.eltrace --cost hard:i32,soft:i32
    scripts/eltr.py run.eltrace --events move_accepted,incumbent_updated
    scripts/eltr.py run.eltrace --format summary     # event counts, final cost
    scripts/eltr.py run.eltrace --format stn         # search trajectory network

The JSON Lines output has the field names of trace::jsonl_recorder, so the
tools that read a JSONL trace read a decoded ELTR trace too. An application
event (tags 128-255) becomes {"event": "user", "tag": ..., "payload": hex}.

A cost is written by the recorder's cost writer, so the decoder has to be told
its layout: --cost takes the field types in the order the writer writes them
(i8, u8, i16, u16, i32, u32, i64, u64, f32, f64, bool), optionally named. The
default, i64, matches the default writer for a signed integral cost; u64 and
f64 match unsigned and floating-point costs. One unnamed field decodes to a
number, several to a list, named fields to an object. A record whose payload does not
fit the layout fails the decoding, but a layout of the right size and the
wrong types (i64 for two i32 fields) decodes to wrong values.

The STN output is the search trajectory network of the solution_visited
events (recorded when the problem has a solution hash): one node per distinct
hash, with its cost and number of visits, and one edge per consecutive pair of
visits within a run, with its count.

Standard library only: `uv run scripts/eltr.py` or `python3`. As a module,
`records(stream, cost)` yields the decoded records one at a time.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from collections import Counter
from dataclasses import dataclass
from typing import IO, Any, Iterator

MAGIC = b"ELTR"
VERSION = 1

PRIMITIVES = {
    "i8": "b",
    "u8": "B",
    "i16": "h",
    "u16": "H",
    "i32": "i",
    "u32": "I",
    "i64": "q",
    "u64": "Q",
    "f32": "f",
    "f64": "d",
    "bool": "?",
}


class FormatError(Exception):
    """The stream is not a valid ELTR trace for the given cost layout."""


@dataclass(frozen=True)
class CostLayout:
    names: tuple[str | None, ...]
    format: struct.Struct

    @classmethod
    def parse(cls, spec: str) -> CostLayout:
        names: list[str | None] = []
        codes = []
        for field in spec.split(","):
            name, _, kind = field.strip().rpartition(":")
            if kind not in PRIMITIVES:
                known = ", ".join(PRIMITIVES)
                raise ValueError(f"unknown cost field type {kind!r} (known: {known})")
            names.append(name or None)
            codes.append(PRIMITIVES[kind])
        if any(names) and not all(names):
            raise ValueError("either every cost field has a name or none has")
        return cls(tuple(names), struct.Struct("<" + "".join(codes)))

    def decode(self, values: tuple[Any, ...]) -> Any:
        if self.names[0] is not None:
            return dict(zip(self.names, values))
        return values[0] if len(values) == 1 else list(values)


class Payload:
    """A cursor on one record's payload."""

    def __init__(self, data: bytes, cost: CostLayout):
        self.data = data
        self.offset = 0
        self.cost_layout = cost

    def take(self, layout: struct.Struct) -> tuple[Any, ...]:
        if self.offset + layout.size > len(self.data):
            raise FormatError("payload shorter than its event")
        values = layout.unpack_from(self.data, self.offset)
        self.offset += layout.size
        return values

    def u64(self) -> int:
        return self.take(U64)[0]

    def f64(self) -> float:
        return self.take(F64)[0]

    def boolean(self) -> bool:
        return self.take(U8)[0] != 0

    def cost(self) -> Any:
        return self.cost_layout.decode(self.take(self.cost_layout.format))

    def route(self) -> list[int]:
        (count,) = self.take(U32)
        return list(self.take(struct.Struct(f"<{count}I")))


U8 = struct.Struct("<B")
U32 = struct.Struct("<I")
U64 = struct.Struct("<Q")
F64 = struct.Struct("<d")
RECORD_HEADER = struct.Struct("<BI")


def counters(p: Payload) -> dict[str, Any]:
    return {"evaluations": p.u64(), "iterations": p.u64()}


# The core events by tag (docs/tracing.md), as builders of the JSONL fields.
CORE_EVENTS = {
    1: ("run_started", lambda p: {"cost": p.cost()}),
    2: (
        "move_evaluated",
        lambda p: {
            **counters(p),
            "current_cost": p.cost(),
            "candidate_cost": p.cost(),
            "neighborhood": p.route(),
        },
    ),
    3: (
        "move_accepted",
        lambda p: {
            **counters(p),
            "previous_cost": p.cost(),
            "cost": p.cost(),
            "neighborhood": p.route(),
        },
    ),
    4: (
        "incumbent_updated",
        lambda p: {**counters(p), "previous_cost": p.cost(), "cost": p.cost()},
    ),
    5: ("local_optimum", lambda p: {**counters(p), "cost": p.cost()}),
    6: (
        "neighborhood_selection",
        lambda p: {
            "attempt": p.u64(),
            "child": p.u64(),
            "bias": p.f64(),
            "active_bias_total": p.f64(),
            "conditional_probability": p.f64(),
            "produced_move": p.boolean(),
            "neighborhood": p.route(),
        },
    ),
    7: ("run_finished", lambda p: {**counters(p), "cost": p.cost()}),
    8: (
        "solution_visited",
        lambda p: {**counters(p), "hash": p.u64(), "cost": p.cost()},
    ),
    9: ("aspiration_applied", lambda p: {**counters(p), "cost": p.cost()}),
    10: ("tabu_escape", lambda p: {**counters(p), "moves": p.u64()}),
}

EVENT_NAMES = [name for name, _ in CORE_EVENTS.values()] + ["user", "unknown"]


def read_exactly(stream: IO[bytes], size: int) -> bytes:
    data = stream.read(size)
    return data if data is not None else b""


def records(
    stream: IO[bytes],
    cost: CostLayout | str = "i64",
    allow_truncated: bool = False,
) -> Iterator[dict[str, Any]]:
    """Yields the records of an ELTR stream as JSONL-shaped dictionaries."""
    layout = CostLayout.parse(cost) if isinstance(cost, str) else cost
    header = read_exactly(stream, 8)
    if len(header) < 8 or header[:4] != MAGIC:
        raise FormatError("not an ELTR trace (no ELTR header)")
    (version,) = U32.unpack_from(header, 4)
    if version != VERSION:
        raise FormatError(f"ELTR version {version}; this decoder reads version {VERSION}")

    offset = 8
    while True:
        record_header = read_exactly(stream, RECORD_HEADER.size)
        if not record_header:
            return
        tag = record_header[0]
        payload = b""
        if len(record_header) == RECORD_HEADER.size:
            (size,) = U32.unpack_from(record_header, 1)
            payload = read_exactly(stream, size)
        if len(record_header) < RECORD_HEADER.size or len(payload) < size:
            message = f"truncated record at byte {offset}"
            if allow_truncated:
                print(f"eltr: {message}; stopping there", file=sys.stderr)
                return
            raise FormatError(message)

        if tag in CORE_EVENTS:
            name, build = CORE_EVENTS[tag]
            cursor = Payload(payload, layout)
            try:
                fields = build(cursor)
            except FormatError:
                fields = None
            if fields is None or cursor.offset != len(payload):
                raise FormatError(
                    f"{name} record at byte {offset} has {len(payload)} payload bytes, "
                    f"which do not match a cost of {layout.format.size} bytes "
                    "(see --cost)"
                )
            yield {"event": name, **fields}
        elif tag >= 128:
            yield {"event": "user", "tag": tag, "payload": payload.hex()}
        else:
            # A core event of a later format revision.
            yield {"event": "unknown", "tag": tag, "payload": payload.hex()}
        offset += RECORD_HEADER.size + len(payload)


def summary(events: Iterator[dict[str, Any]]) -> dict[str, Any]:
    counts: Counter[str] = Counter()
    runs = []
    solutions = set()
    for record in events:
        name = record["event"]
        counts[name if name != "user" else f"user:{record['tag']}"] += 1
        if name == "run_started":
            runs.append({"initial_cost": record["cost"]})
        elif name == "run_finished":
            run = runs[-1] if runs and "final_cost" not in runs[-1] else {}
            if not run:
                runs.append(run)
            run.update(
                final_cost=record["cost"],
                evaluations=record["evaluations"],
                iterations=record["iterations"],
            )
        elif name == "solution_visited":
            solutions.add(record["hash"])
    result: dict[str, Any] = {"events": dict(counts), "runs": runs}
    if solutions:
        result["distinct_solutions"] = len(solutions)
    return result


def search_trajectory_network(events: Iterator[dict[str, Any]]) -> dict[str, Any]:
    nodes: dict[int, dict[str, Any]] = {}
    edges: Counter[tuple[int, int]] = Counter()
    previous = None
    for record in events:
        if record["event"] == "run_started":
            previous = None
        elif record["event"] == "solution_visited":
            node = nodes.setdefault(
                record["hash"], {"hash": record["hash"], "cost": record["cost"], "visits": 0}
            )
            node["visits"] += 1
            if previous is not None:
                edges[(previous, record["hash"])] += 1
            previous = record["hash"]
    return {
        "nodes": list(nodes.values()),
        "edges": [
            {"source": source, "target": target, "count": count}
            for (source, target), count in edges.items()
        ],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Decode an ELTR binary trace of EasyLocal.",
        epilog="See the module documentation (scripts/eltr.py) and docs/tracing.md.",
    )
    parser.add_argument("trace", help="the ELTR file, or - for standard input")
    parser.add_argument(
        "--cost",
        default="i64",
        help="the cost layout, e.g. i64 (default), f64 or hard:i32,soft:i32",
    )
    parser.add_argument(
        "--format",
        choices=["jsonl", "summary", "stn"],
        default="jsonl",
        help="JSON Lines records (default), a summary or a search trajectory network",
    )
    parser.add_argument(
        "--events",
        help="comma-separated event names to keep in the JSONL output",
    )
    parser.add_argument(
        "--allow-truncated",
        action="store_true",
        help="stop at a truncated last record (an interrupted run) instead of failing",
    )
    parser.add_argument("-o", "--output", help="the output file (default: standard output)")
    args = parser.parse_args(argv)

    try:
        layout = CostLayout.parse(args.cost)
    except ValueError as error:
        parser.error(str(error))
    keep = None
    if args.events:
        keep = {name.strip() for name in args.events.split(",")}
        unknown = keep.difference(EVENT_NAMES)
        if unknown:
            parser.error(f"unknown events: {', '.join(sorted(unknown))}")

    stream = sys.stdin.buffer if args.trace == "-" else open(args.trace, "rb")
    output = open(args.output, "w", encoding="utf-8") if args.output else sys.stdout
    try:
        events = records(stream, layout, allow_truncated=args.allow_truncated)
        if args.format == "jsonl":
            for record in events:
                if keep is None or record["event"] in keep:
                    output.write(json.dumps(record, separators=(",", ":")) + "\n")
        elif args.format == "summary":
            json.dump(summary(events), output, indent=2)
            output.write("\n")
        else:
            json.dump(search_trajectory_network(events), output, indent=2)
            output.write("\n")
    except FormatError as error:
        print(f"eltr: {args.trace}: {error}", file=sys.stderr)
        return 1
    except BrokenPipeError:
        return 0
    finally:
        if stream is not sys.stdin.buffer:
            stream.close()
        if output is not sys.stdout:
            output.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
