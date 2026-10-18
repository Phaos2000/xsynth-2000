"""Original fixed Xsynth 2000 wordmark. GPL-2.0-or-later.
Independent geometric construction for this project, not an extracted font.
Only the eight characters needed by this wordmark are defined. Rasterization
samples binary polygon coverage at native pixel centers; no antialiasing,
external artwork inputs, bitmap transformation or outline matching.
"""
from pathlib import Path
import json
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
WIDTH, HEIGHT = 336, 38
STROKE = 5
# Shapes use grid-line coordinates: [0,5) is exactly five native pixels.
# X/N bands have a 6px horizontal intercept, giving 5.13px perpendicular
# thickness at their slope. 45-degree joins use a 7px intercept (4.95px).
# All shade changes are inside these silhouettes, never an outward dilation.
SHAPES = {
    'X': [[(0,0),(6,0),(29,38),(23,38)],
          [(23,0),(29,0),(6,38),(0,38)]],
    'S': [[(7,0),(29,0),(29,5),(9,5),(5,9),(5,14),(9,18),
           (22,18),(29,25),(29,31),(22,38),(0,38),(0,33),
           (20,33),(24,29),(24,27),(20,23),(7,23),(0,16),(0,7)]],
    'Y': [[(0,0),(6,0),(14.5,14),(23,0),(29,0),(17,20),
           (17,38),(12,38),(12,20)]],
    'N': [[(0,0),(5,0),(5,38),(0,38)],
          [(24,0),(29,0),(29,38),(24,38)],
          [(0,0),(6,0),(29,38),(23,38)]],
    'T': [[(0,0),(29,0),(29,5),(17,5),(17,38),(12,38),(12,5),(0,5)]],
    'H': [[(0,0),(5,0),(5,16),(24,16),(24,0),(29,0),
           (29,38),(24,38),(24,21),(5,21),(5,38),(0,38)]],
    '2': [[(7,0),(22,0),(29,7),(29,14),(10,33),(29,33),
           (29,38),(0,38),(0,36),(24,12),(24,9),(20,5),
           (9,5),(5,9),(5,12),(0,12),(0,7)]],
    '0': [[(8,0),(21,0),(29,8),(29,30),(21,38),(8,38),(0,30),(0,8)]],
}
# Offset octagonal counter: outer diagonal x+y=8, inner x+y=15.
# Normal thickness at 45 degrees = 7/sqrt(2), consistent with 5px stems.
ZERO_COUNTER = [(10,5),(19,5),(24,10),(24,28),(19,33),(10,33),(5,28),(5,10)]
PLACEMENTS = list(zip('XSYNTH2000', [0,32,64,96,128,160,208,241,274,307]))


def inside(poly, x, y):
    result = False
    j = len(poly)-1
    for i, (xi,yi) in enumerate(poly):
        xj,yj = poly[j]
        if (yi > y) != (yj > y) and x < (xj-xi)*(y-yi)/(yj-yi)+xi:
            result = not result
        j = i
    return result


def master(char):
    return {(x,y) for y in range(HEIGHT) for x in range(29)
            if any(inside(poly,x+.5,y+.5) for poly in SHAPES[char])
            and not (char == '0' and inside(ZERO_COUNTER,x+.5,y+.5))}


def generate():
    masters = {char: master(char) for char in SHAPES}
    image = Image.new('RGBA', (WIDTH,HEIGHT))
    # Crisp planar steel face with one-pixel internal bevel only. Existing
    # Editor theme::title mapping and hard-shadow renderer remain unchanged.
    for char,left in PLACEMENTS:
        mask = masters[char]
        for x,y in mask:
            if (x-1,y-1) not in mask:
                shade = 243
            elif (x+1,y+1) not in mask:
                shade = 63
            else:
                shade = 177 - 8*(y//10)
            image.putpixel((left+x,y),(shade,shade,shade,255))
    pixels = [(a<<24)|(r<<16)|(g<<8)|b
              for y in range(HEIGHT) for x in range(WIDTH)
              for r,g,b,a in [image.getpixel((x,y))]]
    image.save(ROOT/'assets/xsynth-title-native.png')
    header = [
        '// Original fixed bitmap wordmark for Xsynth 2000. GPL-2.0-or-later.',
        '// Independently constructed polygons; tools/GenerateTitle.py. No font/image input.',
        '#pragma once','#include <cstdint>','namespace xs::title {',
        'inline constexpr int Width = 336, Height = 38;',
        'inline constexpr uint32_t Pixels[] = {',
    ]
    for y in range(HEIGHT):
        header.append('  '+','.join(f'0x{v:08x}u' for v in pixels[y*WIDTH:(y+1)*WIDTH])+',')
    header += ['};','} // namespace xs::title','']
    (ROOT/'port/TitleBitmap.h').write_text('\n'.join(header),encoding='utf-8')
    return image


if __name__ == '__main__':
    image = generate()
    print(json.dumps({'canvas':image.size,'occupied_bbox':image.getbbox(),
                      'cap_top':0,'baseline_exclusive':38,'master_stroke':STROKE}))
