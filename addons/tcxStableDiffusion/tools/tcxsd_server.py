from __future__ import annotations

import argparse
import base64
import json
import pathlib
import subprocess
import sys
import time
import urllib.error
import urllib.request
from dataclasses import dataclass
from typing import Any, Dict, Mapping, Sequence

import setup_sd
import tcxsd_job


ADDON_ROOT = pathlib.Path(__file__).resolve().parents[1]
TERMINAL_STATES = {"completed", "failed", "cancelled"}


@dataclass
class ServerProcess:
    process: subprocess.Popen[bytes] | None
    log_path: pathlib.Path

    def close(self) -> None:
        if not self.process:
            return
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=10)


def runtime_value(job: Mapping[str, Any], key: str, default: Any = None) -> Any:
    runtime = job.get("runtime", {}) or {}
    return runtime.get(key, default) if isinstance(runtime, dict) else default


def server_host(job: Mapping[str, Any]) -> str:
    return str(runtime_value(job, "server_host", "127.0.0.1"))


def server_port(job: Mapping[str, Any]) -> int:
    return tcxsd_job.as_int(runtime_value(job, "server_port", 1234), 1234)


def server_url(job: Mapping[str, Any]) -> str:
    return f"http://{server_host(job)}:{server_port(job)}"


def resolve_runtime_path(job: Mapping[str, Any], key: str) -> pathlib.Path | None:
    value = runtime_value(job, key)
    if not value:
        return None
    path = pathlib.Path(str(value))
    if path.is_absolute():
        return path
    job_dir = job.get("_job_dir")
    if isinstance(job_dir, pathlib.Path):
        return (job_dir / path).resolve()
    return path.resolve()


def _add_bool(args: list[str], key: str, enabled: bool) -> None:
    if enabled:
        args.append(key)


def build_server_args(resolved: tcxsd_job.ResolvedJob) -> list[str]:
    job = resolved.job
    runtime = job.get("runtime", {}) or {}
    server_path = setup_sd.find_server_file(resolved.cli_path.parents[1])
    if runtime.get("server"):
        server_path = pathlib.Path(str(runtime["server"]))
    if not server_path or not server_path.exists():
        raise tcxsd_job.JobError(f"sd-server was not found near {resolved.cli_path}")

    args = [
        str(server_path),
        "--listen-ip",
        server_host(job),
        "--listen-port",
        str(server_port(job)),
    ]
    for role, path in resolved.asset_paths.items():
        cli_arg = tcxsd_job.ROLE_TO_CLI_ARG.get(role)
        if cli_arg:
            args.extend([cli_arg, str(path)])

    if runtime.get("backend"):
        args.extend(["--backend", str(runtime["backend"])])
    if runtime.get("params_backend"):
        args.extend(["--params-backend", str(runtime["params_backend"])])
    lora_dir = resolve_runtime_path(job, "lora_model_dir")
    if lora_dir:
        args.extend(["--lora-model-dir", str(lora_dir)])
    upscaler_dir = resolve_runtime_path(job, "hires_upscalers_dir")
    if upscaler_dir:
        args.extend(["--hires-upscalers-dir", str(upscaler_dir)])
    if tcxsd_job.as_int(runtime.get("threads"), 0) > 0:
        args.extend(["--threads", str(runtime["threads"])])
    if tcxsd_job.as_float(runtime.get("max_vram_gib"), 0.0) != 0.0:
        args.extend(["--max-vram", str(runtime["max_vram_gib"])])

    _add_bool(args, "--mmap", tcxsd_job.as_bool(runtime.get("mmap"), True))
    _add_bool(args, "--offload-to-cpu", tcxsd_job.as_bool(runtime.get("offload_to_cpu"), False))
    _add_bool(args, "--clip-on-cpu", tcxsd_job.as_bool(runtime.get("clip_on_cpu"), False))
    _add_bool(args, "--vae-on-cpu", tcxsd_job.as_bool(runtime.get("vae_on_cpu"), False))
    _add_bool(args, "--control-net-cpu", tcxsd_job.as_bool(runtime.get("control_net_cpu"), False))
    _add_bool(args, "--stream-layers", tcxsd_job.as_bool(runtime.get("stream_layers"), False))
    _add_bool(args, "--fa", tcxsd_job.as_bool(runtime.get("flash_attention"), False))
    _add_bool(args, "--diffusion-fa", tcxsd_job.as_bool(runtime.get("diffusion_flash_attention"), True))
    _add_bool(args, "--diffusion-conv-direct", tcxsd_job.as_bool(runtime.get("diffusion_conv_direct"), False))
    _add_bool(args, "--vae-conv-direct", tcxsd_job.as_bool(runtime.get("vae_conv_direct"), False))
    args.append("-v")
    return args


def _file_to_data_url(path: pathlib.Path | None) -> str | None:
    if not path:
        return None
    data = path.read_bytes()
    return "data:image/png;base64," + base64.b64encode(data).decode("ascii")


def _maybe_image(job: Mapping[str, Any], key: str) -> str | None:
    value = job.get(key)
    if not value:
        return None
    text = str(value)
    if text.startswith("data:") or len(text) > 128 and not pathlib.Path(text).exists():
        return text
    path = tcxsd_job.resolve_user_path(job, key)
    return _file_to_data_url(path)


def sdcpp_request_body(job: Mapping[str, Any]) -> Dict[str, Any]:
    body: Dict[str, Any] = {
        "prompt": tcxsd_job.prompt_text(job),
        "negative_prompt": str(job.get("negative_prompt", "")),
        "width": tcxsd_job.as_int(job.get("width"), 1024),
        "height": tcxsd_job.as_int(job.get("height"), 1024),
        "strength": tcxsd_job.as_float(job.get("strength"), 0.75),
        "seed": tcxsd_job.as_int(job.get("seed"), -1),
        "batch_count": tcxsd_job.as_int(job.get("batch_count"), 1),
        "control_strength": tcxsd_job.as_float(job.get("control_strength"), 1.0),
        "sample_params": {
            "sample_steps": tcxsd_job.as_int(job.get("steps"), 8),
            "guidance": {
                "txt_cfg": tcxsd_job.as_float(job.get("cfg_scale"), 1.0),
            },
        },
        "lora": [],
        "output_format": "png",
        "output_compression": 100,
    }
    sampler = job.get("sampler")
    if sampler:
        body["sample_params"]["sample_method"] = str(sampler)
    for request_key, server_key in (
        ("init_image", "init_image"),
        ("mask_image", "mask_image"),
        ("control_image", "control_image"),
    ):
        image = _maybe_image(job, request_key)
        if image:
            body[server_key] = image
    for lora in job.get("loras", []) or []:
        if not isinstance(lora, Mapping):
            raise tcxsd_job.JobError("loras entries must be objects")
        path = lora.get("path")
        if not path:
            raise tcxsd_job.JobError("lora.path is required")
        body["lora"].append({
            "path": str(path),
            "multiplier": tcxsd_job.as_float(lora.get("weight", lora.get("multiplier")), 1.0),
            "is_high_noise": tcxsd_job.as_bool(lora.get("is_high_noise"), False),
        })
    return body


def http_json(method: str, url: str, body: Mapping[str, Any] | None = None, timeout: int = 10) -> Dict[str, Any]:
    data = None
    headers = {"Accept": "application/json"}
    if body is not None:
        data = json.dumps(body, ensure_ascii=False).encode("utf-8")
        headers["Content-Type"] = "application/json"
    request = urllib.request.Request(url, data=data, method=method, headers=headers)
    with urllib.request.urlopen(request, timeout=timeout) as response:
        payload = response.read().decode("utf-8")
    return json.loads(payload) if payload else {}


def wait_for_server(url: str, timeout_seconds: int) -> None:
    deadline = time.monotonic() + timeout_seconds
    last_error: BaseException | None = None
    while time.monotonic() < deadline:
        try:
            http_json("GET", f"{url}/sdcpp/v1/capabilities", timeout=3)
            return
        except BaseException as exc:  # noqa: BLE001 - health polling reports the final connection error.
            last_error = exc
            time.sleep(0.5)
    raise tcxsd_job.JobError(f"sd-server did not become ready at {url}: {last_error}")


def start_server(resolved: tcxsd_job.ResolvedJob) -> ServerProcess:
    args = build_server_args(resolved)
    log_path = resolved.output_dir / f"{resolved.output_name}.server.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log = log_path.open("wb")
    try:
        process = subprocess.Popen(args, cwd=str(pathlib.Path(args[0]).parent), stdout=log, stderr=subprocess.STDOUT)
    finally:
        log.close()
    return ServerProcess(process=process, log_path=log_path)


def run_against_server(resolved: tcxsd_job.ResolvedJob, url: str) -> Dict[str, Any]:
    started = time.perf_counter()
    timeout = tcxsd_job.as_int(runtime_value(resolved.job, "timeout_seconds", 300), 300)
    submit = http_json("POST", f"{url}/sdcpp/v1/img_gen", sdcpp_request_body(resolved.job), timeout=30)
    poll_url = str(submit.get("poll_url") or f"/sdcpp/v1/jobs/{submit.get('id', '')}")
    status = str(submit.get("status", "queued"))
    job_state: Dict[str, Any] = submit
    deadline = time.monotonic() + timeout

    while status not in TERMINAL_STATES:
        if time.monotonic() >= deadline:
            raise subprocess.TimeoutExpired("sd-server job", timeout)
        time.sleep(0.5)
        job_state = http_json("GET", f"{url}{poll_url}", timeout=30)
        status = str(job_state.get("status", "failed"))

    ok = status == "completed"
    error = ""
    if ok:
        images = (((job_state.get("result") or {}).get("images")) or [])
        if not images:
            ok = False
            error = "sd-server completed but returned no images"
        else:
            encoded = str(images[0].get("b64_json", ""))
            resolved.output_path.parent.mkdir(parents=True, exist_ok=True)
            resolved.output_path.write_bytes(base64.b64decode(encoded))
    else:
        error_obj = job_state.get("error") or {}
        error = str(error_obj.get("message") or error_obj.get("code") or f"sd-server job {status}")

    duration = time.perf_counter() - started
    sidecar = tcxsd_job.sidecar_for_result(
        resolved,
        ok=ok,
        state="complete" if ok else ("cancelled" if status == "cancelled" else "failed"),
        duration_seconds=duration,
        error=error,
        exit_code=None,
    )
    sidecar["metadata"]["execution_mode"] = "persistent_server"
    sidecar["metadata"]["runtime_execution_mode"] = "persistent_server"
    sidecar["metadata"]["backend_log"] = str(resolved.log_path)
    sidecar["metadata"]["server_log"] = str(resolved.log_path)
    sidecar["metadata"]["server_url"] = url
    sidecar["metadata"]["server_job_id"] = str(job_state.get("id", submit.get("id", "")))
    sidecar["metadata"]["server_status"] = status
    return sidecar


def run_resolved_job(resolved: tcxsd_job.ResolvedJob) -> Dict[str, Any]:
    url = server_url(resolved.job)
    process: ServerProcess | None = None
    if not tcxsd_job.as_bool(runtime_value(resolved.job, "reuse_server", False), False):
        process = start_server(resolved)
        wait_for_server(url, tcxsd_job.as_int(runtime_value(resolved.job, "server_startup_timeout_seconds", 120), 120))
    else:
        wait_for_server(url, tcxsd_job.as_int(runtime_value(resolved.job, "server_startup_timeout_seconds", 10), 10))
    try:
        return run_against_server(resolved, url)
    finally:
        if process and not tcxsd_job.as_bool(runtime_value(resolved.job, "keep_server_running", False), False):
            process.close()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run tcxStableDiffusion jobs through persistent sd-server")
    sub = parser.add_subparsers(dest="command", required=True)
    args_cmd = sub.add_parser("server-args", help="Print sd-server startup arguments for a job")
    args_cmd.add_argument("path", type=pathlib.Path)
    request_cmd = sub.add_parser("request", help="Print the /sdcpp/v1/img_gen JSON body for a job")
    request_cmd.add_argument("path", type=pathlib.Path)
    run_cmd = sub.add_parser("run", help="Run a JSON job through sd-server")
    run_cmd.add_argument("path", type=pathlib.Path)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        job = tcxsd_job.load_job(args.path)
        resolved = tcxsd_job.resolve_job(job)
        if args.command == "server-args":
            for item in build_server_args(resolved):
                print(item)
            return 0
        if args.command == "request":
            print(json.dumps(sdcpp_request_body(job), ensure_ascii=False, indent=2))
            return 0
        if args.command == "run":
            sidecar = run_resolved_job(resolved)
            tcxsd_job.write_sidecar(resolved.sidecar_path, sidecar)
            print(json.dumps({
                "ok": sidecar.get("ok"),
                "state": sidecar.get("state"),
                "output": sidecar.get("saved_image_path"),
                "sidecar": sidecar.get("sidecar_path"),
                "error": sidecar.get("error"),
            }, ensure_ascii=False, indent=2))
            return 0 if sidecar.get("ok") else 1
    except (tcxsd_job.JobError, KeyError, urllib.error.URLError, subprocess.TimeoutExpired) as exc:
        print(f"[FAIL] {exc}")
        return 1
    return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
