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


def same(decoded, expected):
    """Equal, with JSONL's six significant digits for floating-point fields."""
    if isinstance(expected, float) or isinstance(decoded, float):
        return math.isclose(decoded, expected, rel_tol=1e-5)
    if isinstance(expected, dict):
        return decoded.keys() == expected.keys() and all(
            same(decoded[key], expected[key]) for key in expected
        )
    if isinstance(expected, list):
        return len(decoded) == len(expected) and all(map(same, decoded, expected))
    return decoded == expected


def decode(path, cost="i64", **options):
    with open(path, "rb") as stream:
        return list(eltr.records(stream, cost, **options))


def trace(*records):
    """An ELTR stream of (tag, payload) records."""
    body = b"".join(struct.pack("<BI", tag, len(payload)) + payload for tag, payload in records)
    return io.BytesIO(b"ELTR" + struct.pack("<I", 1) + body)


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
        lines = (self.path / name).read_text().splitlines()
        return [json.loads(line) for line in lines]

    def test_every_core_event_decodes_like_the_jsonl_recorder(self):
        decoded = decode(self.path / "integral.eltr")
        core = [record for record in decoded if record["event"] != "user"]
        expected = self.expected("integral.jsonl")
        self.assertEqual(len(core), len(expected))
        for got, want in zip(core, expected):
            self.assertTrue(same(got, want), f"{got} != {want}")
        self.assertEqual(
            {record["event"] for record in core},
            set(eltr.EVENT_NAMES) - {"user", "unknown"},
        )

    def test_application_events_are_raw(self):
        users = [r for r in decode(self.path / "integral.eltr") if r["event"] == "user"]
        self.assertEqual(len(users), 2)
        self.assertEqual(users[0]["tag"], 131)
        self.assertEqual(
            struct.unpack("<Qd", bytes.fromhex(users[0]["payload"])), (3, 0.5)
        )

    def test_named_cost_fields_decode_to_an_object(self):
        decoded = decode(self.path / "structured.eltr", "hard:i32,soft:i32")
        self.assertEqual(decoded, self.expected("structured.jsonl"))

    def test_a_cost_layout_of_the_wrong_size_is_reported(self):
        # Only the size can be checked: hard:i32,soft:i32 fills an i64 too.
        with self.assertRaisesRegex(eltr.FormatError, "see --cost"):
            decode(self.path / "structured.eltr", "i32")

    def test_a_truncated_trace_fails_unless_allowed(self):
        data = (self.path / "integral.eltr").read_bytes()
        truncated = self.path / "truncated.eltr"
        truncated.write_bytes(data[:-3])
        with self.assertRaisesRegex(eltr.FormatError, "truncated record"):
            decode(truncated)
        complete = decode(self.path / "integral.eltr")
        self.assertEqual(decode(truncated, allow_truncated=True), complete[:-1])

    def test_summary(self):
        result = eltr.summary(iter(decode(self.path / "integral.eltr")))
        self.assertEqual(result["events"]["solution_visited"], 6)
        self.assertEqual(result["events"]["user:131"], 2)
        self.assertEqual(
            result["runs"],
            [
                {"initial_cost": 40, "final_cost": -7, "evaluations": 4, "iterations": 3},
                {"initial_cost": 41, "final_cost": -7, "evaluations": 4, "iterations": 3},
            ],
        )
        self.assertEqual(result["distinct_solutions"], 2)

    def test_search_trajectory_network(self):
        network = eltr.search_trajectory_network(iter(decode(self.path / "integral.eltr")))
        start, local = 0xFEEDFACECAFEBEEF, 7
        self.assertEqual(
            network["nodes"],
            [
                {"hash": start, "cost": 40, "visits": 4},
                {"hash": local, "cost": -7, "visits": 2},
            ],
        )
        # No edge joins the end of the first run to the start of the second.
        self.assertEqual(
            network["edges"],
            [
                {"source": start, "target": local, "count": 2},
                {"source": local, "target": start, "count": 2},
            ],
        )

    def test_command_line(self):
        result = subprocess.run(
            [
                sys.executable,
                eltr.__file__,
                self.path / "integral.eltr",
                "--events",
                "run_finished",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        lines = result.stdout.splitlines()
        self.assertEqual(len(lines), 2)
        self.assertEqual(
            json.loads(lines[0]),
            {"event": "run_finished", "evaluations": 4, "iterations": 3, "cost": -7},
        )
        failed = subprocess.run(
            [sys.executable, eltr.__file__, self.path / "integral.eltr", "--cost", "f32"],
            capture_output=True,
            text=True,
        )
        self.assertEqual(failed.returncode, 1)
        self.assertIn("see --cost", failed.stderr)


class HandcraftedTraces(unittest.TestCase):
    def test_header_and_version_are_checked(self):
        with self.assertRaisesRegex(eltr.FormatError, "no ELTR header"):
            list(eltr.records(io.BytesIO(b"JSON\x01\x00\x00\x00")))
        with self.assertRaisesRegex(eltr.FormatError, "version 2"):
            list(eltr.records(io.BytesIO(b"ELTR\x02\x00\x00\x00")))

    def test_an_empty_trace_has_no_records(self):
        self.assertEqual(list(eltr.records(trace())), [])

    def test_cost_layouts(self):
        payload = struct.pack("<d", 2.5)
        self.assertEqual(
            list(eltr.records(trace((1, payload)), "f64")),
            [{"event": "run_started", "cost": 2.5}],
        )
        payload = struct.pack("<iI", -1, 3)
        self.assertEqual(
            list(eltr.records(trace((1, payload)), "i32,u32")),
            [{"event": "run_started", "cost": [-1, 3]}],
        )
        with self.assertRaises(ValueError):
            eltr.CostLayout.parse("i128")
        with self.assertRaises(ValueError):
            eltr.CostLayout.parse("hard:i32,i32")

    def test_unknown_core_tags_are_kept_raw(self):
        self.assertEqual(
            list(eltr.records(trace((42, b"\x01\x02")))),
            [{"event": "unknown", "tag": 42, "payload": "0102"}],
        )


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("usage: eltr_decode.py <easylocal_eltr_fixture> [unittest options]")
    FIXTURE = sys.argv.pop(1)
    unittest.main()
