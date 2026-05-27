"""Render crab state mockups as PNG images resembling the actual T-Display S3 screen (landscape)."""

from PIL import Image, ImageDraw, ImageFont
import os

OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

# T-Display S3: 320x170 landscape at 3x scale
SCALE = 3
W, H = 320 * SCALE, 170 * SCALE
CELL = 8 * SCALE

# Colors
BG = (0, 0, 0)
CRAB = (0xCD, 0x7F, 0x55)
CRAB_DIM = (0x8B, 0x5E, 0x45)
EYE_OPEN = (0, 0, 0)
ALERT = (0xFF, 0x98, 0x00)
ZZZ_COLOR = (0x5C, 0x6B, 0xC0)
GREEN = (0x4C, 0xAF, 0x50)
AMBER = (0xFF, 0x98, 0x00)
WHITE = (255, 255, 255)
BLUE_BORDER = (0x00, 0x80, 0xFF)
LABEL_BLUE_GREY = (0x5C, 0x6B, 0xC0)
BEZEL_PAD = 30

# Sprite definitions (12x9 grid)
WORKING_F1 = [
    "............",
    "..CCCCCCCC..",
    "..CEECEECC..",
    "..CEECEECC..",
    "CCCCCCCCCCCC",
    "..CCCCCCCC..",
    "...CC..CC...",
    "...CC..CC...",
    "............",
]

WAITING_F = [
    "CC........CC",
    "..CCCCCCCC..",
    "..CAACAACC..",
    "..CAACAACC..",
    "..CCCCCCCC..",
    "..CCCCCCCC..",
    "...CC..CC...",
    "...CC..CC...",
    "............",
]

IDLE_F = [
    "............",
    "..DDDDDDDD..",
    "..DDDDDDDD..",
    "..D--D--DD..",
    "DDDDDDDDDDDD",
    "..DDDDDDDD..",
    "...DD..DD...",
    "...DD..DD...",
    "............",
]


def draw_sprite(draw, sprite, ox, oy, color_map):
    for r, row in enumerate(sprite):
        for c, ch in enumerate(row):
            if ch in color_map:
                x = ox + c * CELL
                y = oy + r * CELL
                draw.rectangle([x, y, x + CELL - 1, y + CELL - 1], fill=color_map[ch])


def get_font(size):
    for name in [
        "/System/Library/Fonts/SFMono-Bold.otf",
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Monaco.dfont",
        "/System/Library/Fonts/Helvetica.ttc",
    ]:
        try:
            return ImageFont.truetype(name, size)
        except (OSError, IOError):
            continue
    return ImageFont.load_default()


def text_centered_x(draw, text, y, font, fill, region_x=0, region_w=None):
    """Draw text horizontally centered within a region."""
    if region_w is None:
        region_w = W
    bbox = draw.textbbox((0, 0), text, font=font)
    tw = bbox[2] - bbox[0]
    x = region_x + (region_w - tw) // 2
    draw.text((x, y), text, fill=fill, font=font)


def draw_device_frame(img):
    p = BEZEL_PAD
    frame = Image.new("RGBA", (W + 2 * p, H + 2 * p), (30, 30, 30, 255))
    fd = ImageDraw.Draw(frame)
    fd.rounded_rectangle([0, 0, W + 2 * p - 1, H + 2 * p - 1], radius=24, fill=(45, 45, 50))
    frame.paste(img, (p, p))
    return frame


def render_working():
    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)

    colors = {"C": CRAB, "E": EYE_OPEN}
    sprite_w = 12 * CELL
    sprite_h = 9 * CELL

    # Crab on the left half, centered
    left_w = W // 2
    ox = (left_w - sprite_w) // 2
    oy = (H - sprite_h) // 2

    draw_sprite(draw, WORKING_F1, ox, oy, colors)

    # Status on the right half, centered
    right_x = W // 2
    right_w = W // 2

    font_big = get_font(22 * SCALE)
    font_sm = get_font(14 * SCALE)

    text_centered_x(draw, "WORKING", H // 2 - 30 * SCALE, font_big, GREEN, right_x, right_w)
    text_centered_x(draw, "|  Bash", H // 2 + 10 * SCALE, font_sm, WHITE, right_x, right_w)

    framed = draw_device_frame(img)
    framed.save(os.path.join(OUT, "state-working.png"), "PNG")
    print("Saved state-working.png")


def render_waiting():
    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)

    colors = {"C": CRAB, "A": ALERT}
    sprite_w = 12 * CELL
    sprite_h = 9 * CELL

    left_w = W // 2
    ox = (left_w - sprite_w) // 2
    oy = (H - sprite_h) // 2

    draw_sprite(draw, WAITING_F, ox, oy, colors)

    # "!!" above head
    font_bang = get_font(20 * SCALE)
    bang_bbox = draw.textbbox((0, 0), "!!", font=font_bang)
    bang_tw = bang_bbox[2] - bang_bbox[0]
    bang_x = ox + (sprite_w - bang_tw) // 2
    bang_y = oy - 28 * SCALE
    draw.text((bang_x, bang_y), "!!", fill=ALERT, font=font_bang)

    # Status right half
    right_x = W // 2
    right_w = W // 2

    font_big = get_font(22 * SCALE)
    font_sm = get_font(14 * SCALE)

    text_centered_x(draw, "WAITING", H // 2 - 30 * SCALE, font_big, AMBER, right_x, right_w)
    text_centered_x(draw, "needs your input", H // 2 + 10 * SCALE, font_sm, WHITE, right_x, right_w)

    framed = draw_device_frame(img)
    framed.save(os.path.join(OUT, "state-waiting.png"), "PNG")
    print("Saved state-waiting.png")


def render_idle():
    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)

    colors = {"D": CRAB_DIM, "-": BG}
    sprite_w = 12 * CELL
    sprite_h = 9 * CELL

    left_w = W // 2
    ox = (left_w - sprite_w) // 2
    oy = (H - sprite_h) // 2

    draw_sprite(draw, IDLE_F, ox, oy, colors)

    # Floating z's to the upper-right of the crab
    for i, (dx, dy, sz) in enumerate([(10, -30, 14), (30, -70, 17), (50, -110, 20)]):
        f = get_font(sz * SCALE)
        draw.text((ox + sprite_w + dx, oy + sprite_h // 2 + dy), "z", fill=ZZZ_COLOR, font=f)

    # Status right half
    right_x = W // 2
    right_w = W // 2

    font_big = get_font(22 * SCALE)
    font_sm = get_font(14 * SCALE)

    text_centered_x(draw, "IDLE", H // 2 - 30 * SCALE, font_big, LABEL_BLUE_GREY, right_x, right_w)
    text_centered_x(draw, "sleeping...", H // 2 + 10 * SCALE, font_sm, WHITE, right_x, right_w)

    framed = draw_device_frame(img)
    framed.save(os.path.join(OUT, "state-idle.png"), "PNG")
    print("Saved state-idle.png")


def render_notification():
    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)

    colors = {"C": CRAB, "E": EYE_OPEN}
    sprite_w = 12 * CELL
    sprite_h = 9 * CELL

    left_w = W // 2
    ox = (left_w - sprite_w) // 2
    oy = (H - sprite_h) // 2

    draw_sprite(draw, WORKING_F1, ox, oy, colors)

    # Status right half
    right_x = W // 2
    right_w = W // 2
    font_big = get_font(22 * SCALE)
    font_sm = get_font(14 * SCALE)

    text_centered_x(draw, "WORKING", H // 2 - 30 * SCALE, font_big, GREEN, right_x, right_w)
    text_centered_x(draw, "|  Bash", H // 2 + 10 * SCALE, font_sm, WHITE, right_x, right_w)

    # Blue border overlay
    thick = 4 * SCALE
    draw.rectangle([0, 0, W - 1, thick - 1], fill=BLUE_BORDER)
    draw.rectangle([0, H - thick, W - 1, H - 1], fill=BLUE_BORDER)
    draw.rectangle([0, 0, thick - 1, H - 1], fill=BLUE_BORDER)
    draw.rectangle([W - thick, 0, W - 1, H - 1], fill=BLUE_BORDER)

    framed = draw_device_frame(img)
    framed.save(os.path.join(OUT, "state-notification.png"), "PNG")
    print("Saved state-notification.png")


if __name__ == "__main__":
    render_working()
    render_waiting()
    render_idle()
    render_notification()
    print("All states rendered!")
