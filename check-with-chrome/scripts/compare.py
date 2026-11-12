#!/usr/bin/env python3
"""Compare litehtml `-FAILED.png` renders against Chrome screenshots.

For every job in the TSV produced by make_jobs.py:
  * load the litehtml image and the Chrome screenshot (cropped to lite size);
  * find the best global integer shift (<= 8 px) between them;
  * compute a blurred pixel difference ("strong" pixels) and a structural
    difference (5x5 morphological opening, which keeps solid misplaced
    regions but drops thin text antialiasing);
  * classify as approve / review / reject.

Outputs results.csv and side-by-side HTML reports (review.html/rejected.html).
"""
import argparse
import csv
import html as htmlmod
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage

# Calibration constants (see README):
R = 8                 # max global shift in px
BLUR = 2              # gaussian sigma for the perceptual diff
STRONG = 32           # per-channel diff (0-255) counted as "strong"
APPROVE_PCT = 2.0     # <= 2% strong pixels may be auto-approved
REVIEW_PCT = 8.0      # 2-8% goes to the review report
STRUCT_THRESHOLD = 64 # raw per-channel diff for the structural mask
STRUCT_KERNEL = 5     # opening kernel (removes features thinner than ~4 px)
STRUCT_MAX_BLOB = 150 # max surviving connected component for approve
STRUCT_MAX_TOTAL = 600
import argparse
import csv
import html as htmlmod
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage

R = 8  # max global shift
BLUR = 2
STRONG = 32
APPROVE_PCT = 2.0
REVIEW_PCT = 8.0


def to_rgb(img):
    if img.mode in ("RGBA", "LA", "P"):
        img = img.convert("RGBA")
        bg = Image.new("RGB", img.size, (255, 255, 255))
        bg.paste(img, mask=img.split()[-1])
        return bg
    return img.convert("RGB")


def overlap_bounds(w, h, dx, dy):
    """Bounds for moving image a's content by (dx, dy) to match b.

    Returns (by0, by1, bx0, bx1, ay0, ay1, ax0, ax1): the destination
    region in b and the corresponding source region in a.
    """
    by0, by1 = max(0, dy), min(h, h + dy)
    bx0, bx1 = max(0, dx), min(w, w + dx)
    ay0, ay1 = by0 - dy, by1 - dy
    ax0, ax1 = bx0 - dx, bx1 - dx
    return by0, by1, bx0, bx1, ay0, ay1, ax0, ax1


def shift_rgb(arr, dx, dy, fill=255):
    h, w = arr.shape[:2]
    out = np.full_like(arr, fill)
    by0, by1, bx0, bx1, ay0, ay1, ax0, ax1 = overlap_bounds(w, h, dx, dy)
    out[by0:by1, bx0:bx1] = arr[ay0:ay1, ax0:ax1]
    return out


def _shift_mae(ga, gb, dx, dy):
    h, w = ga.shape
    by0, by1, bx0, bx1, ay0, ay1, ax0, ax1 = overlap_bounds(w, h, dx, dy)
    if ay1 - ay0 < 8 or ax1 - ax0 < 8:
        return float("inf")
    return float(np.abs(ga[ay0:ay1, ax0:ax1] - gb[by0:by1, bx0:bx1]).mean())


def best_shift(ga, gb):
    h, w = ga.shape
    # coarse search on a downscaled copy
    scale = max(h, w) / 400.0
    if scale > 1:
        size = (max(1, int(w / scale)), max(1, int(h / scale)))
        sa = np.asarray(Image.fromarray(ga).resize(size, Image.BILINEAR), dtype=np.float32)
        sb = np.asarray(Image.fromarray(gb).resize(size, Image.BILINEAR), dtype=np.float32)
    else:
        sa, sb = ga, gb
    best = (float("inf"), 0, 0)
    for dy in range(-R, R + 1):
        for dx in range(-R, R + 1):
            m = _shift_mae(sa, sb, dx, dy)
            if m < best[0]:
                best = (m, dx, dy)
    # refine at full resolution around the coarse result
    cdx, cdy = best[1], best[2]
    best = (float("inf"), cdx, cdy)
    for dy in range(max(-R, cdy - 2), min(R, cdy + 2) + 1):
        for dx in range(max(-R, cdx - 2), min(R, cdx + 2) + 1):
            m = _shift_mae(ga, gb, dx, dy)
            if m < best[0]:
                best = (m, dx, dy)
    return best[1], best[2]


def compare(lite_path, chrome_path, meta_path):
    a = to_rgb(Image.open(lite_path))
    b = to_rgb(Image.open(chrome_path))
    w, h = a.size
    if b.size != (w, h):
        canvas = Image.new("RGB", (w, h), (255, 255, 255))
        canvas.paste(b, (0, 0))
        b = canvas

    # shift search on blurred grayscale
    ga = np.asarray(a.filter(ImageFilter.GaussianBlur(BLUR)).convert("L"), np.float32)
    gb = np.asarray(b.filter(ImageFilter.GaussianBlur(BLUR)).convert("L"), np.float32)
    dx, dy = best_shift(ga, gb)

    ab = np.asarray(a.filter(ImageFilter.GaussianBlur(BLUR)), np.float32)
    bb = np.asarray(b.filter(ImageFilter.GaussianBlur(BLUR)), np.float32)
    ash = shift_rgb(ab, dx, dy)

    by0, by1, bx0, bx1, ay0, ay1, ax0, ax1 = overlap_bounds(w, h, dx, dy)
    if ay1 - ay0 < 4 or ax1 - ax0 < 4:
        return None
    d = np.abs(ash[by0:by1, bx0:bx1] - bb[by0:by1, bx0:bx1])
    strong = float((d.max(axis=2) > STRONG).mean() * 100)
    rmse = float(np.sqrt((d**2).mean()))

    # no-shift reference
    d0 = np.abs(ab - bb)
    strong0 = float((d0.max(axis=2) > STRONG).mean() * 100)

    # structural: solid misplaced regions survive a 5x5 opening, thin text
    # antialiasing / 1px edge rounding does not.
    ar = np.asarray(a, np.int32)
    br = np.asarray(b, np.int32)
    ash0 = shift_rgb(ar, dx, dy)
    raw = np.abs(ash0[by0:by1, bx0:bx1] - br[by0:by1, bx0:bx1]).max(axis=2)
    op = ndimage.binary_opening(
        raw > STRUCT_THRESHOLD,
        structure=np.ones((STRUCT_KERNEL, STRUCT_KERNEL), bool),
    )
    if op.any():
        lab, n = ndimage.label(op)
        sizes = ndimage.sum(op, lab, range(1, n + 1))
        struct_max = int(sizes.max())
        struct_total = int(op.sum())
    else:
        struct_max = 0
        struct_total = 0

    content_h = None
    if meta_path and Path(meta_path).exists():
        try:
            content_h = float(json.loads(Path(meta_path).read_text())["content_h"])
        except Exception:
            content_h = None

    return {
        "w": w,
        "h": h,
        "content_h": content_h,
        "dx": dx,
        "dy": dy,
        "strong": strong,
        "strong0": strong0,
        "rmse": rmse,
        "struct_max": struct_max,
        "struct_total": struct_total,
    }


def classify(res):
    if res is None:
        return "reject", "dimension-overlap"
    if res["w"] < 8 or res["h"] < 8:
        return "reject", "degenerate-size"
    ch = res["content_h"]
    if ch is not None and ch > 600:
        tol = max(8.0, 0.03 * res["h"])
        if abs(ch - res["h"]) > tol:
            return "reject", f"height-delta({ch:.0f} vs {res['h']})"
    structural_ok = (
        res["struct_max"] <= STRUCT_MAX_BLOB
        and res["struct_total"] <= STRUCT_MAX_TOTAL
    )
    if structural_ok and res["strong"] <= APPROVE_PCT:
        return "approve", ""
    if res["strong"] <= REVIEW_PCT:
        if not structural_ok:
            return "review", (
                f"structural(blob={res['struct_max']},total={res['struct_total']})"
            )
        return "review", ""
    return "reject", f"strong-diff {res['strong']:.1f}%"


def write_report(path, rows, title):
    out = [
        "<!doctype html><meta charset='utf-8'><title>",
        htmlmod.escape(title),
        "</title><style>body{font-family:sans-serif;margin:12px}"
        "table{border-collapse:collapse}td,th{border:1px solid #ccc;padding:4px;"
        "vertical-align:top;font-size:12px}img{max-width:420px;display:block}"
        ".mono{font-family:monospace}</style><h2>",
        htmlmod.escape(title),
        f"</h2><p>{len(rows)} entries</p><table>"
        "<tr><th>#</th><th>test</th><th>metrics</th><th>litehtml</th>"
        "<th>chrome</th><th>diff (x3)</th></tr>",
    ]
    for i, (html, res, cls, reason, lite, chrome) in enumerate(rows):
        diff = chrome + ".diff.png"
        metrics = (
            f"w={res['w']} h={res['h']} content_h={res['content_h']} "
            f"shift=({res['dx']},{res['dy']}) strong={res['strong']:.2f}% "
            f"nos={res['strong0']:.2f}% rmse={res['rmse']:.1f} "
            f"blob={res['struct_max']} total={res['struct_total']}<br>"
            f"<b>{cls}</b> {htmlmod.escape(reason)}"
        )
        out.append(
            f"<tr><td>{i}</td><td class='mono'>{htmlmod.escape(html)}</td>"
            f"<td>{metrics}</td>"
            f"<td><img loading='lazy' src='file://{lite}'></td>"
            f"<td><img loading='lazy' src='file://{chrome}'></td>"
            f"<td><img loading='lazy' src='file://{diff}'></td></tr>"
        )
    out.append("</table>")
    Path(path).write_text("\n".join(out))


def make_diff(lite_path, chrome_path, out_path, res):
    a = to_rgb(Image.open(lite_path))
    b = to_rgb(Image.open(chrome_path))
    w, h = a.size
    if b.size != (w, h):
        c = Image.new("RGB", (w, h), (255, 255, 255))
        c.paste(b, (0, 0))
        b = c
    ab = np.asarray(a.filter(ImageFilter.GaussianBlur(BLUR)), np.float32)
    bb = np.asarray(b.filter(ImageFilter.GaussianBlur(BLUR)), np.float32)
    ash = shift_rgb(ab, res["dx"], res["dy"])
    d = np.abs(ash - bb).max(axis=2)
    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(np.clip(d * 3, 0, 255).astype("uint8")).save(out_path)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--list", required=True, help="jobs.tsv from make_jobs.py")
    p.add_argument("--csv", required=True, help="results CSV to write")
    p.add_argument("--report-dir", required=True, help="directory for HTML reports")
    p.add_argument(
        "--make-diffs",
        action="store_true",
        help="write amplified diff images for review and rejected entries",
    )
    args = p.parse_args()

    jobs = []
    with open(args.list) as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            html, w, h, out = line.split("\t")
            jobs.append((html, int(w), int(h), out))
    print(f"comparing {len(jobs)}", file=sys.stderr, flush=True)

    Path(args.report_dir).mkdir(parents=True, exist_ok=True)
    rows = []
    counts = {}
    for i, (html, w, h, chrome) in enumerate(jobs):
        lite = html + "-FAILED.png"
        try:
            res = compare(lite, chrome, chrome + ".meta.json")
        except Exception as e:
            res = None
            print(f"ERR compare {html}: {e}", file=sys.stderr, flush=True)
        cls, reason = classify(res)
        counts[cls] = counts.get(cls, 0) + 1
        if res is None:
            res = {
                "w": w, "h": h, "content_h": None, "dx": 0, "dy": 0,
                "strong": 100.0, "strong0": 100.0, "rmse": 0.0,
                "struct_max": 0, "struct_total": 0,
            }
        rows.append({"html": html, "res": res, "cls": cls, "reason": reason,
                     "lite": lite, "chrome": chrome})
        if args.make_diffs and cls in ("review", "reject"):
            try:
                make_diff(lite, chrome, chrome + ".diff.png", res)
            except Exception:
                pass
        if (i + 1) % 100 == 0 or i + 1 == len(jobs):
            print(f"compared {i + 1}/{len(jobs)} {counts}", file=sys.stderr, flush=True)

    with open(args.csv, "w", newline="") as f:
        wr = csv.writer(f)
        wr.writerow(
            ["html", "w", "h", "content_h", "dx", "dy", "strong_pct",
             "strong_pct_noshift", "rmse", "struct_max", "struct_total",
             "class", "reason"]
        )
        for r in rows:
            res = r["res"]
            wr.writerow(
                [r["html"], res["w"], res["h"], res["content_h"], res["dx"],
                 res["dy"], f"{res['strong']:.3f}", f"{res['strong0']:.3f}",
                 f"{res['rmse']:.3f}", res["struct_max"], res["struct_total"],
                 r["cls"], r["reason"]]
            )

    review = [(r["html"], r["res"], r["cls"], r["reason"], r["lite"], r["chrome"]) for r in rows if r["cls"] == "review"]
    rejected = [(r["html"], r["res"], r["cls"], r["reason"], r["lite"], r["chrome"]) for r in rows if r["cls"] == "reject"]
    write_report(Path(args.report_dir) / "review.html", review, "Review: 2-8% strong diff")
    write_report(Path(args.report_dir) / "rejected.html", rejected, "Rejected: >8% strong diff or dimension mismatch")
    print(counts, file=sys.stderr)
    print(f"approved={counts.get('approve',0)} review={counts.get('review',0)} "
          f"rejected={counts.get('reject',0)}", file=sys.stderr)


if __name__ == "__main__":
    main()
