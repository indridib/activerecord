#!/usr/bin/env python3
"""Check that two versions of a C/C++ file differ only in formatting.

Usage: token_compare.py <old-file> <new-file>

Lexes both files with clang (`-Xclang -dump-raw-tokens`, no preprocessing) and
compares the token sequences (kind and exact spelling, so whitespace inside string
literals counts), ignoring whitespace tokens and token locations. Comments are
compared too, with runs of whitespace collapsed, since clang-format may reflow them.
Exits 0 if the token sequences match, 1 otherwise (and prints the first difference).
Used to prove formatting-only commits (AGENTS.md clang-format rule).
"""

import re
import subprocess
import sys

TOKEN = re.compile(r"^(\w+) '(.*?)'(?:\s*\[[^\]]*\])*\s*Loc=<", re.S | re.M)


def tokens(path):
    out = subprocess.run(
        ["clang++", "-x", "c++", "-std=c++23", "-fsyntax-only", "-Xclang", "-dump-raw-tokens", path],
        capture_output=True, text=True).stderr
    result = []
    for kind, spelling in TOKEN.findall(out):
        if kind == "unknown":  # whitespace
            continue
        if kind == "comment":
            spelling = " ".join(spelling.split())
        result.append((kind, spelling))
    return result


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    old, new = tokens(sys.argv[1]), tokens(sys.argv[2])
    if not old:
        sys.exit(f"no tokens read from {sys.argv[1]}")
    if old == new:
        print(f"IDENTICAL: {len(old)} tokens")
        return
    for i, (a, b) in enumerate(zip(old, new)):
        if a != b:
            print(f"DIFFER at token {i}: {a} vs {b}")
            break
    else:
        print(f"DIFFER in length: {len(old)} vs {len(new)} tokens")
    sys.exit(1)


if __name__ == "__main__":
    main()
