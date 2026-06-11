from __future__ import annotations

from typing import Any, Dict, List


UNKNOWN = "UNKNOWN"
CUDA_OOM = "CUDA_OOM"
MODEL_ASSET_MISSING = "MODEL_ASSET_MISSING"
SERVER_START_FAILED = "SERVER_START_FAILED"
BACKEND_UNSUPPORTED = "BACKEND_UNSUPPORTED"
CANCEL_NOT_INTERRUPTIBLE = "CANCEL_NOT_INTERRUPTIBLE"
OUTPUT_MISSING = "OUTPUT_MISSING"
TIMEOUT = "TIMEOUT"


HINTS: Dict[str, List[str]] = {
    CUDA_OOM: [
        "Use runtime.preset=low_vram or RuntimeSettings::lowVramCuda().",
        "Reduce width, height, steps, or batch_count before retrying.",
        "Set params_backend=cpu, offload_to_cpu=true, stream_layers=true, and a max_vram_gib budget.",
        "Close other GPU-heavy apps and retry after the CUDA memory pool is released.",
    ],
    MODEL_ASSET_MISSING: [
        "Run python tools/setup_sd.py download-model --model <model-id>.",
        "Check model_dir points at the folder containing every required model asset.",
        "Run python tools/verify_sd.py --model <model-id> to list missing files.",
    ],
    SERVER_START_FAILED: [
        "Confirm libs/stable-diffusion/current/bin/sd-server.exe exists.",
        "Check the server log path in the sidecar metadata for the upstream error.",
        "Try a different server_port or stop any existing process using that port.",
    ],
    BACKEND_UNSUPPORTED: [
        "Switch runtime.execution_mode to persistent_server for this request.",
        "Remove the unsupported request field for the selected backend.",
        "Use tools/tcxsd_server.py request <job.json> to inspect the supported server body.",
    ],
    CANCEL_NOT_INTERRUPTIBLE: [
        "The cancel request was sent, but upstream generation may finish the active step first.",
        "Use a shorter timeout_seconds value for long jobs.",
        "For immediate recovery, stop the managed sd-server process and restart it.",
    ],
    OUTPUT_MISSING: [
        "Open the backend log path recorded in the sidecar metadata.",
        "Check output_root/output_dir permissions and free disk space.",
        "Retry with a shorter output path if the backend log mentions path issues.",
    ],
    TIMEOUT: [
        "Increase runtime.timeout_seconds for large images or final quality jobs.",
        "Use draft quality first to confirm the model and prompt path work.",
        "If timeout repeats, lower image size or switch to low_vram runtime preset.",
    ],
    UNKNOWN: [
        "Check the backend log path recorded in the sidecar metadata.",
        "Run the job with tools/tcxsd_job.py args or tools/tcxsd_server.py request to inspect resolved inputs.",
    ],
}


def classify_error(message: str) -> str:
    text = (message or "").lower()
    if not text:
        return UNKNOWN
    if "out of memory" in text or "cuda oom" in text or "cuda_error_out_of_memory" in text:
        return CUDA_OOM
    if "missing model asset" in text or "does not exist" in text or "was not found" in text:
        return MODEL_ASSET_MISSING
    if "sd-server did not become ready" in text or "failed to start sd-server" in text or "server is not reachable" in text:
        return SERVER_START_FAILED
    if "backend_unsupported" in text or "unsupported" in text or "not supported" in text:
        return BACKEND_UNSUPPORTED
    if "may not interrupt active generation" in text or "cancel" in text and "interrupt" in text:
        return CANCEL_NOT_INTERRUPTIBLE
    if "did not create output image" in text or "no image payload" in text or "returned no images" in text:
        return OUTPUT_MISSING
    if "timed out" in text or "timeout" in text:
        return TIMEOUT
    return UNKNOWN


def remediation_hints(code: str) -> List[str]:
    return list(HINTS.get(code, HINTS[UNKNOWN]))


def error_payload(message: str, code: str | None = None) -> Dict[str, Any]:
    resolved = code or classify_error(message)
    return {
        "code": resolved,
        "message": message,
        "remediation_hints": remediation_hints(resolved),
    }


def unsupported_backend_message(backend: str, feature: str, supported_backend: str) -> str:
    return (
        f"{BACKEND_UNSUPPORTED}: {feature} is not supported by {backend}. "
        f"Use {supported_backend} for this request."
    )
