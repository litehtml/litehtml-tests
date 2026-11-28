# Tests for litehtml

This repository contains tests for [litehtml](https://github.com/litehtml/litehtml).
Rendering tests depend on the environment. The currently supported Linux
distributions are Ubuntu 24.04 and Fedora 40.

## Requirements

Building the tests requires CMake 3.20 or newer, C99 and C++17 compilers,
pkg-config, and development packages for GTK3, GdkPixbuf, fontconfig, Cairo,
Pango and PangoCairo. `LITEHTML_PATH` must point to a litehtml source checkout
that provides the `litehtml_render2png` library target.

For Ubuntu, install the build dependencies with:

```shell
sudo apt-get install -y --no-install-recommends --no-install-suggests 
    build-essential cmake clang-tidy pkg-config libcairo2-dev libpango1.0-dev libgtk-3-dev
```

GoogleTest is fetched during configuration by default. To use installed
GoogleTest headers and libraries instead, configure with `-DEXTERNAL_GTEST=ON`.

## Running tests locally

From the root of this repository, run:

```shell
cmake -S . -B build -DLITEHTML_PATH=/absolute/path/to/litehtml
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
```

The renderer library is always built for the tests. To also build the
`render2png` executable, configure with `-DLITEHTML_BUILD_RENDER2PNG=ON`.
The executable is disabled by default and runs as
`build/render2png path/to/page.html output.png`.

## Running tests in Docker

The provided script builds and runs the tests in a container:

```shell
./run_render_tests.sh /absolute/path/to/litehtml 4
```

The arguments are the path to the litehtml checkout and the number of threads
to use for building and testing. The script reuses the `litehtml-build:latest`
image if it already exists. Rebuild that image after updating the Dockerfile:

```shell
docker build -t litehtml-build:latest .
```

The `manage-tests` tool also reuses this image.

## Running tests from the litehtml repository

From a litehtml checkout with a compatible test-suite pin:

```shell
cmake -S . -B build-tests -DLITEHTML_BUILD_TESTING=ON
cmake --build build-tests
ctest --test-dir build-tests/litehtml-tests-build --output-on-failure
```

This workflow fetches a pinned commit of this repository, so it does not use
local or newly published changes until the pin is updated. Build this checkout
directly as described above to test local changes.

## Positioned percentage-padding regressions

`render/positioned-percentage-padding-*.htm` and
`render/fixed-percentage-padding.htm` cover containing-block padding bases,
static offsets, exact-width layout, and replaced images. Their `.cfg` files
fix the viewport at 800x600. The sibling golden images are rendered from
independent pixel-sized reference pages in `render/support/`, verified to
match the subject pages in Chrome. The runner compares the golden PNGs;
it does not evaluate the HTML `rel=match` links.

## Inline break and line-height regressions

`render/inline-break-heights-*.htm` covers consecutive, leading, and trailing
breaks, nested inline fonts, explicit line-height, vertical alignment, and
whitespace/alignment at forced breaks. The three pages exercise
quirks, limited-quirks, and standards modes with the suite's Ahem font. In the
first two modes all eight boxes in the first row have equal heights; in
standards mode the larger parent font makes the last pair taller.

Their golden images come from Chrome through `render.py --repo . -w 800`,
using the sibling viewport configs, and are converted to RGBA for the runner's
byte comparison. Existing references affected by the corrected standards-mode
strut, per-inline half-leading, and resulting baselines were also checked
against Chrome and refreshed with the test renderer. These retain the test
renderer's font rasterization and whole-pixel font metrics; they do not
rebaseline unrelated Chrome/litehtml differences.

## Overflow clipping of out-of-flow boxes

`render/overflow-hidden-*` and the re-enabled `render/css21/abspos-overflow-*` tests cover the
CSS 2.1 §11.1.1 clipping rules:

- an absolutely positioned box is not clipped by the `overflow` of a **static** ancestor between it
  and its containing block (`overflow-hidden-absolute-child`, `overflow-hidden-absolute-deep`,
  `css21/abspos-overflow-001..009`, `012`);
- it is clipped by the `overflow` of its positioned containing block
  (`overflow-hidden-absolute-positioned-ancestor`);
- a fixed box is not clipped by any ancestor (`overflow-hidden-fixed-child`);
- in-flow boxes (including `position:relative`) stay clipped by every ancestor
  (`overflow-hidden-relative-child`).

The pages use only solid colour boxes, so their goldens are pixel-identical between `render2png` and
Chrome; the geometry was cross-checked with `render.py -w 800 --no-bestfit`. The `*.cfg` files pin the
viewport to 800x600 (`bestfit: false`) so the escaping boxes are visible.

## Paint order of atomic inline-level boxes

`render/css21/inline-table-zorder-001..005.htm` (including the re-enabled `002` and `005`) and
`render/css21/inline-block-zorder-001..005.htm` cover the CSS 2.1 Appendix E §E.2 painting order:

- step 4 paints the backgrounds and borders of the in-flow, non-positioned block-level descendants
  before step 7 paints the inline-level content, so the inline-level box paints above a block
  background that overlaps it through a negative margin (`001`, `004`);
- within step 7 the inline-level boxes paint in tree order, so of two overlapping inline-level
  siblings the later one paints on top (`003`, `004`);
- step 7.2.1 treats an `inline-block` or `inline-table` — and, per CSS Display 3, an `inline-flex` or
  `inline-grid` — as if it created a new stacking context, so everything inside it, including its
  own block-level descendants, paints as one group in that inline pass instead of in the block pass
  before the parent's other blocks (`002`, `005`).

The pages only use solid colour boxes. The goldens of the two re-enabled tests were produced with
`render2png` and are pixel-identical to the `inline-table-zorder-001` and `inline-table-zorder-004`
goldens, which their `rel=reference` links point at.

## Fixed table layout

`render/table-layout-fixed-*.htm` and the re-enabled
`render/css21/fixed-table-layout-*` tests (including `004`, `005`, `006`, `009`, `010`, `012`,
`013`–`016`) cover the CSS 2.1 §17.5.2.1 fixed table-layout algorithm:

- only first-row cells and `<col>` elements set column widths; later rows are ignored
  (`table-layout-fixed-first-row.htm`);
- a `<col>` width beats a first-row cell width, and `<colgroup>` widths are ignored
  (`table-layout-fixed-col-vs-cell.htm`, `table-layout-fixed-colgroup.htm`);
- a `colspan` cell's width is divided over the columns it spans
  (`table-layout-fixed-colspan.htm`);
- pixel column widths widen the table, percentage columns resolve against the table width, and
  auto columns share the remaining space (`table-layout-fixed-overflow-table.htm`,
  `table-layout-fixed-percent.htm`);
- an auto-width table falls back to the automatic layout algorithm
  (`table-layout-fixed-auto-width.htm`).

The pages use solid colour boxes and `bestfit: false` configs pinning the viewport to 800x600, so
their geometry is pixel-identical between `render2png` and Chrome (verified with `render.py`).

The goldens for `css21/float-applies-to-005..007`, `css21/font-applies-to-014`,
`css21/text-align-applies-to-014`, and `css21/text-indent-applies-to-013` were refreshed because their
`table-layout: fixed; width: 100%` tables now fill their containing block as browsers do.

A table cell's `height` is now a minimum row height (CSS 2.1 §17.5.3), so empty cells with a
declared height get painted backgrounds/borders and rows no longer collapse. Goldens in the
`css21/*-applies-to-*` family that relied on `td { height: … }` were refreshed for this corrected
behavior; their box heights were cross-checked against Chrome.

## Table captions

`render/css21/caption-vs-table-box-001.htm`, `caption-vs-table-box-002.htm`,
`table-caption-002.htm`, `table-caption-003.htm` and `table-caption-margins-001.htm` cover the
CSS 2.1 §17.4 caption rules:

- a caption is a block box inside the table wrapper, outside the table border/padding box, with its
  own margin, border and padding (`caption-vs-table-box-001..002`, including `inline-table`);
- caption margins do not collapse with the table's margins (`table-caption-margins-001`);
- multiple captions and captions between rows are moved above the table in document order
  (`table-caption-002`);
- a caption between cells splits the anonymous rows, so the cells fall into separate rows
  (`table-caption-003`);
- a table without rows or columns still honours a specified width and height
  (`table-caption-margins-001`).

These are text-rendering tests, so their goldens come from `render2png`; box and line geometry was
cross-checked against Chrome with `render.py --no-bestfit -w 800 --repo .`. The goldens for
`css21/basic-css-table-001.htm` and `css21/basic-html-table-001.htm` were refreshed because the table
border no longer surrounds the caption.

## Table height algorithm and cell baselines

`render/css21/table-height-algorithm-001.htm`, `004`, `009`, `010`, `011`, `012`, `016`, `019`–`026`,
`029`, `031`, `032` cover the CSS 2.1 §17.5.3 height and `vertical-align` rules for tables:

- a table with `height: auto` is as tall as its rows plus cell spacing and borders, and a cell's
  `height` is a minimum row height (`001`, `004`, `009`, `010`);
- `sub`, `super`, `text-top`, `text-bottom` and `<length>`/`<percentage>` values of `vertical-align`
  on a table cell behave as `baseline` (`019`–`024`);
- cells with `vertical-align: baseline` share the row baseline, which is the largest distance from a
  cell-box top to its baseline; the remaining cells are placed at the top, middle or bottom afterwards
  (`011`, `012`, `016`, `029`);
- a cell's baseline is the baseline of its first in-flow line box or table row, and an `inline-table`
  uses the baseline of its first row (`025`, `031`, `032`);
- a caption does not move the baseline of the table it belongs to (`031`, `032`);
- a button's label defines the baseline of the control, so a button and a block next to it line up
  (`026`).

`render/table-baseline-nested-no-overlap.htm` checks that an empty anonymous context before a nested
table does not supply the cell baseline, and that rows grow to contain baseline-shifted cells.
Spanning cells use the aligned row heights and the spacing between rows; a cell's specified minimum
height is not increased merely because its content shifts to the row baseline.

Tables inherit the font properties of their ancestors in standards mode (§6.2), so `em` lengths inside
a table resolve against the inherited font size. The HTML4-era reset of the table font
(`font-size: medium`, `font-weight: normal`, `font-style: normal`) is applied in quirks mode only,
matching Gecko.

These pages mix Ahem box tests with text-rendering tests, so their goldens come from `render2png`;
the box, row and baseline geometry was cross-checked against Chrome with headless Chrome using the
`fonts/` directory and `serif`/`sans-serif` aliases (see `render.py`).

The goldens for `render/acid1.htm`, `render/css21/c5526c-display-000.htm`,
`render/css21/floats-wrap-bfc-005.htm` and `render/table-flex-github.htm` were refreshed because their
tables now inherit the page font, and the goldens for `render/button-inline-content.htm`,
`render/button-inline-formatting.htm` and `render/focus-checkbox-keyboard.htm` because form controls
are baseline-aligned like in browsers. The goldens for
`render/css21/table-anonymous-block-014.htm` and `render/css21/table-caption-optional-002.htm` were
refreshed because the baseline of an `inline-table` is now the baseline of its first row.
The `empty-cells-applies-to-002`, `003`, and `005` and `table-height-algorithm-012` and `025`
goldens were refreshed for the corrected row heights; their visible content is unchanged.
`table-height-algorithm-014` was refreshed for the adjusted distribution of a specified table height
between rows when a cell spans them.
