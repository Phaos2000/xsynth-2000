"""Wordmark contract tests on actual generated output, not old-art similarity.
GPL-2.0-or-later.
"""
import importlib.util
import hashlib
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('wordmark', root/'tools/GenerateTitle.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
image = Image.open(root/'assets/xsynth-title-native.png').convert('RGBA')
assert image.size == (336,38) and image.getbbox() == (0,0,336,38)
assert {image.getpixel((x,y))[3] for y in range(38) for x in range(336)} == {0,255}, 'Antialiased mask'
# Actual three output zero bitmaps must match, including shade and alpha.
zeros = [image.crop((x,0,x+29,38)).tobytes() for x in (241,274,307)]
assert zeros[0] == zeros[1] == zeros[2], 'Zeros differ'
ink = lambda x,y: bool(image.getpixel((x,y))[3])
assert sum(ink(241+x,19) for x in range(29)) == 10, 'Zero side stems not 5px each'
assert sum(ink(241+14,y) for y in range(38)) == 10, 'Zero caps not 5px each'
assert sum(ink(160+x,9) for x in range(29)) == 10, 'H stems not 5px each'
assert sum(ink(128+x,25) for x in range(29)) == 5, 'T stem not 5px'
assert sum(ink(160+14,y) for y in range(38)) == 5, 'H crossbar not 5px'
assert not any(ink(x,y) for y in range(38) for x in range(189,208)), 'Word gap filled'
# Regeneration may use only authored geometry, not an atlas. Verify deterministic
# PNG/header output by regenerating twice, rather than updating any baseline.
paths = [root/'port/TitleBitmap.h', root/'assets/xsynth-title-native.png']
before = [hashlib.sha256(p.read_bytes()).hexdigest() for p in paths]
module.generate()
assert before == [hashlib.sha256(p.read_bytes()).hexdigest() for p in paths]
module.generate()
assert before == [hashlib.sha256(p.read_bytes()).hexdigest() for p in paths]
print('PASS: 336x38 canvas/ink, binary alpha, identical zero masters, 5px strokes, word gap, deterministic PNG/header')
