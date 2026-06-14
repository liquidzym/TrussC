import json
import pathlib
import sys
import tempfile
import unittest


ADDON_ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ADDON_ROOT / "tools"))

import tcxsd_errors  # noqa: E402
import tcxsd_job  # noqa: E402
import tcxsd_models  # noqa: E402
import tcxsd_prompts  # noqa: E402
import tcxsd_quality  # noqa: E402
import tcxsd_storage  # noqa: E402


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


def write_minimal_png(path: pathlib.Path, width: int = 32, height: int = 16) -> None:
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


class ProfileStorageQualityTests(unittest.TestCase):
    def test_model_profiles_expose_quality_and_runtime_presets(self):
        registry = tcxsd_models.load_model_registry()
        ideogram = registry.model("ideogram4-q4_0")
        flux = registry.model("flux2-klein-4b-q4_0")
        z_image = registry.model("z-image-turbo-q3_k")

        self.assertEqual(ideogram.quality_presets["balanced"].width, 1024)
        self.assertEqual(ideogram.quality_presets["balanced"].cfg_scale, 7.0)
        self.assertEqual(flux.quality_presets["draft"].steps, 4)
        self.assertEqual(z_image.quality_presets["balanced"].height, 512)

        low_vram = tcxsd_models.model_runtime_defaults("ideogram4-q4_0", "low_vram")
        full_speed = tcxsd_models.model_runtime_defaults("ideogram4-q4_0", "rtx4090_full_speed")

        self.assertEqual(low_vram["params_backend"], "cpu")
        self.assertTrue(low_vram["offload_to_cpu"])
        self.assertEqual(full_speed["params_backend"], "cuda0")
        self.assertFalse(full_speed["offload_to_cpu"])

    def test_job_defaults_apply_quality_and_runtime_preset_without_overwriting_explicit_values(self):
        job = tcxsd_models.apply_model_profile_defaults({
            "model": "z-image-turbo-q3_k",
            "prompt": "wide scene",
            "quality": "draft",
            "width": 768,
            "runtime": {
                "preset": "low_vram",
                "backend": "cuda1",
            },
        })

        self.assertEqual(job["width"], 768)
        self.assertEqual(job["height"], 512)
        self.assertEqual(job["steps"], 4)
        self.assertEqual(job["cfg_scale"], 1.0)
        self.assertEqual(job["runtime"]["backend"], "cuda1")
        self.assertEqual(job["runtime"]["params_backend"], "cpu")
        self.assertTrue(job["runtime"]["stream_layers"])

    def test_cli_args_support_img2img_inpaint_and_controlnet(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            write_required_model_files(root / "models")
            write_fake_cli(root / "native")
            for filename in ("init.png", "mask.png", "control.png"):
                write_minimal_png(root / "jobs" / filename)

            job = {
                "_job_dir": root / "jobs",
                "model": "ideogram4-q4_0",
                "model_dir": "../models",
                "native_dir": "../native",
                "prompt": "edit this",
                "width": 512,
                "height": 512,
                "steps": 3,
                "init_image": "init.png",
                "mask_image": "mask.png",
                "control_image": "control.png",
                "strength": 0.55,
                "control_strength": 0.8,
            }

            resolved = tcxsd_job.resolve_job(job, addon_root=ADDON_ROOT)
            args = tcxsd_job.build_sd_cli_args(resolved)

        self.assertIn("--init-img", args)
        self.assertIn("--mask", args)
        self.assertIn("--control-image", args)
        self.assertIn("--strength", args)
        self.assertIn("0.55", args)
        self.assertIn("--control-strength", args)
        self.assertIn("0.8", args)

    def test_cli_lora_reports_structured_unsupported_backend_error(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            write_required_model_files(root / "models")
            write_fake_cli(root / "native")
            job = {
                "_job_dir": root / "jobs",
                "model": "ideogram4-q4_0",
                "model_dir": "../models",
                "native_dir": "../native",
                "prompt": "style this",
                "width": 512,
                "height": 512,
                "steps": 3,
                "loras": [{"path": "style.safetensors", "weight": 0.7}],
            }

            resolved = tcxsd_job.resolve_job(job, addon_root=ADDON_ROOT)
            with self.assertRaises(tcxsd_job.JobError) as failure:
                tcxsd_job.build_sd_cli_args(resolved)

        self.assertIn("BACKEND_UNSUPPORTED", str(failure.exception))
        self.assertIn("PersistentServer", str(failure.exception))

    def test_storage_roots_are_explicit_and_cleanup_removes_old_artifacts(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            job_dir = root / "jobs"
            job_dir.mkdir()
            job = {
                "_job_dir": job_dir,
                "output_root": "../assets",
                "runtime": {
                    "temp_root": "../tmp",
                    "cache_root": "../cache",
                },
            }
            roots = tcxsd_storage.resolve_storage_roots(job, addon_root=ADDON_ROOT)
            sidecar = roots.output_root / "old.json"
            log = roots.output_root / "old.log"
            temp_file = roots.temp_root / "old.tmp"
            for item in (sidecar, log, temp_file):
                item.parent.mkdir(parents=True, exist_ok=True)
                item.write_text("old", encoding="utf-8")

            removed = tcxsd_storage.cleanup_storage(roots, older_than_seconds=0, dry_run=False)

        self.assertEqual(roots.output_root, (root / "assets").resolve())
        self.assertEqual(roots.temp_root, (root / "tmp").resolve())
        self.assertEqual(roots.cache_root, (root / "cache").resolve())
        self.assertEqual({pathlib.Path(item).name for item in removed}, {"old.json", "old.log", "old.tmp"})

    def test_quality_checks_flag_placeholder_tiny_png_and_size_mismatch(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            output = root / "tiny.png"
            write_minimal_png(output, 32, 16)
            sidecar = {
                "ok": True,
                "saved_image_path": str(output),
                "image_width": 32,
                "image_height": 16,
                "metadata": {
                    "prompt": "TODO {subject}",
                    "width": "512",
                    "height": "512",
                    "visible_text": "tcxStableDiffusion",
                },
            }

            report = tcxsd_quality.assess_sidecar(sidecar)

        self.assertFalse(report["ok"])
        self.assertIn("SIZE_MISMATCH", report["error_codes"])
        self.assertIn("PLACEHOLDER_PROMPT", report["warning_codes"])
        self.assertIn("TEXT_NOT_VERIFIED", report["warning_codes"])

    def test_chinese_ideogram_prompt_pack_round_trips_without_ascii_escaping(self):
        packed = tcxsd_prompts.ideogram4_poster(
            subject="一张展示本地 AI 生图工作流的中文海报",
            visible_text="本地生图",
            language="zh",
        )
        job = {
            "prompt_json": packed["prompt_json"],
            "negative_prompt": packed["negative_prompt"],
        }
        encoded = json.dumps(job, ensure_ascii=False)

        self.assertIn("本地生图", tcxsd_job.prompt_text(job))
        self.assertIn("中文海报", encoded)
        self.assertNotIn("\\u672c\\u5730", encoded)

    def test_error_classifier_returns_remediation_hints(self):
        payload = tcxsd_errors.error_payload("CUDA out of memory while allocating tensor")

        self.assertEqual(payload["code"], "CUDA_OOM")
        self.assertTrue(any("low_vram" in hint for hint in payload["remediation_hints"]))


if __name__ == "__main__":
    unittest.main()
