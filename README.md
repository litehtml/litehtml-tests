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
