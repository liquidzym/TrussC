import os
import unittest

import setup_cef


class SetupCefBuildEnvTest(unittest.TestCase):
    def test_windows_msvc_build_env_enables_utf8_source_decoding(self):
        build_env = getattr(setup_cef, "wrapper_build_env", None)
        self.assertIsNotNone(build_env)
        if build_env is None:
            return

        env = build_env("windows64", {"CL": "/MP"})

        self.assertIn("/MP", env["CL"])
        self.assertIn("/utf-8", env["CL"])

    def test_non_windows_build_env_preserves_environment(self):
        original = dict(os.environ)
        build_env = getattr(setup_cef, "wrapper_build_env", None)
        self.assertIsNotNone(build_env)
        if build_env is None:
            return

        env = build_env("macosarm64", original)

        self.assertEqual(original, env)

    def test_cmake_paths_use_forward_slashes(self):
        cmake_path = getattr(setup_cef, "cmake_path", None)
        cmake_list = getattr(setup_cef, "cmake_list", None)
        self.assertIsNotNone(cmake_path)
        self.assertIsNotNone(cmake_list)
        if cmake_path is None or cmake_list is None:
            return

        self.assertEqual("G:/TrussC/addons/tcxCEF", cmake_path(r"G:\TrussC\addons\tcxCEF"))
        self.assertIn('"G:/TrussC/addons/tcxCEF/libcef.dll"', cmake_list([r"G:\TrussC\addons\tcxCEF\libcef.dll"]))
        self.assertNotIn("\\", cmake_list([r"G:\TrussC\addons\tcxCEF\libcef.dll"]))

    def test_windows_wrapper_runtime_defaults_to_dynamic_crt(self):
        runtime = getattr(setup_cef, "wrapper_runtime_library", None)
        self.assertIsNotNone(runtime)
        if runtime is None:
            return

        self.assertEqual("/MD", runtime("windows64"))
        self.assertEqual("/MT", runtime("windows64", "/MT"))
        self.assertEqual("", runtime("macosarm64"))


if __name__ == "__main__":
    unittest.main()
