import base64
import math
import os
from pathlib import Path
import struct
import tempfile
import unittest

from playwright.sync_api import sync_playwright

from render import capture_page, generate_fonts_conf


REPO = Path(__file__).resolve().parent


class CaptureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.fonts_conf = generate_fonts_conf(REPO, self.directory)

    def capture(self, html, bestfit=True, cfg="", use_repo_fonts=True):
        source = self.directory / "test.htm"
        source.write_text(html, encoding="utf-8")
        Path(str(source) + ".cfg").write_text(cfg, encoding="utf-8")
        output = self.directory / "test.png"
        capture_page(str(source), str(output), 800, 1, False, 0,
                     bestfit, self.fonts_conf if use_repo_fonts else None)
        return source, output.read_bytes()

    def assert_text_capture(self, doctype, bestfit=True, cfg="", viewport_width=800,
                            body_style=""):
        html = (doctype + f'<style>body {{ margin:50px; background:olive; color:black; {body_style} }}</style>'
                '<body>Hello Hello Hello</body>')
        source, png = self.capture(html, bestfit, cfg)
        width, height = struct.unpack(">II", png[16:24])
        with sync_playwright() as p:
            browser = p.chromium.launch(
                headless=True, channel="chrome",
                env={**os.environ, "FONTCONFIG_FILE": str(self.fonts_conf)},
                args=["--hide-scrollbars", "--disable-font-subpixel-positioning"],
            )
            try:
                page = browser.new_page(viewport={"width": viewport_width, "height": 600})
                page.goto(source.as_uri())
                page.add_style_tag(content="html { font-family: serif; font-size: 16px; }")
                page.evaluate("() => document.fonts.ready")
                rect = page.evaluate("""() => {
                    const range = document.createRange();
                    range.selectNodeContents(document.body);
                    return range.getBoundingClientRect().toJSON();
                }""")
                self.assertEqual(width, math.ceil(rect["right"] + 50))
                self.assertEqual(height, math.ceil(rect["bottom"] + 50))
                self.assertLess(height, 600)
                ink = page.evaluate("""async data => {
                    const image = await createImageBitmap(await (await fetch(data)).blob());
                    const canvas = document.createElement('canvas');
                    canvas.width = image.width;
                    canvas.height = image.height;
                    const ctx = canvas.getContext('2d');
                    ctx.drawImage(image, 0, 0);
                    const pixels = ctx.getImageData(0, 0, image.width, image.height).data;
                    let count = 0, left = image.width, top = image.height;
                    let right = 0, bottom = 0;
                    for (let y = 0; y < image.height; y++) {
                        for (let x = 0; x < image.width; x++) {
                            const i = (y * image.width + x) * 4;
                            if (pixels[i] < 64 && pixels[i + 1] < 64 && pixels[i + 2] < 64) {
                                count++;
                                left = Math.min(left, x); top = Math.min(top, y);
                                right = Math.max(right, x); bottom = Math.max(bottom, y);
                            }
                        }
                    }
                    image.close();
                    return {count, left, top, right, bottom};
                }""", "data:image/png;base64," + base64.b64encode(png).decode("ascii"))
                self.assertGreater(ink["count"], 0)
                self.assertGreaterEqual(ink["left"], 50)
                self.assertGreaterEqual(ink["top"], 50)
                self.assertLess(ink["right"], width - 50)
                self.assertLess(ink["bottom"], height - 50)
            finally:
                browser.close()

    def test_default_serif_font(self):
        html = ('<!doctype html><style>body { margin:0; %s }</style>'
                '<body>Hello world</body>')
        for use_repo_fonts, family in ((True, '"Noto Serif"'), (False, "serif")):
            with self.subTest(use_repo_fonts=use_repo_fonts):
                _, default = self.capture(html % "", use_repo_fonts=use_repo_fonts)
                _, explicit = self.capture(
                    html % f"font:16px {family};", use_repo_fonts=use_repo_fonts
                )
                self.assertEqual(default, explicit)

    def test_default_monospace_size(self):
        html = ('<!doctype html><style>body, pre { margin:0; } %s</style>'
                '<body><code>Hello</code><pre>World</pre></body>')
        _, default = self.capture(html % "")
        _, explicit = self.capture(html % "code, pre { font-size:16px; }")
        self.assertEqual(default, explicit)
        _, smaller = self.capture(html % "code, pre { font-size:13px; }")
        self.assertNotEqual(default, smaller)
        _, inherited = self.capture(html % "body { font-size:20px; }")
        _, inherited_explicit = self.capture(
            html % "body { font-size:20px; } code, pre { font-size:20px; }"
        )
        self.assertEqual(inherited, inherited_explicit)

    def test_default_font_beyond_viewport(self):
        html = ('<!doctype html><style>body { margin:0; }'
                '#outer { position:absolute; left:820px; top:620px;'
                'width:160px; height:30px; %s }</style><div id=outer>%s</div>')
        _, default = self.capture(html % ("", "Hello world"))
        _, explicit = self.capture(html % ('font:16px "Noto Serif";', "Hello world"))
        _, empty = self.capture(html % ("", ""))
        self.assertEqual(struct.unpack(">II", default[16:24]), (980, 650))
        self.assertEqual(default, explicit)
        self.assertNotEqual(default, empty)

    def test_authored_font_overrides(self):
        self.assert_text_capture(
            "<!doctype html>", body_style='font-family:"Noto Sans"; font-size:20px;'
        )

    def test_body_text_and_margins(self):
        for doctype in ("", "<!doctype html>"):
            for bestfit in (False, True):
                with self.subTest(doctype=doctype, bestfit=bestfit):
                    self.assert_text_capture(doctype, bestfit)

    def test_body_text_with_cfg(self):
        self.assert_text_capture("", cfg="width: 180", viewport_width=180)
        self.assert_text_capture("", cfg="bestfit: false\nwidth: 180")

    def test_inline_block_whitespace_does_not_expand_capture(self):
        html = ('<!doctype html><style>body { margin:8px; }'
                'div { display:inline-block; width:32px; height:32px;'
                'border:1px solid red; }</style><div></div>\n<div></div>')
        _, initial = self.capture(html, bestfit=False)
        _, png = self.capture(html)
        self.assertEqual(struct.unpack(">II", png[16:24]),
                         struct.unpack(">II", initial[16:24]))
        self.assertEqual(struct.unpack(">II", png[16:24])[1], 50)

    def test_hidden_text_does_not_expand_capture(self):
        body = '<body style="margin:50px;background:olive">Hello'
        _, visible = self.capture(body + '</body>')
        _, hidden = self.capture(
            body + '<span style="visibility:hidden">hidden text</span>'
            '<span style="display:none">hidden text</span>'
            '<span style="opacity:0">hidden text</span></body>'
        )
        self.assertEqual(struct.unpack(">II", visible[16:24]),
                         struct.unpack(">II", hidden[16:24]))


if __name__ == "__main__":
    unittest.main()
