#!/usr/bin/env python3
"""Approve tests classified as `approve` in results.csv.

Renames `<test>.html-FAILED.png` to `<test>.html.png` (the golden image path
used by render_test.cpp).  All other classes are left untouched.

Use --dry-run to only print what would be renamed.
"""
import argparse
import csv
import os
import sys
from collections import Counter
from pathlib import Path


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--csv", required=True, help="results.csv from compare.py")
    ap.add_argument(
        "--tests-dir",
        default=None,
        help="test root used for the per-directory summary (optional)",
    )
    ap.add_argument(
        "--list-out",
        default=None,
        help="write the list of approved tests here (default: next to --csv)",
    )
    ap.add_argument("--dry-run", action="store_true", help="do not rename anything")
    args = ap.parse_args()

    approved = []
    skipped = []
    with open(args.csv) as f:
        for row in csv.DictReader(f):
            if row["class"] != "approve":
                continue
            html = row["html"]
            src = Path(html + "-FAILED.png")
            dst = Path(html + ".png")
            if not src.exists():
                skipped.append((html, "missing -FAILED.png"))
                continue
            if dst.exists():
                skipped.append((html, "golden already exists"))
                continue
            if not args.dry_run:
                os.replace(src, dst)
            approved.append(html)

    print(f"approved={len(approved)} skipped={len(skipped)}", file=sys.stderr)
    for html, why in skipped:
        print(f"skip {why}: {html}", file=sys.stderr)

    if args.tests_dir:
        root = Path(args.tests_dir).resolve()
        by_dir = Counter()
        for html in approved:
            try:
                by_dir[str(Path(html).parent.relative_to(root))] += 1
            except ValueError:
                by_dir["?"] += 1
        for directory, count in sorted(by_dir.items(), key=lambda t: -t[1]):
            print(f"{count:5d}  {directory}", file=sys.stderr)

    out = Path(args.list_out) if args.list_out else Path(args.csv).parent / "approved.txt"
    out.write_text("\n".join(approved) + "\n")
    print(f"wrote {out}", file=sys.stderr)


if __name__ == "__main__":
    main()
