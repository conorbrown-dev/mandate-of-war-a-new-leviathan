"""Radio voice generator: no network. A fake transport stands in for ElevenLabs."""
import contextlib
import io
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import generate_radio_voice as tool

ROOT = Path(__file__).resolve().parents[2]
ENV = {"ELEVENLABS_API_KEY": "test-key", "ELEVENLABS_VOICE_ELITE_COMMAND": "voice-a",
       "ELEVENLABS_VOICE_MASS_COMMAND": "voice-b", "ELEVENLABS_VOICE_INDUSTRIAL_COMMAND": "voice-c"}


def tone(seconds=0.5):
    silence = b"\x00\x00" * 2400
    body = b"".join(struct.pack("<h", round(9000 * math.sin(2 * math.pi * 800 * n / 24000)))
                    for n in range(int(seconds * 24000)))
    return silence + body + silence


class GeneratorTests(unittest.TestCase):
    def setUp(self):
        self.folder = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.folder, True)
        shutil.copy(tool.RADIO_DIR / "radio_catalog.json", self.folder)
        self.calls = []

    def transport(self, text, config, api_key):
        self.calls.append((text, config["voice_id"], api_key))
        return tone()

    def run_tool(self, *args, env=ENV):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = tool.main(["--radio-dir", str(self.folder), *args], transport=self.transport, environ=env)
        return code, out.getvalue(), err.getvalue()

    def manifest(self):
        return json.loads((self.folder / "voice_manifest.json").read_text())

    def test_clip_ids_match_the_game(self):
        lines = tool.expand_lines(tool.load_catalog(self.folder / "radio_catalog.json"))
        ids = {line["id"] for line in lines}
        # The same ids RadioAnnouncer.clip_id builds (see test_radio_callouts.gd).
        self.assertIn("elite_command/objective.fuel_depot_secured.v1", ids)
        self.assertIn("elite_command/air.hostile_inbound.v1.ne", ids)
        self.assertEqual(len(lines), 3 * (8 + 1 + 1), "three voices: eight compass points plus two depot lines")
        north_east = next(line for line in lines if line["id"] == "mass_command/air.hostile_inbound.v1.ne")
        self.assertIn("from the northeast", north_east["text"])
        self.assertFalse(any("{" in line["text"] for line in lines))

    def test_parked_alerts_are_left_out_unless_asked_for(self):
        catalog = tool.load_catalog(self.folder / "radio_catalog.json")
        self.assertFalse(any(line["event"] == "naval.carrier_lost" for line in tool.expand_lines(catalog)))
        self.assertTrue(any(line["event"] == "naval.carrier_lost"
                            for line in tool.expand_lines(catalog, include_parked=True)))

    def test_a_spoken_bearing_is_refused(self):
        catalog = tool.load_catalog(self.folder / "radio_catalog.json")
        catalog["events"][0]["lines"]["elite_command"] = ["Bandits, bearing {bearing}."]
        with self.assertRaises(tool.GeneratorError):
            tool.expand_lines(catalog)

    def test_dry_run_sends_and_writes_nothing(self):
        code, out, _ = self.run_tool()
        self.assertEqual(code, 0)
        self.assertEqual(self.calls, [])
        self.assertIn("Dry run", out)
        self.assertIn("30 to render", out)
        self.assertEqual(sorted(path.name for path in self.folder.iterdir()), ["radio_catalog.json"])

    def test_generate_audition_approve_and_cache(self):
        code, _, err = self.run_tool("--generate", "--event", "objective.fuel_depot_secured")
        self.assertEqual((code, err), (0, ""))
        self.assertEqual(len(self.calls), 3)
        self.assertEqual({call[1] for call in self.calls}, {"voice-a", "voice-b", "voice-c"})
        manifest = self.manifest()
        self.assertEqual((len(manifest["clips"]), len(manifest["pending"])), (0, 3), "nothing reaches the game unheard")
        self.assertTrue((self.folder / "masters" / ".gdignore").is_file())
        self.assertTrue((self.folder / "audition" / ".gdignore").is_file())
        for clip in manifest["pending"]:
            self.assertEqual(tool.sha256_file(self.folder / clip["file"]), clip["sha256"])
            self.assertEqual(tool.sha256_file(self.folder / clip["master"]), clip["master_sha256"])
            self.assertNotIn("test-key", json.dumps(clip))
        master = tool.read_wav(self.folder / manifest["pending"][0]["master"])
        clip = tool.read_wav(self.folder / manifest["pending"][0]["file"])
        self.assertEqual(master, tone(), "the master is the untouched source")
        self.assertLess(len(clip), len(master), "dead air is trimmed")
        peak = max(abs(value) for value in tool.to_samples(clip))
        self.assertAlmostEqual(20 * math.log10(peak), -3.0, delta=0.2)

        code, out, _ = self.run_tool("--generate", "--event", "objective.fuel_depot_secured")
        self.assertEqual(code, 0)
        self.assertEqual(len(self.calls), 3, "unchanged lines are not rendered or paid for again")
        self.assertIn("nothing to render", out)

        self.assertEqual(self.run_tool("--approve-all")[0], 0)
        manifest = self.manifest()
        self.assertEqual((len(manifest["clips"]), len(manifest["pending"])), (3, 0))
        for clip in manifest["clips"]:
            self.assertTrue(clip["path"].startswith("res://audio/radio/clips/"))
            self.assertEqual(tool.sha256_file(self.folder / clip["file"]), clip["sha256"])

        # Changing a line's words makes only that line stale.
        catalog = json.loads((self.folder / "radio_catalog.json").read_text())
        event = next(event for event in catalog["events"] if event["id"] == "objective.fuel_depot_secured")
        event["lines"]["mass_command"] = ["Command, depot taken."]
        (self.folder / "radio_catalog.json").write_text(json.dumps(catalog))
        self.run_tool("--generate", "--event", "objective.fuel_depot_secured")
        self.assertEqual(len(self.calls), 4)
        self.assertEqual(self.calls[-1][0], "Command, depot taken.")
        manifest = self.manifest()
        self.assertEqual((len(manifest["clips"]), len(manifest["pending"])), (2, 1), "a re-render needs approval again")

    def test_selection_and_force(self):
        self.run_tool("--generate", "--event", "objective.fuel_depot_lost", "--voice-set", "elite_command")
        self.assertEqual(len(self.calls), 1)
        self.run_tool("--generate", "--event", "objective.fuel_depot_lost", "--voice-set", "elite_command", "--force")
        self.assertEqual(len(self.calls), 2)
        code, _, err = self.run_tool("--event", "no.such_event")
        self.assertEqual(code, 1)
        self.assertIn("no lines match", err)

    def test_budget_key_and_voice_failures_send_nothing(self):
        code, _, err = self.run_tool("--generate", "--max-characters", "100")
        self.assertEqual(code, 1)
        self.assertIn("exceeds --max-characters", err)
        code, _, err = self.run_tool("--generate", env={})
        self.assertEqual(code, 1)
        self.assertIn("ELEVENLABS_API_KEY is not set", err)
        code, _, err = self.run_tool("--generate", env={"ELEVENLABS_API_KEY": "test-key"})
        self.assertEqual(code, 1)
        self.assertIn("no ElevenLabs voice id", err)
        self.assertEqual(self.calls, [])

    def test_reprocess_rebuilds_from_masters_without_the_api(self):
        self.run_tool("--generate", "--event", "objective.fuel_depot_secured", "--voice-set", "elite_command")
        before = self.manifest()["pending"][0]["sha256"]
        self.assertEqual(self.run_tool("--reprocess", "--no-radio")[0], 0)
        after = self.manifest()["pending"][0]
        self.assertNotEqual(after["sha256"], before)
        self.assertEqual(tool.sha256_file(self.folder / after["file"]), after["sha256"])
        self.assertEqual(len(self.calls), 1)

    def test_rate_limits_are_retried_and_bad_keys_are_explained(self):
        import urllib.error
        from unittest.mock import patch
        attempts = []

        def flaky(request, timeout):
            attempts.append(request)
            if len(attempts) < 3:
                raise urllib.error.HTTPError(request.full_url, 429, "Too Many", {"Retry-After": "0"}, io.BytesIO(b"slow down"))
            return contextlib.closing(io.BytesIO(tone()))

        config = {"voice_id": "voice-a", "model_id": "m", "voice_settings": {}}
        with patch("urllib.request.urlopen", flaky), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(tool.synthesize("hello", config, "test-key", sleep=lambda _: None), tone())
        self.assertEqual(len(attempts), 3)
        self.assertEqual(attempts[0].get_header("Xi-api-key"), "test-key")
        self.assertIn("output_format=pcm_24000", attempts[0].full_url)
        self.assertEqual(json.loads(attempts[0].data)["text"], "hello")

        def refused(request, timeout):
            raise urllib.error.HTTPError(request.full_url, 401, "Unauthorized", {}, io.BytesIO(b"bad key"))

        with patch("urllib.request.urlopen", refused):
            with self.assertRaisesRegex(tool.GeneratorError, "check ELEVENLABS_API_KEY"):
                tool.synthesize("hello", config, "wrong", sleep=lambda _: None)

    def test_secrets_stay_out_of_version_control(self):
        ignored = subprocess.run(["git", "check-ignore", ".env"], cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(ignored.returncode, 0, ".env must be ignored by git")
        tracked = subprocess.run(["git", "grep", "-l", "-I", "-E", "xi-api-key|ELEVENLABS_API_KEY *= *[A-Za-z0-9]{8,}"],
                                 cwd=ROOT, capture_output=True, text=True).stdout.split()
        self.assertFalse([path for path in tracked if not path.startswith("tools/")], "no key material outside the tool")


if __name__ == "__main__":
    unittest.main()
