import json
import tempfile
import unittest
from pathlib import Path

from static_release import PROFILE_ID, build_release, verify_release


REPOSITORY = Path(__file__).resolve().parents[1]


class StaticReleaseTests(unittest.TestCase):
    def test_builds_and_verifies_exact_static_release(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / PROFILE_ID
            manifest = build_release(
                REPOSITORY / "dashboard",
                output,
                version="20260720-test.1",
                source_commit="a" * 40,
            )
            verified = verify_release(output)
            html = (output / "index.html").read_text(encoding="utf-8")
            self.assertEqual(manifest, verified)
            self.assertIn('src="app.js?v=20260720-test.1"', html)
            self.assertEqual("static-https-web-serial", manifest["runtime"])
            self.assertFalse(any(term in html for term in ("评分", "得分", "满分", "评委")))
            self.assertEqual(7, len(manifest["assets"]))

    def test_verifier_rejects_tampered_asset(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / PROFILE_ID
            build_release(
                REPOSITORY / "dashboard",
                output,
                version="20260720-test.2",
                source_commit="b" * 40,
            )
            with (output / "app.js").open("a", encoding="utf-8") as handle:
                handle.write("\n// tampered\n")
            with self.assertRaisesRegex(ValueError, "size mismatch"):
                verify_release(output)

    def test_manifest_is_machine_readable(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / PROFILE_ID
            build_release(
                REPOSITORY / "dashboard",
                output,
                version="20260720-test.3",
                source_commit="c" * 40,
            )
            manifest = json.loads((output / "asset-manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(PROFILE_ID, manifest["profileId"])
            self.assertEqual("no-cloud-command-relay-no-mqtt-no-score-ui", manifest["deploymentBoundary"])


if __name__ == "__main__":
    unittest.main()
