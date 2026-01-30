#!/usr/bin/env python3
"""
Quick benchmarking runner for the raytracer.

Runs a small sweep (five resolutions by default) that finishes in a few
seconds and writes results to CSV using the extended instrumentation
(warmup, thread pinning, GPU timing). By default it tests every logical
CPU thread count from 1..N on the host.

Each run writes a single full CSV (all fields from the renderer) with a timestamped
name by default. It also records every individual attempt (including warmups) to
a raw per-run CSV.

Extra object counts default to three levels: small, medium, large.
"""

from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import sys
from datetime import datetime


def default_executable() -> pathlib.Path:
    root = pathlib.Path(__file__).resolve().parents[1]
    return root / "build-hpc" / "raytracer"


def detect_hw_threads() -> int:
    count = os.cpu_count()
    return count if count and count > 0 else 1


def thread_list_upto(n: int) -> str:
    return ",".join(str(i) for i in range(1, n + 1))


def parse_args() -> argparse.Namespace:
    hw_threads = detect_hw_threads()
    default_threads = thread_list_upto(hw_threads)
    parser = argparse.ArgumentParser(description="Run a quick benchmark sweep.")
    parser.add_argument(
        "--exe",
        type=pathlib.Path,
        default=default_executable(),
        help="Path to the raytracer executable (default: build-hpc/raytracer).",
    )
    parser.add_argument(
        "--csv",
        type=pathlib.Path,
        default=None,
        help="Full CSV output path (deprecated alias for --csv-full). Default now timestamped.",
    )
    parser.add_argument(
        "--csv-full",
        type=pathlib.Path,
        default=None,
        help="Full CSV output path (default: benchmark_results_<timestamp>.csv).",
    )
    parser.add_argument(
        "--csv-raw",
        type=pathlib.Path,
        default=None,
        help="Raw per-run CSV (includes warmups). Default: benchmark_runs_<timestamp>.csv.",
    )
    parser.add_argument(
        "--sizes",
        default="320x180,1280x720,1920x1080",
        help="Comma-separated resolutions. Default runs five sizes: 320x180,640x360,800x450,1280x720,1920x1080.",
    )
    parser.add_argument(
        "--samples",
        default="10,20",
        help="Comma-separated samples-per-pixel values.",
    )
    parser.add_argument(
        "--depths",
        default="10,20",
        help="Comma-separated max-depth values.",
    )
    parser.add_argument(
        "--threads",
        default=default_threads,
        help=f"Comma-separated thread counts for MT mode (default: 1..{hw_threads}).",
    )
    parser.add_argument(
        "--repeats",
        default="2",
        help="Minimum repeats per configuration.",
    )
    parser.add_argument(
        "--min-runtime-ms",
        default="80",
        help="Minimum cumulative render time before stopping repeats.",
    )
    parser.add_argument(
        "--warmup",
        default="1",
        help="Number of unrecorded warmup runs.",
    )
    parser.add_argument(
        "--extra-objects",
        default="0,100",
        help="Comma-separated extra object counts (default small/medium/large: 0,25,100).",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Silence progress output from the renderer.",
    )
    return parser.parse_args()




def main() -> int:
    args = parse_args()
    exe = args.exe
    if not exe.exists():
        print(f"Executable not found: {exe}", file=sys.stderr)
        return 1

    ts = datetime.now().strftime("%Y%m%dT%H%M%S")
    root = pathlib.Path(__file__).resolve().parents[1]
    csv_full = args.csv or args.csv_full or (root / f"benchmark_results_{ts}.csv")
    csv_raw = args.csv_raw or (root / f"benchmark_runs_{ts}.csv")

    cmd = [
        str(exe),
        "--benchmark",
        "--bench-sizes",
        args.sizes,
        "--bench-samples",
        args.samples,
        "--bench-depths",
        args.depths,
        "--bench-thread-counts",
        args.threads,
        "--bench-repeats",
        args.repeats,
        "--bench-min-runtime-ms",
        args.min_runtime_ms,
        "--bench-warmup",
        args.warmup,
        "--bench-objects",
        args.extra_objects,
        "--bench-csv",
        str(csv_full),
        "--bench-raw-csv",
        str(csv_raw),
        "--bench-pin-threads",
        "--bench-gpu-timing",
    ]
    if args.quiet:
        cmd.append("--quiet")

    print("Running:", " ".join(cmd))
    subprocess.run(cmd, check=True)
    print(f"[done] Full results: {csv_full}")
    print(f"[done] Raw runs:    {csv_raw}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
