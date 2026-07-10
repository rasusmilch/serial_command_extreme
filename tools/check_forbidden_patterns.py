#!/usr/bin/env python3
"""Check Serial Command Extreme core source for forbidden patterns."""

from __future__ import annotations

import re
import sys
from pathlib import Path

SOURCE_SUFFIXES = {".c", ".h"}
FORBIDDEN = [
    ("heap allocation", re.compile(r"\b(malloc|calloc|realloc|free)\s*\(")),
    ("C++ allocation", re.compile(r"\b(new|delete)\b")),
    ("Arduino String", re.compile(r"\bString\b")),
    ("Arduino include", re.compile(r"#\s*include\s*[<\"]Arduino\.h[>\"]")),
    ("ESP-IDF include", re.compile(r"#\s*include\s*[<\"](?:esp_|driver/|freertos/)")),
    ("STL/std usage", re.compile(r"\bstd::|#\s*include\s*<(?:vector|string|map|array|algorithm|memory)>")),
    ("unbounded sprintf", re.compile(r"\bsprintf\s*\(")),
    ("unsafe string copy", re.compile(r"\b(strcpy|strcat)\s*\(")),
    ("atoi/atof", re.compile(r"\b(atoi|atof)\s*\(")),
]


def strip_comments(text: str) -> str:
    """Remove C and C++ comments before pattern scanning."""
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*", "", text)
    return text


def iter_source_files(root: Path):
    """Yield regular .c and .h files recursively below root in sorted path order."""
    for path in sorted(root.rglob("*")):
        if path.is_file() and path.suffix in SOURCE_SUFFIXES and path.name.lower() != "readme.md":
            yield path


def main(argv: list[str]) -> int:
    """Validate arguments and scan core source files for forbidden patterns.

    Args:
        argv: Process-style argument list containing the program name and
            exactly one source-directory path.

    Returns:
        Zero when no forbidden patterns are found, one when one or more
        forbidden patterns are found, or two for invalid command-line usage or
        a source path that is not a directory.

    Raises:
        OSError: Propagated from recursive traversal or source-file reading.
        UnicodeDecodeError: Propagated when an eligible source file cannot be
            decoded as UTF-8.

    Side Effects:
        Recursively reads eligible .c and .h files below the supplied
        directory. Prints violations or success output to standard output, and
        prints usage or path-validation errors to standard error.

    Notes:
        Comments are stripped before applying forbidden-pattern checks. The
        function does not modify scanned files, and filesystem or decoding
        exceptions are not converted to tool exit codes.
    """
    if len(argv) != 2:
        print("usage: check_forbidden_patterns.py <src-dir>", file=sys.stderr)
        return 2

    root = Path(argv[1])
    if not root.is_dir():
        print(f"error: not a directory: {root}", file=sys.stderr)
        return 2

    failures: list[str] = []
    for path in iter_source_files(root):
        text = strip_comments(path.read_text(encoding="utf-8"))
        for line_number, line in enumerate(text.splitlines(), start=1):
            for label, pattern in FORBIDDEN:
                if pattern.search(line):
                    failures.append(f"{path}:{line_number}: forbidden {label}: {line.strip()}")

    if failures:
        for failure in failures:
            print(failure)
        return 1

    print(f"No forbidden core patterns found under {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
