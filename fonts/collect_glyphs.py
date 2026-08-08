#!/usr/bin/env python3
"""
Dump the glyph list without building a font.

Same collection logic build_font.py uses, but it only writes the merged
character list and prints a breakdown by Unicode block. Useful for
answering "why did the subset grow" without running fontTools.

    python3 collect_glyphs.py

Writes glyphs_merged.txt. That file is generated output - it is not
read by anything and can be deleted freely.
"""

import sys
import unicodedata
from pathlib import Path

from build_font import collect_glyphs, HERE

OUTPUT = HERE / "glyphs_merged.txt"

# Blocks worth reporting separately. Anything outside these lands in
# "other", which is usually a sign that glyphs.txt picked up something
# unintended.
BLOCKS = [
    ("ASCII", 0x0020, 0x007E),
    ("Latin-1 supplement", 0x00A0, 0x00FF),
    ("CJK punctuation", 0x3000, 0x303F),
    ("Hiragana", 0x3040, 0x309F),
    ("Katakana", 0x30A0, 0x30FF),
    ("CJK ideographs", 0x4E00, 0x9FFF),
    ("Fullwidth forms", 0xFF00, 0xFFEF),
]


def classify(ch):
    code = ord(ch)
    for name, low, high in BLOCKS:
        if low <= code <= high:
            return name
    return "other"


def main():
    chars = collect_glyphs()

    counts = {}
    others = []
    for ch in chars:
        block = classify(ch)
        counts[block] = counts.get(block, 0) + 1
        if block == "other":
            others.append(ch)

    print(f"\nTotal: {len(chars)} glyphs\n")
    for name, _, _ in BLOCKS + [("other", 0, 0)]:
        if name in counts:
            print(f"  {name:22} {counts[name]:5}")

    if others:
        print("\nOutside the expected blocks:")
        for ch in sorted(others):
            try:
                label = unicodedata.name(ch)
            except ValueError:
                label = "unnamed"
            print(f"  U+{ord(ch):04X}  {ch}  {label}")

    OUTPUT.write_text("".join(sorted(chars)), encoding="utf-8")
    print(f"\nWrote {OUTPUT.name}")


if __name__ == "__main__":
    main()