import pathlib
import sys
import tempfile
import unittest


ADDON_ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ADDON_ROOT / "tools"))

import setup_sd  # noqa: E402


class SetupToolTests(unittest.TestCase):
    def test_windows_cuda_configure_uses_ninja_when_available(self):
        ninja = pathlib.Path("C:/Tools/ninja.exe")
        nvcc = pathlib.Path("C:/CUDA/bin/nvcc.exe")

        args = setup_sd.cmake_configure_command(
            source_dir=pathlib.Path("S:/sd.cpp"),
            build_dir=pathlib.Path("B:/build"),
            install_dir=pathlib.Path("I:/install"),
            profile="windows-cuda",
            extra_flags=["-DSD_CUDA=ON"],
            ninja_path=ninja,
            nvcc_path=nvcc,
        )

        self.assertIn("-G", args)
        self.assertIn("Ninja", args)
        self.assertIn(f"-DCMAKE_MAKE_PROGRAM={ninja}", args)
        self.assertIn(f"-DCMAKE_CUDA_COMPILER={nvcc}", args)
        self.assertIn("-DSD_CUDA=ON", args)

    def test_windows_cuda_configure_without_ninja_keeps_default_generator(self):
        args = setup_sd.cmake_configure_command(
            source_dir=pathlib.Path("S:/sd.cpp"),
            build_dir=pathlib.Path("B:/build"),
            install_dir=pathlib.Path("I:/install"),
            profile="windows-cuda",
            extra_flags=[],
            ninja_path=None,
            nvcc_path=None,
        )

        self.assertNotIn("-G", args)
        self.assertFalse(any(item.startswith("-DCMAKE_MAKE_PROGRAM=") for item in args))
        self.assertFalse(any(item.startswith("-DCMAKE_CUDA_COMPILER=") for item in args))

    def test_windows_cuda_builds_library_and_cli_targets_only(self):
        self.assertEqual(setup_sd.native_build_targets("windows-cuda"), ["stable-diffusion", "sd-cli", "sd-server"])
        self.assertEqual(setup_sd.native_build_targets("macos-metal"), [])

    def test_runtime_files_include_cli_and_persistent_server_binaries(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            install_dir = pathlib.Path(temp_dir)
            bin_dir = install_dir / "bin"
            lib_dir = install_dir / "lib"
            bin_dir.mkdir()
            lib_dir.mkdir()
            dll = bin_dir / "stable-diffusion.dll"
            cli = bin_dir / "sd-cli.exe"
            server = bin_dir / "sd-server.exe"
            library = lib_dir / "stable-diffusion.lib"
            dll.write_bytes(b"dll")
            cli.write_bytes(b"exe")
            server.write_bytes(b"server")
            library.write_bytes(b"lib")

            files = setup_sd.runtime_files(install_dir)

            self.assertIn(dll, files)
            self.assertIn(cli, files)
            self.assertIn(server, files)
            self.assertNotIn(library, files)
            self.assertEqual(setup_sd.find_cli_file(install_dir), cli)
            self.assertEqual(setup_sd.find_server_file(install_dir), server)

    def test_default_model_target_uses_shared_example_data_models_folder(self):
        registry = setup_sd.tcxsd_models.load_model_registry()

        ideogram = setup_sd.model_target_dir(registry.model("ideogram4-q4_0"), None)
        flux = setup_sd.model_target_dir(registry.model("flux2-klein-4b-q4_0"), None)
        z_image = setup_sd.model_target_dir(registry.model("z-image-turbo-q3_k"), None)

        self.assertEqual(ideogram, ADDON_ROOT / "examples" / "ideogram4-basic" / "data" / "models" / "ideogram4-q4_0")
        self.assertEqual(flux, ADDON_ROOT / "examples" / "ideogram4-basic" / "data" / "models" / "flux2-klein-4b-q4_0")
        self.assertEqual(z_image, ADDON_ROOT / "examples" / "ideogram4-basic" / "data" / "models" / "z-image-turbo-q3_k")


if __name__ == "__main__":
    unittest.main()
