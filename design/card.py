"""PaperMono 電子名刺 — デザインA（ミニマル・横）

実行すると次の2つを生成します:
  docs/card.png                 … 名刺ページ用の画像
  firmware/include/card_png.h   … ファームに埋め込む同じ画像（PNGバイト列）

使い方:  pip install pillow qrcode && python design/card.py
内容を変えたいときは下の PROFILE だけ編集してください。
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import qrcode

PROFILE = {
    "url": "https://inai17ibar.github.io/card/",
    "name_ja": "稲谷 壮一郎",
    "name_en": "Soichiro Inatani",
    "org": "ソニー株式会社",
    "title": "研究開発エンジニア",
    "github": "github.com/inai17ibar",
    "linkedin": "linkedin.com/in/soichiro-inatani",
}

ROOT = Path(__file__).resolve().parent.parent
W, H = 800, 480  # 横向き（PaperMonoを横持ち）
BLACK, DARK, LIGHT, WHITE = 0, 85, 170, 255  # e-paper 4階調

# フォント（無ければ環境に合わせてパスを変更。Macなら "ヒラギノ角ゴシック" 等）
CJK = "/usr/share/fonts/opentype/noto/NotoSansCJK-{}.ttc"
INTER = "/usr/share/fonts/opentype/inter/Inter-{}.otf"


def ja(w, s): return ImageFont.truetype(CJK.format(w), s, index=0)
def en(w, s): return ImageFont.truetype(INTER.format(w), s)


def qr_img(url, size):
    q = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M, border=0)
    q.add_data(url); q.make(fit=True)
    n = q.modules_count; box = size // n  # モジュールを整数ピクセルに揃えて読み取りやすく
    img = q.make_image(fill_color="black", back_color="white").convert("L")
    img = img.resize((n * box, n * box), Image.NEAREST)
    canvas = Image.new("L", (size, size), WHITE)
    off = (size - n * box) // 2
    canvas.paste(img, (off, off))
    return canvas


def quantize(img):
    lut = [min((BLACK, DARK, LIGHT, WHITE), key=lambda v: abs(v - i)) for i in range(256)]
    return img.point(lut)


def nfc_icon(d, x, y, s, fill):
    for r in (s * 0.35, s * 0.65, s * 0.95):
        d.arc([x - r, y - r, x + r, y + r], -45, 45, fill=fill, width=max(2, s // 10))
    d.ellipse([x - s * 0.1, y - s * 0.1, x + s * 0.1, y + s * 0.1], fill=fill)


def render(p=PROFILE):
    img = Image.new("L", (W, H), WHITE)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, 12, H], fill=BLACK)
    d.text((64, 70), p["org"], font=ja("Medium", 24), fill=DARK)
    d.text((64, 108), p["title"], font=ja("Regular", 24), fill=DARK)
    d.text((60, 170), p["name_ja"], font=ja("Bold", 72), fill=BLACK)
    d.text((64, 262), p["name_en"], font=en("Medium", 30), fill=DARK)
    d.line([64, 330, 470, 330], fill=LIGHT, width=2)
    for y, k, v in ((352, "GitHub", p["github"]), (388, "LinkedIn", p["linkedin"])):
        d.text((64, y), k, font=en("SemiBold", 20), fill=BLACK)
        d.text((170, y), v, font=en("Regular", 20), fill=DARK)
    img.paste(qr_img(p["url"], 230), (528, 96))
    d.text((643, 356), "Scan or Tap", font=en("Medium", 20), fill=DARK, anchor="mm")
    nfc_icon(d, 643, 400, 28, DARK)
    return quantize(img)


def write_header(png: bytes, path: Path):
    rows = [", ".join(f"0x{b:02X}" for b in png[i:i + 16]) for i in range(0, len(png), 16)]
    path.write_text(
        "// 自動生成: design/card.py — 手で編集しないでください\n"
        "#pragma once\n#include <cstdint>\n#include <cstddef>\n\n"
        f"static const uint8_t CARD_PNG[] = {{\n  " + ",\n  ".join(rows) + "\n};\n"
        f"static const size_t CARD_PNG_LEN = {len(png)};\n"
    )


if __name__ == "__main__":
    import io
    img = render()
    buf = io.BytesIO(); img.save(buf, format="PNG", optimize=True)
    png = buf.getvalue()
    (ROOT / "docs").mkdir(exist_ok=True)
    (ROOT / "docs/card.png").write_bytes(png)
    write_header(png, ROOT / "firmware/include/card_png.h")
    print(f"card.png {len(png)} bytes → docs/card.png, firmware/include/card_png.h")
