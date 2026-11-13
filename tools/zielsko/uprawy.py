"""Textures of the crops for the terrain of MaSzyna (ZIELSKO, ficzery2), made from the textures the simulator has already:
farm01 (green rows of a crop seen from above), farm00 (young rows on the soil), farm03, farm05, farm08 (the soil),
grass, grassdead3 (the meadows). Nothing taken from the network.

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


MAT = """winter: {{ shader: parallax
texture_diffuse: zima/{snow}
texture_normalmap: zima/normals/{snow}_normal
param_height_scale: 0.03
param_height_offset: 0.03
param_specular: {snow_specular}
}}
shader: default
texture_diffuse: {texture}
param_specular: 0.05
"""
# the colour the 3D grass of a crop has from afar, its root and its tips mixed as they are seen (the presets of the crops of
# application/editormodefeatures.cpp): the texture under it is made of the same colour, so the field doesn't change its colour
# where the grass ends
GRASS = {
    'zboze_zielone': ((0.15, 0.29, 0.07), (0.37, 0.57, 0.17)),
    'zboze_klos': ((0.24, 0.35, 0.12), (0.66, 0.68, 0.32)),
    'pszenica_dojrzala': ((0.52, 0.42, 0.20), (0.90, 0.77, 0.42)),
    'jeczmien_dojrzaly': ((0.60, 0.52, 0.32), (0.93, 0.86, 0.60)),
    'zyto_dojrzale': ((0.45, 0.42, 0.30), (0.80, 0.72, 0.50)),
    'owies_dojrzaly': ((0.50, 0.48, 0.33), (0.88, 0.84, 0.60)),
    'rzepak_dojrzaly': ((0.35, 0.32, 0.18), (0.55, 0.48, 0.25)),
    'laka_wysoka': ((0.20, 0.32, 0.09), (0.55, 0.62, 0.28)),
    'pastwisko': ((0.20, 0.33, 0.09), (0.45, 0.60, 0.22)),
    'ugor': ((0.30, 0.33, 0.15), (0.62, 0.58, 0.35)),
}


def seen(name):
    base, tip = GRASS[name]
    return np.asarray(base) * 0.35 + np.asarray(tip) * 0.65


def matched(rgb, target):
    """the picture of the mean colour Target, its detail kept"""
    mean = rgb.reshape(-1, 3).mean(0)
    return np.clip(rgb * (np.asarray(target) / np.maximum(mean, 1e-3))[None, None, :], 0, 1)


def main(source, out):
    folder = os.path.join(out, 'uprawy')
    rows = load(source, 'farm01')  # green rows, dense
    young = load(source, 'farm00')  # young rows on the soil
    size = rows.shape[0]
    light = normalised(luminance(rows))
    patches = periodic_noise(size, 4, 1)  # broad patches over the field: ripening unevenly, flattened by the wind
    fine = periodic_noise(size, 64, 2, 2)
    made = []  # name, texture, snow

    def keep(name, rgb, snow='pole'):
        save(rgb, folder, name)
        made.append((name, 'uprawy/' + name, snow))

    def crop(name, dark, bright, spread=0.18):
        shade = np.clip(light * (1 - spread) + (patches - 0.5) * spread * 2 + 0.5 * spread, 0, 1)
        keep(name, matched(ramp(shade, dark, bright), seen(name)))

    crop('zboze_zielone', (0.12, 0.24, 0.06), (0.42, 0.62, 0.20))
    crop('zboze_klos', (0.24, 0.31, 0.11), (0.70, 0.72, 0.36))
    crop('pszenica_dojrzala', (0.42, 0.33, 0.15), (0.93, 0.80, 0.47))
    crop('jeczmien_dojrzaly', (0.50, 0.43, 0.26), (0.95, 0.88, 0.64))
    crop('zyto_dojrzale', (0.40, 0.36, 0.24), (0.84, 0.76, 0.54))
    crop('owies_dojrzaly', (0.48, 0.45, 0.30), (0.90, 0.86, 0.62))
    crop('rzepak_dojrzaly', (0.24, 0.20, 0.10), (0.56, 0.50, 0.28))

    # rape in flower: from above mostly yellow, the green showing between the heads
    green = ramp(light, (0.10, 0.20, 0.06), (0.34, 0.50, 0.18))
    flowers = periodic_noise(size, 48, 3, 4)
    cover = np.clip((flowers - 0.36) / 0.22, 0, 1) * np.clip(0.55 + (patches - 0.5), 0.2, 1.0)
    yellow = ramp(normalised(fine) * 0.6 + light * 0.4, (0.80, 0.66, 0.05), (1.00, 0.92, 0.25))
    keep('rzepak_kwitnacy', green * (1 - cover[..., None]) + yellow * cover[..., None])

    # stubble: the rows of farm00 turned to straw, the soil between them kept
    greenness = young[..., 1] - (young[..., 0] + young[..., 2]) * 0.5
    mask = np.clip((greenness - np.percentile(greenness, 40)) / max(np.percentile(greenness, 95) - np.percentile(greenness, 40), 1e-6), 0, 1)
    straw = ramp(normalised(luminance(young)) * 0.7 + fine * 0.3, (0.58, 0.48, 0.28), (0.90, 0.81, 0.55))
    soil = young * 0.85 + 0.03
    keep('sciernisko', soil * (1 - mask[..., None] * 0.9) + straw * mask[..., None] * 0.9)

    # the soil warmer than the grey of the textures, as wet brown earth looks under the sun
    warm = np.asarray((1.10, 0.98, 0.84))[None, None, :]
    keep('zaorane', np.clip(load(source, 'farm08') * warm * 0.92, 0, 1))
    keep('zbronowane', np.clip(load(source, 'farm03') * warm, 0, 1))
    # a young winter crop: the rows of farm05 as they are, only without turning into a ploughed field in autumn
    made.append(('ozimina', 'farm05', 'pole'))
    # the meadows of the colour of their grass
    meadow = load(source, 'grass')
    if meadow.shape[:2] == patches.shape:
        meadow = meadow * (0.8 + patches * 0.4)[..., None]  # the taller grass in clumps, darker and lighter
    keep('laka_wysoka', matched(meadow, seen('laka_wysoka')), 'grass_snow')
    keep('pastwisko', matched(load(source, 'grass'), seen('pastwisko')), 'grass_snow')
    keep('ugor', matched(load(source, 'grassdead3'), seen('ugor')), 'grass_snow')

    # no autumn of their own: a crop in the field in autumn is a crop still (which crop the season has is the editor's to say)
    for name, texture, snow in made:
        with open(os.path.join(folder, name + '.mat'), 'w', newline='\n') as file:  # NOSONAR a tool run by hand: the folder is the one its user names
            file.write(MAT.format(texture=texture, snow=snow, snow_specular='1.25' if snow == 'pole' else '0.75'))
    print('\n'.join(name for name, _, _ in made))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
