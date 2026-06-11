import json
import pathlib
import sys
import tempfile
import unittest


ADDON_ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ADDON_ROOT / "tools"))

import tcxsd_server  # noqa: E402


def write_minimal_png(path: pathlib.Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + (13).to_bytes(4, "big")
        + b"IHDR"
        + (16).to_bytes(4, "big")
        + (16).to_bytes(4, "big")
        + b"\x08\x02\x00\x00\x00"
        + b"\x00\x00\x00\x00"
    )


class ServerToolTests(unittest.TestCase):
    def test_sdcpp_request_body_maps_img2img_control_and_lora(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            image = root / "input.png"
            control = root / "control.png"
            write_minimal_png(image)
            write_minimal_png(control)
            job = {
                "_job_dir": root,
                "prompt": "test prompt",
                "width": 256,
                "height": 256,
                "steps": 4,
                "cfg_scale": 1.5,
                "strength": 0.55,
                "control_strength": 0.7,
                "init_image": "input.png",
                "control_image": "control.png",
                "loras": [
                    {"path": "style.safetensors", "weight": 0.8},
                ],
            }

            body = tcxsd_server.sdcpp_request_body(job)

        self.assertEqual(body["strength"], 0.55)
        self.assertEqual(body["control_strength"], 0.7)
        self.assertTrue(body["init_image"].startswith("data:image/png;base64,"))
        self.assertTrue(body["control_image"].startswith("data:image/png;base64,"))
        self.assertEqual(body["sample_params"]["sample_steps"], 4)
        self.assertEqual(body["sample_params"]["guidance"]["txt_cfg"], 1.5)
        self.assertEqual(body["lora"][0]["path"], "style.safetensors")
        self.assertEqual(body["lora"][0]["multiplier"], 0.8)


if __name__ == "__main__":
    unittest.main()
