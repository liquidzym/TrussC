from __future__ import annotations

from typing import Any, Dict


def ideogram4_poster(subject: str, visible_text: str, language: str = "en") -> Dict[str, Any]:
    zh = language.lower().startswith("zh")
    if zh:
        high_level = (
            f"{subject}。画面中必须包含清晰可读的文字“{visible_text}”。"
            "整体是高级、干净、适合展示的海报设计。"
        )
        aesthetics = "中文海报设计，高级编辑排版，清晰层级，干净背景，精致商业视觉"
        layout = "竖版或方形海报构图，主体明确，文字水平摆放，从左到右可读，边距均衡"
        text_rule = (
            f"只打印准确文字“{visible_text}”，保持水平、端正、完整、无错字、无镜像、无额外字符。"
        )
        negative = "低质量，模糊，错别字，不可读文字，镜像文字，旋转文字，裁切文字，水印，签名"
    else:
        high_level = f"{subject}. Include the exact readable text \"{visible_text}\"."
        aesthetics = "premium editorial poster design, crisp layout, refined typography, high-end visual direction"
        layout = "upright poster layout with clear subject priority, readable horizontal text, and balanced margins"
        text_rule = (
            f"Print the exact text \"{visible_text}\" horizontally, upright, and readable left to right. "
            "The spelling must be exact, with no missing, extra, mirrored, rotated, or distorted characters."
        )
        negative = "low quality, blurry, misspelled text, unreadable text, cropped text, mirrored text, watermark, signature"

    return {
        "prompt_json": {
            "high_level_description": high_level,
            "style_description": {
                "aesthetics": aesthetics,
                "lighting": "clear controlled lighting with readable contrast",
                "medium": "local AI image generation with polished commercial-art direction",
                "color_palette": ["#F4F1EA", "#111111", "#2F80ED", "#27AE60", "#FFFFFF"],
            },
            "compositional_deconstruction": {
                "canvas": "upright image canvas; do not rotate the image or any text",
                "background": "clean intentional background that supports the subject without clutter",
                "layout": layout,
                "elements": [
                    {"type": "obj", "desc": subject},
                    {"type": "text", "desc": text_rule},
                ],
            },
        },
        "negative_prompt": negative,
        "metadata": {
            "prompt_profile": "ideogram4",
            "prompt_kind": "poster",
            "visible_text": visible_text,
            "language": "zh" if zh else "en",
        },
    }
