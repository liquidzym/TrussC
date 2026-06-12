from pathlib import Path
import unittest


ADDON_ROOT = Path(__file__).resolve().parents[1]
WORKBENCH_CPP = ADDON_ROOT / "examples" / "ideogram4-basic" / "src" / "tcApp.cpp"


class WorkbenchUiTests(unittest.TestCase):
    def test_workbench_exposes_model_workflow_and_preview_context(self):
        source = WORKBENCH_CPP.read_text(encoding="utf-8")

        required_ui_text = [
            "模型能力",
            "默认参数",
            "后端与路径",
            "一键示例",
            "当前工作流",
            "输入面板",
            "预览",
            "输出",
            "源图",
            "蒙版",
            "控制图",
            "Sidecar",
            "推荐入口",
            "C++ 原生",
            "Node/JSON",
            "真实 ControlNet",
        ]

        missing = [text for text in required_ui_text if text not in source]
        self.assertEqual([], missing)


if __name__ == "__main__":
    unittest.main()
