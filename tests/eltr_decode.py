"""The ELTR decoder (scripts/eltr.py) against the JSONL recorder.

CTest runs it as `easylocal.trace.eltr-decode` with the fixture that writes the
same events in both formats; by hand:

    python3 tests/eltr_decode.py build/<preset>/tests/easylocal_eltr_fixture

Standard library only (unittest), like the decoder.
"""

import io
import json
import math
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "scripts"))

import eltr  # noqa: E402

FIXTURE = None

CORE_EVENTS = {
    "run_started",
    "move_evaluated",
    "move_accepted",
    "incumbent_updated",
    "local_optimum",
    "neighborhood_selection",
    "run_finished",
    "solution_visited",
    "aspiration_applied",
    "tabu_escape",
    "tabu_tenure_changed",
    "run_context",
    "temperature_changed",
}


def decode(path, **options):
    with open(path, "rb") as stream:
        return list(eltr.Trace(stream, **options))


def string(text):
    data = text.encode()
    return struct.pack("<I", len(data)) + data


def trace(cost_fields=(("", 8),), schemas=(), records=()):
    """An ELTR stream with the given cost layout, schemas and records."""

    def fields(items):
        return struct.pack("<I", len(items)) + b"".join(
            string(name) + struct.pack("<B", kind) for name, kind in items
        )

    header = struct.pack("<I", 0) + fields(cost_fields) + struct.pack("<I", len(schemas))
    for tag, name, items in schemas:
        header += struct.pack("<B", tag) + string(name) + fields(items)
    body = b"".join(struct.pack("<BI", tag, len(payload)) + payload for tag, payload in records)
    return io.BytesIO(b"ELTR" + struct.pack("<II", 1, len(header)) + header + body)


def run_cli(*arguments):
    return subprocess.run(
        [sys.executable, eltr.__file__, *map(str, arguments)],
        capture_output=True,
        text=True,
    )


class FixtureTraces(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.path = pathlib.Path(cls.directory.name)
        subprocess.run([FIXTURE, cls.path], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def expected(self, name):
        """The records of a JSONL trace, after its header line."""
        lines = [json.loads(line) for line in (self.path / name).read_text().splitlines()]
        self.assertEqual(lines[0]["event"], "trace")
        return lines[1:]

    def test_the_jsonl_header_line_is_the_decoder_trace_line(self):
        header = json.loads((self.path / "integral.jsonl").read_text().splitlines()[0])
        with open(self.path / "integral.eltr", "rb") as stream:
            decoded = eltr.Trace(stream).describe()
        # The cost layout is ELTR's own: JSON costs describe themselves.
        del decoded["cost"]
        self.assertEqual(header, decoded)

    def test_the_header_describes_the_trace(self):
        with open(self.path / "integral.eltr", "rb") as stream:
            header = eltr.Trace(stream)
            self.assertEqual(header.metadata, {"instance": "fixture", "runner": "none"})
            self.assertEqual(header.cost_fields, [("", 8)])
            self.assertEqual({name for name, _ in header.schemas.values()}, CORE_EVENTS)

    def test_every_core_event_decodes_like_the_jsonl_recorder(self):
        decoded = decode(self.path / "integral.eltr")
        core = [record for record in decoded if record["event"] in CORE_EVENTS]
        expected = self.expected("integral.jsonl")
        # Exactly: both write the shortest text of each double, NaN as null.
        self.assertEqual(core, expected)
        self.assertEqual({record["event"] for record in core}, CORE_EVENTS)

    def test_application_events_by_their_schema_or_raw(self):
        decoded = decode(self.path / "integral.eltr")
        described = [r for r in decoded if r["event"] == "weight_changed"]
        self.assertEqual(
            described, 2 * [{"event": "weight_changed", "iteration": 3, "weight": 0.5}]
        )
        raw = [r for r in decoded if r["event"] == "user"]
        self.assertEqual(raw, 2 * [{"event": "user", "tag": 132, "payload": "0201"}])

    def test_a_structured_cost_decodes_by_its_field_names(self):
        self.assertEqual(decode(self.path / "structured.eltr"), self.expected("structured.jsonl"))

    def test_the_default_layout_of_a_hierarchical_cost(self):
        with open(self.path / "hierarchical.eltr", "rb") as stream:
            decoded = eltr.Trace(stream)
            self.assertEqual(decoded.cost_fields, [("hard.0", 8), ("hard.1", 8), ("soft", 10)])
            self.assertEqual(
                list(decoded), [{"event": "run_started", "cost": {"hard": [1, 2], "soft": 0.5}}]
            )

    def test_the_default_json_writer_writes_costs_as_they_decode(self):
        self.assertEqual(
            decode(self.path / "hierarchical.eltr"), self.expected("hierarchical.jsonl")
        )

    def test_timestamps_end_the_core_events(self):
        with open(self.path / "timed.eltr", "rb") as stream:
            timed = eltr.Trace(stream)
            for name, fields in timed.schemas.values():
                self.assertEqual(fields[-1], ("elapsed_ns", 7), name)
            records = list(timed)
        self.assertEqual(
            [record["event"] for record in records],
            ["run_context", "run_started", "weight_changed", "run_finished"],
        )
        self.assertNotIn("elapsed_ns", records[2])
        core = [records[index]["elapsed_ns"] for index in (0, 1, 3)]
        self.assertEqual(core, sorted(core))
        json_lines = self.expected("timed.jsonl")
        self.assertEqual(
            [list(line)[-1] for line in json_lines], 3 * ["elapsed_ns"]
        )

    def test_a_truncated_trace_fails_unless_allowed(self):
        data = (self.path / "integral.eltr").read_bytes()
        truncated = self.path / "truncated.eltr"
        truncated.write_bytes(data[:-3])
        with self.assertRaisesRegex(eltr.FormatError, "truncated record"):
            decode(truncated)
        complete = decode(self.path / "integral.eltr")
        self.assertEqual(decode(truncated, allow_truncated=True), complete[:-1])

    def test_summary(self):
        with open(self.path / "integral.eltr", "rb") as stream:
            result = eltr.summary(eltr.Trace(stream))
        self.assertEqual(result["metadata"]["instance"], "fixture")
        self.assertEqual(result["events"]["solution_visited"], 6)
        self.assertEqual(result["events"]["user:132"], 2)
        self.assertEqual(
            result["runs"],
            [
                {
                    "stage": 'anneal "hot"',
                    "stage_index": 1,
                    "attempt": 0,
                    "initial_cost": 40,
                    "final_cost": -7,
                    "evaluations": 4,
                    "iterations": 3,
                    "termination": "completed",
                },
                {
                    "stage": 'anneal "hot"',
                    "stage_index": 1,
                    "attempt": 1,
                    "initial_cost": 41,
                    "final_cost": -7,
                    "evaluations": 4,
                    "iterations": 3,
                    "termination": "completed",
                },
            ],
        )
        self.assertEqual(result["distinct_solutions"], 2)

    def test_a_pipeline_solve_decodes_like_the_jsonl_recorder(self):
        decoded = decode(self.path / "pipeline.eltr")
        self.assertEqual(decoded, self.expected("pipeline.jsonl"))
        self.assertEqual([record["event"] for record in decoded].count("run_context"), 3)

    def test_the_summary_of_a_pipeline_solve(self):
        with open(self.path / "pipeline.eltr", "rb") as stream:
            result = eltr.summary(eltr.Trace(stream))

        def run(stage, stage_index, attempt, initial_cost, final_cost, termination):
            return {
                "stage": stage,
                "stage_index": stage_index,
                "attempt": attempt,
                "initial_cost": initial_cost,
                "final_cost": final_cost,
                "evaluations": 4,
                "iterations": 3,
                "termination": termination,
            }

        self.assertEqual(
            result["runs"],
            [
                run("climb", 0, 0, 6, 3, "evaluation budget exhausted"),
                run("climb", 0, 1, 6, 3, "evaluation budget exhausted"),
                run("descend", 1, 0, 3, 0, "local optimum"),
            ],
        )

    def test_a_run_without_run_finished_is_unfinished(self):
        schemas = [
            (1, "run_started", [("cost", 15)]),
            (7, "run_finished", [("evaluations", 7), ("iterations", 7), ("cost", 15)]),
        ]
        started = (1, struct.pack("<q", 5))
        finished = (7, struct.pack("<QQq", 1, 1, 4))
        # The first run threw; the second finished; the third threw.
        records = [started, started, finished, started]
        result = eltr.summary(eltr.Trace(trace(schemas=schemas, records=records)))
        self.assertEqual(
            result["runs"],
            [
                {"initial_cost": 5, "unfinished": True},
                {"initial_cost": 5, "final_cost": 4, "evaluations": 1, "iterations": 1},
                {"initial_cost": 5, "unfinished": True},
            ],
        )

    def test_search_trajectory_network(self):
        with open(self.path / "integral.eltr", "rb") as stream:
            network = eltr.search_trajectory_network(eltr.Trace(stream))
        # Hashes are 16 hexadecimal digits, as jsonl_recorder writes them.
        start, local = "feedfacecafebeef", "0000000000000007"
        self.assertEqual(
            network["nodes"],
            [
                {"hash": start, "cost": 40, "visits": 4, "runs": [0, 1]},
                {"hash": local, "cost": -7, "visits": 2, "runs": [0, 1]},
            ],
        )
        # No edge joins the end of the first run to the start of the second.
        self.assertEqual(
            network["edges"],
            [
                {"source": start, "target": local, "count": 2, "runs": [0, 1]},
                {"source": local, "target": start, "count": 2, "runs": [0, 1]},
            ],
        )
        self.assertEqual(network["starts"], [{"run": 0, "hash": start}, {"run": 1, "hash": start}])
        self.assertEqual(network["ends"], [{"run": 0, "hash": start}, {"run": 1, "hash": start}])

    def test_a_visit_without_a_move_has_no_edge(self):
        class Records(list):
            metadata = {}

        def visit(hash, previous):
            return {"event": "solution_visited", "hash": hash, "cost": 1, "previous_hash": previous}

        a, b, c = "000000000000000a", "000000000000000b", "000000000000000c"
        network = eltr.search_trajectory_network(
            Records(
                [
                    {"event": "run_started", "cost": 1},
                    visit(a, eltr.NO_HASH),
                    # A sample of a population, then a move from the start.
                    visit(b, eltr.NO_HASH),
                    visit(c, a),
                ]
            )
        )
        self.assertEqual(network["edges"], [{"source": a, "target": c, "count": 1, "runs": [0]}])
        self.assertEqual(network["starts"], [{"run": 0, "hash": a}])
        self.assertEqual(network["ends"], [{"run": 0, "hash": c}])

    def run_cli(self, *arguments):
        return subprocess.run(
            [sys.executable, eltr.__file__, *map(str, arguments)],
            capture_output=True,
            text=True,
        )

    def test_command_line(self):
        result = self.run_cli(self.path / "integral.eltr", "--events", "trace,run_finished")
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = [json.loads(line) for line in result.stdout.splitlines()]
        self.assertEqual(
            lines[0],
            {
                "event": "trace",
                "version": 1,
                "metadata": {"instance": "fixture", "runner": "none"},
                "cost": [{"name": "", "type": "i64"}],
            },
        )
        self.assertEqual(
            lines[1:],
            2
            * [
                {
                    "event": "run_finished",
                    "evaluations": 4,
                    "iterations": 3,
                    "cost": -7,
                    "termination": "completed",
                }
            ],
        )

        schema = json.loads(self.run_cli(self.path / "integral.eltr", "--format", "schema").stdout)
        tenure = next(event for event in schema["events"] if event["tag"] == 11)
        self.assertEqual(tenure["name"], "tabu_tenure_changed")
        self.assertEqual(
            [field["name"] for field in tenure["fields"]],
            ["evaluations", "iterations", "previous_tenure", "tenure"],
        )

        (self.path / "garbage.eltr").write_bytes(b"not a trace")
        failed = self.run_cli(self.path / "garbage.eltr")
        self.assertEqual(failed.returncode, 1)
        self.assertIn("no ELTR header", failed.stderr)


class HandcraftedTraces(unittest.TestCase):
    def test_header_and_version_are_checked(self):
        with self.assertRaisesRegex(eltr.FormatError, "no ELTR header"):
            eltr.Trace(io.BytesIO(b"JSON\x01\x00\x00\x00"))
        with self.assertRaisesRegex(eltr.FormatError, "version 2"):
            eltr.Trace(io.BytesIO(b"ELTR\x02\x00\x00\x00"))
        with self.assertRaisesRegex(eltr.FormatError, "truncated header"):
            eltr.Trace(io.BytesIO(b"ELTR\x01\x00\x00\x00\x10\x00\x00\x00"))

    def test_an_empty_file_is_an_empty_trace(self):
        with self.assertRaisesRegex(eltr.FormatError, "empty trace"):
            eltr.Trace(io.BytesIO(b""))

    def test_a_trace_without_records(self):
        self.assertEqual(list(eltr.Trace(trace())), [])

    def test_a_record_must_match_its_schema(self):
        schemas = [(1, "run_started", [("cost", 15)])]
        with self.assertRaisesRegex(eltr.FormatError, "shorter than its fields"):
            list(eltr.Trace(trace(schemas=schemas, records=[(1, b"\x01")])))
        with self.assertRaisesRegex(eltr.FormatError, "longer than its fields"):
            list(eltr.Trace(trace(schemas=schemas, records=[(1, bytes(9))])))

    def test_a_schema_record_describes_the_records_after_it(self):
        schema = struct.pack("<B", 200) + string("ping") + struct.pack("<I", 1)
        schema += string("label") + struct.pack("<B", 12)
        records = [(200, b"\x00"), (0, schema), (200, string("hi"))]
        self.assertEqual(
            list(eltr.Trace(trace(records=records))),
            [
                {"event": "user", "tag": 200, "payload": "00"},
                {"event": "ping", "label": "hi"},
            ],
        )

    def test_unknown_core_tags_are_kept_raw(self):
        self.assertEqual(
            list(eltr.Trace(trace(records=[(42, b"\x01\x02")]))),
            [{"event": "unknown", "tag": 42, "payload": "0102"}],
        )

    def test_non_finite_numbers_are_null(self):
        schemas = [(1, "run_started", [("cost", 15)])]
        records = [(1, struct.pack("<d", value)) for value in (math.inf, -math.inf, math.nan)]
        stream = trace(cost_fields=(("", 10),), schemas=schemas, records=records)
        self.assertEqual(
            list(eltr.Trace(stream)), 3 * [{"event": "run_started", "cost": None}]
        )
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "non-finite.eltr"
            stream.seek(0)
            path.write_bytes(stream.read())
            result = run_cli(path)
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        self.assertEqual(lines[1:], 3 * ['{"event":"run_started","cost":null}'])

    def test_a_string_that_is_not_utf8_is_a_format_error(self):
        schemas = [(1, "run_finished", [("termination", 12)])]
        records = [(1, struct.pack("<I", 2) + b"\xff\xfe")]
        with self.assertRaisesRegex(eltr.FormatError, "not UTF-8"):
            list(eltr.Trace(trace(schemas=schemas, records=records)))

    def test_a_file_that_cannot_be_read_is_reported_without_a_traceback(self):
        with tempfile.TemporaryDirectory() as directory:
            missing = run_cli(pathlib.Path(directory) / "missing.eltr")
            self.assertEqual(missing.returncode, 1)
            self.assertIn("missing.eltr", missing.stderr)
            self.assertNotIn("Traceback", missing.stderr)

            path = pathlib.Path(directory) / "corrupt.eltr"
            schemas = [(1, "run_finished", [("termination", 12)])]
            records = [(1, struct.pack("<I", 2) + b"\xff\xfe")]
            path.write_bytes(trace(schemas=schemas, records=records).read())
            corrupt = run_cli(path)
            self.assertEqual(corrupt.returncode, 1)
            self.assertIn("not UTF-8", corrupt.stderr)
            self.assertNotIn("Traceback", corrupt.stderr)

    def test_cost_values(self):
        self.assertEqual(eltr.nest([("", 3)]), 3)
        self.assertEqual(eltr.nest([("0", 1), ("1", 2)]), [1, 2])
        self.assertEqual(
            eltr.nest([("hard", 1), ("soft.0", 2), ("soft.1", 3)]),
            {"hard": 1, "soft": [2, 3]},
        )


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("usage: eltr_decode.py <easylocal_eltr_fixture> [unittest options]")
    FIXTURE = sys.argv.pop(1)
    unittest.main()
