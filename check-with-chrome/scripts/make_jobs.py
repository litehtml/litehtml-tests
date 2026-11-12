#!/usr/bin/env python3
"""Build a Chrome-capture job list from failing render tests.

Every `<name>-FAILED.png` produced by the render tests is one job:
  <html_path>\t<width>\t<height>\t<chrome_output_path>

The litehtml image dimensions are used as the Chrome viewport width because
litehtml renders in "best fit" mode: it re-renders at the document's intrinsic
width, which is not necessarily 800 px.
"""
import argparse
import sys
from pathlib import Path

from PIL import Image


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--tests-dir",
        required=True,
        help="directory tree that contains *-FAILED.png (usually <repo>/render)",
    )
    ap.add_argument("--work", required=True, help="working directory for outputs")
    ap.add_argument(
        "--skip-refs",
        action="store_true",
        help="skip *-ref.html / *-ref.htm (WPT reference pages)",
    )
    ap.add_argument(
        "--skip-dir",
        action="append",
        default=[],
        metavar="NAME",
        help="skip paths containing a directory with this name (repeatable)",
    )
    args = ap.parse_args()

    root = Path(args.tests_dir).resolve()
    work = Path(args.work).resolve()
    chrome_dir = work / "chrome"
    skip_dirs = set(args.skip_dir)

    jobs = []
    for failed in sorted(root.rglob("*-FAILED.png")):
        html = Path(str(failed)[: -len("-FAILED.png")])
        try:
            rel = html.relative_to(root)
        except ValueError:
            continue
        if args.skip_refs and html.name.endswith(("-ref.html", "-ref.htm")):
            continue
        if skip_dirs and any(part in skip_dirs for part in rel.parts[:-1]):
            continue
        try:
            with Image.open(failed) as im:
                width, height = im.size
        except Exception as e:
            print(f"cannot read {failed}: {e}", file=sys.stderr)
            continue
        chrome_out = chrome_dir / (str(rel) + ".png")
        jobs.append((str(html), width, height, str(chrome_out)))

    if not jobs:
        sys.exit(f"no *-FAILED.png files found under {root}")

    work.mkdir(parents=True, exist_ok=True)
    tsv = work / "jobs.tsv"
    with open(tsv, "w") as f:
        for html, width, height, chrome_out in jobs:
            f.write(f"{html}\t{width}\t{height}\t{chrome_out}\n")
    print(f"jobs={len(jobs)} -> {tsv}")


if __name__ == "__main__":
    main()
