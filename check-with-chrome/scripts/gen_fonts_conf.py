#!/usr/bin/env python3
"""Generate a fontconfig setup that mirrors litehtml's fonts.cpp.

litehtml's render tests replace the current fontconfig config with one that
only contains the repository's top-level `fonts/*.ttf` files plus serif /
sans-serif / fantasy / cursive / monospace aliases.  Chrome must use the same
fonts and aliases or the comparison is meaningless.

This script creates:
  <out>/fonts-litehtml/   symlinks to the repo's top-level .ttf files
  <out>/fonts.conf        the fontconfig file to pass as FONTCONFIG_FILE
"""
import argparse
import sys
from pathlib import Path

TEMPLATE = """<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
\t<dir>{fonts}</dir>
\t<cachedir>{cache}</cachedir>
\t<alias>
\t\t<family>serif</family>
\t\t<prefer>
\t\t\t<family>Noto Serif</family>
\t\t</prefer>
\t</alias>
\t<alias>
\t\t<family>sans-serif</family>
\t\t<prefer>
\t\t\t<family>Noto Sans</family>
\t\t</prefer>
\t</alias>
\t<alias>
\t\t<family>fantasy</family>
\t\t<prefer>
\t\t\t<family>Noto Sans</family>
\t\t</prefer>
\t</alias>
\t<alias>
\t\t<family>cursive</family>
\t\t<prefer>
\t\t\t<family>Noto Sans</family>
\t\t</prefer>
\t</alias>
\t<alias>
\t\t<family>monospace</family>
\t\t<prefer>
\t\t\t<family>Noto Sans Mono</family>
\t\t</prefer>
\t</alias>
</fontconfig>
"""


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--repo", required=True, help="litehtml-tests repo root (contains fonts/)")
    ap.add_argument("--out", required=True, help="output directory")
    args = ap.parse_args()

    repo = Path(args.repo).resolve()
    out = Path(args.out).resolve()
    fonts = repo / "fonts"
    if not fonts.is_dir():
        sys.exit(f"fonts directory not found: {fonts}")

    link = out / "fonts-litehtml"
    link.mkdir(parents=True, exist_ok=True)
    for old in link.iterdir():
        old.unlink()

    count = 0
    # litehtml uses a non-recursive directory_iterator and lowercases the
    # extension, so only top-level *.ttf / *.TTF files are loaded.  In
    # particular fonts/tmp/ must NOT be exposed (it contains files literally
    # named "serif", "monospace", ... that would shadow the aliases).
    for f in sorted(fonts.iterdir()):
        if f.is_file() and f.suffix.lower() == ".ttf":
            (link / f.name).symlink_to(f.resolve())
            count += 1

    cache = out / "fontcache"
    cache.mkdir(exist_ok=True)
    conf = out / "fonts.conf"
    conf.write_text(TEMPLATE.format(fonts=link, cache=cache))
    print(f"fonts: {count} files, config: {conf}")


if __name__ == "__main__":
    main()
