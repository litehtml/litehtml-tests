# manage-tests

GUI triage tool for litehtml golden-image render tests. It lets you review
failing, passing and disabled tests side-by-side and apply approvals / disables
in bulk.

Built on gtkmm-4.0 (GTK4/Adwaita), C++17.

## Build

```shell
mkdir -p build-manage-tests && cd build-manage-tests
cmake -S ../manage-tests -B .
make
```

Building the tool itself requires CMake 3.20 or newer, `pkg-config`,
`gtkmm-4.0` and a C++17 toolchain.
At runtime it additionally needs:

- Docker (to build and run `render2png`)
- Google Chrome/Chromium + `python3` with `playwright` (to render comparison
  images via `../render.py`)

## Usage

```shell
manage-tests [tests_folder] [litehtml_folder]
```

| Argument        | Default       | Purpose                                      |
|-----------------|---------------|----------------------------------------------|
| `tests_folder`  | `render`      | Tree of `.htm`/`.html` tests to triage       |
| `litehtml_folder` | `../litehtml` | litehtml checkout used to build `render2png` |

`manage-tests --help` prints the usage. The tests folder can also be changed
from the header bar ("Folder" button) without restarting.

## Modes

- **Manage Failed** — tests with a `<file>-FAILED.png`. Shows the golden
  `<file>.png` on the left and the `-FAILED.png` on the right.
- **Manage Passed** — tests with no `-FAILED.png` whose name does not start with
  `-`. Shows the golden on the left and a Google Chrome render on the right.
- **Manage Disabled** — tests whose name starts with `-`/`--`. Shows the
  `render2png` output on the left and a Google Chrome render on the right.

Each side can be switched independently: left shows the golden image or the
`render2png` output, right shows the `-FAILED.png` or the Chrome output.

## Buttons / hotkeys

| Button  | Hotkey     | Action                                                        |
|---------|------------|---------------------------------------------------------------|
| Prev    | Ctrl+Left  | Previous test                                                 |
| Next    | Ctrl+Right | Next test                                                     |
| Approve | Ctrl+A     | Approve (enabled when the right side shows `-FAILED.png`, or in Passed mode when the left side shows `render2png`) |
| Disable | Ctrl+D     | Mark test as disabled                                         |
| Ignore  | Ctrl+Space | Remove any marks                                              |
| Save    | Ctrl+S     | Apply all marks                                               |
| Config  | —          | Edit the test's `.cfg` sidecar; the render2png/Chrome views re-render on save |
| Search  | Ctrl+F     | Open a search dialog to find and jump to a test                |

**Save** semantics:

- *Disable* renames the test by adding a single leading `-` (never a second
  one); the `.png` files are left untouched.
- *Approve* removes leading `-` from the test name and promotes either
  `<file>-FAILED.png` or the `render2png` output to the golden `<file>.png`.

## How rendering works

- **render2png** runs inside Docker. On startup the tool ensures the
  `litehtml-build:latest` image exists, starts a persistent container that
  mounts the litehtml checkout, the tests folder and a build/cache directory,
  builds `render2png` (`cmake` + `make`), then invokes it per test via
  `docker exec`. The litehtml checkout must provide the `litehtml_render2png`
  library target. If the image already exists, rebuild it after updating the
  Dockerfile with `docker build -t litehtml-build:latest .` from the test-suite
  root.
- **Chrome** images are rendered with `../render.py` (Playwright). render.py
  applies the same bestfit + `<file>.cfg` width logic as `render2png` (starting
  from its 800 px first pass) and generates a fontconfig from the repository's
  `fonts/` so Chrome uses the same fonts. The screenshot is clipped to the
  exact content bounds, so Chrome rendering no longer depends on the
  `render2png` output or Docker.

Temp images (render2png output, Chrome output, approved stashes) are cached
under `${TMPDIR:-/tmp}/litehtml-manage-tests/`.
