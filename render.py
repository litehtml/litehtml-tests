import argparse
import base64
import os
import sys
import tempfile
from pathlib import Path
from urllib.parse import unquote, urlparse
from playwright.sync_api import sync_playwright


# Mirrors litehtml's fonts.cpp: only top-level fonts/*.ttf are exposed, with
# serif/sans-serif/fantasy/cursive/monospace aliases, so Chrome uses the same
# fonts as render2png instead of the host's.
FONTS_CONF_TEMPLATE = """<?xml version="1.0"?>
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


def generate_fonts_conf(repo: Path, out: Path) -> Path:
    """Generate the litehtml fontconfig under `out`; return its fonts.conf path."""
    fonts = repo / "fonts"
    if not fonts.is_dir():
        raise FileNotFoundError(f"fonts directory not found: {fonts}")

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
    conf.write_text(FONTS_CONF_TEMPLATE.format(fonts=link, cache=cache))
    print(f"fonts: {count} files, config: {conf}")
    return conf


def local_path(source: str):
    """Return the local filesystem path for a source that is a file, else None."""
    if source.startswith("file://"):
        return unquote(urlparse(source).path)
    return source if os.path.exists(source) else None


def parse_cfg(path: str) -> dict:
    """Parse the sibling `<file>.cfg` keys used by litehtml's render2png (bestfit/width)."""
    cfg = {}
    cfg_path = Path(path + ".cfg")
    if cfg_path.is_file():
        for line in cfg_path.read_text(encoding="utf-8", errors="replace").splitlines():
            line = line.strip()
            if not line or line.startswith("#") or ":" not in line:
                continue
            key, _, value = line.partition(":")
            cfg[key.strip()] = value.strip()
    return cfg


# litehtml's document::render returns the widest rendered line (not the fully
# unwrapped text width). A block box spans its containing width, so its border
# box hides its line width; measuring max-content and clamping to the rendered
# width recovers it: min(maxContent, rendered).
BESTFIT_JS = """() => {
    const body = document.body;
    const els = Array.from(document.querySelectorAll('body *'));
    const saved = els.map(el => el.style.cssText);
    let best = 0;
    els.forEach((el, i) => {
        const style = window.getComputedStyle(el);
        if (style.display === 'none' || style.visibility === 'hidden' || parseFloat(style.opacity) === 0) {
            return;
        }
        if (style.position === 'absolute' || style.position === 'fixed') {
            return;
        }
        const rendered = el.getBoundingClientRect();
        if (rendered.width <= 0 || rendered.height <= 0) {
            return;
        }
        // Force shrink-to-fit to measure the widest unbroken line of this box.
        el.style.width = 'max-content';
        const maxContent = el.getBoundingClientRect().width;
        el.style.cssText = saved[i];
        if (saved[i] === '') {
            el.removeAttribute('style');
        }
        best = Math.max(best, Math.min(maxContent, rendered.width));
    });
    const bodyStyle = window.getComputedStyle(body);
    best += (parseFloat(bodyStyle.marginLeft) || 0) + (parseFloat(bodyStyle.marginRight) || 0);
    return Math.max(Math.ceil(best), 1);
}"""


def capture_page(source: str, output: str, width: int, scale: float, transparent: bool, wait_ms: int, bestfit: bool, fonts_conf=None):
    launch_env = dict(os.environ)
    if fonts_conf:
        launch_env["FONTCONFIG_FILE"] = str(fonts_conf)
    with sync_playwright() as p:
        # Use the system Google Chrome (remove channel="chrome" if it is not installed)
        try:
            browser = p.chromium.launch(headless=True, channel="chrome", env=launch_env, args=[
                "--disable-web-security",
                "--disable-gpu",
                "--hide-scrollbars",
                "--force-device-scale-factor=1",
                "--no-first-run",
                "--no-default-browser-check",
                "--disable-extensions",
                "--disable-sync",
                "--disable-default-apps",
                "--disable-component-update",
                "--disable-background-networking",
                "--disable-features=Translate,MediaRouter",
                "--font-render-hinting=none",
                "--disable-lcd-text",
                "--disable-font-subpixel-positioning",
                "--allow-file-access-from-files",
                "--mute-audio",
                ])
        except Exception:
            browser = p.chromium.launch(headless=True, env=launch_env)

        context = browser.new_context(
            viewport={"width": width, "height": 600},
            device_scale_factor=scale,
            offline=True
        )
        page = context.new_page()

        # Determine what was passed in: a URL, a local file or an HTML string
        if source.startswith(("http://", "https://", "file://")):
            page.goto(source, wait_until="networkidle")
        elif os.path.exists(source):
            file_url = Path(source).resolve().as_uri()
            page.goto(file_url, wait_until="networkidle")
        else:
            page.set_content(source, wait_until="networkidle")

        # Extra pause for font or JS chart rendering, if requested
        if wait_ms > 0:
            page.wait_for_timeout(wait_ms)

        # Wait for fonts to finish loading before measuring text widths (the
        # fontconfig exposes the repo fonts as regular families).
        try:
            page.evaluate("""() => document.fonts.ready.then(
                () => new Promise(r => requestAnimationFrame(() => requestAnimationFrame(r))))""")
        except Exception:
            pass

        # litehtml-compatible bestfit: re-render at the widest rendered line,
        # mirroring render2png's two passes. The sibling <file>.cfg can disable
        # it (bestfit: false) or override the width (width: N).
        local = local_path(source)
        cfg = parse_cfg(local) if local else {}
        cfg_bestfit = cfg.get("bestfit", "true").strip().lower() not in ("false", "0", "no")
        if bestfit and cfg_bestfit:
            if "width" in cfg:
                render_width = int(cfg["width"])
            else:
                render_width = page.evaluate(BESTFIT_JS)
            if render_width != page.viewport_size["width"]:
                page.set_viewport_size({"width": render_width, "height": page.viewport_size["height"]})

        # Compute the exact content bounds for width and height
        dimensions = page.evaluate("""() => {
            const body = document.body;
            const html = document.documentElement;

            let maxRight = 0;
            let maxBottom = 0;

            document.querySelectorAll('body *').forEach(el => {
                const style = window.getComputedStyle(el);
                if (style.display === 'none' || style.visibility === 'hidden' || parseFloat(style.opacity) === 0) {
                    return;
                }
                const rect = el.getBoundingClientRect();
                if (rect.width > 0 && rect.height > 0) {
                    // getBoundingClientRect() is the border box, so add the margins
                    // outside it (parseFloat("auto") is NaN, hence the fallback)
                    const marginRight = parseFloat(style.marginRight) || 0;
                    const marginBottom = parseFloat(style.marginBottom) || 0;
                    maxRight = Math.max(maxRight, rect.right + window.scrollX + marginRight);
                    maxBottom = Math.max(maxBottom, rect.bottom + window.scrollY + marginBottom);
                }
            });

            // The body's border box spans the viewport, so its edge can't be
            // used directly. Add its right/bottom margins to the content extent
            // instead (left/top are already reflected in the child offsets).
            const bodyStyle = window.getComputedStyle(body);
            maxRight += parseFloat(bodyStyle.marginRight) || 0;
            maxBottom += parseFloat(bodyStyle.marginBottom) || 0;

            const finalWidth = Math.ceil(maxRight);
            const finalHeight = Math.ceil(maxBottom);

            return { width: finalWidth, height: finalHeight };
        }""")

        final_width = max(dimensions["width"], 1)
        final_height = max(dimensions["height"], 1)

        # page.screenshot() clamps the clip to the viewport (and to the document
        # size when full_page is set), which cuts off anything outside it. CDP can
        # capture the requested box even when it is larger than both.
        cdp = context.new_cdp_session(page)
        if transparent:
            cdp.send("Emulation.setDefaultBackgroundColorOverride",
                     {"color": {"r": 0, "g": 0, "b": 0, "a": 0}})
        cdp.send("Animation.enable")
        cdp.send("Animation.setPlaybackRate", {"playbackRate": 0})
        result = cdp.send("Page.captureScreenshot", {
            "format": "png",
            "captureBeyondViewport": True,
            "clip": {"x": 0, "y": 0, "width": final_width, "height": final_height, "scale": scale},
        })
        Path(output).parent.mkdir(parents=True, exist_ok=True)
        Path(output).write_bytes(base64.b64decode(result["data"]))

        browser.close()
        print(f"Done: {output} ({final_width}x{final_height}px, scale={scale}x)")

def main():
    parser = argparse.ArgumentParser(
        description="Render HTML/URL to PNG with automatic width and height fitting."
    )
    parser.add_argument(
        "source",
        help="Site URL, path to a local .html file or a string with HTML code"
    )
    parser.add_argument(
        "-o", "--output",
        default="output.png",
        help="Path to save the PNG to (default: output.png)"
    )
    parser.add_argument(
        "-w", "--width",
        type=int,
        default=1280,
        help="Initial window width in pixels (default: 1280)"
    )
    parser.add_argument(
        "-s", "--scale",
        type=float,
        default=1.0,
        help="Scale/pixel density, e.g. 2 for Retina (default: 1.0)"
    )
    parser.add_argument(
        "-t", "--transparent",
        action="store_true",
        help="Make the background transparent (if no background is set in CSS)"
    )
    parser.add_argument(
        "--wait",
        type=int,
        default=0,
        help="Extra wait after loading in milliseconds (default: 0)"
    )
    parser.add_argument(
        "--no-bestfit",
        action="store_false",
        dest="bestfit",
        help="Disable bestfit: capture at the given -w width instead of re-rendering at the natural width"
    )
    parser.add_argument(
        "--repo",
        default=None,
        help="litehtml-tests repo root containing fonts/; enables the litehtml fontconfig"
    )
    parser.add_argument(
        "--fonts-out",
        default=None,
        help="Directory for the generated fontconfig (default: a temp dir)"
    )

    args = parser.parse_args()

    fonts_conf = None
    if args.repo:
        repo = Path(args.repo).resolve()
        if not (repo / "fonts").is_dir():
            parser.error(f"fonts directory not found: {repo / 'fonts'}")
        fonts_out = Path(args.fonts_out).resolve() if args.fonts_out else Path(tempfile.mkdtemp(prefix="litehtml-fonts-"))
        fonts_conf = generate_fonts_conf(repo, fonts_out)

    capture_page(
        source=args.source,
        output=args.output,
        width=args.width,
        scale=args.scale,
        transparent=args.transparent,
        wait_ms=args.wait,
        bestfit=args.bestfit,
        fonts_conf=fonts_conf
    )

if __name__ == "__main__":
    main()
