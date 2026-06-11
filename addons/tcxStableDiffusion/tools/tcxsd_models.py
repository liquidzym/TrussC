from __future__ import annotations

import dataclasses
import os
import pathlib
import shutil
import time
import urllib.request
from urllib.error import HTTPError
from typing import Callable, Dict, Iterable, List, Mapping, Optional


DEFAULT_TIMEOUT_SECONDS = 60


@dataclasses.dataclass(frozen=True)
class ModelAsset:
    role: str
    filename: str
    url: str
    required: bool = True


@dataclasses.dataclass(frozen=True)
class ModelSpec:
    id: str
    family: str
    example: str
    description: str
    files: List[ModelAsset]
    notes: List[str] = dataclasses.field(default_factory=list)
    recommended_args: Mapping[str, str] = dataclasses.field(default_factory=dict)

    @property
    def manual_urls(self) -> Dict[str, str]:
        return {asset.filename: asset.url for asset in self.files}


class ModelRegistry:
    def __init__(self, models: Iterable[ModelSpec], priority: Iterable[str]) -> None:
        self._models = {model.id: model for model in models}
        self.priority = list(priority)

        missing = [model_id for model_id in self.priority if model_id not in self._models]
        if missing:
            raise ValueError(f"Priority references unknown model ids: {missing}")

    def model(self, model_id: str) -> ModelSpec:
        try:
            return self._models[model_id]
        except KeyError as exc:
            known = ", ".join(sorted(self._models))
            raise KeyError(f"Unknown model '{model_id}'. Known models: {known}") from exc

    def all(self) -> List[ModelSpec]:
        return [self._models[model_id] for model_id in self.priority]


class DownloadFailure(RuntimeError):
    def __init__(
        self,
        model_id: str,
        attempts: int,
        manual_urls: Mapping[str, str],
        last_error: BaseException,
    ) -> None:
        super().__init__(
            f"Failed to download model '{model_id}' after {attempts} attempts: {last_error}"
        )
        self.model_id = model_id
        self.attempts = attempts
        self.manual_urls = dict(manual_urls)
        self.last_error = last_error


FetchFn = Callable[[str, pathlib.Path, int], None]


def hf_url(repo: str, filename: str, revision: str = "main") -> str:
    return f"https://huggingface.co/{repo}/resolve/{revision}/{filename}"


def load_model_registry() -> ModelRegistry:
    ideogram = ModelSpec(
        id="ideogram4-q4_0",
        family="Ideogram4",
        example="ideogram4-basic",
        description="Ideogram4 GGUF Q4_0 starter profile for the first tcxStableDiffusion example.",
        files=[
            ModelAsset(
                role="diffusion",
                filename="ideogram4-Q4_0.gguf",
                url=hf_url("leejet/ideogram-4-GGUF", "ideogram4-Q4_0.gguf"),
            ),
            ModelAsset(
                role="uncond_diffusion",
                filename="ideogram4_uncond-Q4_0.gguf",
                url=hf_url("leejet/ideogram-4-GGUF", "ideogram4_uncond-Q4_0.gguf"),
            ),
            ModelAsset(
                role="llm",
                filename="Qwen3VL-8B-Instruct-Q4_K_M.gguf",
                url=hf_url("Qwen/Qwen3-VL-8B-Instruct-GGUF", "Qwen3VL-8B-Instruct-Q4_K_M.gguf"),
            ),
            ModelAsset(
                role="vae",
                filename="flux2_ae.safetensors",
                url=hf_url("nvidia/PiD", "checkpoints/flux2_ae.safetensors"),
            ),
        ],
        notes=[
            "Ideogram4 needs diffusion, unconditional diffusion, Qwen3-VL LLM and FLUX.2 VAE assets.",
            "The original fp8 Ideogram4 repository is gated; this starter uses leejet's stable-diffusion.cpp GGUF conversion.",
        ],
        recommended_args={
            "width": "1024",
            "height": "1024",
            "steps": "8",
            "cfg_scale": "1.0",
            "diffusion_flash_attention": "true",
            "offload_to_cpu": "true",
        },
    )

    flux2_klein = ModelSpec(
        id="flux2-klein-4b-q4_0",
        family="FLUX.2-klein",
        example="flux2-klein-basic",
        description="FLUX.2-klein 4B GGUF starter profile for fast local generation/editing.",
        files=[
            ModelAsset(
                role="diffusion",
                filename="flux-2-klein-4b-Q4_0.gguf",
                url=hf_url("leejet/FLUX.2-klein-4B-GGUF", "flux-2-klein-4b-Q4_0.gguf"),
            ),
            ModelAsset(
                role="llm",
                filename="Qwen3-4B-Q4_K_M.gguf",
                url=hf_url("unsloth/Qwen3-4B-GGUF", "Qwen3-4B-Q4_K_M.gguf"),
            ),
            ModelAsset(
                role="vae",
                filename="flux2_ae.safetensors",
                url=hf_url("nvidia/PiD", "checkpoints/flux2_ae.safetensors"),
            ),
        ],
        notes=[
            "Use cfg scale 1 and low step counts for the non-base klein model.",
        ],
        recommended_args={
            "width": "1024",
            "height": "1024",
            "steps": "4",
            "cfg_scale": "1.0",
            "diffusion_flash_attention": "true",
            "offload_to_cpu": "true",
        },
    )

    z_image = ModelSpec(
        id="z-image-turbo-q3_k",
        family="Z-Image",
        example="z-image-basic",
        description="Z-Image Turbo GGUF profile, kept ready for the second wave of examples.",
        files=[
            ModelAsset(
                role="diffusion",
                filename="z_image_turbo-Q3_K.gguf",
                url=hf_url("leejet/Z-Image-Turbo-GGUF", "z_image_turbo-Q3_K.gguf"),
            ),
            ModelAsset(
                role="llm",
                filename="Qwen3-4B-Instruct-2507-Q4_K_M.gguf",
                url=hf_url(
                    "unsloth/Qwen3-4B-Instruct-2507-GGUF",
                    "Qwen3-4B-Instruct-2507-Q4_K_M.gguf",
                ),
            ),
            ModelAsset(
                role="vae",
                filename="z_image_ae.safetensors",
                url=hf_url("Comfy-Org/z_image_turbo", "split_files/vae/ae.safetensors"),
            ),
        ],
        notes=[
            "Z-Image Turbo is listed by upstream as runnable on low VRAM GPUs; RTX 4090 can use higher quantizations later.",
        ],
        recommended_args={
            "width": "1024",
            "height": "512",
            "steps": "8",
            "cfg_scale": "1.0",
            "diffusion_flash_attention": "true",
            "offload_to_cpu": "true",
        },
    )

    return ModelRegistry(
        models=(ideogram, flux2_klein, z_image),
        priority=("ideogram4-q4_0", "flux2-klein-4b-q4_0", "z-image-turbo-q3_k"),
    )


def stable_diffusion_cmake_flags(profile: str) -> List[str]:
    disabled_accelerators = {
        "SD_CUDA",
        "SD_HIPBLAS",
        "SD_METAL",
        "SD_VULKAN",
        "SD_OPENCL",
        "SD_SYCL",
        "SD_MUSA",
    }

    enabled = set()
    if profile == "windows-cuda":
        enabled.add("SD_CUDA")
    elif profile == "macos-metal":
        enabled.add("SD_METAL")
    elif profile == "cpu-dev":
        enabled = set()
    else:
        raise ValueError(f"Unknown stable-diffusion.cpp build profile: {profile}")

    build_examples = "ON" if profile == "windows-cuda" else "OFF"

    flags = [
        f"-DSD_BUILD_EXAMPLES={build_examples}",
        "-DSD_BUILD_SHARED_LIBS=ON",
        "-DSD_BUILD_SHARED_GGML_LIB=OFF",
        "-DSD_SERVER_BUILD_FRONTEND=OFF",
        "-DSD_WEBP=OFF",
        "-DSD_WEBM=OFF",
        "-DSD_USE_SYSTEM_GGML=OFF",
    ]

    for option in sorted(disabled_accelerators):
        flags.append(f"-D{option}={'ON' if option in enabled else 'OFF'}")

    return flags


def download_file(
    url: str,
    target: pathlib.Path,
    timeout: int = DEFAULT_TIMEOUT_SECONDS,
    opener=urllib.request.urlopen,
) -> None:
    target = pathlib.Path(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    partial = target.with_suffix(target.suffix + ".part")

    headers = {"User-Agent": "tcxStableDiffusion/0.1"}
    mode = "wb"
    if partial.exists() and partial.stat().st_size > 0:
        headers["Range"] = f"bytes={partial.stat().st_size}-"
        mode = "ab"

    request = urllib.request.Request(url, headers=headers)
    try:
        with opener(request, timeout=timeout) as response:
            with partial.open(mode) as handle:
                shutil.copyfileobj(response, handle, length=1024 * 1024)
    except HTTPError as exc:
        if exc.code != 416 or "Range" not in headers:
            raise
        partial.unlink(missing_ok=True)
        request = urllib.request.Request(url, headers={"User-Agent": "tcxStableDiffusion/0.1"})
        with opener(request, timeout=timeout) as response:
            with partial.open("wb") as handle:
                shutil.copyfileobj(response, handle, length=1024 * 1024)

    os.replace(partial, target)


def download_model(
    model: ModelSpec,
    target_dir: pathlib.Path,
    max_attempts: int = 3,
    fetch: FetchFn = download_file,
    timeout: int = DEFAULT_TIMEOUT_SECONDS,
) -> List[pathlib.Path]:
    if max_attempts < 1:
        raise ValueError("max_attempts must be >= 1")

    target_dir = pathlib.Path(target_dir)
    target_dir.mkdir(parents=True, exist_ok=True)
    downloaded: List[pathlib.Path] = []

    for asset in model.files:
        target = target_dir / asset.filename
        if target.exists() and target.stat().st_size > 0:
            downloaded.append(target)
            continue

        last_error: Optional[BaseException] = None
        for attempt in range(1, max_attempts + 1):
            try:
                fetch(asset.url, target, timeout)
                downloaded.append(target)
                break
            except BaseException as exc:  # noqa: BLE001 - surface the original network/tooling failure.
                last_error = exc
                if attempt >= max_attempts:
                    raise DownloadFailure(
                        model_id=model.id,
                        attempts=max_attempts,
                        manual_urls=model.manual_urls,
                        last_error=exc,
                    ) from exc
                time.sleep(min(2 ** (attempt - 1), 8))

        if last_error is not None and not target.exists():
            raise DownloadFailure(
                model_id=model.id,
                attempts=max_attempts,
                manual_urls=model.manual_urls,
                last_error=last_error,
            )

    return downloaded


def manual_download_lines(model: ModelSpec, target_dir: pathlib.Path) -> List[str]:
    lines = [
        f"Model: {model.id}",
        f"Target directory: {pathlib.Path(target_dir)}",
        "Download these files manually and place them in the target directory:",
    ]
    for asset in model.files:
        lines.append(f"- {asset.filename}: {asset.url}")
    return lines
