# -*- coding: utf-8 -*-
"""Crop + upscale a region of a screenshot so small UI text becomes readable."""
import sys
from PIL import Image

src = sys.argv[1]
out = sys.argv[2]
l, t, r, b = (int(v) for v in sys.argv[3:7])
scale = int(sys.argv[7]) if len(sys.argv) > 7 else 3

im = Image.open(src)
box = im.crop((l, t, r, b))
box = box.resize((box.width * scale, box.height * scale), Image.LANCZOS)
box.save(out)
print(f"saved {out} {box.width}x{box.height} from box {(l, t, r, b)}")
