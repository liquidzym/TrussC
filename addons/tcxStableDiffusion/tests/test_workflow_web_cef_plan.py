import json
import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
EXAMPLE = ROOT / "examples" / "workflow-web-cef"


class WorkflowWebCefPlanTest(unittest.TestCase):
    def test_scaffold_files_exist(self):
        required = [
            EXAMPLE / "addons.make",
            EXAMPLE / "README.md",
            EXAMPLE / "CMakeLists.txt",
            EXAMPLE / "src" / "main.cpp",
            EXAMPLE / "src" / "tcApp.h",
            EXAMPLE / "src" / "tcApp.cpp",
            EXAMPLE / "src" / "NodeWorkerProcess.h",
            EXAMPLE / "src" / "NodeWorkerProcess.cpp",
            EXAMPLE / "web" / "package.json",
            EXAMPLE / "web" / "index.html",
            EXAMPLE / "web" / "src" / "main.ts",
            EXAMPLE / "web" / "src" / "workflow" / "schema.ts",
            EXAMPLE / "web" / "src" / "workflow" / "examples.ts",
            EXAMPLE / "web" / "src" / "workflow" / "validate.ts",
            EXAMPLE / "worker" / "package.json",
            EXAMPLE / "worker" / "src" / "worker.mjs",
            EXAMPLE / "workflows" / "text-to-image.zh.json",
            EXAMPLE / "workflows" / "controlnet-canny.json",
            EXAMPLE / "workflows" / "inpaint.json",
            EXAMPLE / "workflows" / "lora-stack.json",
            EXAMPLE / "bin" / "data" / "README.md",
        ]
        missing = [path for path in required if not path.exists()]
        self.assertEqual([], missing)

    def test_cmake_copies_cef_web_worker_and_runtime_assets(self):
        cmake = (EXAMPLE / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("tcxcef_copy_runtime_files(workflow-web-cef)", cmake)
        self.assertIn("workflow-web-cef/web", cmake)
        self.assertIn("workflow-web-cef/worker", cmake)
        self.assertIn("workflow-web-cef/workflows", cmake)
        self.assertIn("runtime/node", cmake)
        self.assertIn("bin/data", cmake)
        self.assertIn("${WORKFLOW_WEB_CEF_DATA_ROOT}", cmake)

    def test_addons_make_declares_cef_and_sd(self):
        addons = (EXAMPLE / "addons.make").read_text(encoding="utf-8")
        for name in ["tcxCEF", "tcxStableDiffusion", "tcxTls", "tcxWebSocket"]:
            self.assertRegex(addons, rf"(?m)^{re.escape(name)}$")

    def test_node_worker_process_uses_createprocess_on_windows(self):
        source = (EXAMPLE / "src" / "NodeWorkerProcess.cpp").read_text(encoding="utf-8")
        self.assertIn("CreateProcessW", source)
        self.assertIn("STARTUPINFOW", source)
        self.assertIn("WriteFile", source)
        self.assertIn("ReadFile", source)
        self.assertIn("WORKER_UNAVAILABLE", source)

    def test_main_dispatches_cef_subprocess_before_trussc_app_start(self):
        source = (EXAMPLE / "src" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn("tcxCEF::executeSubprocess", source)
        self.assertIn("workflow-web-cef-native.log", source)
        self.assertLess(source.index("tcxCEF::executeSubprocess"), source.index("TC_RUN_APP"))

    def test_tcxcef_windows_initialization_uses_explicit_runtime_paths(self):
        browser = (ROOT.parent / "tcxCEF" / "src" / "tcxcef" / "Browser.cpp").read_text(encoding="utf-8")
        cmake = (ROOT.parent / "tcxCEF" / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("settings.browser_subprocess_path", browser)
        self.assertIn("settings.root_cache_path", browser)
        self.assertIn("settings.resources_dir_path", browser)
        self.assertIn("settings.locales_dir_path", browser)
        self.assertIn("disable-gpu", browser)
        self.assertIn("data\" / \"workflows\" / \"cache\" / \"cef", browser)
        self.assertIn("cef-debug.log", browser)
        self.assertIn("TCXCEF_RESOURCE_DIR}/locales", cmake)
        self.assertIn("Copying CEF locales", cmake)

    def test_no_runtime_python_dependency_in_cpp_or_worker(self):
        files = list((EXAMPLE / "src").glob("*.cpp")) + list((EXAMPLE / "src").glob("*.h"))
        files += list((EXAMPLE / "worker" / "src").glob("*.mjs"))
        joined = "\n".join(path.read_text(encoding="utf-8") for path in files)
        self.assertNotIn("python ", joined.lower())
        self.assertNotIn("python.exe", joined.lower())
        self.assertNotIn("tools/setup_", joined)

    def test_chinese_workflow_is_utf8_and_not_garbled(self):
        workflow = json.loads((EXAMPLE / "workflows" / "text-to-image.zh.json").read_text(encoding="utf-8"))
        text = json.dumps(workflow, ensure_ascii=False)
        self.assertIn("本地生成", text)
        self.assertNotRegex(text, r"[涓锛绛妯]")

    def test_workflow_json_files_use_real_backend_fields(self):
        controlnet = json.loads((EXAMPLE / "workflows" / "controlnet-canny.json").read_text(encoding="utf-8"))
        controlnet_text = json.dumps(controlnet, ensure_ascii=False)
        self.assertIn("sd15-controlnet-canny", controlnet_text)
        self.assertIn("bin/data/inputs/control/canny-guide.png", controlnet_text)

        inpaint = json.loads((EXAMPLE / "workflows" / "inpaint.json").read_text(encoding="utf-8"))
        inpaint_text = json.dumps(inpaint, ensure_ascii=False)
        self.assertIn("bin/data/inputs/inpaint/source.png", inpaint_text)
        self.assertIn("bin/data/inputs/inpaint/mask.png", inpaint_text)

    def test_asset_and_packaging_tools_are_explicit(self):
        prepare = ROOT / "tools" / "prepare_workflow_web_cef_assets.py"
        package = ROOT / "tools" / "package_workflow_web_cef.py"
        self.assertTrue(prepare.exists())
        self.assertTrue(package.exists())

        prepare_text = prepare.read_text(encoding="utf-8")
        for flag in ["--cef", "--node-runtime", "--native-sd", "--models", "--all", "--verify-only"]:
            self.assertIn(flag, prepare_text)
        self.assertIn("bin/data/inputs/control/canny-guide.png", prepare_text)
        self.assertIn("bin/data/models", prepare_text)

        package_text = package.read_text(encoding="utf-8")
        for entry in ["workflow-web-cef.exe", "runtime/node/node.exe", "workflow-web-cef/web/dist", "workflow-web-cef/worker/dist", "workflow-web-cef/workflows", "data/models", "locales/zh-CN.pak"]:
            self.assertIn(entry, package_text)
        self.assertIn("copy_data_assets", package_text)

    def test_web_ui_uses_chinese_user_facing_labels(self):
        main = (EXAMPLE / "web" / "src" / "main.ts").read_text(encoding="utf-8")
        graph = (EXAMPLE / "web" / "src" / "ui" / "GraphCanvas.ts").read_text(encoding="utf-8")
        inspector = (EXAMPLE / "web" / "src" / "ui" / "Inspector.ts").read_text(encoding="utf-8")
        queue = (EXAMPLE / "web" / "src" / "ui" / "QueuePanel.ts").read_text(encoding="utf-8")
        gallery = (EXAMPLE / "web" / "src" / "ui" / "Gallery.ts").read_text(encoding="utf-8")
        examples = (EXAMPLE / "web" / "src" / "workflow" / "examples.ts").read_text(encoding="utf-8")
        labels = (EXAMPLE / "web" / "src" / "workflow" / "labels.ts").read_text(encoding="utf-8")
        text = "\n".join([main, graph, inspector, queue, gallery, examples, labels])

        for label in ["工作流", "节点", "检查", "运行", "队列", "图库", "侧车", "模型档位", "运行预设", "提示词", "质量检查", "保存产物"]:
            self.assertIn(label, text)
        for forbidden in [">Validate<", ">Run<", ">Workflows<", ">Nodes<", ">Inspector<", ">Queue<", "No active jobs", "No outputs yet", "Select a node"]:
            self.assertNotIn(forbidden, text)


if __name__ == "__main__":
    unittest.main()
