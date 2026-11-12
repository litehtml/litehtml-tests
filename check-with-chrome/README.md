# check-with-chrome

Triage tool for litehtml golden-image failures: re-render every failing test in
Google Chrome and auto-approve the ones whose litehtml output matches Chrome.

A "failure" here is a `<test>.html-FAILED.png` file written by
`render_test.cpp`. Approving means renaming it to `<test>.html.png`, which is
the golden image used by the test suite.

## Requirements

- Google Chrome (or Chromium) on the host
- Python 3.9+ with: `pillow numpy scipy websockets`
  ```shell
  pip install pillow numpy scipy websockets
  ```
- The `fonts/` directory of this repository (the tool generates a fontconfig
  file from it)
- Docker only if you also want to run the test suite itself

## Quick start

1. Run the render tests and produce the failure images (inside Docker, see
   `AGENTS.md`):
   ```shell
   cd litehtml-tests
   ./run_render_tests.sh /path/to/litehtml 14
   ```
2. Compare the failures with Chrome and approve the matching ones:
   ```shell
   ./check-with-chrome/run.sh --css-grid --work /tmp/css-grid-check --apply
   ```

Without `--apply` nothing is renamed; the tool only writes results and
reports. Use this first if you want to inspect before approving.

The `--css-grid` shorthand selects `render/css-grid` and skips WPT reference
pages and non-visual folders:

```
--tests-dir render/css-grid
--skip-refs                        # *-ref.html
--skip-dir parsing
--skip-dir animation
--skip-dir reference
```

## Outputs

All output is written to the `--work` directory (default
`check-with-chrome/work`):

| Path | Contents |
|---|---|
| `jobs.tsv` | test html, litehtml width/height, Chrome screenshot path |
| `chrome/` | full-page Chrome screenshots (`*.png`) and metadata (`*.meta.json`) |
| `results.csv` | per-test metrics and classification |
| `approved.txt` | tests classified `approve` |
| `report/review.html` | side-by-side litehtml / Chrome / diff for borderline cases |
| `report/rejected.html` | side-by-side images for clear mismatches |
| `capture_errors.txt` | pages Chrome failed to capture |

`results.csv` columns:

```
html, w, h, content_h, dx, dy, strong_pct, strong_pct_noshift, rmse,
struct_max, struct_total, class, reason
```

## How the comparison works

- **Viewport width** is the width of the litehtml `-FAILED.png`. litehtml
  renders in "best fit" mode (it re-renders at the document's intrinsic
  width), so using 800 px would make `%`/`fr` layouts incomparable.
- **Fonts**: Chrome is started with `FONTCONFIG_FILE` pointing at a generated
  config that exposes the repository's `fonts/*.ttf` and the same aliases as
  `fonts.cpp` (Noto Serif/Sans/Mono, Ahem, CSSTest). Without this, Chrome
  would use the host's fonts.
- **Rendering flags** approximate Cairo's settings used by the tests:
  `--font-render-hinting=none --disable-lcd-text
  --disable-font-subpixel-positioning`.
- **Alignment**: the best global integer shift within ±8 px is applied before
  comparing. litehtml and Chrome have slightly different default `<p>`/line
  metrics, which produces a consistent few-pixel vertical offset on many
  pages; after the shift the layouts line up.
- **`strong_pct`**: after a Gaussian blur (σ=2), the percentage of pixels whose
  max channel difference is > 32/255.
- **`struct_max` / `struct_total`**: a raw diff > 64/255 is morphologically
  opened with a 5×5 kernel, then connected components are measured. Thin text
  antialiasing and 1 px rounding disappear; solid misplaced boxes remain. This
  catches layout errors that a canvas-wide percentage would miss.

Classification:

| class | rule |
|---|---|
| `approve` | `strong_pct <= 2` **and** `struct_max <= 150` **and** `struct_total <= 600` |
| `review` | `strong_pct <= 8` (including structural failures) |
| `reject` | `strong_pct > 8`, height delta > max(8 px, 3 %) when Chrome content is taller than 600 px, degenerate size, or unreadable images |

`review` entries are genuine differences worth looking at; the tool does not
auto-approve them.

## Using it on other folders

Scan every failing test in `render/`:

```shell
./check-with-chrome/run.sh --work /tmp/all-check
```

Scan a single folder:

```shell
./check-with-chrome/run.sh --tests-dir render/flex --work /tmp/flex-check
```

Re-run only the comparison after the screenshots already exist:

```shell
./check-with-chrome/run.sh --tests-dir render/flex --work /tmp/flex-check --no-capture
```

Full option list: `./check-with-chrome/run.sh --help`.

You can also run the individual steps yourself:

```shell
python3 check-with-chrome/scripts/gen_fonts_conf.py --repo . --out /tmp/cw
python3 check-with-chrome/scripts/make_jobs.py --tests-dir render/css-grid --work /tmp/cw \
  --skip-refs --skip-dir parsing --skip-dir animation --skip-dir reference
python3 check-with-chrome/scripts/chrome_capture.py --list /tmp/cw/jobs.tsv \
  --errors /tmp/cw/capture_errors.txt --fontconfig /tmp/cw/fonts.conf
python3 check-with-chrome/scripts/compare.py --list /tmp/cw/jobs.tsv \
  --csv /tmp/cw/results.csv --report-dir /tmp/cw/report --make-diffs
python3 check-with-chrome/scripts/approve.py --csv /tmp/cw/results.csv \
  --tests-dir render/css-grid --dry-run
```

## Latest run (css-grid, November 2026)

- Test suite: 9,921 tests, 3,708 failures, all in `render/css-grid`
  (that folder had no golden images).
- Triaged: 2,479 tests (reference pages, `parsing/` and `animation/` skipped).
- Result: **683 approved, 360 review, 1,436 rejected**.
- Verification run after approval: **3,025 failures remain, all in css-grid;
  none of the approved tests fail**.

`artifacts/` contains a snapshot of that run:

- `results.csv` – all metrics and decisions
- `approved.txt` – the 683 approved tests
- `remaining_failures.txt` – the 3,025 tests still failing after approval
- `review.html`, `rejected.html` – side-by-side reports (relative image links)
- `chrome/` – Chrome screenshots and amplified diff images used by the reports
  (~77 MB; can be deleted and regenerated)
- `capture_errors.txt` – pages Chrome could not capture (one test in this run:
  `firefox-bug-2063893-crash.html`, page height > 20,000 px)

The report HTML files reference the golden/failure images relative to this
repository and the Chrome screenshots in `artifacts/chrome/`.

## Notes and limitations

- Chrome version matters; the thresholds above were calibrated with
  Chrome 157. Re-check a few `review` entries if you use a very different
  version.
- Root-relative resources such as `/fonts/ahem.css` do not resolve when pages
  are opened as `file://` in either renderer. The css-grid import has no
  `<script>` elements, so its tests are static; other folders with scripts may
  need `--virtual-time-budget` or similar tuning.
- Only tests with a `-FAILED.png` are processed. Tests that pass are ignored.
- Approving freezes the current litehtml output as the reference. Review the
  `review.html`/`rejected.html` reports before committing new goldens.
