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


if __name__ == "__main__":
    unittest.main()
