"""Render crab state mockups as PNG images resembling the actual T-Display S3 screen."""

from PIL import Image, ImageDraw, ImageFont
import os

OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

# T-Display S3: 170x320 portrait, we render at 3x scale for crisp images
SCALE = 3
W, H = 170 * SCALE, 320 * SCALE
CELL = 8 * SCALE  # sprite cell size

# Colors
BG = (0, 0, 0)
CRAB = (0xCD, 0x7F, 0x55)
CRAB_DIM = (0x8B, 0x5E, 0x45)
EYE_OPEN = (0, 0, 0)  # black pupils on crab body
ALERT = (0xFF, 0x98, 0x00)
ZZZ_COLOR = (0x5C, 0x6B, 0xC0)
GREEN = (0x4C, 0xAF, 0x50)
AMBER = (0xFF, 0x98, 0x00)
WHITE = (255, 255, 255)
BLUE_BORDER = (0x00, 0x80, 0xFF)
LABEL_BLUE_GREY = (0x5C, 0x6B, 0xC0)

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

WORKING_F2 = [
    "............",
    "..CCCCCCCC..",
    "..CEECEECC..",
    "..CEECEECC..",
    "CCCCCCCCCCCC",
    "..CCCCCCCC..",
    "...CC..CC...",
    "..CC....CC..",
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
    """Draw a sprite on the image."""
    for r, row in enumerate(sprite):
        for c, ch in enumerate(row):
            if ch in color_map:
                x = ox + c * CELL
                y = oy + r * CELL
                draw.rectangle([x, y, x + CELL - 1, y + CELL - 1], fill=color_map[ch])


def get_font(size):
    """Try to load a good font, fall back to default."""
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


def draw_device_frame(img):
    """Draw a rounded-rect device bezel around the screen."""
    frame = Image.new("RGBA", (W + 40, H + 40), (30, 30, 30, 255))
    fd = ImageDraw.Draw(frame)
    # Bezel
    fd.rounded_rectangle([0, 0, W + 39, H + 39], radius=24, fill=(45, 45, 50))
    # Screen area
    frame.paste(img, (20, 20))
    return frame


def render_working():
    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)

    colors = {"C": CRAB, "E": EYE_OPEN}
    sprite_w = 12 * CELL
    sprite_h = 9 * CELL
    ox = (W - sprite_w) // 2
    oy = (H * 55 // 100 - sprite_h) // 2

    # Draw frame 1 (main)
    draw_sprite(draw, WORKING_F1, ox, oy, colors)

    # Status bar area
    bar_y = H * 60 // 100
    draw.line([(20, bar_y), (W - 20, bar_y)], fill=(40, 40, 40), width=1)

    font_big = get_font(20 * SCALE)
    font_sm = get_font(14 * SCALE)

    # "WORKING" label
    draw.text((W // 2 - 80 * SCALE, bar_y + 15), "WORKING", fill=GREEN, font=font_big)

    # Spinner + tool name
    draw.text((W // 2 - 80 * SCALE, bar_y + 55), "|  Bash", fill=WHITE, font=font_sm)

    framed = draw_device_frame(img)
    framed.save(os.path.join(OUT, "state-working.png"), "PNG")
    print("Saved state-working.png")


def render_waiting():
    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)

    colors = {"C": CRAB, "A": ALERT}
    sprite_w = 12 * CELL
    sprite_h = 9 * CELL
    ox = (W - sprite_w) // 2
    oy = (H * 55 // 100 - sprite_h) // 2

    draw_sprite(draw, WAITING_F, ox, oy, colors)

    # Blinking "!!" above head
    font_bang = get_font(24 * SCALE)
    bang_x = W // 2 - 12 * SCALE
    bang_y = oy - 35 * SCALE
    draw.text((bang_x, bang_y), "!!", fill=ALERT, font=font_bang)

    # Status bar
    bar_y = H * 60 // 100
    draw.line([(20, bar_y), (W - 20, bar_y)], fill=(40, 40, 40), width=1)

    font_big = get_font(20 * SCALE)
    font_sm = get_font(14 * SCALE)

    draw.text((W // 2 - 80 * SCALE, bar_y + 15), "WAITING", fill=AMBER, font=font_big)
    draw.text((W // 2 - 80 * SCALE, bar_y + 55), "needs your input", fill=WHITE, font=font_sm)

    framed = draw_device_frame(img)
    framed.save(os.path.join(OUT, "state-waiting.png"), "PNG")
    print("Saved state-waiting.png")


def render_idle():
    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)

    colors = {"D": CRAB_DIM, "-": BG}
    sprite_w = 12 * CELL
    sprite_h = 9 * CELL
    ox = (W - sprite_w) // 2
    oy = (H * 55 // 100 - sprite_h) // 2

    draw_sprite(draw, IDLE_F, ox, oy, colors)

    # Floating z's - snake pattern drifting up-right
    font_z = get_font(18 * SCALE)
    z_positions = [
        (ox + sprite_w + 5, oy + sprite_h - 50),
        (ox + sprite_w + 30, oy + sprite_h - 95),
        (ox + sprite_w + 55, oy + sprite_h - 140),
    ]
    for i, (zx, zy) in enumerate(z_positions):
        size = 14 + i * 3
        f = get_font(size * SCALE)
        draw.text((zx, zy), "z", fill=ZZZ_COLOR, font=f)

    # Status bar
    bar_y = H * 60 // 100
    draw.line([(20, bar_y), (W - 20, bar_y)], fill=(40, 40, 40), width=1)

    font_big = get_font(20 * SCALE)
    font_sm = get_font(14 * SCALE)

    draw.text((W // 2 - 80 * SCALE, bar_y + 15), "IDLE", fill=LABEL_BLUE_GREY, font=font_big)
    draw.text((W // 2 - 80 * SCALE, bar_y + 55), "sleeping...", fill=WHITE, font=font_sm)

    framed = draw_device_frame(img)
    framed.save(os.path.join(OUT, "state-idle.png"), "PNG")
    print("Saved state-idle.png")


def render_notification():
    """Notification is an overlay — show it on top of the working state."""
    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)

    colors = {"C": CRAB, "E": EYE_OPEN}
    sprite_w = 12 * CELL
    sprite_h = 9 * CELL
    ox = (W - sprite_w) // 2
    oy = (H * 55 // 100 - sprite_h) // 2

    draw_sprite(draw, WORKING_F1, ox, oy, colors)

    # Status bar (same as working)
    bar_y = H * 60 // 100
    font_big = get_font(20 * SCALE)
    font_sm = get_font(14 * SCALE)
    draw.text((W // 2 - 80 * SCALE, bar_y + 15), "WORKING", fill=GREEN, font=font_big)
    draw.text((W // 2 - 80 * SCALE, bar_y + 55), "|  Bash", fill=WHITE, font=font_sm)

    # Blue blinking border (4px * SCALE)
    thick = 4 * SCALE
    draw.rectangle([0, 0, W - 1, thick - 1], fill=BLUE_BORDER)           # top
    draw.rectangle([0, H - thick, W - 1, H - 1], fill=BLUE_BORDER)       # bottom
    draw.rectangle([0, 0, thick - 1, H - 1], fill=BLUE_BORDER)           # left
    draw.rectangle([W - thick, 0, W - 1, H - 1], fill=BLUE_BORDER)       # right

    framed = draw_device_frame(img)
    framed.save(os.path.join(OUT, "state-notification.png"), "PNG")
    print("Saved state-notification.png")


if __name__ == "__main__":
    render_working()
    render_waiting()
    render_idle()
    render_notification()
    print("All states rendered!")
