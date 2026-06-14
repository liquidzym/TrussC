from __future__ import annotations

import argparse
import json
import pathlib
import time
from dataclasses import dataclass
from typing import Any, Iterable, List, Mapping, Sequence


ADDON_ROOT = pathlib.Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class StorageRoots:
    output_root: pathlib.Path
    temp_root: pathlib.Path
    cache_root: pathlib.Path


def _job_dir(job: Mapping[str, Any]) -> pathlib.Path:
    value = job.get("_job_dir")
    return value if isinstance(value, pathlib.Path) else pathlib.Path.cwd()


def _resolve(value: Any, base: pathlib.Path) -> pathlib.Path:
    path = pathlib.Path(str(value))
    if path.is_absolute():
        return path.resolve()
    return (base / path).resolve()


def resolve_storage_roots(job: Mapping[str, Any] | None = None, addon_root: pathlib.Path = ADDON_ROOT) -> StorageRoots:
    job = job or {}
    runtime = job.get("runtime", {}) or {}
    runtime = runtime if isinstance(runtime, Mapping) else {}
    base = _job_dir(job)

    model_example = "ideogram4-basic"
    output_default = addon_root / "examples" / model_example / "outputs"

    output_value = job.get("output_root") or runtime.get("output_root") or output_default
    output_root = _resolve(output_value, base)
    temp_root = _resolve(runtime.get("temp_root") or job.get("temp_root") or output_root / "tmp", base)
    cache_root = _resolve(runtime.get("cache_root") or job.get("cache_root") or output_root / "cache", base)
    return StorageRoots(output_root=output_root, temp_root=temp_root, cache_root=cache_root)


def _iter_cleanup_candidates(roots: StorageRoots) -> Iterable[pathlib.Path]:
    patterns = (
        (roots.output_root, ("*.json", "*.log")),
        (roots.temp_root, ("*.tmp", "*.part", "*.png", "*.json", "*.log")),
        (roots.cache_root, ("*.tmp", "*.part")),
    )
    for root, root_patterns in patterns:
        if not root.exists():
            continue
        for pattern in root_patterns:
            yield from root.rglob(pattern)


def _is_within(path: pathlib.Path, root: pathlib.Path) -> bool:
    try:
        path.resolve().relative_to(root.resolve())
        return True
    except ValueError:
        return False


def cleanup_storage(roots: StorageRoots, older_than_seconds: int = 24 * 60 * 60, dry_run: bool = True) -> List[str]:
    cutoff = time.time() - max(0, int(older_than_seconds))
    removed: List[str] = []
    allowed_roots = (roots.output_root, roots.temp_root, roots.cache_root)

    for candidate in _iter_cleanup_candidates(roots):
        if not candidate.is_file():
            continue
        if not any(_is_within(candidate, root) for root in allowed_roots):
            continue
        if candidate.stat().st_mtime > cutoff:
            continue
        removed.append(str(candidate))
        if not dry_run:
            candidate.unlink(missing_ok=True)
    return removed


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Inspect and clean tcxStableDiffusion storage roots")
    sub = parser.add_subparsers(dest="command", required=True)

    roots = sub.add_parser("roots", help="Print resolved output/temp/cache roots")
    roots.add_argument("--job-dir", type=pathlib.Path, default=pathlib.Path.cwd())
    roots.add_argument("--output-root")
    roots.add_argument("--temp-root")
    roots.add_argument("--cache-root")

    cleanup = sub.add_parser("cleanup", help="Remove old sidecars, native logs, and temp outputs")
    cleanup.add_argument("--job-dir", type=pathlib.Path, default=pathlib.Path.cwd())
    cleanup.add_argument("--output-root")
    cleanup.add_argument("--temp-root")
    cleanup.add_argument("--cache-root")
    cleanup.add_argument("--older-than-seconds", type=int, default=24 * 60 * 60)
    cleanup.add_argument("--dry-run", action="store_true")
    return parser


def _job_from_args(args: argparse.Namespace) -> Mapping[str, Any]:
    runtime = {}
    if args.temp_root:
        runtime["temp_root"] = args.temp_root
    if args.cache_root:
        runtime["cache_root"] = args.cache_root
    return {
        "_job_dir": args.job_dir,
        "output_root": args.output_root,
        "runtime": runtime,
    }


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    roots = resolve_storage_roots(_job_from_args(args))
    if args.command == "roots":
        print(json.dumps({
            "output_root": str(roots.output_root),
            "temp_root": str(roots.temp_root),
            "cache_root": str(roots.cache_root),
        }, ensure_ascii=False, indent=2))
        return 0
    if args.command == "cleanup":
        removed = cleanup_storage(roots, older_than_seconds=args.older_than_seconds, dry_run=args.dry_run)
        print(json.dumps({"dry_run": args.dry_run, "removed": removed}, ensure_ascii=False, indent=2))
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
