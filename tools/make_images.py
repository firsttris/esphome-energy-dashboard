"""Erzeugt die vorgerenderten Bilder in images/ (Glow, Sonne, Mond, Akku-Welle, Akku-Maske).

Alles, was weich leuchtet oder rund maskiert ist, liegt als fertiges PNG mit Alpha vor:
LVGL muss dann nur noch Bilder kopieren statt Schatten oder Masken zu rechnen.
Aufruf: python3 tools/make_images.py
"""
import math
import os

from PIL import Image, ImageDraw, ImageFilter

OUT = os.path.join(os.path.dirname(__file__), "..", "images")
BG = (11, 13, 16)  # Hintergrundfarbe der Seite (0x0B0D10)

COLORS = {
    "pv": (255, 182, 39),
    "grid": (46, 184, 255),
    "exp": (181, 123, 255),
    "bat": (255, 61, 139),
    "boil": (255, 106, 43),
    "white": (255, 255, 255),
}


def save(img, name):
    img.save(os.path.join(OUT, name + ".png"))


def glow_dot(color, size=20, core=3.0):
    """Kometenkopf: heller Kern mit weichem Schein."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    c = (size - 1) / 2
    px = img.load()
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - c, y - c)
            if d <= core:
                a = 1.0
            else:
                a = 0.5 * math.exp(-((d - core) ** 2) / 14.0)
            if d <= core + 1:
                # Kern zur Mitte hin weißlich
                t = max(0.0, 1 - d / (core + 1))
                col = tuple(int(v + (255 - v) * t * 0.55) for v in color)
            else:
                col = color
            px[x, y] = (*col, int(255 * min(a, 1)))
    return img


def sun(size=56):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    c = (size - 1) / 2
    px = img.load()
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - c, y - c)
            a = 0.55 * math.exp(-(d ** 2) / 220.0)
            px[x, y] = (255, 211, 107, int(255 * a))
    d = ImageDraw.Draw(img)
    for i in range(8):
        ang = i * math.pi / 4
        x0, y0 = c + math.cos(ang) * 11, c + math.sin(ang) * 11
        x1, y1 = c + math.cos(ang) * 15.5, c + math.sin(ang) * 15.5
        d.line([(x0, y0), (x1, y1)], fill=(255, 226, 154, 220), width=2)
    d.ellipse([c - 7.5, c - 7.5, c + 7.5, c + 7.5], fill=(255, 211, 107, 255))
    return img


def moon(size=32):
    s = 4
    big = Image.new("RGBA", (size * s, size * s), (0, 0, 0, 0))
    c = size * s / 2
    glow = Image.new("RGBA", big.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse([c - 13 * s, c - 13 * s, c + 13 * s, c + 13 * s], fill=(170, 195, 235, 60))
    glow = glow.filter(ImageFilter.GaussianBlur(5 * s))
    big.alpha_composite(glow)
    mask = Image.new("L", big.size, 0)
    md = ImageDraw.Draw(mask)
    md.ellipse([c - 8 * s, c - 8 * s, c + 8 * s, c + 8 * s], fill=255)
    md.ellipse([c - 4 * s, c - 11 * s, c + 10 * s, c + 3 * s], fill=0)
    disc = Image.new("RGBA", big.size, (216, 226, 242, 255))
    big.paste(disc, (0, 0), mask)
    return big.resize((size, size), Image.LANCZOS)



def home_glow(size=200, r_inner=64):
    """Weicher grüner Schein rund ums Haus (innen transparent, dort liegt der Kreis)."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    c = (size - 1) / 2
    px = img.load()
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - c, y - c)
            if d < r_inner - 2:
                continue
            a = 0.16 * math.exp(-((d - r_inner) ** 2) / 260.0)
            px[x, y] = (46, 230, 166, int(255 * a))
    return img


def wave(width, height, period, amp, top_rgb, bottom_rgb, alpha):
    """Akku-Welle: Sinus-Oberkante, darunter Verlauf. Breite = Fenster + eine Periode,
    damit sich das Bild nahtlos seitlich verschieben lässt."""
    s = 4
    big = Image.new("RGBA", (width * s, height * s), (0, 0, 0, 0))
    px = big.load()
    for X in range(width * s):
        x = X / s
        top = amp + amp * math.sin(2 * math.pi * x / period)
        for Y in range(int(top * s), height * s):
            t = (Y / s - top) / max(height - top, 1)
            col = tuple(int(a + (b - a) * t) for a, b in zip(top_rgb, bottom_rgb))
            px[X, Y] = (*col, int(255 * alpha))
    return big.resize((width, height), Image.LANCZOS)


def bat_mask(size, r):
    """Deckt alles außerhalb des Akku-Kreises mit der Hintergrundfarbe ab:
    die Welle darunter wirkt rund, ohne dass LVGL maskieren muss."""
    s = 4
    big = Image.new("RGBA", (size * s, size * s), (*BG, 255))
    d = ImageDraw.Draw(big)
    c = size * s / 2
    d.ellipse([c - r * s, c - r * s, c + r * s, c + r * s], fill=(0, 0, 0, 0))
    return big.resize((size, size), Image.LANCZOS)


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, col in COLORS.items():
        save(glow_dot(col), "dot_" + name)
    save(sun(), "sun")
    save(moon(), "moon")
    save(home_glow(), "home_glow")
    # Akku: Kreis r = 46 (Rand 2 px), Inhalt r = 43 -> Fenster 86 px
    save(wave(86 + 48, 110, 48, 3.0, (255, 61, 139), (122, 20, 70), 0.55), "wave_front")
    save(wave(86 + 64, 110, 64, 3.5, (255, 110, 170), (90, 16, 56), 0.35), "wave_back")
    save(bat_mask(96, 44), "bat_mask")


if __name__ == "__main__":
    main()
