#!/usr/bin/env python3
"""
Build the embedded font subset.

Collects every character the firmware can render, merges it with the
base glyph set, and produces a subsetted font containing only those
glyphs.

Usage, from the fonts/ directory with the ESP-IDF environment active:

    pip install fonttools brotli
    python3 build_font.py NotoSansJP-Light.ttf

The output overwrites ../main/ui_font_jp.otf.

Characters absent from the subset render as blank space. There is no
fallback and no warning, so rerun this after changing UI_LANG, editing
a link label, or editing the avatar caption.

To inspect the glyph list without building a font, run
collect_glyphs.py instead.
"""

import argparse
import re
import sys
from pathlib import Path

try:
    from fontTools import subset
    from fontTools.ttLib import TTFont
except ImportError:
    sys.exit("fontTools not installed. Run: pip install fonttools brotli")

HERE = Path(__file__).resolve().parent
MAIN = HERE.parent / "main"

# Headers holding text that reaches the panel. Every string literal is
# treated as displayable, including literals behind an inactive UI_LANG
# branch: subsetting for one language only would silently break the
# other the moment somebody flips the switch.
SOURCES = [
    MAIN / "ui_strings.h",
    MAIN / "socials.h",
    MAIN / "user_profile.h",
]

BASE_GLYPHS = HERE / "glyphs.txt"

# The output name is fixed. Changing it requires updating EMBED_FILES in
# main/CMakeLists.txt and the asm symbol names in embedded_assets.h,
# which the build derives from the filename.
DEFAULT_OUTPUT = MAIN / "ui_font_jp.otf"

# Family name written into the subset. Neither this nor the filename
# reuses the upstream name: the subset is a Modified Version under OFL
# terms, and presenting it as the original would be misleading.
SUBSET_FAMILY = "UI Font JP Subset"

STRING_LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')


def extract_from(path):
    """Pull displayable characters out of C string literals."""
    if not path.exists():
        print(f"  MISSING: {path}", file=sys.stderr)
        return set()

    chars = set()
    for match in STRING_LITERAL.finditer(path.read_text(encoding="utf-8")):
        literal = match.group(1)

        # URLs are encoded into QR codes, never drawn with the font.
        if literal.startswith("http"):
            continue

        chars.update(literal)

    print(f"  {path.name}: {len(chars)} unique")
    return chars


def collect_glyphs(verbose=True):
    """Union of characters from the headers and the base set."""
    if verbose:
        print("Collecting glyphs:")
    chars = set()

    for path in SOURCES:
        chars |= extract_from(path)

    if BASE_GLYPHS.exists():
        base = set(BASE_GLYPHS.read_text(encoding="utf-8"))
        if verbose:
            print(f"  {BASE_GLYPHS.name}: {len(base)} unique")
        chars |= base
    else:
        print(f"  MISSING: {BASE_GLYPHS}", file=sys.stderr)

    # Control characters are never rendered; a plain space is.
    return {c for c in chars if c.isprintable() or c == " "}


def rename(font):
    """
    Retitle the subset.

    The Reserved Font Name declared in this font's license is 'Source',
    inherited from Adobe Source Han Sans, which the output name does
    not use - so this is not strictly required. It is done anyway so
    the binary does not present itself as the unmodified original.
    """
    name_table = font["name"]
    for record in name_table.names:
        # 1 = family, 4 = full name, 6 = PostScript name,
        # 16 = typographic family
        if record.nameID in (1, 4, 16):
            record.string = SUBSET_FAMILY
        elif record.nameID == 6:
            record.string = SUBSET_FAMILY.replace(" ", "")

    print(f"Renamed family to: {SUBSET_FAMILY}")


def build(input_path, output_path, chars, keep_name):
    font = TTFont(input_path)

    # sfntVersion "OTTO" means CFF outlines, anything else is glyf.
    # tiny_ttf handles both; the distinction matters when reading the
    # output size, since CFF is usually more compact for CJK.
    flavour = "CFF" if font.sfntVersion == "OTTO" else "TrueType"
    print(f"\nInput: {Path(input_path).name} ({flavour} outlines)")

    options = subset.Options()

    # tiny_ttf does not apply OpenType shaping, so layout tables are
    # dead weight. For CJK they are a large fraction of the file.
    options.layout_features = []

    # Hinting only matters for small antialiased text on LCDs. This
    # panel is 16-level grayscale e-ink at 24 px.
    options.hinting = False

    options.desubroutinize = True
    options.drop_tables += ["DSIG"]

    # Keep the name table so the OFL copyright notice embedded in the
    # font survives. Condition 2 of the license allows the notice to
    # live in machine-readable metadata; dropping it would leave only
    # the external OFL.txt.
    options.name_IDs = ["*"]
    options.name_legacy = True
    options.name_languages = ["*"]

    subsetter = subset.Subsetter(options=options)
    subsetter.populate(text="".join(sorted(chars)))
    subsetter.subset(font)

    if not keep_name:
        rename(font)

    font.save(output_path)


def main():
    parser = argparse.ArgumentParser(
        description="Subset a font down to the glyphs this firmware renders."
    )
    parser.add_argument(
        "input",
        help="Source font, e.g. NotoSansJP-Light.ttf from Google Fonts",
    )
    parser.add_argument(
        "-o", "--output",
        default=str(DEFAULT_OUTPUT),
        help=f"Output path (default: {DEFAULT_OUTPUT})",
    )
    parser.add_argument(
        "--keep-name",
        action="store_true",
        help="Do not retitle the font family in the name table",
    )
    args = parser.parse_args()

    input_path = Path(args.input)
    if not input_path.exists():
        sys.exit(f"Input font not found: {input_path}")

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    chars = collect_glyphs()
    print(f"\nTotal: {len(chars)} glyphs")

    size_before = input_path.stat().st_size
    previous = output_path.stat().st_size if output_path.exists() else None

    build(str(input_path), str(output_path), chars, args.keep_name)

    size_after = output_path.stat().st_size
    print(f"\nWrote {output_path}")
    print(f"  {size_before / 1024:.0f} KB -> {size_after / 1024:.0f} KB "
          f"({100 * size_after / size_before:.1f}% of source)")

    if previous is not None:
        delta = size_after - previous
        sign = "+" if delta >= 0 else ""
        print(f"  previous build: {previous / 1024:.0f} KB "
              f"({sign}{delta / 1024:.1f} KB)")

    # The application partition is 4 MB and holds the firmware too.
    if size_after > 2 * 1024 * 1024:
        print("\nWARNING: font exceeds 2 MB. Check that glyphs.txt is not "
              "pulling in more than intended - the firmware shares a 4 MB "
              "partition with it.")

    print("\nNext: idf.py build")


if __name__ == "__main__":
    main()