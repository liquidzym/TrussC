import json
import pathlib
import sys
import tempfile
import unittest


ADDON_ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ADDON_ROOT / "tools"))

import tcxsd_job  # noqa: E402


def write_required_model_files(model_dir: pathlib.Path) -> None:
    model_dir.mkdir(parents=True, exist_ok=True)
    for filename in (
        "ideogram4-Q4_0.gguf",
        "ideogram4_uncond-Q4_0.gguf",
        "Qwen3VL-8B-Instruct-Q4_K_M.gguf",
        "flux2_ae.safetensors",
    ):
        (model_dir / filename).write_bytes(b"model")


def write_fake_cli(native_dir: pathlib.Path) -> pathlib.Path:
    bin_dir = native_dir / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)
    cli = bin_dir / "sd-cli.exe"
    cli.write_bytes(b"exe")
    return cli


def write_fake_server(native_dir: pathlib.Path) -> pathlib.Path:
    bin_dir = native_dir / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)
    server = bin_dir / "sd-server.exe"
    server.write_bytes(b"server")
    return server


def write_minimal_png(path: pathlib.Path, width: int = 512, height: int = 512) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + (13).to_bytes(4, "big")
        + b"IHDR"
        + width.to_bytes(4, "big")
        + height.to_bytes(4, "big")
        + b"\x08\x02\x00\x00\x00"
        + b"\x00\x00\x00\x00"
    )


def sample_job() -> dict:
    return {
        "model": "ideogram4-q4_0",
        "model_dir": "../models",
        "native_dir": "../native",
        "output_dir": "../outputs",
        "output_name": "job_test",
        "prompt_json": {
            "high_level_description": "A compact product poster for tcxStableDiffusion",
            "style_description": {"aesthetics": "clean technical poster"},
            "compositional_deconstruction": {"layout": "upright", "elements": []},
        },
        "negative_prompt": "blurry",
        "width": 512,
        "height": 512,
        "steps": 3,
        "seed": 47,
        "cfg_scale": 1.0,
        "runtime": {
            "backend": "cuda0",
            "params_backend": "cpu",
            "offload_to_cpu": True,
            "diffusion_flash_attention": True,
            "timeout_seconds": 30,
        },
        "metadata": {
            "source": "unit-test",
            "prompt_profile": "ideogram4",
            "prompt_kind": "poster",
        },
    }


class JobToolTests(unittest.TestCase):
    def test_validate_rejects_missing_prompt(self):
        failures = tcxsd_job.validate_job({"width": 512, "height": 512, "steps": 1})

        self.assertIn("missing prompt or prompt_json", failures)

    def test_load_resolves_relative_paths_and_builds_args(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            write_required_model_files(root / "models")
            write_fake_cli(root / "native")
            job_dir = root / "jobs"
            job_dir.mkdir()
            job_path = job_dir / "job.json"
            job_path.write_text(json.dumps(sample_job()), encoding="utf-8")

            job = tcxsd_job.load_job(job_path)
            resolved = tcxsd_job.resolve_job(job, addon_root=ADDON_ROOT)
            args = tcxsd_job.build_sd_cli_args(resolved)

        self.assertEqual(resolved.model.id, "ideogram4-q4_0")
        self.assertEqual(resolved.output_path.name, "job_test.png")
        self.assertIn("--diffusion-model", args)
        self.assertIn("--uncond-diffusion-model", args)
        self.assertIn("--llm", args)
        self.assertIn("--vae", args)
        self.assertIn("--offload-to-cpu", args)
        self.assertIn("--diffusion-fa", args)
        self.assertIn("-o", args)

    def test_run_job_writes_success_sidecar_with_fake_runner(self):
        calls = []

        def fake_runner(args, cwd, log_path, timeout):
            calls.append((list(args), cwd, log_path, timeout))
            output_path = pathlib.Path(args[args.index("-o") + 1])
            write_minimal_png(output_path, 512, 512)
            log_path.write_text("fake sd-cli log", encoding="utf-8")
            return 0

        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            write_required_model_files(root / "models")
            write_fake_cli(root / "native")
            job = sample_job()
            job["_job_dir"] = root / "jobs"

            sidecar = tcxsd_job.run_job(job, addon_root=ADDON_ROOT, process_runner=fake_runner)
            sidecar_path = pathlib.Path(sidecar["sidecar_path"])
            data = json.loads(sidecar_path.read_text(encoding="utf-8"))

        self.assertTrue(sidecar["ok"])
        self.assertEqual(data["image_width"], 512)
        self.assertEqual(data["metadata"]["execution_mode"], "cli_process")
        self.assertEqual(data["metadata"]["backend"], "cuda0")
        self.assertIn("diffusion_model_path", data["metadata"])
        self.assertEqual(calls[0][3], 30)

    def test_persistent_server_job_uses_server_backend(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            write_required_model_files(root / "models")
            write_fake_cli(root / "native")
            write_fake_server(root / "native")
            job = sample_job()
            job["_job_dir"] = root / "jobs"
            job["runtime"]["execution_mode"] = "persistent_server"
            job["runtime"]["server_host"] = "127.0.0.1"
            job["runtime"]["server_port"] = 19999

            sidecar = tcxsd_job.run_job(
                job,
                addon_root=ADDON_ROOT,
                server_runner=lambda resolved: {
                    "ok": True,
                    "state": "complete",
                    "duration_seconds": 0.25,
                    "saved_image_path": str(resolved.output_path),
                    "native_output_path": str(resolved.output_path),
                    "image_width": 512,
                    "image_height": 512,
                    "sidecar_path": str(resolved.sidecar_path),
                    "metadata": {
                        "execution_mode": "persistent_server",
                        "server_url": "http://127.0.0.1:19999",
                    },
                },
            )

        self.assertTrue(sidecar["ok"])
        self.assertEqual(sidecar["metadata"]["execution_mode"], "persistent_server")
        self.assertEqual(sidecar["metadata"]["server_url"], "http://127.0.0.1:19999")

    def test_string_boolean_runtime_values_are_parsed(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            write_required_model_files(root / "models")
            write_fake_cli(root / "native")
            job = sample_job()
            job["_job_dir"] = root / "jobs"
            job["runtime"]["mmap"] = "false"
            job["runtime"]["diffusion_flash_attention"] = "true"

            resolved = tcxsd_job.resolve_job(job, addon_root=ADDON_ROOT)
            args = tcxsd_job.build_sd_cli_args(resolved)

        self.assertNotIn("--mmap", args)
        self.assertIn("--diffusion-fa", args)

    def test_invalid_numeric_value_is_reported_by_validation(self):
        job = sample_job()
        job["width"] = "wide"

        failures = tcxsd_job.validate_job(job)

        self.assertTrue(any("expected integer value" in item for item in failures))


if __name__ == "__main__":
    unittest.main()
