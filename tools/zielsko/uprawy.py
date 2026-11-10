"""Textures of the crops for the terrain of MaSzyna (ZIELSKO, ficzery2), made from the textures the simulator has already:
farm01 (green rows of a crop seen from above), farm00 (young rows on the soil). Nothing taken from the network.

    python uprawy.py <folder of textures of MaSzyna> <folder out>

Writes <out>/uprawy/*.png and *.mat; the pictures tile as the sources do (the noise added wraps around)."""
import math
import os
import sys

import numpy as np
from PIL import Image


def periodic_noise(size, cells, seed, octaves=3):
    """value noise of 0..1 which wraps around a picture of size x size, cells across at the first octave"""
    rng = np.random.default_rng(seed)
    out = np.zeros((size, size))
    amplitude, total = 1.0, 0.0
    for octave in range(octaves):
        n = cells * (2 ** octave)
        grid = rng.random((n, n))
        t = np.arange(size) * n / size
        i0 = np.floor(t).astype(int) % n
        i1 = (i0 + 1) % n
        f = t - np.floor(t)
        f = f * f * (3 - 2 * f)
        rows = grid[i0][:, i0] * (1 - f)[None, :] + grid[i0][:, i1] * f[None, :]
        rows1 = grid[i1][:, i0] * (1 - f)[None, :] + grid[i1][:, i1] * f[None, :]
        layer = rows * (1 - f)[:, None] + rows1 * f[:, None]
        out += layer * amplitude
        total += amplitude
        amplitude *= 0.5
    return out / total


def load(folder, name):
    image = Image.open(os.path.join(folder, name + '.dds'))
    image.load()
    return np.asarray(image.convert('RGB'), dtype=np.float64) / 255.0


def luminance(rgb):
    return rgb[..., 0] * 0.3 + rgb[..., 1] * 0.59 + rgb[..., 2] * 0.11


def ramp(value, dark, light):
    """the brightness 0..1 of the source turned into colours from dark to light"""
    v = np.clip(value, 0, 1)[..., None]
    return np.asarray(dark)[None, None, :] * (1 - v) + np.asarray(light)[None, None, :] * v


def normalised(value):
    low, high = np.percentile(value, 2), np.percentile(value, 98)
    return np.clip((value - low) / max(high - low, 1e-6), 0, 1)


def save(rgb, folder, name):
    os.makedirs(folder, exist_ok=True)
    Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)).save(os.path.join(folder, name + '.png'))


MAT = """// ZIELSKO (ficzery2): {about}; made by tools/zielsko/uprawy.py from the textures of MaSzyna
winter: {{ shader: parallax
texture_diffuse: zima/pole
texture_normalmap: zima/normals/pole_normal
param_height_scale: 0.03
param_height_offset: 0.03
param_specular: 1.25
}}
{autumn}shader: default
texture_diffuse: uprawy/{name}
param_specular: 0.05
"""
PLOWED = """autumn: { shader: normalmap
texture_diffuse: field_plowed_01
texture_normalmap: normals/field_plowed_01_normal
}
"""


def main(source, out):
    folder = os.path.join(out, 'uprawy')
    rows = load(source, 'farm01')  # green rows, dense
    young = load(source, 'farm00')  # young rows on the soil
    size = rows.shape[0]
    light = normalised(luminance(rows))
    patches = periodic_noise(size, 4, 1)  # broad patches over the field: ripening unevenly, flattened by the wind
    fine = periodic_noise(size, 64, 2, 2)
    made = []

    def crop(name, dark, bright, about, spread=0.18, autumn=PLOWED):
        shade = np.clip(light * (1 - spread) + (patches - 0.5) * spread * 2 + 0.5 * spread, 0, 1)
        save(ramp(shade, dark, bright), folder, name)
        made.append((name, about, autumn))

    crop('pszenica_dojrzala', (0.42, 0.33, 0.15), (0.93, 0.80, 0.47), 'wheat, ripe')
    crop('jeczmien_dojrzaly', (0.50, 0.43, 0.26), (0.95, 0.88, 0.64), 'barley, ripe')
    crop('zyto_dojrzale', (0.40, 0.36, 0.24), (0.84, 0.76, 0.54), 'rye, ripe')
    crop('owies_dojrzaly', (0.48, 0.45, 0.30), (0.90, 0.86, 0.62), 'oats, ripe')
    crop('zboze_klos', (0.24, 0.31, 0.11), (0.70, 0.72, 0.36), 'cereal heading, green going yellow', autumn='')

    # rape in flower: from above mostly yellow, the green showing between the heads
    green = ramp(light, (0.10, 0.20, 0.06), (0.34, 0.50, 0.18))
    flowers = periodic_noise(size, 48, 3, 4)
    cover = np.clip((flowers - 0.36) / 0.22, 0, 1) * np.clip(0.55 + (patches - 0.5), 0.2, 1.0)
    yellow = ramp(normalised(fine) * 0.6 + light * 0.4, (0.80, 0.66, 0.05), (1.00, 0.92, 0.25))
    save(green * (1 - cover[..., None]) + yellow * cover[..., None], folder, 'rzepak_kwitnacy')
    made.append(('rzepak_kwitnacy', 'rape in flower', ''))
    crop('rzepak_dojrzaly', (0.24, 0.20, 0.10), (0.56, 0.50, 0.28), 'rape, ripe pods')

    # stubble: the rows of farm00 turned to straw, the soil between them kept
    greenness = young[..., 1] - (young[..., 0] + young[..., 2]) * 0.5
    mask = np.clip((greenness - np.percentile(greenness, 40)) / max(np.percentile(greenness, 95) - np.percentile(greenness, 40), 1e-6), 0, 1)
    straw = ramp(normalised(luminance(young)) * 0.7 + fine * 0.3, (0.58, 0.48, 0.28), (0.90, 0.81, 0.55))
    soil = young * 0.85 + 0.03
    save(soil * (1 - mask[..., None] * 0.9) + straw * mask[..., None] * 0.9, folder, 'sciernisko')
    made.append(('sciernisko', 'stubble', PLOWED))

    for name, about, autumn in made:
        with open(os.path.join(folder, name + '.mat'), 'w', newline='\n') as file:
            file.write(MAT.format(about=about, autumn=autumn, name=name))
    print('\n'.join(name for name, _, _ in made))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
