#!/usr/bin/env python3
"""Render every panel with its components into one HTML page (for checking by eye).
    python3 scripts/preview_panels.py out.html
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_panels as mp

ROOT = mp.ROOT
S = float(sys.argv[2]) if len(sys.argv) > 2 else 5  # px per mm
out = ['<html><body style="background:#555;margin:0;display:flex;gap:8px;padding:8px;align-items:flex-start">']
for p in mp.P:
    out.append(f'<div style="position:relative;width:{p.w*S}px;height:{mp.H*S}px">')
    out.append(f'<img src="file://{ROOT}/res/{p.slug}.svg" style="position:absolute;left:0;top:0;width:{p.w*S}px;height:{mp.H*S}px">')
    for it in p.items:
        k, x, y = it["kind"], it["x"], it["y"]
        if k == "led":
            out.append(f'<div style="position:absolute;left:{(x-1)*S}px;top:{(y-1)*S}px;width:{2*S}px;height:{2*S}px;border-radius:50%;background:#d9622b"></div>')
            continue
        name, w, h = {
            "knob": (it.get("k", {}).get("svg"), it.get("k", {}).get("size"), it.get("k", {}).get("size")),
            "jack": ("TxJack", 8, 8),
            "switch": ("Toggle2_0", mp.SW_W, mp.SW_H),
            "button": ("TxButton_0", mp.BTN_W, mp.BTN_H),
        }[k]
        out.append(f'<img src="file://{ROOT}/res/components/{name}.svg" style="position:absolute;left:{(x-w/2)*S}px;top:{(y-h/2)*S}px;width:{w*S}px;height:{h*S}px">')
        if k == "button" and it.get("light"):
            out.append(f'<div style="position:absolute;left:{(x-1)*S}px;top:{(y-1)*S}px;width:{2*S}px;height:{2*S}px;border-radius:50%;background:#511"></div>')
    out.append('</div>')
out.append('</body></html>')
open(sys.argv[1], 'w').write("\n".join(out))
