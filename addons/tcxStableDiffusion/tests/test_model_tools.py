import pathlib
import sys
import tempfile
import unittest
from urllib.error import HTTPError


ADDON_ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ADDON_ROOT / "tools"))

import tcxsd_models  # noqa: E402


class ModelToolTests(unittest.TestCase):
    def test_ideogram4_is_first_priority_example_model(self):
        registry = tcxsd_models.load_model_registry()

        self.assertEqual(registry.priority[0], "ideogram4-q4_0")
        model = registry.model("ideogram4-q4_0")
        self.assertEqual(model.example, "ideogram4-basic")
        self.assertEqual(model.family, "Ideogram4")
        self.assertTrue(model.files)

        file_names = [asset.filename for asset in model.files]
        self.assertIn("ideogram4-Q4_0.gguf", file_names)
        self.assertIn("ideogram4_uncond-Q4_0.gguf", file_names)
        self.assertIn("flux2_ae.safetensors", file_names)
        self.assertIn(
            "https://huggingface.co/leejet/ideogram-4-GGUF/resolve/main/ideogram4-Q4_0.gguf",
            [asset.url for asset in model.files],
        )
        self.assertNotIn(
            "https://huggingface.co/black-forest-labs/FLUX.2-dev/resolve/main/ae.safetensors",
            [asset.url for asset in model.files],
        )

    def test_priority_models_share_the_main_example(self):
        registry = tcxsd_models.load_model_registry()

        self.assertEqual(
            [registry.model(model_id).example for model_id in registry.priority],
            ["ideogram4-basic", "ideogram4-basic", "ideogram4-basic"],
        )

    def test_download_stops_after_three_failures_with_manual_urls(self):
        registry = tcxsd_models.load_model_registry()
        model = registry.model("ideogram4-q4_0")
        attempts = []

        def always_fails(url, target, timeout):
            attempts.append((url, pathlib.Path(target).name, timeout))
            raise OSError("network disconnected")

        with tempfile.TemporaryDirectory() as temp_dir:
            with self.assertRaises(tcxsd_models.DownloadFailure) as failure:
                tcxsd_models.download_model(
                    model,
                    pathlib.Path(temp_dir),
                    max_attempts=3,
                    fetch=always_fails,
                )

        self.assertEqual(failure.exception.attempts, 3)
        self.assertIn("ideogram4-Q4_0.gguf", failure.exception.manual_urls)
        self.assertEqual(
            failure.exception.manual_urls["ideogram4-Q4_0.gguf"],
            "https://huggingface.co/leejet/ideogram-4-GGUF/resolve/main/ideogram4-Q4_0.gguf",
        )
        self.assertEqual(len(attempts), 3)

    def test_windows_cuda_preset_disables_other_backends(self):
        flags = tcxsd_models.stable_diffusion_cmake_flags("windows-cuda")

        self.assertIn("-DSD_CUDA=ON", flags)
        for disabled in (
            "SD_VULKAN",
            "SD_OPENCL",
            "SD_HIPBLAS",
            "SD_SYCL",
            "SD_METAL",
            "SD_MUSA",
        ):
            self.assertIn(f"-D{disabled}=OFF", flags)

        self.assertIn("-DSD_BUILD_EXAMPLES=ON", flags)
        self.assertIn("-DSD_SERVER_BUILD_FRONTEND=OFF", flags)
        self.assertIn("-DSD_BUILD_SHARED_LIBS=ON", flags)

    def test_download_file_resumes_partial_downloads_with_range_header(self):
        requests = []

        class Response:
            def __init__(self):
                self._done = False

            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc, tb):
                return False

            def read(self, size=-1):
                if self._done:
                    return b""
                self._done = True
                return b"world"

        def opener(request, timeout):
            requests.append((request.full_url, request.headers.get("Range"), timeout))
            return Response()

        with tempfile.TemporaryDirectory() as temp_dir:
            target = pathlib.Path(temp_dir) / "model.gguf"
            target.with_suffix(".gguf.part").write_bytes(b"hello ")

            tcxsd_models.download_file("https://example.invalid/model.gguf", target, timeout=7, opener=opener)

            self.assertEqual(target.read_bytes(), b"hello world")
            self.assertEqual(requests, [("https://example.invalid/model.gguf", "bytes=6-", 7)])
            self.assertFalse(target.with_suffix(".gguf.part").exists())

    def test_download_file_restarts_when_server_rejects_resume(self):
        requests = []

        class Response:
            def __init__(self):
                self._done = False

            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc, tb):
                return False

            def read(self, size=-1):
                if self._done:
                    return b""
                self._done = True
                return b"fresh"

        def opener(request, timeout):
            range_header = request.headers.get("Range")
            requests.append(range_header)
            if range_header:
                raise HTTPError(request.full_url, 416, "range not satisfiable", hdrs=None, fp=None)
            return Response()

        with tempfile.TemporaryDirectory() as temp_dir:
            target = pathlib.Path(temp_dir) / "model.gguf"
            target.with_suffix(".gguf.part").write_bytes(b"stale")

            tcxsd_models.download_file("https://example.invalid/model.gguf", target, opener=opener)

            self.assertEqual(target.read_bytes(), b"fresh")
            self.assertEqual(requests, ["bytes=5-", None])


if __name__ == "__main__":
    unittest.main()
