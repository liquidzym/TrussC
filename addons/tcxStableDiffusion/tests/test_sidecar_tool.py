import json
import pathlib
import sys
import tempfile
import unittest


ADDON_ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ADDON_ROOT / "tools"))

import tcxsd_sidecar  # noqa: E402


def sample_sidecar():
    return {
        "job_id": 1,
        "ok": True,
        "state": "complete",
        "duration_seconds": 8.986,
        "saved_image_path": "outputs/poster.png",
        "native_output_path": "Temp/tcxsd_job_1.png",
        "image_width": 512,
        "image_height": 512,
        "metadata": {
            "prompt": "{\"high_level_description\":\"poster\"}",
            "steps": "3",
            "seed": "47",
            "cfg_scale": "1.000000",
            "execution_mode": "cli_process",
            "backend": "cuda",
            "prompt_profile": "ideogram4",
            "prompt_kind": "poster",
            "model": "ideogram4-q4_0",
            "backend_log": "Temp/tcxsd_job_1.log",
            "cli_log": "Temp/tcxsd_job_1.log",
        },
    }


class SidecarToolTests(unittest.TestCase):
    def test_load_validate_and_summarize_sidecar(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = pathlib.Path(temp_dir) / "result.json"
            path.write_text(json.dumps(sample_sidecar()), encoding="utf-8")

            data = tcxsd_sidecar.load_sidecar(path)
            failures = tcxsd_sidecar.validate_sidecar(data)
            summary = tcxsd_sidecar.summarize_sidecar(data)

        self.assertEqual(failures, [])
        self.assertEqual(summary["ok"], True)
        self.assertEqual(summary["execution_mode"], "cli_process")
        self.assertEqual(summary["backend"], "cuda")
        self.assertEqual(summary["backend_log"], "Temp/tcxsd_job_1.log")
        self.assertEqual(summary["cli_log"], "Temp/tcxsd_job_1.log")
        self.assertEqual(summary["prompt_profile"], "ideogram4")

    def test_persistent_server_summary_uses_backend_log_without_cli_log(self):
        sidecar = sample_sidecar()
        sidecar["metadata"]["execution_mode"] = "persistent_server"
        sidecar["metadata"]["server_log"] = "Temp/tcxsd_server.log"
        sidecar["metadata"]["backend_log"] = "Temp/tcxsd_server.log"

        summary = tcxsd_sidecar.summarize_sidecar(sidecar)

        self.assertEqual(summary["execution_mode"], "persistent_server")
        self.assertEqual(summary["backend_log"], "Temp/tcxsd_server.log")
        self.assertEqual(summary["server_log"], "Temp/tcxsd_server.log")
        self.assertIsNone(summary["cli_log"])

    def test_validate_reports_missing_fields(self):
        failures = tcxsd_sidecar.validate_sidecar({"ok": "yes", "metadata": {}})

        self.assertIn("missing top-level field: job_id", failures)
        self.assertIn("missing metadata field: prompt", failures)
        self.assertIn("ok must be boolean", failures)

    def test_cli_validate_returns_success(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = pathlib.Path(temp_dir) / "result.json"
            path.write_text(json.dumps(sample_sidecar()), encoding="utf-8")

            rc = tcxsd_sidecar.main(["validate", str(path), "--require-success-image"])

        self.assertEqual(rc, 0)


if __name__ == "__main__":
    unittest.main()
