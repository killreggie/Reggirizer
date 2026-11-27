#!/usr/bin/env python3
"""Builds the 07BASED plugin UI (Source/v2/ui/index.html) from the design mockup.

The mockup (planning/dvd.src.html) is the single source of truth for how the UI
looks and behaves. This script turns it into the copy that ships inside the plugin:
  - Google Fonts links -> the bundled open-licence fonts in ui/fonts
  - drops the page text around the TV and the art we can't redistribute
  - sizes the TV to fill the plugin window
  - loads bridge.js, which wires the UI to the plugin's parameters
Run it after changing the mockup:  python3 tools/make_ui.py
"""
import pathlib, re, shutil

root = pathlib.Path(__file__).resolve().parent.parent
src = root / "planning" / "dvd.src.html"
ui = root / "Source" / "v2" / "ui"
s = src.read_text(encoding="utf-8")

def sub(pattern, repl, flags=0, count=1, required=True):
    global s
    new, n = re.subn(pattern, repl, s, count=count, flags=flags)
    if required and n == 0:
        raise SystemExit(f"make_ui: pattern not found: {pattern[:80]}")
    s = new

fonts = [
    ("Orbitron", "Orbitron.ttf", "400 900"), ("VT323", "VT323.ttf", "400"), ("Michroma", "Michroma.ttf", "400"),
    ("Cinzel Decorative", "CinzelDecorative-Bold.ttf", "700"), ("Courier Prime", "CourierPrime-Bold.ttf", "700"),
    ("Lilita One", "LilitaOne.ttf", "400"), ("Roboto Condensed", "RobotoCondensed.ttf", "100 900"),
]
face = "\n".join(f'@font-face {{ font-family: "{n}"; src: url("fonts/{f}") format("truetype"); font-weight: {w}; font-display: block; }}' for n, f, w in fonts)

sub(r'<link rel="preconnect"[^>]*>\n', "")
sub(r'<link rel="stylesheet" href="https://fonts\.googleapis\.com[^>]*>\n', "")
sub(r'@font-face \{ font-family: "Skull";[^\n]*\n', "")
sub(r"<style>\n", "<style>\n" + face + "\n")
sub(r"<title>[^<]*</title>", '<meta charset="utf-8">\n<title>07BASED</title>')
sub(r'\s*<p class="intro">.*?</p>', "", flags=re.S)
sub(r'\s*<div class="notes">.*?</div>', "", flags=re.S)
sub(r'\s*<img class="splat"[^>]*>', "", count=0)
sub(r'im\.src = "dvd/pill_hand\.png";', "/* the pill-hand photo isn't shipped */")

plugin_css = """
/* ---- inside the plugin: the TV fills the window, nothing else on the page ---- */
html, body { margin: 0; height: 100%; overflow: hidden; background: #0c0b0a; -webkit-user-select: none; user-select: none; }
.wrap { max-width: none; padding: 0 !important; gap: 0; display: block; }
.room { padding: 0; }
.room::after { display: none; }
"""
sub(r"</style>", plugin_css + "</style>")
sub(r"<script>\n\(function \(\) \{", '<script>window.BASED = { plugin: !!window.__JUCE__, emit() {}, api: {} };</script>\n<script>\n(function () {')
s = s.rstrip()
assert s.endswith("</html>") or s.endswith("</script>"), "make_ui: unexpected file end"
s = re.sub(r"(</script>)(?![\s\S]*</script>)", r'\1\n<script type="module" src="bridge.js"></script>', s, count=1)

(ui / "index.html").write_text(s, encoding="utf-8")
for img in ["studio.jpg", "blue_metal_plate.jpg", "rusty_metal_04.jpg"]:
    shutil.copy(root / "planning" / "dvd" / img, ui / img)
print("wrote", ui / "index.html")
