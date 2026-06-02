"""Create a side-by-side and pixel-diff visualization of two screenshots.
Usage: python tools/_diff_screenshots.py <before.png> <after.png> <out.png>
"""
import sys
from PIL import Image, ImageChops

a = Image.open(sys.argv[1]).convert('RGB')
b = Image.open(sys.argv[2]).convert('RGB')
if a.size != b.size:
    b = b.resize(a.size)

w, h = a.size
combo = Image.new('RGB', (w * 3 + 20, h), 'black')
combo.paste(a, (0, 0))
combo.paste(b, (w + 10, 0))

# Diff: amplify differences
diff = ImageChops.difference(a, b)
# Boost
import numpy as np
arr = np.array(diff)
arr = np.minimum(arr.astype(np.uint16) * 8, 255).astype(np.uint8)
diff_amp = Image.fromarray(arr)
combo.paste(diff_amp, (2 * w + 20, 0))

combo.save(sys.argv[3])
print(f'wrote {sys.argv[3]}  ({combo.size})')

# Quick stats: total diff magnitude
total = arr.sum()
non_zero = (arr.sum(axis=2) > 0).sum()
print(f'total diff: {total}, non-zero pixels: {non_zero}/{w*h} ({100*non_zero/(w*h):.2f}%)')
