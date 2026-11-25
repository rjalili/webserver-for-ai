#!/usr/bin/env python3
"""Build the AI Chat extension for every browser.
   python3 build.py  ->  dist/chromium (Chrome, Brave, Edge, Opera), dist/firefox, dist/safari (+ .zip files)"""
import json, os, shutil, zipfile
from pathlib import Path

ROOT = Path(__file__).parent
SRC, DIST = ROOT / "src", ROOT / "dist"
VERSION = "0.1.0"
NAME = "AI Chat for websites"
DESC = "Talk to websites instead of clicking through them. Uses the open http-b protocol: sites tell you where you are, what you can do and where you can go."
ICONS = {s: f"icons/icon-{s}.png" for s in (16, 32, 48, 128)}
HOSTS = ["http://*/*", "https://*/*"]

def make_icons():
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("Pillow not installed: keeping existing icons"); return
    (SRC / "icons").mkdir(exist_ok=True)
    for s in ICONS:
        k = 8  # draw large, scale down for smooth edges
        S = s * k
        im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        d = ImageDraw.Draw(im)
        d.rounded_rectangle([0, 0, S - 1, S - 1], radius=S // 5, fill=(11, 92, 173, 255))
        # speech bubble
        m = S // 5
        d.rounded_rectangle([m, m, S - m, S - m - S // 7], radius=S // 8, fill=(255, 255, 255, 255))
        d.polygon([(m + S // 6, S - m - S // 7 - 1), (m + S // 6, S - m), (m + S // 3, S - m - S // 7 - 1)], fill=(255, 255, 255, 255))
        # three dots
        r = S // 18
        cy = (m + S - m - S // 7) // 2
        for i in (-1, 0, 1):
            cx = S // 2 + i * S // 6
            d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(11, 92, 173, 255))
        im.resize((s, s), Image.LANCZOS).save(SRC / ICONS[s])

def base():
    return {
        "manifest_version": 3, "name": NAME, "version": VERSION, "description": DESC,
        "icons": {str(k): v for k, v in ICONS.items()},
        "action": {"default_title": "AI Chat", "default_icon": {str(k): v for k, v in ICONS.items() if k <= 48}},
        "permissions": ["storage", "tabs", "webRequest"],
        "host_permissions": HOSTS,
    }

def manifests():
    chromium = base()
    chromium["permissions"].append("sidePanel")
    chromium["side_panel"] = {"default_path": "panel.html"}
    chromium["background"] = {"service_worker": "background.js"}
    chromium["minimum_chrome_version"] = "116"

    firefox = base()
    firefox["sidebar_action"] = {"default_panel": "panel.html", "default_title": "AI Chat", "default_icon": ICONS[32]}
    firefox["background"] = {"scripts": ["background.js"]}
    firefox["browser_specific_settings"] = {"gecko": {"id": "ai-chat@webserver-for-ai.org", "strict_min_version": "140.0",
                                                     "data_collection_permissions": {"required": ["none"]}},
                                           "gecko_android": {"strict_min_version": "142.0"}}
    safari = base()
    safari["action"]["default_popup"] = "panel.html"
    safari["background"] = {"scripts": ["background.js"], "persistent": False}
    return {"chromium": chromium, "firefox": firefox, "safari": safari}

def main():
    make_icons()
    if DIST.exists(): shutil.rmtree(DIST)
    for target, man in manifests().items():
        out = DIST / target
        shutil.copytree(SRC, out)
        (out / "manifest.json").write_text(json.dumps(man, indent=2) + "\n")
        z = DIST / f"http-b-{target}-{VERSION}.zip"
        with zipfile.ZipFile(z, "w", zipfile.ZIP_DEFLATED) as zf:
            for f in sorted(out.rglob("*")):
                if f.is_file(): zf.write(f, f.relative_to(out))
        print(f"built {target:9s} -> {z.relative_to(ROOT)}")

if __name__ == "__main__":
    main()
