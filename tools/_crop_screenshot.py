"""Crop a screenshot to a region of interest and save it.
Usage: python tools/_crop_screenshot.py <in.png> <out.png> <x> <y> <w> <h>
"""
import sys
from PIL import Image

src, dst, x, y, w, h = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), int(sys.argv[6])
img = Image.open(src)
print(f'src: {img.size}')
crop = img.crop((x, y, x+w, y+h))
crop.save(dst)
print(f'saved {dst}  size={crop.size}')
