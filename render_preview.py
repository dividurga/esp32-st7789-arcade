#!/usr/bin/env python3
"""
Renders a static preview of the Pacman game as it would appear on the
ST7789 170×320 display with 6×6-pixel tiles (nearest-neighbour scaled from 8×8).

Usage:
    python3 render_preview.py
Output:
    st7789_preview.png  (170×320 logical, 4× upscaled to 680×1280 for readability)

Requires Pillow:
    pip install Pillow
"""

import os
import re
import sys

try:
    from PIL import Image, ImageDraw
except ImportError:
    sys.exit("Pillow not found.  Run:  pip install Pillow")

# ── Paths ──────────────────────────────────────────────────────────────────────
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TILES_H    = os.path.join(SCRIPT_DIR, "esp32_ILI9328_Pacman", "PacmanTiles.h")
OUTPUT     = os.path.join(SCRIPT_DIR, "st7789_preview.png")
SCALE      = 4   # output upscale factor for readability

# ── Display / game constants ───────────────────────────────────────────────────
DISPLAY_W    = 170
DISPLAY_H    = 320
TILE_SZ      = 6       # 6×6 px per tile on screen
MAZE_W       = 28      # tiles across
MAZE_H       = 36      # tiles down (rows 34-35 are icon area)
OFF_X        = (DISPLAY_W - MAZE_W * TILE_SZ) // 2   # 1 px
OFF_Y        = (DISPLAY_H - MAZE_H * TILE_SZ) // 2   # 52 px

# ── Palette (DrawIndexedMap.h) ─────────────────────────────────────────────────
PALETTE = [
    (  0,   0,   0),   #  0 black
    (255,   0,   0),   #  1 red
    (222, 151,  81),   #  2 orange
    (255,   0, 255),   #  3 pink/magenta
    (  0,   0,   0),   #  4 black
    (  0, 255, 255),   #  5 cyan
    ( 71,  84, 255),   #  6 mid blue
    (255, 184,  81),   #  7 lt brown
    (  0,   0,   0),   #  8 black
    (255, 255,   0),   #  9 yellow
    (  0,   0,   0),   # 10 black
    ( 33,  33, 255),   # 11 blue  ← maze walls on level 1
    (  0, 255,   0),   # 12 green
    ( 71,  84, 174),   # 13 aqua
    (255, 184, 174),   # 14 lt pink  ← dots / pen gate
    (222, 222, 255),   # 15 white-ish  ← text
]

DOT     = 7
PILL    = 14
PENGATE = 0x1B

# ── Sample scores to render ────────────────────────────────────────────────────
SAMPLE_SCORE   = 13750
SAMPLE_HISCORE = 47820


def format_score_row(score, width=7):
    """Right-align score into a width-element list of tile indices (0 = blank).
    Matches the game's  sprintf → SetScoreChar  logic exactly."""
    s = str(score)
    row = [0] * width
    offset = width - len(s)
    for i, ch in enumerate(s):
        row[offset + i] = ord(ch)
    return row

# ── Banner constants (must match drawBanner() in .ino) ────────────────────────
BANNER_TEXT   = "PACMAN"
BANNER_SCALE  = 2                              # 2× the 8×8 font tiles → 16×16 px
BANNER_CHAR_W = 8 * BANNER_SCALE               # 16 px per character
BANNER_TEXT_W = len(BANNER_TEXT) * BANNER_CHAR_W  # 96 px total
BANNER_X      = (DISPLAY_W - BANNER_TEXT_W) // 2  # 37 px from left
BANNER_Y      = (OFF_Y - 8 * BANNER_SCALE) // 2   # 18 px from top
BANNER_COLOR  = (255, 220, 0)                  # yellow

# ── Initial sprite positions from _initSprites[] ──────────────────────────────
# (name, cell_x, cell_y, fill_rgb, outline_rgb)
SPRITES = [
    ("BINKY",  14, 14, (220,   0,   0), (255, 80,  80)),
    ("PINKY",  12, 17, (255, 130, 220), (255, 180, 240)),
    ("INKY",   14, 17, (  0, 200, 220), ( 80, 240, 255)),
    ("CLYDE",  16, 17, (240, 150,  30), (255, 200,  80)),
    ("PACMAN", 14, 26, (255, 230,   0), (255, 255,  80)),
]


# ── C-array parser ─────────────────────────────────────────────────────────────
def parse_c_bytes(src, name):
    pat = rf'const byte {re.escape(name)}\[\][^{{]*\{{(.*?)\}};'
    m = re.search(pat, src, re.DOTALL)
    if not m:
        raise ValueError(f"Array '{name}' not found in PacmanTiles.h")
    body = m.group(1)
    body = re.sub(r'//[^\n]*', '', body)
    body = body.replace('EXCM', '91')                           # 'Z'+1 = 91
    body = re.sub(r"'(.)'", lambda x: str(ord(x.group(1))), body)
    return [int(t, 0) for t in re.findall(r'0x[0-9A-Fa-f]+|\d+', body)]


# ── Tile renderer ──────────────────────────────────────────────────────────────
def tile_palette_indices(play_tiles, play_map, cx, cy, has_dot, score_row, hiscore_row):
    """Return a flat 64-element (8×8) list of palette indices for one tile."""
    pixels = [0] * 64

    if cy >= 34:            # rows 34-35 are icon area; leave black for preview
        return pixels

    idx = cy * MAZE_W + cx
    b = play_map[idx] if idx < len(play_map) else 0

    # Override cy==1 with live score/hiscore strings (mirrors DrawBG logic)
    if cy == 1:
        if cx < 7:
            b = score_row[cx]
        elif 10 <= cx < 17:
            b = hiscore_row[cx - 10]
        else:
            b = 0

    # Wall colour for level 1  (LEVEL % 8 == 1 → colour 11 = blue)
    c = 11

    if b == DOT or b == PILL:
        if not has_dot:
            return pixels   # eaten dot
        c = 14              # lt pink
    elif b == PENGATE:
        c = 14

    if b >= ord('0'):       # ASCII printable → white text
        c = 15

    bg_off = b * 8
    if bg_off + 7 >= len(play_tiles):
        return pixels

    for row in range(8):
        raw = play_tiles[bg_off + row]
        # Replicate the original  `signed char bits; while (bits) { if (bits < 0) draw; bits <<= 1; }`
        bits = raw if raw < 128 else raw - 256
        for col in range(8):
            if bits == 0:
                break
            if bits < 0:
                pixels[row * 8 + col] = c
            bits = (bits << 1) & 0xFF
            if bits >= 128:
                bits -= 256

    return pixels


# Nearest-neighbour 8→6 scale table: output col → source col
_NN6 = [(ox * 8) // 6 for ox in range(6)]   # [0, 1, 2, 4, 5, 6]

def scale_tile_6x6(pixels8):
    """Nearest-neighbour downsample 8×8 palette list → 6×6."""
    out = []
    for oy in range(6):
        sy = (oy * 8) // 6
        row_base = sy * 8
        for ox in range(6):
            out.append(pixels8[row_base + _NN6[ox]])
    return out


# ── Main render ────────────────────────────────────────────────────────────────
def render(tiles_path):
    with open(tiles_path, 'r') as f:
        src = f.read()

    play_tiles = parse_c_bytes(src, 'playTiles')
    play_map   = parse_c_bytes(src, 'playMap1')

    # All dots present at game start
    dot_set = set()
    for cy in range(MAZE_H):
        for cx in range(MAZE_W):
            idx = cy * MAZE_W + cx
            if idx < len(play_map) and play_map[idx] in (DOT, PILL):
                dot_set.add((cx, cy))

    score_row   = format_score_row(SAMPLE_SCORE)
    hiscore_row = format_score_row(SAMPLE_HISCORE)

    img = Image.new('RGB', (DISPLAY_W, DISPLAY_H), (0, 0, 0))
    pix = img.load()

    # Draw maze tiles at 6×6
    for cy in range(MAZE_H):
        for cx in range(MAZE_W):
            raw8   = tile_palette_indices(play_tiles, play_map, cx, cy,
                                          (cx, cy) in dot_set,
                                          score_row, hiscore_row)
            scaled = scale_tile_6x6(raw8)
            sx = OFF_X + cx * TILE_SZ
            # Label row (cy==0) is rendered 2px above normal to create a gap
            # before the score number row, preventing them from visually merging.
            sy = OFF_Y + cy * TILE_SZ - (2 if cy == 0 else 0)
            for ty in range(TILE_SZ):
                for tx in range(TILE_SZ):
                    pix[sx + tx, sy + ty] = PALETTE[scaled[ty * TILE_SZ + tx]]

    # Draw sprites as small filled circles with a lighter outline
    draw = ImageDraw.Draw(img)
    r = TILE_SZ // 2
    for _name, cx, cy, fill, outline in SPRITES:
        cx_px = OFF_X + cx * TILE_SZ + TILE_SZ // 2
        cy_px = OFF_Y + cy * TILE_SZ + TILE_SZ // 2
        draw.ellipse([cx_px - r, cy_px - r, cx_px + r, cy_px + r],
                     fill=fill, outline=outline)

    # Draw "PACMAN" banner in the top band using the game's own font tiles
    draw_banner(img, play_tiles)

    return img


def draw_banner(img, play_tiles):
    """Render PACMAN title at 2× font scale in the top 52 px band."""
    pix = img.load()
    for ci, ch in enumerate(BANNER_TEXT):
        bg_off = ord(ch) * 8
        if bg_off + 7 >= len(play_tiles):
            continue
        cx = BANNER_X + ci * BANNER_CHAR_W
        for row in range(8):
            raw  = play_tiles[bg_off + row]
            bits = raw if raw < 128 else raw - 256
            for col in range(8):
                if bits == 0:
                    break
                if bits < 0:
                    for dy in range(BANNER_SCALE):
                        for dx in range(BANNER_SCALE):
                            px = cx + col * BANNER_SCALE + dx
                            py = BANNER_Y + row * BANNER_SCALE + dy
                            if 0 <= px < DISPLAY_W and 0 <= py < DISPLAY_H:
                                pix[px, py] = BANNER_COLOR
                bits = (bits << 1) & 0xFF
                if bits >= 128:
                    bits -= 256


def main():
    if not os.path.exists(TILES_H):
        sys.exit(f"Cannot find {TILES_H}\nRun this script from the project root directory.")

    print(f"Rendering {DISPLAY_W}×{DISPLAY_H} preview …")
    img = render(TILES_H)

    # Upscale with nearest-neighbour so individual pixels stay sharp
    preview = img.resize((DISPLAY_W * SCALE, DISPLAY_H * SCALE), Image.NEAREST)
    preview.save(OUTPUT)
    print(f"Saved  {OUTPUT}")
    print(f"       logical size : {DISPLAY_W}×{DISPLAY_H}")
    print(f"       saved size   : {DISPLAY_W * SCALE}×{DISPLAY_H * SCALE}  (×{SCALE} for readability)")
    print(f"       maze area    : {MAZE_W * TILE_SZ}×{MAZE_H * TILE_SZ} px  "
          f"(offset {OFF_X},{OFF_Y})")


if __name__ == '__main__':
    main()
