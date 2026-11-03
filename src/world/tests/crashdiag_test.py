"""Exercise real diagnostics in child processes; fatal errors never touch the game."""
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import unittest

PROBE = Path(sys.argv.pop(1)).resolve()


class CrashDiagnosticsTests(unittest.TestCase):
    def run_probe(self, mode):
        work = Path(tempfile.mkdtemp(prefix=f"diag-{mode}-", dir=PROBE.parent))
        (work / "OpenNeoUA.ini").write_text("game.crash_diagnostics = yes\n")
        (work / "Nucleus.ini").write_text("game.debug = no\n")
        result = subprocess.run([str(PROBE), mode], cwd=work, timeout=25,
                                capture_output=True)
        sessions = list((work / "env" / "Diagnostics").glob("*"))
        return result, sessions

    def test_disabled_creates_no_session(self):
        result, sessions = self.run_probe("disabled")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(sessions, [])

    def test_normal_session_has_persistent_history_and_configuration(self):
        result, sessions = self.run_probe("normal")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(len(sessions), 1)
        session = sessions[0]
        events = (session / "Events.log").read_text()
        self.assertIn("event=2199", events)
        self.assertIn("event=0\n", events)
        self.assertIn("[truncated]", events)
        state = (session / "LatestState.txt").read_text()
        self.assertIn("unit_count=42", state)
        self.assertIn("active_gid=101", state)
        self.assertIn("active_vehicle_id=149", state)
        self.assertIn("active_position=10.000|-20.000|30.000", state)
        self.assertIn("private_bytes=", state)
        self.assertTrue((session / "SessionEnd.txt").exists())
        self.assertIn("probe_checkpoint", next(session.glob("Checkpoint_*.txt")).read_text())
        self.assertEqual((session / "OpenNeoUA.ini").read_text(),
                         "game.crash_diagnostics = yes\n")

    def test_forced_exit_preserves_evidence_without_a_crash_handler(self):
        result, sessions = self.run_probe("forced")
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((sessions[0] / "SessionEnd.txt").exists())
        self.assertIn("event=2199", (sessions[0] / "Events.log").read_text())
        self.assertIn("level_id=17", (sessions[0] / "LatestState.txt").read_text())

    def test_access_violation_has_report_and_valid_minidump(self):
        self.check_crash("av", "0xc0000005")

    def test_abort_has_report_and_valid_minidump(self):
        self.check_crash("abort", "fatal_reason=SIGABRT")

    def test_terminate_has_report_and_valid_minidump(self):
        self.check_crash("terminate", "fatal_reason=std::terminate")

    def check_crash(self, mode, evidence):
        result, sessions = self.run_probe(mode)
        self.assertNotEqual(result.returncode, 0)
        report = (sessions[0] / "CrashReport.txt").read_text()
        self.assertTrue(evidence in report, f"missing {evidence} in {sessions[0]}")
        self.assertIn("phase=ProbeSimulation", report)
        self.assertIn("dump_writer_exit=0", report)
        self.assertIn("[Stack trace]", report)
        dump = (sessions[0] / "CrashDump.dmp").read_bytes()
        self.assertEqual(dump[:4], b"MDMP")
        count, directory = struct.unpack_from("<II", dump, 8)
        streams = {kind: (size, offset) for kind, size, offset in
                   (struct.unpack_from("<III", dump, directory + index * 12)
                    for index in range(count))}
        self.assertIn(6, streams, "minidump must retain the actual exception stream")
        exception_code = struct.unpack_from("<I", dump, streams[6][1] + 8)[0]
        self.assertEqual(exception_code, 0xC0000005 if mode == "av" else 0xE0000001)

    def test_journal_rotates_without_unbounded_growth(self):
        result, sessions = self.run_probe("rotation")
        self.assertEqual(result.returncode, 0)
        current = sessions[0] / "Events.log"
        previous = sessions[0] / "Events.previous.log"
        self.assertTrue(previous.exists())
        self.assertLessEqual(current.stat().st_size, 16 * 1024 * 1024 + 512)
        self.assertLessEqual(previous.stat().st_size, 16 * 1024 * 1024 + 512)
        self.assertIn("event=59999", current.read_text())

    def test_watchdog_records_stall(self):
        result, sessions = self.run_probe("hang")
        self.assertEqual(result.returncode, 0)
        self.assertTrue(list(sessions[0].glob("HangReport_*.txt")))

    def test_dump_writer_error_is_recorded(self):
        result, sessions = self.run_probe("writer_error")
        self.assertNotEqual(result.returncode, 0)
        report = (sessions[0] / "CrashReport.txt").read_text()
        self.assertIn("dump_writer_exit=1234", report)
        self.assertIn("exception_code=0xc0000005", report)

    def test_dump_writer_timeout_does_not_hang_the_game_forever(self):
        result, sessions = self.run_probe("writer_timeout")
        self.assertNotEqual(result.returncode, 0)
        report = (sessions[0] / "CrashReport.txt").read_text()
        self.assertIn("dump_writer_wait=258", report)
        self.assertIn("exception_code=0xc0000005", report)


if __name__ == "__main__":
    unittest.main(verbosity=2)
