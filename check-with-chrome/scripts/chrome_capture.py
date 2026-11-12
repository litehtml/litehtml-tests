#!/usr/bin/env python3
"""Capture full-page Google Chrome screenshots at a per-test viewport width.

Input TSV (produced by make_jobs.py):
    html_path<TAB>width<TAB>height<TAB>output_png

Each page is loaded in a fixed 800x600-class headless tab whose width is the
litehtml output width.  The screenshot is captured beyond the viewport so tall
documents are complete.

Requires: Google Chrome/Chromium, Python 3, `websockets`.
"""
import argparse
import asyncio
import base64
import json
import os
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path

import websockets

CHROME_NAMES = ("google-chrome", "google-chrome-stable", "chromium", "chromium-browser")


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def find_chrome(requested):
    if requested:
        return requested
    for name in CHROME_NAMES:
        path = shutil.which(name)
        if path:
            return path
    sys.exit("Google Chrome/Chromium not found (use --chrome PATH)")


class Browser:
    def __init__(self, ws):
        self.ws = ws
        self.nid = 0
        self.pending = {}
        self.queues = {}
        self.task = None

    async def start(self):
        self.task = asyncio.create_task(self._reader())

    async def _reader(self):
        async for raw in self.ws:
            d = json.loads(raw)
            if "id" in d:
                fut = self.pending.get(d["id"])
                if fut and not fut.done():
                    fut.set_result(d)
            else:
                q = self.queues.get(d.get("sessionId"))
                if q is not None:
                    q.put_nowait(d)

    async def send(self, method, params=None, session=None, timeout=60):
        self.nid += 1
        mid = self.nid
        msg = {"id": mid, "method": method, "params": params or {}}
        if session:
            msg["sessionId"] = session
        fut = asyncio.get_running_loop().create_future()
        self.pending[mid] = fut
        try:
            await self.ws.send(json.dumps(msg))
            d = await asyncio.wait_for(fut, timeout)
        finally:
            self.pending.pop(mid, None)
        if "error" in d:
            raise RuntimeError(f"{method}: {d['error']}")
        return d.get("result", {})

    async def wait_event(self, method, session, timeout=30):
        q = self.queues[session]
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout
        while True:
            remaining = deadline - loop.time()
            if remaining <= 0:
                raise TimeoutError(method)
            d = await asyncio.wait_for(q.get(), remaining)
            if d.get("method") == method:
                return d


async def capture_one(browser, job, cap_height):
    html, width, lite_h, out = job
    uri = Path(html).resolve().as_uri()
    target_id = None
    session = None
    try:
        target = await browser.send("Target.createTarget", {"url": "about:blank"})
        target_id = target["targetId"]
        attached = await browser.send(
            "Target.attachToTarget", {"targetId": target_id, "flatten": True}
        )
        session = attached["sessionId"]
        browser.queues[session] = asyncio.Queue()

        await browser.send("Page.enable", {}, session)
        await browser.send(
            "Emulation.setDeviceMetricsOverride",
            {
                "width": width,
                "height": 600,
                "deviceScaleFactor": 1,
                "mobile": False,
                "screenWidth": width,
                "screenHeight": 600,
            },
            session,
        )
        await browser.send("Page.navigate", {"url": uri}, session)
        await browser.wait_event("Page.loadEventFired", session, 30)
        try:
            await browser.send(
                "Runtime.evaluate",
                {
                    "expression": (
                        "document.fonts.ready.then(()=>new Promise(r=>"
                        "requestAnimationFrame(()=>requestAnimationFrame(r))))"
                    ),
                    "awaitPromise": True,
                },
                session,
                15,
            )
        except Exception:
            pass

        metrics = await browser.send("Page.getLayoutMetrics", {}, session)
        size = metrics.get("cssContentSize") or metrics.get("contentSize") or {}
        content_h = float(size.get("height") or 0)
        if content_h > cap_height:
            raise RuntimeError(f"content height {content_h:.0f} > cap {cap_height}")
        height = max(content_h, float(lite_h), 1.0)

        shot = await browser.send(
            "Page.captureScreenshot",
            {
                "format": "png",
                "captureBeyondViewport": True,
                "fromSurface": True,
                "clip": {
                    "x": 0,
                    "y": 0,
                    "width": width,
                    "height": height,
                    "scale": 1,
                },
            },
            session,
            60,
        )
        Path(out).parent.mkdir(parents=True, exist_ok=True)
        Path(out).write_bytes(base64.b64decode(shot["data"]))
        Path(out + ".meta.json").write_text(
            json.dumps({"width": width, "lite_h": lite_h, "content_h": content_h})
        )
        return None
    except Exception as e:
        return f"{html}: {e}"
    finally:
        if target_id is not None:
            try:
                await browser.send("Target.closeTarget", {"targetId": target_id}, timeout=15)
            except Exception:
                pass
        if session is not None:
            browser.queues.pop(session, None)


async def run(args):
    jobs = []
    with open(args.list) as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            html, width, height, out = line.split("\t")
            jobs.append((html, int(width), int(height), out))
    if not jobs:
        sys.exit("no jobs")
    print(f"jobs: {len(jobs)}", file=sys.stderr, flush=True)

    port = free_port()
    profile = Path(args.profile).resolve()
    profile.mkdir(parents=True, exist_ok=True)
    home = Path(args.home).resolve()
    home.mkdir(parents=True, exist_ok=True)

    env = dict(os.environ)
    env["FONTCONFIG_FILE"] = str(Path(args.fontconfig).resolve())
    env["HOME"] = str(home)

    cmd = [
        find_chrome(args.chrome),
        "--headless",
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
        # litehtml renders with CAIRO_ANTIALIAS_GRAY + CAIRO_HINT_STYLE_NONE
        "--font-render-hinting=none",
        "--disable-lcd-text",
        "--disable-font-subpixel-positioning",
        "--allow-file-access-from-files",
        "--mute-audio",
        f"--remote-debugging-port={port}",
        f"--user-data-dir={profile}",
        "about:blank",
    ]
    proc = subprocess.Popen(
        cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )
    try:
        ws_url = None
        deadline = time.time() + 30
        while time.time() < deadline:
            try:
                import urllib.request

                with urllib.request.urlopen(
                    f"http://127.0.0.1:{port}/json/version", timeout=2
                ) as r:
                    ws_url = json.load(r)["webSocketDebuggerUrl"]
                break
            except Exception:
                await asyncio.sleep(0.2)
        if not ws_url:
            sys.exit("Chrome did not start")

        async with websockets.connect(ws_url, max_size=256 * 1024 * 1024) as ws:
            browser = Browser(ws)
            await browser.start()
            sem = asyncio.Semaphore(args.jobs)
            errors = []
            done = 0
            started = time.time()

            async def worker(job):
                nonlocal done
                async with sem:
                    err = await capture_one(browser, job, args.cap_height)
                done += 1
                if err:
                    errors.append(err)
                    print("ERR " + err, file=sys.stderr, flush=True)
                if done % 25 == 0 or done == len(jobs):
                    rate = done / (time.time() - started)
                    print(
                        f"progress {done}/{len(jobs)} ({rate:.2f}/s)",
                        file=sys.stderr,
                        flush=True,
                    )

            await asyncio.gather(*(worker(j) for j in jobs))
            if errors:
                Path(args.errors).write_text("\n".join(errors) + "\n")
            print(f"errors: {len(errors)}", file=sys.stderr)
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except Exception:
            proc.kill()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--list", required=True, help="jobs.tsv from make_jobs.py")
    ap.add_argument("--errors", required=True, help="file to write capture errors")
    ap.add_argument("--fontconfig", required=True, help="fonts.conf from gen_fonts_conf.py")
    ap.add_argument("--chrome", default=None, help="Chrome executable (auto-detected)")
    ap.add_argument("--profile", default="chrome-profile", help="Chrome profile dir")
    ap.add_argument("--home", default="chrome-home", help="HOME for the Chrome process")
    ap.add_argument("--jobs", type=int, default=8, help="concurrent pages (default 8)")
    ap.add_argument("--cap-height", type=int, default=20000, help="max page height")
    asyncio.run(run(ap.parse_args()))


if __name__ == "__main__":
    main()
