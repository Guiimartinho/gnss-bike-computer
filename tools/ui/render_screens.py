"""Build the host renderer of the interface, draw every screen and make PNG sheets.

Runs CMake on zephyr_app/tests/ui (LVGL of the NCS plus zephyr_app/src/ui, built with the PC's GCC),
runs ui_render, which draws every screen of docs/18-interface-telas.md with sample data and checks the
panel colours, labels out of their boxes and the key navigation, then turns the PPM frames into PNG
files and contact sheets.

Usage: python tools/ui/render_screens.py [--build-dir DIR] [--sheets DIR] [--screens DIR] [--no-build]
       (defaults: build/ui, docs/img/telas-lvgl and docs/telas)
Exit status 1 when the build or a check fails.
"""
import argparse
import pathlib
import shutil
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / "zephyr_app" / "tests" / "ui"

# Contact sheets: file name, title, frames (name stem without theme)
SHEETS = [
    ("telas-crs", "CRS", ["02_crs1", "03_crs1_1seg", "05_crs1_2seg", "09_crs2", "10_crs3"]),
    ("telas-segmentos", "Segmentos", ["04_crs1_1seg_perto", "06_crs1_2seg_1perto", "07_crs1_2seg_2perto",
                                     "08_crs1_2seg_perto", "11_notificacao", "18c_sem_percursos"]),
    ("telas-modos", "Modos", ["13_prc", "14_prc_sem_percurso", "16_fec", "15_fec_conectando", "12_gnss",
                              "17_dbg"]),
    ("telas-menus", "Menus", ["18_menu", "18b_percursos", "19_ajustes", "20_sensores", "21_parear",
                              "22_valor", "23_tela_luz", "24_formatar"]),
    ("telas-sistema", "Sistema", ["01_partida", "25_energia", "26_usb", "27_desligando"]),
]

CAPTIONS = {
    "01_partida": "Partida",
    "02_crs1": "CRS, página 1",
    "03_crs1_1seg": "CRS, 1 segmento",
    "04_crs1_1seg_perto": "1 segmento, chegando",
    "05_crs1_2seg": "CRS, 2 segmentos",
    "06_crs1_2seg_1perto": "2 segmentos, 1º chegando",
    "07_crs1_2seg_2perto": "2 segmentos, 2º chegando",
    "08_crs1_2seg_perto": "2 segmentos chegando",
    "09_crs2": "CRS, página 2",
    "10_crs3": "CRS, página 3",
    "11_notificacao": "Notificação",
    "12_gnss": "GNSS procurando",
    "13_prc": "PRC",
    "14_prc_sem_percurso": "PRC sem percurso",
    "15_fec_conectando": "FEC conectando",
    "16_fec": "FEC",
    "17_dbg": "DBG",
    "18_menu": "Menu",
    "18b_percursos": "Percursos",
    "18c_sem_percursos": "Nenhum percurso",
    "19_ajustes": "Ajustes",
    "20_sensores": "Sensores",
    "21_parear": "Parear",
    "22_valor": "Editar valor",
    "23_tela_luz": "Tela e luz",
    "24_formatar": "Formatar",
    "25_energia": "Energia",
    "26_usb": "Modo USB",
    "27_desligando": "Desligando",
}

SCALE = 1
GAP = 24
CAPTION_H = 28


def run(cmd):
    print("+", " ".join(str(c) for c in cmd))
    return subprocess.run(cmd, cwd=ROOT).returncode


def caption_font():
    for name in ("DejaVuSans.ttf", "arial.ttf"):
        try:
            return ImageFont.truetype(name, 15)
        except OSError:
            continue
    return ImageFont.load_default()


def make_sheet(frames_dir, stems, theme, title, out_path):
    frames = [Image.open(frames_dir / f"{s}_{theme}.png") for s in stems]
    w, h = frames[0].size
    w, h = w * SCALE, h * SCALE
    sheet_w = GAP + len(frames) * (w + GAP)
    sheet_h = GAP + CAPTION_H + h + GAP
    sheet = Image.new("RGB", (sheet_w, sheet_h), (255, 255, 255))
    draw = ImageDraw.Draw(sheet)
    font = caption_font()
    for i, (stem, img) in enumerate(zip(stems, frames)):
        x = GAP + i * (w + GAP)
        text = CAPTIONS.get(stem, stem)
        tw = draw.textlength(text, font=font)
        draw.text((x + (w - tw) / 2, GAP), text, fill=(29, 29, 31), font=font)
        sheet.paste(img.resize((w, h), Image.NEAREST), (x, GAP + CAPTION_H))
        draw.rectangle([x - 1, GAP + CAPTION_H - 1, x + w, GAP + CAPTION_H + h], outline=(29, 29, 31))
    sheet.save(out_path, optimize=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default=str(ROOT / "build" / "ui"))
    ap.add_argument("--sheets", default=str(ROOT / "docs" / "img" / "telas-lvgl"))
    ap.add_argument("--screens", default=str(ROOT / "docs" / "telas"),
                    help="folder that receives one PNG per screen and theme")
    ap.add_argument("--no-build", action="store_true")
    args = ap.parse_args()

    build = pathlib.Path(args.build_dir)
    out = build / "out"
    if not args.no_build:
        gen = ["-G", "Ninja"] if shutil.which("ninja") else []
        if run(["cmake", "-S", str(SRC), "-B", str(build), *gen]) != 0:
            return 1
        if run(["cmake", "--build", str(build)]) != 0:
            return 1
    exe = build / ("ui_render.exe" if sys.platform == "win32" else "ui_render")
    status = run([str(exe), str(out)])

    screens = pathlib.Path(args.screens)
    screens.mkdir(parents=True, exist_ok=True)
    for ppm in sorted(out.glob("*.ppm")):
        png = ppm.with_suffix(".png")
        Image.open(ppm).save(png, optimize=True)
        shutil.copyfile(png, screens / png.name)

    sheets = pathlib.Path(args.sheets)
    sheets.mkdir(parents=True, exist_ok=True)
    for name, title, stems in SHEETS:
        for theme in ("cor", "mono"):
            make_sheet(out, stems, theme, title, sheets / f"{name}-{theme}.png")
            print(sheets / f"{name}-{theme}.png")
    return 0 if status == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
