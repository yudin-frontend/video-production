"""Генерирует ваши личные картинки: подпись (caption.png) и логотип (logo.png).

1. Измените переменные в блоке НАСТРОЙКИ ниже на свои данные.
2. Запустите:  python tools/make_assets.py
Можно и не запускать — тогда просто положите свои PNG в папку assets/.
"""
from PIL import Image, ImageDraw, ImageFont
import os

# ---------------- НАСТРОЙКИ (меняйте под себя) ----------------
AUTHOR = "Юдин Максим"                 # имя в подписи
SUBTITLE = "студент ИСП9-Kh32"   # вторая строка подписи
LOGO_TEXT = "ЮМ"                       # буквы в логотипе-заглушке (если нет своей картинки)
LOGO_SOURCE = "ithub_original.png"     # своя картинка логотипа (лежит в assets/); если файла нет — рисуются буквы
ACCENT = (124, 58, 237)                # основной цвет (R, G, B)
# --------------------------------------------------------------

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets")
FONT_BOLD_CANDIDATES = [
    "DejaVuSans-Bold.ttf", "arialbd.ttf", "Arial Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
    "C:/Windows/Fonts/arialbd.ttf",
]
FONT_REG_CANDIDATES = [
    "DejaVuSans.ttf", "arial.ttf", "Arial.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    "C:/Windows/Fonts/arial.ttf",
]


def load_font(candidates, size):
    for name in candidates:
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    raise SystemExit("Не найден шрифт с кириллицей. Укажите путь к .ttf в FONT_*_CANDIDATES.")


def centered(draw, text, font, cx, cy, fill):
    box = draw.textbbox((0, 0), text, font=font)
    w, h = box[2] - box[0], box[3] - box[1]
    draw.text((cx - w / 2 - box[0], cy - h / 2 - box[1]), text, font=font, fill=fill)


def make_caption(path):
    """Плашка 640x80 с градиентом, именем и подписью (RGBA, полупрозрачная)."""
    w, h = 640, 80
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for x in range(w):
        k = x / (w - 1)
        r = int(20 + (ACCENT[0] - 20) * k * 0.6)
        g = int(15 + (ACCENT[1] - 15) * k * 0.6)
        b = int(45 + (ACCENT[2] - 45) * k * 0.6)
        for y in range(h):
            px[x, y] = (r, g, b, 215)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, 6, h], fill=ACCENT + (255,))          # цветная полоска слева
    centered(d, AUTHOR, load_font(FONT_BOLD_CANDIDATES, 32), w / 2, 29, (255, 255, 255, 255))
    centered(d, SUBTITLE, load_font(FONT_REG_CANDIDATES, 17), w / 2, 60, (225, 225, 240, 255))
    img.save(path)


def make_logo(path):
    """Круглый логотип 86x86 с инициалами (RGBA, фон прозрачный)."""
    s = 86
    scale = 4                                   # рисуем крупнее для сглаживания
    big = Image.new("RGBA", (s * scale, s * scale), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    d.ellipse([0, 0, s * scale - 1, s * scale - 1], fill=ACCENT + (255,))
    m = 7 * scale
    d.ellipse([m, m, s * scale - m, s * scale - m], outline=(255, 255, 255, 255), width=3 * scale)
    centered(d, LOGO_TEXT, load_font(FONT_BOLD_CANDIDATES, 30 * scale), s * scale / 2, s * scale / 2, (255, 255, 255, 255))
    big.resize((s, s), Image.LANCZOS).save(path)


def make_logo_from_image(src, path, width=130, pad=8):
    """Берёт вашу картинку (тёмный логотип на светлом фоне), обрезает поля и кладёт
    на белую скруглённую подложку, чтобы логотип был виден на любом видео."""
    gray = Image.open(src).convert("L")
    bbox = gray.point(lambda v: 255 if v < 128 else 0).getbbox()   # область с чернилами
    gray = gray.crop(bbox)
    inner_w = width - 2 * pad
    inner_h = round(gray.height * inner_w / gray.width)
    ink = gray.resize((inner_w, inner_h), Image.LANCZOS)
    w, h = width, inner_h + 2 * pad
    k = 4                                                         # сглаживание скруглений
    plate = Image.new("L", (w * k, h * k), 0)
    ImageDraw.Draw(plate).rounded_rectangle([0, 0, w * k - 1, h * k - 1], radius=10 * k, fill=235)
    plate = plate.resize((w, h), Image.LANCZOS)
    img = Image.new("RGBA", (w, h), (255, 255, 255, 0))
    img.putalpha(plate)
    black = Image.new("RGBA", (inner_w, inner_h), (0, 0, 0, 255))
    img.paste(black, (pad, pad), Image.eval(ink, lambda v: 255 - v))   # чернила = чёрным
    img.save(path)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    make_caption(os.path.join(OUT, "caption.png"))
    src = os.path.join(OUT, LOGO_SOURCE)
    if os.path.exists(src):
        make_logo_from_image(src, os.path.join(OUT, "logo.png"))
    else:
        make_logo(os.path.join(OUT, "logo.png"))
    print("Готово: assets/caption.png и assets/logo.png")
