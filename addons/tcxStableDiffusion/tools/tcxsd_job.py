from __future__ import annotations

import argparse
import json
import pathlib
import subprocess
import sys
import time
from dataclasses import dataclass
from typing import Any, Callable, Dict, List, Mapping, Sequence

import setup_sd
import tcxsd_models


ADDON_ROOT = pathlib.Path(__file__).resolve().parents[1]

ROLE_TO_CLI_ARG = {
    "model": "-m",
    "diffusion": "--diffusion-model",
    "high_noise_diffusion": "--high-noise-diffusion-model",
    "uncond_diffusion": "--uncond-diffusion-model",
    "clip_l": "--clip_l",
    "clip_g": "--clip_g",
    "clip_vision": "--clip_vision",
    "t5xxl": "--t5xxl",
    "llm": "--llm",
    "llm_vision": "--llm_vision",
    "vae": "--vae",
    "audio_vae": "--audio-vae",
    "control_net": "--control-net",
    "photo_maker": "--photo-maker",
}

ROLE_TO_METADATA_KEY = {
    "model": "model_path",
    "diffusion": "diffusion_model_path",
    "high_noise_diffusion": "high_noise_diffusion_model_path",
    "uncond_diffusion": "unconditional_diffusion_model_path",
    "clip_l": "clip_l_path",
    "clip_g": "clip_g_path",
    "clip_vision": "clip_vision_path",
    "t5xxl": "t5xxl_path",
    "llm": "llm_path",
    "llm_vision": "llm_vision_path",
    "vae": "vae_path",
    "audio_vae": "audio_vae_path",
    "control_net": "control_net_path",
    "photo_maker": "photo_maker_path",
}


class JobError(ValueError):
    pass


def as_int(value: Any, default: int = 0) -> int:
    if value is None or value == "":
        return default
    try:
        return int(value)
    except (TypeError, ValueError) as exc:
        raise JobError(f"expected integer value, got {value!r}") from exc


def as_float(value: Any, default: float = 0.0) -> float:
    if value is None or value == "":
        return default
    try:
        return float(value)
    except (TypeError, ValueError) as exc:
        raise JobError(f"expected numeric value, got {value!r}") from exc


def as_bool(value: Any, default: bool = False) -> bool:
    if value is None:
        return default
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    if isinstance(value, str):
        text = value.strip().lower()
        if text in {"1", "true", "yes", "on"}:
            return True
        if text in {"0", "false", "no", "off"}:
            return False
    raise JobError(f"expected boolean value, got {value!r}")


@dataclass(frozen=True)
class ResolvedJob:
    job: Mapping[str, Any]
    model: tcxsd_models.ModelSpec
    model_dir: pathlib.Path
    output_dir: pathlib.Path
    output_name: str
    output_path: pathlib.Path
    sidecar_path: pathlib.Path
    log_path: pathlib.Path
    cli_path: pathlib.Path
    asset_paths: Mapping[str, pathlib.Path]


RunProcess = Callable[[Sequence[str], pathlib.Path, pathlib.Path, int], int]
RunServerJob = Callable[[ResolvedJob], Dict[str, Any]]


def load_job(path: pathlib.Path | str) -> Dict[str, Any]:
    job_path = pathlib.Path(path)
    try:
        data = json.loads(job_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise JobError(f"Invalid JSON job file: {job_path}: {exc}") from exc
    except OSError as exc:
        raise JobError(f"Could not read job file: {job_path}: {exc}") from exc

    if not isinstance(data, dict):
        raise JobError("Job root must be a JSON object")
    data["_job_dir"] = job_path.parent.resolve()
    return data


def prompt_text(job: Mapping[str, Any]) -> str:
    if "prompt_json" in job:
        return json.dumps(job["prompt_json"], ensure_ascii=False, separators=(",", ":"))
    prompt = job.get("prompt", "")
    return prompt if isinstance(prompt, str) else ""


def validate_job(job: Mapping[str, Any]) -> List[str]:
    failures: List[str] = []
    if not prompt_text(job):
        failures.append("missing prompt or prompt_json")

    try:
        for key in ("width", "height", "steps"):
            value = as_int(job.get(key), 0)
            if value <= 0:
                failures.append(f"{key} must be > 0")

        if as_int(job.get("batch_count"), 1) != 1:
            failures.append("batch_count must be 1 for this first job runner")
        as_int(job.get("seed"), -1)
        as_float(job.get("cfg_scale"), 1.0)
    except JobError as exc:
        failures.append(str(exc))

    runtime = job.get("runtime", {})
    if runtime is not None and not isinstance(runtime, dict):
        failures.append("runtime must be an object")
    elif isinstance(runtime, dict):
        for key in (
            "mmap",
            "offload_to_cpu",
            "clip_on_cpu",
            "vae_on_cpu",
            "control_net_cpu",
            "stream_layers",
            "flash_attention",
            "diffusion_flash_attention",
            "diffusion_conv_direct",
            "vae_conv_direct",
        ):
            try:
                as_bool(runtime.get(key), key in {"mmap", "diffusion_flash_attention"})
            except JobError as exc:
                failures.append(f"runtime.{key}: {exc}")
        try:
            as_int(runtime.get("threads"), 0)
            as_int(runtime.get("timeout_seconds"), 300)
            as_float(runtime.get("max_vram_gib"), 0.0)
        except JobError as exc:
            failures.append(f"runtime: {exc}")

    metadata = job.get("metadata", {})
    if metadata is not None and not isinstance(metadata, dict):
        failures.append("metadata must be an object")

    return failures


def default_output_name() -> str:
    return f"tcxsd_job_{int(time.time() * 1000)}"


def resolve_user_path(job: Mapping[str, Any], key: str) -> pathlib.Path | None:
    value = job.get(key)
    if not value:
        return None
    path = pathlib.Path(str(value))
    if path.is_absolute():
        return path
    job_dir = job.get("_job_dir")
    if isinstance(job_dir, pathlib.Path):
        return (job_dir / path).resolve()
    return path.resolve()


def resolve_job(job: Mapping[str, Any], addon_root: pathlib.Path = ADDON_ROOT) -> ResolvedJob:
    failures = validate_job(job)
    if failures:
        raise JobError("; ".join(failures))

    registry = tcxsd_models.load_model_registry()
    model_id = str(job.get("model", "ideogram4-q4_0"))
    model = registry.model(model_id)

    model_dir = resolve_user_path(job, "model_dir") or setup_sd.model_target_dir(model, None)
    output_dir = resolve_user_path(job, "output_dir") or (addon_root / "examples" / model.example / "outputs" / "jobs").resolve()
    output_name = str(job.get("output_name") or default_output_name())
    output_path = output_dir / f"{output_name}.png"
    sidecar_path = output_dir / f"{output_name}.json"
    log_path = output_dir / f"{output_name}.log"

    native_dir = resolve_user_path(job, "native_dir") or setup_sd.CURRENT_DIR
    cli_path = resolve_user_path(job, "cli") or setup_sd.find_cli_file(native_dir)
    if not cli_path or not cli_path.exists():
        raise JobError(f"sd-cli was not found under {native_dir}")

    asset_paths: Dict[str, pathlib.Path] = {}
    for asset in model.files:
        asset_path = model_dir / asset.filename
        if asset.required and (not asset_path.exists() or asset_path.stat().st_size <= 0):
            raise JobError(f"missing model asset for role '{asset.role}': {asset_path}")
        asset_paths[asset.role] = asset_path

    return ResolvedJob(
        job=job,
        model=model,
        model_dir=model_dir,
        output_dir=output_dir,
        output_name=output_name,
        output_path=output_path,
        sidecar_path=sidecar_path,
        log_path=log_path,
        cli_path=cli_path,
        asset_paths=asset_paths,
    )


def _add_bool(args: List[str], key: str, enabled: bool) -> None:
    if enabled:
        args.append(key)


def build_sd_cli_args(resolved: ResolvedJob) -> List[str]:
    job = resolved.job
    runtime = job.get("runtime", {}) or {}
    args: List[str] = [str(resolved.cli_path)]

    for role, path in resolved.asset_paths.items():
        cli_arg = ROLE_TO_CLI_ARG.get(role)
        if cli_arg:
            args.extend([cli_arg, str(path)])

    args.extend(["-p", prompt_text(job)])
    negative_prompt = job.get("negative_prompt")
    if isinstance(negative_prompt, str) and negative_prompt:
        args.extend(["-n", negative_prompt])

    args.extend([
        "--cfg-scale",
        str(job.get("cfg_scale", 1.0)),
        "--steps",
        str(job.get("steps", 8)),
        "-W",
        str(job.get("width", 1024)),
        "-H",
        str(job.get("height", 1024)),
    ])

    if as_int(job.get("seed"), -1) >= 0:
        args.extend(["--seed", str(job.get("seed"))])
    if job.get("sampler"):
        args.extend(["--sampling-method", str(job["sampler"])])

    if runtime.get("backend"):
        args.extend(["--backend", str(runtime["backend"])])
    if runtime.get("params_backend"):
        args.extend(["--params-backend", str(runtime["params_backend"])])
    if as_int(runtime.get("threads"), 0) > 0:
        args.extend(["--threads", str(runtime["threads"])])
    if as_float(runtime.get("max_vram_gib"), 0.0) != 0.0:
        args.extend(["--max-vram", str(runtime["max_vram_gib"])])

    _add_bool(args, "--mmap", as_bool(runtime.get("mmap"), True))
    _add_bool(args, "--offload-to-cpu", as_bool(runtime.get("offload_to_cpu"), False))
    _add_bool(args, "--clip-on-cpu", as_bool(runtime.get("clip_on_cpu"), False))
    _add_bool(args, "--vae-on-cpu", as_bool(runtime.get("vae_on_cpu"), False))
    _add_bool(args, "--control-net-cpu", as_bool(runtime.get("control_net_cpu"), False))
    _add_bool(args, "--stream-layers", as_bool(runtime.get("stream_layers"), False))
    _add_bool(args, "--fa", as_bool(runtime.get("flash_attention"), False))
    _add_bool(args, "--diffusion-fa", as_bool(runtime.get("diffusion_flash_attention"), True))
    _add_bool(args, "--diffusion-conv-direct", as_bool(runtime.get("diffusion_conv_direct"), False))
    _add_bool(args, "--vae-conv-direct", as_bool(runtime.get("vae_conv_direct"), False))

    args.extend(["-v", "-o", str(resolved.output_path)])
    return args


def read_png_size(path: pathlib.Path) -> tuple[int, int] | None:
    try:
        with path.open("rb") as handle:
            header = handle.read(24)
    except OSError:
        return None
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        return None
    width = int.from_bytes(header[16:20], "big")
    height = int.from_bytes(header[20:24], "big")
    return width, height


def default_process_runner(args: Sequence[str], cwd: pathlib.Path, log_path: pathlib.Path, timeout: int) -> int:
    with log_path.open("wb") as log:
        completed = subprocess.run(
            list(args),
            cwd=str(cwd),
            stdout=log,
            stderr=subprocess.STDOUT,
            timeout=timeout if timeout > 0 else None,
        )
    return completed.returncode


def sidecar_for_result(
    resolved: ResolvedJob,
    ok: bool,
    state: str,
    duration_seconds: float,
    error: str = "",
    exit_code: int | None = None,
) -> Dict[str, Any]:
    job = resolved.job
    runtime = job.get("runtime", {}) or {}
    metadata = dict(job.get("metadata", {}) or {})
    metadata.update({
        "prompt": prompt_text(job),
        "negative_prompt": str(job.get("negative_prompt", "")),
        "width": str(job.get("width", 1024)),
        "height": str(job.get("height", 1024)),
        "steps": str(job.get("steps", 8)),
        "seed": str(job.get("seed", -1)),
        "cfg_scale": str(job.get("cfg_scale", 1.0)),
        "sampler": str(job.get("sampler", "euler")),
        "execution_mode": "cli_process",
        "backend": str(runtime.get("backend", "auto")),
        "runtime_execution_mode": "cli_process",
        "offload_params_to_cpu": "true" if as_bool(runtime.get("offload_to_cpu"), False) else "false",
        "diffusion_flash_attention": "true" if as_bool(runtime.get("diffusion_flash_attention"), True) else "false",
        "model": resolved.model.id,
        "model_family": resolved.model.family,
        "backend_log": str(resolved.log_path),
        "cli_log": str(resolved.log_path),
        "cli_executable": str(resolved.cli_path),
        "duration_seconds": f"{duration_seconds:.6f}",
    })
    if exit_code is not None:
        metadata["cli_exit_code"] = str(exit_code)
    for role, path in resolved.asset_paths.items():
        metadata[ROLE_TO_METADATA_KEY.get(role, f"{role}_path")] = str(path)

    image_size = read_png_size(resolved.output_path) if ok else None
    sidecar: Dict[str, Any] = {
        "job_id": int(job.get("job_id", 1) or 1),
        "job_label": resolved.output_name,
        "ok": ok,
        "state": state,
        "error": error,
        "duration_seconds": round(duration_seconds, 3),
        "native_output_path": str(resolved.output_path),
        "metadata": metadata,
    }
    if ok:
        sidecar["saved_image_path"] = str(resolved.output_path)
        if image_size:
            sidecar["image_width"] = image_size[0]
            sidecar["image_height"] = image_size[1]
        else:
            sidecar["image_width"] = int(job.get("width", 0) or 0)
            sidecar["image_height"] = int(job.get("height", 0) or 0)
    sidecar["sidecar_path"] = str(resolved.sidecar_path)
    return sidecar


def write_sidecar(path: pathlib.Path, data: Mapping[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def run_job(
    job: Mapping[str, Any],
    addon_root: pathlib.Path = ADDON_ROOT,
    process_runner: RunProcess = default_process_runner,
    server_runner: RunServerJob | None = None,
) -> Dict[str, Any]:
    resolved = resolve_job(job, addon_root=addon_root)
    runtime = job.get("runtime", {}) or {}
    execution_mode = str(runtime.get("execution_mode", "")).strip().lower() if isinstance(runtime, dict) else ""
    if execution_mode in {"persistent_server", "server", "sd_server"}:
        if server_runner is None:
            import tcxsd_server
            server_runner = tcxsd_server.run_resolved_job
        sidecar = server_runner(resolved)
        write_sidecar(resolved.sidecar_path, sidecar)
        return sidecar

    resolved.output_dir.mkdir(parents=True, exist_ok=True)
    args = build_sd_cli_args(resolved)
    timeout = as_int((job.get("runtime", {}) or {}).get("timeout_seconds"), 300)
    started = time.perf_counter()
    exit_code = 1
    error = ""
    ok = False

    try:
        exit_code = process_runner(args, resolved.cli_path.parent, resolved.log_path, timeout)
        ok = exit_code == 0 and resolved.output_path.exists()
        if exit_code != 0:
            error = f"sd-cli failed with exit code {exit_code}. Log: {resolved.log_path}"
        elif not resolved.output_path.exists():
            error = f"sd-cli did not create output image: {resolved.output_path}. Log: {resolved.log_path}"
    except subprocess.TimeoutExpired as exc:
        error = f"sd-cli timed out after {timeout} seconds. Log: {resolved.log_path}"
        exit_code = 124
    except OSError as exc:
        error = f"Failed to run sd-cli: {exc}. Log: {resolved.log_path}"
        exit_code = 1

    duration = time.perf_counter() - started
    sidecar = sidecar_for_result(
        resolved,
        ok=ok,
        state="complete" if ok else "failed",
        duration_seconds=duration,
        error=error,
        exit_code=exit_code,
    )
    write_sidecar(resolved.sidecar_path, sidecar)
    return sidecar


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run a tcxStableDiffusion JSON job through sd-cli")
    sub = parser.add_subparsers(dest="command", required=True)

    validate = sub.add_parser("validate", help="Validate a job JSON file and local paths")
    validate.add_argument("path", type=pathlib.Path)

    run = sub.add_parser("run", help="Run a job JSON file")
    run.add_argument("path", type=pathlib.Path)

    args_cmd = sub.add_parser("args", help="Print the sd-cli argument list for a job")
    args_cmd.add_argument("path", type=pathlib.Path)

    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        job = load_job(args.path)
        resolved = resolve_job(job)
    except (JobError, KeyError) as exc:
        print(f"[FAIL] {exc}")
        return 1

    if args.command == "validate":
        print(f"[OK] job is valid: {resolved.model.id} -> {resolved.output_path}")
        return 0

    if args.command == "args":
        for item in build_sd_cli_args(resolved):
            print(item)
        return 0

    if args.command == "run":
        sidecar = run_job(job)
        print(json.dumps({
            "ok": sidecar.get("ok"),
            "state": sidecar.get("state"),
            "output": sidecar.get("saved_image_path"),
            "sidecar": sidecar.get("sidecar_path"),
            "error": sidecar.get("error"),
        }, ensure_ascii=False, indent=2))
        return 0 if sidecar.get("ok") else 1

    return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
