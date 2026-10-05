#!/usr/bin/env python3
"""Decode ELTR binary traces (trace::binary_recorder, async_binary_recorder).

    scripts/eltr.py run.eltrace                      # JSON Lines on stdout
    scripts/eltr.py run.eltrace --events move_accepted,incumbent_updated
    scripts/eltr.py run.eltrace --format summary     # metadata, event counts, costs
    scripts/eltr.py run.eltrace --format stn         # search trajectory network
    scripts/eltr.py run.eltrace --format schema      # what the trace records

A trace describes itself (docs/tracing.md): its header gives the metadata of
the run, the layout of the costs and the fields of every core event, and an
application event that has a schema is described before its first record. So
the decoder needs no options to read any trace: a cost decodes to a number, or
to an object nested as its fields are named ("hard.0" is ["hard"][0]; levels
named 0, 1, ... become a list).

The JSON Lines output starts with a "trace" line (format version, metadata,
cost layout) and goes on with one line per record, with the field names of
trace::jsonl_recorder (and null for NaN and the infinities, as it writes them),
so the tools that read a JSONL trace read a decoded ELTR trace too. A record without a schema becomes {"event": "user" (or "unknown"
for a core tag), "tag": ..., "payload": hex}.

The summary lists the runs in order, each with its stage, stage index and
attempt when a solver emitted a run_context before it.

The STN output is the search trajectory network of the solution_visited
events (recorded when the problem has a solution hash): one node per distinct
hash, with its cost and number of visits, and one edge per consecutive pair of
visits within a run, with its count.

Standard library only: `uv run scripts/eltr.py` or `python3`. As a module,
`Trace(stream)` reads the header (`metadata`, `cost_fields`, `schemas`) and
iterating over it yields the decoded records one at a time.
"""

from __future__ import annotations

import argparse
import itertools
import json
import math
import struct
import sys
from collections import Counter
from typing import IO, Any, Iterator

MAGIC = b"ELTR"
VERSION = 1
SCHEMA_TAG = 0
FIRST_USER_TAG = 128

U8 = struct.Struct("<B")
U32 = struct.Struct("<I")
RECORD_HEADER = struct.Struct("<BI")

# binary_type: name and fixed layout (None for the sized types).
TYPES = {
    1: ("u8", struct.Struct("<B")),
    2: ("i8", struct.Struct("<b")),
    3: ("u16", struct.Struct("<H")),
    4: ("i16", struct.Struct("<h")),
    5: ("u32", struct.Struct("<I")),
    6: ("i32", struct.Struct("<i")),
    7: ("u64", struct.Struct("<Q")),
    8: ("i64", struct.Struct("<q")),
    9: ("f32", struct.Struct("<f")),
    10: ("f64", struct.Struct("<d")),
    11: ("bool", struct.Struct("<?")),
    12: ("string", None),
    13: ("bytes", None),
    14: ("route", None),
    15: ("cost", None),
}


class FormatError(Exception):
    """The stream is not a valid ELTR trace."""


class Reader:
    """A cursor on a block of bytes: a header or a record's payload."""

    def __init__(self, data: bytes, what: str):
        self.data = data
        self.offset = 0
        self.what = what

    def take(self, layout: struct.Struct) -> tuple[Any, ...]:
        if self.offset + layout.size > len(self.data):
            raise FormatError(f"{self.what} is shorter than its fields")
        values = layout.unpack_from(self.data, self.offset)
        self.offset += layout.size
        return values

    def raw(self, size: int) -> bytes:
        if self.offset + size > len(self.data):
            raise FormatError(f"{self.what} is shorter than its fields")
        value = self.data[self.offset : self.offset + size]
        self.offset += size
        return value

    def u8(self) -> int:
        return self.take(U8)[0]

    def u32(self) -> int:
        return self.take(U32)[0]

    def string(self) -> str:
        try:
            return self.raw(self.u32()).decode("utf-8")
        except UnicodeDecodeError:
            raise FormatError(f"{self.what} has a string that is not UTF-8") from None

    def fields(self) -> list[tuple[str, int]]:
        result = []
        for _ in range(self.u32()):
            name = self.string()
            kind = self.u8()
            if kind not in TYPES:
                raise FormatError(f"{self.what}: unknown field type {kind}")
            result.append((name, kind))
        return result

    def schema(self) -> tuple[int, str, list[tuple[str, int]]]:
        tag = self.u8()
        return tag, self.string(), self.fields()

    def value(self, kind: int, cost_fields: list[tuple[str, int]]) -> Any:
        name, layout = TYPES[kind]
        if layout is not None:
            value = self.take(layout)[0]
            # NaN and the infinities as null, as trace::jsonl_recorder writes them.
            if isinstance(value, float) and not math.isfinite(value):
                return None
            return value
        if name == "string":
            return self.string()
        if name == "bytes":
            return self.raw(self.u32()).hex()
        if name == "route":
            return list(self.take(struct.Struct(f"<{self.u32()}I")))
        return nest([(field, self.value(field_kind, [])) for field, field_kind in cost_fields])

    def finished(self) -> bool:
        return self.offset == len(self.data)


def nest(values: list[tuple[str, Any]]) -> Any:
    """A cost's fields as a value: a number, or objects and lists by name."""
    if len(values) == 1 and values[0][0] == "":
        return values[0][1]
    root: dict[str, Any] = {}
    for path, value in values:
        node = root
        *parents, leaf = path.split(".")
        for part in parents:
            node = node.setdefault(part, {})
        node[leaf] = value
    return listify(root)


def listify(node: Any) -> Any:
    if not isinstance(node, dict):
        return node
    items = {key: listify(value) for key, value in node.items()}
    if list(items) == [str(index) for index in range(len(items))]:
        return list(items.values())
    return items


def type_name(kind: int) -> str:
    return TYPES[kind][0]


class Trace:
    """An ELTR stream: the header on construction, the records on iteration."""

    def __init__(self, stream: IO[bytes], allow_truncated: bool = False):
        self.stream = stream
        self.allow_truncated = allow_truncated
        start = stream.read(12) or b""
        if not start:
            raise FormatError("empty trace (the recorder wrote nothing)")
        if len(start) < 8 or start[:4] != MAGIC:
            raise FormatError("not an ELTR trace (no ELTR header)")
        (self.version,) = U32.unpack_from(start, 4)
        if self.version != VERSION:
            raise FormatError(
                f"ELTR version {self.version}; this decoder reads version {VERSION}"
            )
        if len(start) < 12:
            raise FormatError("truncated header")
        (size,) = U32.unpack_from(start, 8)
        data = stream.read(size) or b""
        if len(data) < size:
            raise FormatError("truncated header")
        header = Reader(data, "the header")
        self.metadata = {}
        for _ in range(header.u32()):
            key = header.string()
            self.metadata[key] = header.string()
        self.cost_fields = header.fields()
        self.schemas: dict[int, tuple[str, list[tuple[str, int]]]] = {}
        for _ in range(header.u32()):
            tag, name, fields = header.schema()
            self.schemas[tag] = (name, fields)
        self.offset = 12 + size

    def describe(self) -> dict[str, Any]:
        """The trace line of the JSONL output."""
        return {
            "event": "trace",
            "version": self.version,
            "metadata": self.metadata,
            "cost": [{"name": name, "type": type_name(kind)} for name, kind in self.cost_fields],
        }

    def schema_listing(self) -> dict[str, Any]:
        return {
            **{key: value for key, value in self.describe().items() if key != "event"},
            "events": [
                {
                    "tag": tag,
                    "name": name,
                    "fields": [
                        {"name": field, "type": type_name(kind)} for field, kind in fields
                    ],
                }
                for tag, (name, fields) in sorted(self.schemas.items())
            ],
        }

    def __iter__(self) -> Iterator[dict[str, Any]]:
        while True:
            record_header = self.stream.read(RECORD_HEADER.size) or b""
            if not record_header:
                return
            payload = b""
            size = 0
            if len(record_header) == RECORD_HEADER.size:
                tag, size = RECORD_HEADER.unpack(record_header)
                payload = self.stream.read(size) or b""
            if len(record_header) < RECORD_HEADER.size or len(payload) < size:
                message = f"truncated record at byte {self.offset}"
                if self.allow_truncated:
                    print(f"eltr: {message}; stopping there", file=sys.stderr)
                    return
                raise FormatError(message)

            what = f"the record at byte {self.offset}"
            self.offset += RECORD_HEADER.size + size
            reader = Reader(payload, what)
            if tag == SCHEMA_TAG:
                described, name, fields = reader.schema()
                self.schemas[described] = (name, fields)
                continue
            if tag not in self.schemas:
                kind = "user" if tag >= FIRST_USER_TAG else "unknown"
                yield {"event": kind, "tag": tag, "payload": payload.hex()}
                continue
            name, fields = self.schemas[tag]
            record = {"event": name}
            for field, kind in fields:
                record[field] = reader.value(kind, self.cost_fields)
            if not reader.finished():
                raise FormatError(f"{what} ({name}) is longer than its fields")
            yield record


def summary(trace: Trace) -> dict[str, Any]:
    counts: Counter[str] = Counter()
    runs: list[dict[str, Any]] = []
    solutions = set()
    context: dict[str, Any] = {}
    for record in trace:
        name = record["event"]
        counts[name if name not in ("user", "unknown") else f"{name}:{record['tag']}"] += 1
        if name == "run_context":
            context = {key: record[key] for key in ("stage", "stage_index", "attempt")}
        elif name == "run_started":
            runs.append({**context, "initial_cost": record["cost"]})
            context = {}
        elif name == "run_finished":
            if not runs or "final_cost" in runs[-1]:
                runs.append({})
            runs[-1].update(
                final_cost=record["cost"],
                evaluations=record["evaluations"],
                iterations=record["iterations"],
            )
            if "termination" in record:
                runs[-1]["termination"] = record["termination"]
        elif name == "solution_visited":
            solutions.add(record["hash"])
    result: dict[str, Any] = {
        "metadata": trace.metadata,
        "events": dict(counts),
        "runs": runs,
    }
    if solutions:
        result["distinct_solutions"] = len(solutions)
    return result


def search_trajectory_network(trace: Trace) -> dict[str, Any]:
    nodes: dict[int, dict[str, Any]] = {}
    edges: Counter[tuple[int, int]] = Counter()
    previous = None
    for record in trace:
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
        "metadata": trace.metadata,
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
        "--format",
        choices=["jsonl", "summary", "stn", "schema"],
        default="jsonl",
        help="JSON Lines records (default), a summary, a search trajectory network "
        "or the trace's schema",
    )
    parser.add_argument(
        "--events",
        help="comma-separated event names to keep in the JSONL output "
        "(trace for the first line, user for records without a schema)",
    )
    parser.add_argument(
        "--allow-truncated",
        action="store_true",
        help="stop at a truncated last record (an interrupted run) instead of failing",
    )
    parser.add_argument("-o", "--output", help="the output file (default: standard output)")
    args = parser.parse_args(argv)
    keep = {name.strip() for name in args.events.split(",")} if args.events else None

    stream: IO[bytes] = sys.stdin.buffer
    output: IO[str] = sys.stdout
    try:
        if args.trace != "-":
            stream = open(args.trace, "rb")
        if args.output:
            output = open(args.output, "w", encoding="utf-8")
        trace = Trace(stream, allow_truncated=args.allow_truncated)
        if args.format == "jsonl":
            for record in itertools.chain([trace.describe()], trace):
                if keep is None or record["event"] in keep:
                    output.write(
                        json.dumps(record, separators=(",", ":"), allow_nan=False) + "\n"
                    )
        else:
            result = {
                "summary": summary,
                "stn": search_trajectory_network,
                "schema": Trace.schema_listing,
            }[args.format](trace)
            json.dump(result, output, indent=2, allow_nan=False)
            output.write("\n")
    except FormatError as error:
        print(f"eltr: {args.trace}: {error}", file=sys.stderr)
        return 1
    except BrokenPipeError:
        return 0
    except OSError as error:
        print(f"eltr: {error.filename or args.trace}: {error.strerror or error}", file=sys.stderr)
        return 1
    finally:
        if stream is not sys.stdin.buffer:
            stream.close()
        if output is not sys.stdout:
            output.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
