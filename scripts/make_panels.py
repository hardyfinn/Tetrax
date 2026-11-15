#!/usr/bin/env python3
"""Generate the Tetrax panels (res/*.svg), the component SVGs (res/components/*.svg) and the
widget placement header (src/generated/Layout.hpp) from one table (spec §3).

Every control is placed from the same coordinates in both outputs, every control gets its
printed label, and the script fails if anything overlaps, sits too close or leaves the panel.

    python3 scripts/make_panels.py [--font /path/to/DejaVuSans-Bold.ttf]

Needs fontTools (pip install fonttools). Text is converted to paths, because Rack does not
render SVG text. Look: flat matte off-white, near-black ink, one accent per module, no silver.
"""
import math
import os
import sys

from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
if "--font" in sys.argv:
    FONT = sys.argv[sys.argv.index("--font") + 1]

# ---------------------------------------------------------------- look ------
GROUND = "#f2f1ed"
INK = "#1b1b1b"
PAPER = "#f2f1ed"      # text on dark output boxes
OUTBOX = "#1b1b1b"
H = 128.5
HP = 5.08
TOP = 11.5             # layout margin (title lives above)
BOTTOM = 121.8         # layout margin (plugin name lives below)
LABEL_CAP = 1.8
SMALL_CAP = 1.5
TITLE_CAP = 3.4
HEAD_CAP = 2.0
CONDENSE = 0.86

KNOB = dict(r=5.5, scale=6.6, box=7.0, svg="TxKnob", size=11.0)
SKNOB = dict(r=4.0, scale=5.0, box=5.4, svg="TxKnobSmall", size=8.0)
TRIM = dict(r=3.0, scale=0, box=2.9, svg="TxTrim", size=6.0)
JACK_R = 4.0
JACK_BOX = 4.2
LED_R = 1.0
SW_W, SW_H = 5.6, 9.6
BTN_W, BTN_H = 7.0, 5.0

# ---------------------------------------------------------------- text ------
_font = TTFont(FONT)
_glyphs = _font.getGlyphSet()
_cmap = _font.getBestCmap()
_cap = getattr(_font["OS/2"], "sCapHeight", 0) or 0.729 * _font["head"].unitsPerEm
_hmtx = _font["hmtx"]


def text_width(s, cap):
    scale = cap / _cap
    w = 0
    for ch in s:
        g = _cmap.get(ord(ch))
        if g is None:
            raise SystemExit(f"font has no glyph for {ch!r}")
        w += _hmtx[g][0]
    return w * scale * CONDENSE


def text_path(s, x, y, cap, anchor="middle"):
    scale = cap / _cap
    w = text_width(s, cap)
    if anchor == "middle":
        x -= w / 2
    elif anchor == "end":
        x -= w
    d = []
    pen_x = x
    for ch in s:
        g = _cmap[ord(ch)]
        sp = SVGPathPen(_glyphs)
        tp = TransformPen(sp, (scale * CONDENSE, 0, 0, -scale, pen_x, y))
        _glyphs[g].draw(tp)
        d.append(sp.getCommands())
        pen_x += _hmtx[g][0] * scale * CONDENSE
    return " ".join(p for p in d if p)


# ---------------------------------------------------------------- model -----
class Box:
    def __init__(self, x0, y0, x1, y1, what):
        self.x0, self.y0, self.x1, self.y1, self.what = x0, y0, x1, y1, what

    def overlaps(self, o, pad=0.25):
        return not (self.x1 + pad <= o.x0 or o.x1 + pad <= self.x0 or self.y1 + pad <= o.y0 or o.y1 + pad <= self.y0)

    def inside(self, x0, y0, x1, y1):
        return self.x0 >= x0 and self.x1 <= x1 and self.y0 >= y0 and self.y1 <= y1


class Panel:
    def __init__(self, slug, title, hp, accent):
        self.slug, self.title, self.hp, self.accent = slug, title, hp, accent
        self.w = hp * HP
        self.items = []
        self.outboxes = []
        self.headings = []   # (text, x, baseline, anchor)
        self.rules = []      # (x0, x1, y) accent rules under headings
        self.display = None

    def add(self, kind, pid, label, x, y, **ex):
        ex.setdefault("cap", LABEL_CAP)
        self.items.append(dict(kind=kind, pid=pid, label=label, x=x, y=y, **ex))

    def knob(self, pid, label, x, y, size="big", cap=None):
        k = {"big": KNOB, "small": SKNOB, "trim": TRIM}[size]
        self.add("knob", pid, label, x, y, k=k, cap=cap or (SMALL_CAP if size == "trim" else LABEL_CAP),
                 below=(size == "trim"))

    def jack(self, pid, label, x, y, cap=None):
        assert pid.endswith("_INPUT") or pid.endswith("_OUTPUT"), pid
        self.add("jack", pid, label, x, y, cap=cap or LABEL_CAP)

    def switch(self, pid, label, positions, x, y):
        self.add("switch", pid, label, x, y, positions=positions)

    def button(self, pid, label, x, y, light=None, color="white", cap=None):
        self.add("button", pid, label, x, y, light=light, color=color, cap=cap or LABEL_CAP)

    def led(self, pid, label, x, y, color="red", cap=None):
        self.add("led", pid, label, x, y, color=color, cap=cap or SMALL_CAP)

    def heading(self, text, x, y, x1=None):
        self.headings.append((text, x, y))
        if x1:
            self.rules.append((x, x1, y + 0.9))

    # ------------------------------------------------ geometry --
    def comp_box(self, it):
        x, y = it["x"], it["y"]
        kind = it["kind"]
        if kind == "knob":
            r = it["k"]["box"]
            return Box(x - r, y - r, x + r, y + r, it["pid"])
        if kind == "jack":
            return Box(x - JACK_BOX, y - JACK_BOX, x + JACK_BOX, y + JACK_BOX, it["pid"])
        if kind == "led":
            return Box(x - LED_R - 0.2, y - LED_R - 0.2, x + LED_R + 0.2, y + LED_R + 0.2, it["pid"])
        if kind == "switch":
            return Box(x - SW_W / 2, y - SW_H / 2, x + SW_W / 2, y + SW_H / 2, it["pid"])
        if kind == "button":
            return Box(x - BTN_W / 2, y - BTN_H / 2, x + BTN_W / 2, y + BTN_H / 2, it["pid"])
        raise ValueError(kind)

    def label_lines(self, it):
        out = []
        if not it["label"]:
            return out
        lines = it["label"].split("\n")
        cap = it["cap"]
        x, y = it["x"], it["y"]
        b = self.comp_box(it)
        kind = it["kind"]
        if kind == "knob" and not it.get("below"):
            base = y - it["k"]["scale"] - 1.4
            for i, ln in enumerate(reversed(lines)):
                out.append((ln, x, base - i * (cap + 0.7), cap, "middle"))
        elif kind in ("jack", "button", "led", "knob"):
            base = b.y1 + 1.0 + cap
            for i, ln in enumerate(lines):
                out.append((ln, x, base + i * (cap + 0.7), cap, "middle"))
        elif kind == "switch":
            base = b.y0 - 1.0
            for i, ln in enumerate(reversed(lines)):
                out.append((ln, x, base - i * (cap + 0.7), cap, "middle"))
        return out

    def switch_labels(self, it):
        pos = it["positions"]
        x, y = it["x"], it["y"]
        ys = [y - 3.4, y + 3.4 + SMALL_CAP] if len(pos) == 2 else [y - 3.4, y + SMALL_CAP / 2, y + 3.4 + SMALL_CAP]
        return [(p, x + SW_W / 2 + 0.8, yy, SMALL_CAP, "start") for p, yy in zip(pos, ys)]

    def all_labels(self):
        out = []
        for it in self.items:
            out += [(t, it) for t in self.label_lines(it)]
            if it["kind"] == "switch":
                out += [(t, it) for t in self.switch_labels(it)]
        return out

    def boxes(self):
        b = [self.comp_box(it) for it in self.items]
        for (t, x, base, cap, anchor), it in self.all_labels():
            w = text_width(t, cap)
            x0 = x - w / 2 if anchor == "middle" else x if anchor == "start" else x - w
            b.append(Box(x0, base - cap, x0 + w, base, f"label '{t}' of {it['pid']}"))
        for (t, x, y) in self.headings:
            w = text_width(t, HEAD_CAP)
            b.append(Box(x, y - HEAD_CAP, x + w, y, f"heading '{t}'"))
        return b

    def check(self):
        errs = []
        bx = self.boxes()
        if self.display:
            bx.append(Box(*self.display, "display"))
        for i, a in enumerate(bx):
            if a.x0 < 1.6 or a.x1 > self.w - 1.6 or a.y0 < TOP + 1.0 or a.y1 > BOTTOM - 1.0:
                errs.append(f"{a.what} leaves the panel area")
            for c in bx[i + 1:]:
                if a.overlaps(c):
                    errs.append(f"{a.what} overlaps {c.what}")
        for ob in self.outboxes:
            for a in bx:
                inside = a.inside(*ob)
                outside = a.x1 <= ob[0] or a.x0 >= ob[2] or a.y1 <= ob[1] or a.y0 >= ob[3]
                if not (inside or outside):
                    errs.append(f"{a.what} crosses an output box")
        for it in self.items:
            if it["kind"] == "jack" and it["pid"].endswith("_OUTPUT"):
                if not any(self.comp_box(it).inside(*ob) for ob in self.outboxes):
                    errs.append(f"{it['pid']} is not on an output box")
        comps = [it for it in self.items if it["kind"] in ("knob", "jack")]
        for i in range(len(comps)):
            for j in range(i + 1, len(comps)):
                a, c = comps[i], comps[j]
                d = math.hypot(a["x"] - c["x"], a["y"] - c["y"])
                ka = "trim" if a["kind"] == "knob" and a["k"] is TRIM else a["kind"]
                kc = "trim" if c["kind"] == "knob" and c["k"] is TRIM else c["kind"]
                pair = tuple(sorted((ka, kc)))
                need = {("knob", "knob"): 14.0, ("jack", "jack"): 10.0, ("jack", "trim"): 7.4,
                        ("trim", "trim"): 8.0, ("knob", "trim"): 10.0}.get(pair, 12.0)
                if d < need - 1e-6:
                    errs.append(f"{a['pid']} and {c['pid']} are {d:.1f} mm apart (need {need})")
        for it in self.items:
            if not it["label"]:
                errs.append(f"{it['pid']} has no label")
        return errs

    # ------------------------------------------------------------ output --
    def svg(self):
        w = self.w
        o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w:.2f}mm" height="{H}mm" viewBox="0 0 {w:.2f} {H}">']
        o.append(f'<rect x="0" y="0" width="{w:.2f}" height="{H}" fill="{GROUND}"/>')
        ink, paper, accent = [], [], []
        cap = TITLE_CAP
        while text_width(self.title, cap) > w - 23:
            cap -= 0.1
        ink.append(text_path(self.title, w / 2, 4.2 + cap, cap))
        ink.append(text_path("TETRAX", w / 2, H - 3.2, SMALL_CAP))
        g = []
        g.append(f'<rect x="{w / 2 - 6:.2f}" y="{H - 7.2:.2f}" width="12" height="0.35" fill="{self.accent}"/>')
        for (x0, y0, x1, y1) in self.outboxes:
            g.append(f'<rect x="{x0:.2f}" y="{y0:.2f}" width="{x1 - x0:.2f}" height="{y1 - y0:.2f}" rx="1.4" fill="{OUTBOX}"/>')
        if self.display:
            x0, y0, x1, y1 = self.display
            g.append(f'<rect x="{x0:.2f}" y="{y0:.2f}" width="{x1 - x0:.2f}" height="{y1 - y0:.2f}" rx="1.0" fill="#1b1b1b"/>')
        for (x0, x1, y) in self.rules:
            g.append(f'<rect x="{x0:.2f}" y="{y:.2f}" width="{x1 - x0:.2f}" height="0.35" fill="{self.accent}"/>')
        for it in self.items:
            x, y = it["x"], it["y"]
            if it["kind"] == "knob" and it["k"]["scale"] > 0:
                r = it["k"]["scale"]
                for t in range(11):
                    a = math.radians(-135 + 27 * t - 90)
                    g.append(f'<line x1="{x + r * math.cos(a):.2f}" y1="{y + r * math.sin(a):.2f}" x2="{x + (r + 0.7) * math.cos(a):.2f}" y2="{y + (r + 0.7) * math.sin(a):.2f}" stroke="{INK}" stroke-width="0.25"/>')

        def on_outbox(x, y):
            return any(ob[0] <= x <= ob[2] and ob[1] <= y <= ob[3] for ob in self.outboxes)
        for (t, x, base, cap_, anchor), it in self.all_labels():
            (paper if on_outbox(x, base - cap_ / 2) else ink).append(text_path(t, x, base, cap_, anchor))
        for (t, x, y) in self.headings:
            accent.append(text_path(t, x, y, HEAD_CAP, "start"))
        o.append("".join(g))
        o.append(f'<path fill="{INK}" d="{" ".join(ink)}"/>')
        o.append(f'<path fill="{PAPER}" d="{" ".join(paper)}"/>')
        o.append(f'<path fill="{self.accent}" d="{" ".join(accent)}"/>')
        o.append("</svg>")
        return "\n".join(o)

    def cpp(self):
        L = [f"template <class M, class W>\ninline void place{self.slug}(W* w, M* m) {{"]
        lights = {"red": "RedLight", "green": "GreenLight", "yellow": "YellowLight", "white": "WhiteLight",
                  "rgb": "RedGreenBlueLight", "gr": "GreenRedLight", "orange": "TxOrangeLight"}
        for it in self.items:
            v = f"mm2px(Vec({it['x']:.3f}f, {it['y']:.3f}f))"
            pid, kind = it["pid"], it["kind"]
            if kind == "knob":
                L.append(f"\tw->addParam(createParamCentered<{it['k']['svg']}>({v}, m, M::{pid}));")
            elif kind == "switch":
                L.append(f"\tw->addParam(createParamCentered<TxToggle2>({v}, m, M::{pid}));")
            elif kind == "jack":
                fn, add = ("createInputCentered", "addInput") if pid.endswith("_INPUT") else ("createOutputCentered", "addOutput")
                L.append(f"\tw->{add}({fn}<TxJack>({v}, m, M::{pid}));")
            elif kind == "led":
                L.append(f"\tw->addChild(createLightCentered<SmallLight<{lights[it['color']]}>>({v}, m, M::{pid}));")
            elif kind == "button":
                if it["light"]:
                    L.append(f"\tw->addParam(createLightParamCentered<LightButton<TxButton, TxButtonLight<{lights[it['color']]}>>>({v}, m, M::{pid}, M::{it['light']}));")
                else:
                    L.append(f"\tw->addParam(createParamCentered<TxButton>({v}, m, M::{pid}));")
        L.append("}\n")
        if self.display:
            x0, y0, x1, y1 = self.display
            L.append(f"constexpr float {self.slug}_DISPLAY_X = {x0:.3f}f, {self.slug}_DISPLAY_Y = {y0:.3f}f, "
                     f"{self.slug}_DISPLAY_W = {x1 - x0:.3f}f, {self.slug}_DISPLAY_H = {y1 - y0:.3f}f;\n")
        return "\n".join(L)

    def labels(self):
        return [(it["pid"], it["label"].replace("\n", " ")) for it in self.items]


P = []
TEAL = "#1f6f78"
ORANGE = "#d9622b"

# ---------------------------------------------------------------- TETRA -----
p = Panel("TETRA", "TETRA", 30, TEAL)
COLW = 34.5
for i in range(4):
    n = i + 1
    c = COLW / 2 + COLW * i
    p.led(f"CH{n}_LIGHT", f"CH {n}", c, 17.0, color="rgb", cap=LABEL_CAP)
    p.knob(f"RISE{n}_PARAM", "RISE", c - 8, 31)
    p.knob(f"FALL{n}_PARAM", "FALL", c + 8, 31)
    p.knob(f"SHAPE{n}_PARAM", "SHAPE", c - 8, 48, size="small")
    p.knob(f"LEVEL{n}_PARAM", "LEVEL", c + 8, 48, size="small")
    p.button(f"MODE{n}_PARAM", "MODE", c - 9, 59.5, light=f"MODE{n}_LIGHT", color="rgb")
    p.button(f"ALT{n}_PARAM", "ALT", c, 59.5, light=f"ALT{n}_LIGHT", color="yellow")
    p.button(f"LINK{n}_PARAM", "LINK", c + 9, 59.5, light=f"LINK{n}_LIGHT", color="rgb")
    p.button(f"TENV{n}_PARAM", "10V", c - 9, 69, light=f"TENV{n}_LIGHT", color="white")
    p.button(f"FIRE{n}_PARAM", "FIRE", c + 9, 69, light=f"FIRE{n}_LIGHT", color="red")
    for (y, a, b) in [(81, "RISE", "FALL"), (96, "SHAPE", "LEVEL")]:
        p.jack(f"{a}CV{n}_INPUT", a, c - 11.2, y, cap=SMALL_CAP)
        p.knob(f"{a}ATT{n}_PARAM", "ATT", c - 3.8, y, size="trim")
        p.jack(f"{b}CV{n}_INPUT", b, c + 4.5, y, cap=SMALL_CAP)
        p.knob(f"{b}ATT{n}_PARAM", "ATT", c + 11.9, y, size="trim")
    p.jack(f"TRIG{n}_INPUT", "TRIG", c - 8, 110)
    p.outboxes.append((c + 1.8, 103.6, c + 14.2, 119.6))
    p.jack(f"OUT{n}_OUTPUT", "OUT", c + 8, 110)
sx = 4 * COLW + (p.w - 4 * COLW) / 2
p.jack("POLYTRIG_INPUT", "POLY\nTRIG", sx, 92, cap=SMALL_CAP)
p.outboxes.append((sx - 5.4, 103.6, sx + 5.4, 119.6))
p.jack("POLY_OUTPUT", "POLY", sx, 110, cap=SMALL_CAP)
P.append(p)

# ---------------------------------------------------------------- TETRAX ----
p = Panel("TETRAX", "TETRA X", 6, TEAL)
p.led("LINKED_LIGHT", "LINKED", p.w / 2, 16.0, color="green")
p.outboxes.append((2.4, 22.0, p.w - 2.4, 119.6))
for i in range(4):
    n = i + 1
    y = 29 + 18 * i
    p.jack(f"EOR{n}_OUTPUT", f"EOR {n}", 9.0, y, cap=SMALL_CAP)
    p.jack(f"EOF{n}_OUTPUT", f"EOF {n}", p.w - 9.0, y, cap=SMALL_CAP)
p.jack("POLYEOR_OUTPUT", "POLY\nEOR", 9.0, 106, cap=SMALL_CAP)
p.jack("POLYEOF_OUTPUT", "POLY\nEOF", p.w - 9.0, 106, cap=SMALL_CAP)
P.append(p)

# ---------------------------------------------------------------- PLINK -----
p = Panel("PLINK", "PLINK", 32, ORANGE)
W = p.w
p.display = (5.0, 13.5, 72.0, 31.0)
bx = [80.0 + 10.5 * i for i in range(7)]
for x, (pid, lab, light, col) in zip(bx, [("XSET", "X SET", "XSET_LIGHT", "orange"), ("YSET", "Y SET", "YSET_LIGHT", "orange"),
                                         ("MODSET", "MOD SET", "MODSET_LIGHT", "orange"), ("DEPTH", "DEPTH", None, None),
                                         ("OCTDN", "OCT -", None, None), ("OCTUP", "OCT +", None, None),
                                         ("QUANT", "QUANT", "QUANT_LIGHT", "white")]):
    p.button(pid + "_PARAM", lab, x, 16.2, light=light, color=col or "white")
for x, (pid, lab, light, col) in zip(bx, [("LOAD", "LOAD", "LOAD_LIGHT", "orange"), ("SAVE", "SAVE", "SAVE_LIGHT", "orange"),
                                         ("PREV", "PREV", None, None), ("NEXT", "NEXT", None, None),
                                         ("RANDOM", "RANDOM", None, None), ("OK", "OK", None, None),
                                         ("TRIGGER", "TRIGGER", "TRIGGER_LIGHT", "red")]):
    p.button(pid + "_PARAM", lab, x, 25.8, light=light, color=col or "white")
p.knob("SELECT_PARAM", "SELECT", 155.4, 22.5, size="small")

ec = [9.0, 23.0, 37.0, 51.0]
p.heading("EXCITER", 3.0, 35.0, 58.0)
rows = [(45, [("MIX", "MIX"), ("STIFF", "STIFF"), ("DENS", "DENSITY"), ("NATK", "ATTACK")]),
        (60, [("NDEC", "DECAY"), ("LPCUT", "LP CUT"), ("LPENV", "LP ENV"), ("LPQ", "LP Q")]),
        (75, [("HPCUT", "HP CUT"), ("HPENV", "HP ENV"), ("HPQ", "HP Q")])]
for y, row in rows:
    for x, (pid, lab) in zip(ec, row):
        p.knob(pid + "_PARAM", lab, x, y, size="small")
p.switch("ENVTYPE_PARAM", "ENV", ["AR", "AHR"], ec[3] - 1.5, 76.0)

oc = [72.0, 86.0, 100.0, 114.0]
p.heading("OBJECT", 66.0, 35.0, 121.0)
for x, (pid, lab) in zip(oc, [("RDEC", "OBJ DECAY"), ("RPOS", "POSITION"), ("RTONE", "TONE"), ("RINH", "INHARM")]):
    p.knob(pid + "_PARAM", lab, x, 45, size="small")
for x, (pid, lab) in zip(oc, [("RLOWCUT", "LOW CUT"), ("PENVAMT", "PENV AMT"), ("PENVDEC", "PENV DEC")]):
    p.knob(pid + "_PARAM", lab, x, 60, size="small")
p.button("TYPE_PARAM", "TYPE", oc[3], 60.0)
for i, name in enumerate(["STRING", "BEAM", "MARIMBA", "DRUMHEAD", "MEMBRANE", "PLATE"]):
    p.led(f"TYPE{i}_LIGHT", name, oc[i % 3], 72.0 + 8.5 * (i // 3), color="orange")

p.heading("PERFORM", 130.0, 35.0, W - 3.0)
p.knob("PITCH_PARAM", "PITCH", 138.0, 47.0)
p.knob("DECAY_PARAM", "DECAY ALL", 153.0, 47.0)
p.knob("X_PARAM", "X", 138.0, 69.0)
p.knob("Y_PARAM", "Y", 153.0, 69.0)

jx = [10.0, 24.0, 38.0, 52.0, 66.0, 80.0, 94.0]
for x, (pid, lab) in zip(jx[:4], [("XATT", "X ATT"), ("YATT", "Y ATT"), ("MODATT", "MOD ATT"), ("DECAYATT", "DEC ATT")]):
    p.knob(pid + "_PARAM", lab, x, 95.0, size="small")
for x, (pid, lab) in zip(jx, [("X", "X"), ("Y", "Y"), ("MOD", "MOD"), ("DECAY", "DECAY"), ("PITCH", "PITCH"), ("TRIG", "TRIG"), ("VEL", "VEL")]):
    p.jack(pid + "_INPUT", lab, x, 110.0)
p.outboxes.append((105.0, 103.6, 119.0, 119.6))
p.jack("OUT_OUTPUT", "OUT", 112.0, 110.0)
p.heading("OUTPUT", 117.0, 86.0, W - 3.0)
for x, (pid, lab) in zip([124.0, 138.0, 152.0], [("GAIN", "GAIN"), ("SAT", "SATURATE"), ("CRUSH", "CRUSH")]):
    p.knob(pid + "_PARAM", lab, x, 97.0, size="small")
p.led("VOICE1_LIGHT", "VOICE 1", 130.0, 110.0, color="orange")
p.led("VOICE2_LIGHT", "VOICE 2", 146.0, 110.0, color="orange")
P.append(p)


# ---------------------------------------------------------------- components --
def components():
    c = {}
    c["TxJack"] = '''<svg xmlns="http://www.w3.org/2000/svg" width="8mm" height="8mm" viewBox="0 0 8 8">
<circle cx="4" cy="4" r="3.9" fill="#2a2a2a"/>
<circle cx="4" cy="4" r="2.75" fill="#4a4a4a"/>
<circle cx="4" cy="4" r="1.9" fill="#0b0b0b"/>
</svg>'''

    def knob(size, name):
        r = size / 2
        c[name] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{size}mm" height="{size}mm" viewBox="0 0 {size} {size}">
<circle cx="{r}" cy="{r}" r="{r}" fill="#1b1b1b"/>
<circle cx="{r}" cy="{r}" r="{r * 0.8:.3f}" fill="#262626"/>
<line x1="{r}" y1="{r * 0.14:.3f}" x2="{r}" y2="{r * 0.75:.3f}" stroke="#f2f1ed" stroke-width="{max(0.45, size * 0.065):.3f}" stroke-linecap="round"/>
</svg>'''
    knob(KNOB["size"], "TxKnob")
    knob(SKNOB["size"], "TxKnobSmall")
    knob(TRIM["size"], "TxTrim")

    def rect_btn(name, w, h, fill, edge, pressed):
        inset = 0.35 if pressed else 0.0
        c[name] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{w}mm" height="{h}mm" viewBox="0 0 {w} {h}">
<rect x="0" y="0" width="{w}" height="{h}" rx="0.8" fill="{edge}"/>
<rect x="{0.4 + inset:.2f}" y="{0.4 + inset:.2f}" width="{w - 0.8 - 2 * inset:.2f}" height="{h - 0.8 - 2 * inset:.2f}" rx="0.6" fill="{fill}"/>
</svg>'''
    rect_btn("TxButton_0", BTN_W, BTN_H, "#3a3a3a", "#1b1b1b", False)
    rect_btn("TxButton_1", BTN_W, BTN_H, "#2a2a2a", "#1b1b1b", True)

    def toggle(frame):
        d = {0: -1, 1: 1}[frame]
        cx, cy = 2.8, 4.8
        tip = cy + d * 3.4
        return f'''<svg xmlns="http://www.w3.org/2000/svg" width="5.6mm" height="9.6mm" viewBox="0 0 5.6 9.6">
<rect x="0.3" y="0.3" width="5.0" height="9.0" rx="0.8" fill="#1b1b1b"/>
<line x1="{cx}" y1="{cy}" x2="{cx}" y2="{tip}" stroke="#f2f1ed" stroke-width="1.3" stroke-linecap="round"/>
<circle cx="{cx}" cy="{tip}" r="1.0" fill="#f2f1ed"/>
</svg>'''
    c["Toggle2_0"] = toggle(0)
    c["Toggle2_1"] = toggle(1)
    return c


def main():
    errs = []
    for p in P:
        for e in p.check():
            errs.append(f"{p.slug}: {e}")
    if errs:
        print("\n".join(errs))
        raise SystemExit(f"{len(errs)} layout problems")
    os.makedirs(os.path.join(ROOT, "res", "components"), exist_ok=True)
    os.makedirs(os.path.join(ROOT, "src", "generated"), exist_ok=True)
    for p in P:
        with open(os.path.join(ROOT, "res", f"{p.slug}.svg"), "w") as f:
            f.write(p.svg())
    for name, svg in components().items():
        with open(os.path.join(ROOT, "res", "components", f"{name}.svg"), "w") as f:
            f.write(svg)
    with open(os.path.join(ROOT, "src", "generated", "Layout.hpp"), "w") as f:
        f.write("// Generated by scripts/make_panels.py. Do not edit.\n#pragma once\n#include \"../widgets/TetraxComponents.hpp\"\n\n")
        for p in P:
            f.write(p.cpp())
    with open(os.path.join(ROOT, "src", "generated", "labels.txt"), "w") as f:
        for p in P:
            for pid, lab in p.labels():
                f.write(f"{p.slug} {pid} {lab}\n")
    print(f"wrote {len(P)} panels")


if __name__ == "__main__":
    main()
