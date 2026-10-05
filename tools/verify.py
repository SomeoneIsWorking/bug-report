#!/usr/bin/env python3
"""bug-report's verifier: clang-format check, Clang build, clang-tidy, CTest."""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CXX_SOURCES = (
    "include/bug_report/report.h",
    "include/bug_report/draft.h",
    "include/bug_report/form.h",
    "src/report.cpp",
    "src/draft.cpp",
    "src/form.cpp",
    "tests/test_bug_report.cpp",
)
TIDY_SOURCES = tuple(path for path in CXX_SOURCES if path.endswith(".cpp"))


def run(command: list[str]) -> None:
    print("verify:", " ".join(command))
    subprocess.run(command, cwd=ROOT, check=True)


def require(tool: str) -> None:
    if not shutil.which(tool):
        raise SystemExit(f"verify: {tool} is required")


def main() -> int:
    for tool in ("cmake", "ninja", "clang++", "clang-format", "clang-tidy"):
        require(tool)
    run(["clang-format", "--dry-run", "--Werror", *CXX_SOURCES])
    if not (ROOT / "build" / "CMakeCache.txt").is_file():
        run(
            [
                "cmake",
                "-S",
                ".",
                "-B",
                "build",
                "-G",
                "Ninja",
                "-DCMAKE_CXX_COMPILER=clang++",
                "-DCMAKE_BUILD_TYPE=Debug",
            ]
        )
    run(["cmake", "--build", "build"])
    run(["clang-tidy", "-p", "build", *TIDY_SOURCES])
    run(["ctest", "--test-dir", "build", "--output-on-failure"])
    print("verify: all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
