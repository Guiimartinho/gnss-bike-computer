#!/usr/bin/env python3
"""Render the board's 3D model to PNG, without a 3D program.

kicad-cli exports the board as GLB, geometry and all, but KiCad 8 has no
command that turns it into a picture. So this reads the GLB - the JSON chunk
describes the nodes and the meshes, the binary chunk holds the vertices - and
rasterises the triangles with a z-buffer, in numpy.

Two pictures come out: the board seen from above, which is the placement, and
an angled view, which is what the thing looks like.

Run: python hardware_gnssbike/cad/make_3d.py
"""

from __future__ import annotations

import json
import math
import pathlib
import struct
import sys

import numpy as np
from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent

TIPOS = {5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2), 5123: ("H", 2),
         5125: ("I", 4), 5126: ("f", 4)}
N_COMP = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def ler_glb(caminho: pathlib.Path) -> tuple[dict, bytes]:
    b = caminho.read_bytes()
    magic, _ver, total = struct.unpack("<III", b[:12])
    if magic != 0x46546C67:
        raise ValueError("nao e um GLB")
    off, js, bina = 12, None, b""
    while off < total:
        tam, tipo = struct.unpack("<II", b[off:off + 8])
        dados = b[off + 8:off + 8 + tam]
        if tipo == 0x4E4F534A:
            js = json.loads(dados)
        elif tipo == 0x004E4942:
            bina = dados
        off += 8 + tam
    return js, bina


def acessor(j: dict, bina: bytes, i: int) -> np.ndarray:
    a = j["accessors"][i]
    n = a["count"]
    nc = N_COMP[a["type"]]
    fmt, tam = TIPOS[a["componentType"]]
    bv = j["bufferViews"][a["bufferView"]]
    inicio = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
    passo = bv.get("byteStride") or nc * tam
    if passo == nc * tam:
        bruto = np.frombuffer(bina, dtype=np.dtype(fmt), count=n * nc, offset=inicio)
        return bruto.reshape(n, nc).astype(np.float64 if fmt == "f" else np.int64)
    out = np.empty((n, nc), dtype=np.float64 if fmt == "f" else np.int64)
    for k in range(n):
        out[k] = np.frombuffer(bina, dtype=np.dtype(fmt), count=nc,
                               offset=inicio + k * passo)
    return out


def matriz(no: dict) -> np.ndarray:
    if "matrix" in no:
        return np.array(no["matrix"], dtype=np.float64).reshape(4, 4).T
    m = np.eye(4)
    if "scale" in no:
        m = np.diag(list(no["scale"]) + [1.0]) @ m
    if "rotation" in no:
        x, y, z, w = no["rotation"]
        r = np.array([
            [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w), 0],
            [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w), 0],
            [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y), 0],
            [0, 0, 0, 1]], dtype=np.float64)
        m = r @ m
    if "translation" in no:
        t = np.eye(4)
        t[:3, 3] = no["translation"]
        m = t @ m
    return m


def triangulos(j: dict, bina: bytes) -> tuple[np.ndarray, np.ndarray]:
    """Every triangle in world space, and the colour of each."""
    cores = []
    for mat in j.get("materials", []):
        pbr = mat.get("pbrMetallicRoughness", {})
        cores.append(np.array(pbr.get("baseColorFactor", [0.7, 0.7, 0.7, 1.0])[:3]))
    if not cores:
        cores = [np.array([0.7, 0.7, 0.7])]

    verts: list[np.ndarray] = []
    cols: list[np.ndarray] = []
    cena = j.get("scenes", [{}])[j.get("scene", 0)]

    def andar(i_no: int, pai: np.ndarray) -> None:
        no = j["nodes"][i_no]
        m = pai @ matriz(no)
        if "mesh" in no:
            for prim in j["meshes"][no["mesh"]].get("primitives", []):
                if prim.get("mode", 4) != 4 or "POSITION" not in prim.get("attributes", {}):
                    continue
                pos = acessor(j, bina, prim["attributes"]["POSITION"])
                if "indices" in prim:
                    idx = acessor(j, bina, prim["indices"]).reshape(-1)
                else:
                    idx = np.arange(len(pos))
                n_tri = len(idx) // 3
                if n_tri == 0:
                    continue
                p = np.concatenate([pos, np.ones((len(pos), 1))], axis=1)
                p = (m @ p.T).T[:, :3]
                verts.append(p[idx[:n_tri * 3]].reshape(n_tri, 3, 3))
                c = cores[prim.get("material", 0)] if prim.get("material") is not None \
                    else cores[0]
                cols.append(np.tile(c, (n_tri, 1)))
        for f in no.get("children", []):
            andar(f, m)

    for i_no in cena.get("nodes", []):
        andar(i_no, np.eye(4))
    if not verts:
        raise ValueError("nenhum triangulo")
    t = np.concatenate(verts)
    # glTF is Y up and this renderer is Z up: (x, y, z) -> (x, -z, y).
    t = np.stack([t[..., 0], -t[..., 2], t[..., 1]], axis=-1)
    return t, np.concatenate(cols)


def render(tris: np.ndarray, cols: np.ndarray, largura: int, altura: int,
           azimute: float, elevacao: float, fundo=(250, 250, 252)) -> Image.Image:
    """Orthographic z-buffer, flat shading, one directional light."""
    az, el = math.radians(azimute), math.radians(elevacao)
    olho = np.array([math.cos(el) * math.sin(az), math.cos(el) * math.cos(az),
                     math.sin(el)])
    # straight down the up axis the cross product vanishes, so use another
    cima = np.array([0.0, 0.0, 1.0])
    if abs(abs(elevacao) - 90.0) < 1e-6:
        cima = np.array([0.0, 1.0, 0.0])
    direita = np.cross(cima, olho)
    n = np.linalg.norm(direita)
    if n < 1e-9:
        direita = np.array([1.0, 0.0, 0.0])
        n = 1.0
    direita /= n
    verdadeiro_cima = np.cross(olho, direita)
    R = np.stack([direita, verdadeiro_cima, olho])

    v = tris.reshape(-1, 3) @ R.T
    v = v.reshape(-1, 3, 3)
    mn, mx = v.reshape(-1, 3).min(axis=0), v.reshape(-1, 3).max(axis=0)
    esc = 0.92 * min(largura / (mx[0] - mn[0]), altura / (mx[1] - mn[1]))
    cx, cy = (mn[0] + mx[0]) / 2, (mn[1] + mx[1]) / 2
    sx = (v[:, :, 0] - cx) * esc + largura / 2
    sy = altura / 2 - (v[:, :, 1] - cy) * esc
    sz = v[:, :, 2]

    normais = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    ln = np.linalg.norm(normais, axis=1)
    ln[ln == 0] = 1.0
    normais /= ln[:, None]
    luz = np.array([0.35, -0.45, 0.82])
    luz /= np.linalg.norm(luz)
    brilho = 0.35 + 0.65 * np.clip(np.abs(normais @ luz), 0, 1)
    rgb = np.clip(cols * brilho[:, None], 0, 1) * 255

    img = np.zeros((altura, largura, 3), dtype=np.float32)
    img[:, :] = fundo
    zbuf = np.full((altura, largura), -1e30, dtype=np.float64)

    ordem = np.argsort(-sz.mean(axis=1))
    for t in ordem:
        x0, x1 = sx[t].min(), sx[t].max()
        y0, y1 = sy[t].min(), sy[t].max()
        if x1 < 0 or y1 < 0 or x0 > largura or y0 > altura:
            continue
        i0, i1 = max(int(math.floor(y0)), 0), min(int(math.ceil(y1)) + 1, altura)
        j0, j1 = max(int(math.floor(x0)), 0), min(int(math.ceil(x1)) + 1, largura)
        if i1 <= i0 or j1 <= j0:
            continue
        ys, xs = np.mgrid[i0:i1, j0:j1]
        px, py = xs + 0.5, ys + 0.5
        ax, ay = sx[t, 0], sy[t, 0]
        bx, by = sx[t, 1], sy[t, 1]
        ccx, ccy = sx[t, 2], sy[t, 2]
        den = (by - ccy) * (ax - ccx) + (ccx - bx) * (ay - ccy)
        if abs(den) < 1e-12:
            continue
        w0 = ((by - ccy) * (px - ccx) + (ccx - bx) * (py - ccy)) / den
        w1 = ((ccy - ay) * (px - ccx) + (ax - ccx) * (py - ccy)) / den
        w2 = 1.0 - w0 - w1
        dentro = (w0 >= -1e-9) & (w1 >= -1e-9) & (w2 >= -1e-9)
        if not dentro.any():
            continue
        z = w0 * sz[t, 0] + w1 * sz[t, 1] + w2 * sz[t, 2]
        sub = zbuf[i0:i1, j0:j1]
        melhor = dentro & (z > sub)
        if not melhor.any():
            continue
        sub[melhor] = z[melhor]
        img[i0:i1, j0:j1][melhor] = rgb[t]
    return Image.fromarray(img.astype(np.uint8))


# Where the board's two faces actually are in the GLB, measured and not
# assumed: kicad-cli puts the substrate from z 0 to 0.820 and the 35 um of
# copper on top of it, so the front surface a part stands on is 0.855 and the
# back one is -0.035. Everything here used 0 and -0.8, which sank every front
# part 0.855 mm INTO the board and put the silkscreen inside it, where the
# copper pour hid it: in the 3D view not one reference designator was visible
# anywhere the pour reached, which is almost everywhere.
FRENTE_Z = 0.855
VERSO_Z = -0.035

KICAD_CLI = pathlib.Path(r"D:\KiCAD\bin\kicad-cli.exe")
SILK_TRACO = 0.15          # a largura do traco da serigrafia, em mm
SILK_COR = (0.92, 0.92, 0.89)


def serigrafia() -> tuple[np.ndarray, np.ndarray]:
    """The silkscreen of both faces, as flat geometry on the board.

    kicad-cli's GLB export has no silkscreen option, so a 3D view of this
    board showed a bare green rectangle with parts on it and not one
    reference designator - which is the one thing silkscreen exists for.

    The SVG export does carry it, and carries it as the simplest possible
    thing: 6.484 `<path d="M x y L x y"/>`, one per stroke, glyphs included.
    No font is needed and nothing is approximated. Exported over the FULL
    page, the SVG's millimetres are the board file's own coordinates, so the
    segments drop straight into the same frame the part bodies use.
    """
    import re
    import subprocess
    import tempfile

    if not KICAD_CLI.exists():
        return np.zeros((0, 3, 3)), np.zeros((0, 3))
    tris: list[list] = []
    with tempfile.TemporaryDirectory() as tmp:
        for camada, z in (("F.SilkS", FRENTE_Z + 0.005),
                          ("B.SilkS", VERSO_Z - 0.005)):
            fora = pathlib.Path(tmp) / (camada.replace(".", "_") + ".svg")
            r = subprocess.run(
                [str(KICAD_CLI), "pcb", "export", "svg", "--output", str(fora),
                 "--layers", camada, "--exclude-drawing-sheet",
                 str(HERE / "gnssbike.kicad_pcb")],
                capture_output=True, text=True)
            if r.returncode != 0 or not fora.exists():
                continue
            texto = fora.read_text(encoding="utf-8")
            h = SILK_TRACO / 2.0
            for m in re.finditer(r'<path d="M([-\d.]+) ([-\d.]+)\s*'
                                 r'L([-\d.]+) ([-\d.]+)', texto):
                x0, y0, x1, y1 = (float(m.group(i)) for i in (1, 2, 3, 4))
                dx, dy = x1 - x0, y1 - y0
                comp = math.hypot(dx, dy)
                if comp < 1e-9:
                    dx, dy, comp = 1.0, 0.0, 1.0
                # a quad of SILK_TRACO wide along the segment
                nx, ny = -dy / comp * h, dx / comp * h
                a = (x0 + nx, y0 + ny, z)
                b = (x1 + nx, y1 + ny, z)
                c = (x1 - nx, y1 - ny, z)
                d = (x0 - nx, y0 - ny, z)
                tris.append([a, b, c])
                tris.append([a, c, d])
            for m in re.finditer(r'<circle cx="([-\d.]+)" cy="([-\d.]+)" '
                                 r'r="([-\d.]+)"', texto):
                cx, cy, rr = (float(m.group(i)) for i in (1, 2, 3))
                n = 10
                for i in range(n):
                    a1 = 2 * math.pi * i / n
                    a2 = 2 * math.pi * (i + 1) / n
                    tris.append([(cx, cy, z),
                                 (cx + rr * math.cos(a1), cy + rr * math.sin(a1), z),
                                 (cx + rr * math.cos(a2), cy + rr * math.sin(a2), z)])
    if not tris:
        return np.zeros((0, 3, 3)), np.zeros((0, 3))
    t = np.array(tris, dtype=np.float64)
    return t, np.array([SILK_COR] * len(t))


def ler_wrl(caminho: pathlib.Path) -> list[tuple[np.ndarray, tuple]]:
    """The shapes of one of our own VRML bodies, in millimetres.

    Only the dialect footprints.py writes: a run of `Shape` blocks, each with
    one diffuseColor and one IndexedFaceSet whose faces are polygons closed
    by -1. Reading it is the whole point of this function - the geometry and
    the colour are already there, in the file the footprint points at, and
    drawing a black cuboid instead threw both away.

    VRML here is in tenths of an inch, the unit KiCad uses for .wrl, so
    everything is multiplied by 2.54 on the way out.
    """
    texto = caminho.read_text(encoding="utf-8")
    saida: list[tuple[np.ndarray, tuple]] = []
    i = 0
    while True:
        i = texto.find("Shape {", i)
        if i < 0:
            break
        j = texto.find("Shape {", i + 7)
        bloco = texto[i:j if j > 0 else len(texto)]
        i = i + 7

        k = bloco.find("diffuseColor")
        cor = (0.13, 0.13, 0.14)
        if k >= 0:
            partes = bloco[k + 12:k + 60].split()
            try:
                cor = (float(partes[0]), float(partes[1]), float(partes[2]))
            except (IndexError, ValueError):
                pass

        k = bloco.find("point [")
        if k < 0:
            continue
        corpo = bloco[k + 7:bloco.index("]", k)]
        pts = []
        for linha in corpo.split(","):
            n = linha.split()
            if len(n) == 3:
                pts.append([float(n[0]) * 2.54, float(n[1]) * 2.54,
                            float(n[2]) * 2.54])
        k = bloco.find("coordIndex [")
        if k < 0 or not pts:
            continue
        idx = bloco[k + 12:bloco.index("]", k)]
        tris = []
        for face in idx.split(","):
            n = [int(q) for q in face.split() if q.lstrip("-").isdigit()]
            n = [q for q in n if q >= 0]
            # a fan: every face this writer emits is convex
            for m in range(1, len(n) - 1):
                tris.append([pts[n[0]], pts[n[m]], pts[n[m + 1]]])
        if tris:
            saida.append((np.array(tris, dtype=np.float64), cor))
    return saida


# The three things that are NOT on the board and decide its shape anyway.
# Every rectangle and every ceiling here is a line of 04-pcb-e-caixa.md, and
# the column that says which one is part of the table on purpose: a number
# with no line behind it does not belong in a drawing that is supposed to be
# checkable.
#
# The thickness of the display and of the cell are the ones the document
# gives for the part; the GAP under each of them is the shadow ceiling, which
# is the clearance the board has to respect, not the part's own thickness.
MONTAGEM = (
    # nome, x0, y0, x1, y1, vao ate a placa, espessura, atras, cor, fonte
    ("display JDI LPM027M128B", 7.46, 5.10, 47.54, 66.90, 2.60, 1.00, False,
     (0.16, 0.17, 0.20), "04-pcb-e-caixa.md, tabela de zonas: contorno "
     "40,08 x 61,8 em x 7,46-47,54 e y 5,1-66,9, teto de 2,6 mm"),
    ("celula LiPo 36 x 60 x 7", 9.50, 22.50, 45.50, 82.50, 1.20, 7.00, True,
     (0.30, 0.31, 0.34), "04-pcb-e-caixa.md: bolsa de 36 x 60 x 7 mm na face "
     "de tras, em x 9,5-45,5 e y 22,5-82,5, teto de 1,2 mm"),
)
# The six 23 x 8 mm solar modules are deliberately NOT here. They live in the
# case walls - two on the sloped face and two on each chamfer - and nothing
# in any document puts them in the BOARD's coordinates, so drawing them over
# it would be drawing a guess. They reach the board through J103, J104 and
# J105, and those are on it, with bodies.


def pecas_da_caixa(afastar: float = 0.0) -> tuple[np.ndarray, np.ndarray]:
    """The display and the cell, in the board's own coordinates.

    With `afastar` at zero they sit where the case really holds them, which
    is what shows whether a part fouls them. Pulled apart, it is the exploded
    view: the same three things in the same order, far enough apart to see
    the board between them.
    """
    tris: list[np.ndarray] = []
    cols: list[np.ndarray] = []
    for _nome, x0, y0, x1, y1, vao, esp, atras, cor, _fonte in MONTAGEM:
        if atras:
            z1 = VERSO_Z - vao - afastar
            z0 = z1 - esp
        else:
            z0 = FRENTE_Z + vao + afastar
            z1 = z0 + esp
        cantos = ((x0, y0), (x1, y0), (x1, y1), (x0, y1))
        base = [(px, py, z0) for px, py in cantos]
        topo = [(px, py, z1) for px, py in cantos]
        faces = [(base[0], base[3], base[2]), (base[0], base[2], base[1]),
                 (topo[0], topo[1], topo[2]), (topo[0], topo[2], topo[3])]
        for k in range(4):
            a, b = k, (k + 1) % 4
            faces.append((base[a], base[b], topo[b]))
            faces.append((base[a], topo[b], topo[a]))
        for t in faces:
            tris.append(np.array(t, dtype=np.float64))
            cols.append(np.array(cor))
    if not tris:
        return np.zeros((0, 3, 3)), np.zeros((0, 3))
    return np.array(tris), np.array(cols)


def caixas_das_pecas() -> tuple[np.ndarray, np.ndarray]:
    """Bodies for the parts whose footprint was generated here.

    KiCad's GLB carries only the STEP models of the library footprints, so the
    parts drawn in footprints.py - the radio module, the GNSS receiver, the
    level translator, the sensors, the key, the buzzer, the receptacle -
    would appear as bare pads.

    Each of them already has a body in cad/3d/*.wrl, with the package's real
    shape and the real colour of its material, and until now this function
    ignored both: it drew a plain cuboid of the courtyard size in one hard
    coded near-black for all of them. That is exactly what the board looked
    like - twenty-two identical black bricks - and it is not what any of
    those parts is. The .wrl is read instead, and only a part with no body
    file at all falls back to the box.
    """
    sys.path.insert(0, str(HERE))
    import footprints as FPS
    import fp_load
    import make_pcb as MP

    arv = fp_load.parse((HERE / "gnssbike.kicad_pcb").read_text(encoding="utf-8"))
    # CORPO_TODOS is filled while the footprints are read, and this runs in a
    # process of its own: without touching them first it would know only the
    # ten generated footprints and miss the eight library ones that got a box.
    for _ref, (nome_fp, _o, _n) in FPS.FP.items():
        try:
            fp_load.corpo(nome_fp)
        except FileNotFoundError:
            pass
    pasta3d = HERE / "3d"
    tris: list[np.ndarray] = []
    cols: list[np.ndarray] = []
    for f in fp_load.kids(arv, "footprint"):
        nome = f[1]
        if nome not in FPS.CORPO_TODOS:
            continue
        w, h, alt = FPS.CORPO_TODOS[nome]
        at = fp_load.kid(f, "at")
        x, y = float(at[1]), float(at[2])
        ang = math.radians(float(at[3])) if len(at) > 3 else 0.0
        atras = fp_load.kid(f, "layer")[1] == "B.Cu"
        ca, sa = math.cos(ang), math.sin(ang)

        def por_no_lugar(v: np.ndarray) -> np.ndarray:
            """The body's own coordinates, put where the board has the part."""
            vx = -v[..., 0] if atras else v[..., 0]
            vy, vz = v[..., 1], v[..., 2]
            return np.stack([x + vx * ca + vy * sa,
                             y - vx * sa + vy * ca,
                             (VERSO_Z - vz) if atras else (FRENTE_Z + vz)],
                            axis=-1)

        arq = pasta3d / (nome.split(":", 1)[1] + ".wrl")
        formas = ler_wrl(arq) if arq.exists() else []
        if formas:
            for malha, cor in formas:
                tris.extend(por_no_lugar(malha))
                cols.extend([np.array(cor)] * len(malha))
            continue

        # no body file: the courtyard box, which is all anybody knows
        cantos = [(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)]
        caixa = np.array([[list(c) + [z] for c in cantos] for z in (0.0, alt)])
        base = [por_no_lugar(np.array(p)) for p in caixa[0]]
        topo = [por_no_lugar(np.array(p)) for p in caixa[1]]
        faces = [(base[0], base[3], base[2]), (base[0], base[2], base[1]),
                 (topo[0], topo[1], topo[2]), (topo[0], topo[2], topo[3])]
        for k in range(4):
            a, b = k, (k + 1) % 4
            faces.append((base[a], base[b], topo[b]))
            faces.append((base[a], topo[b], topo[a]))
        for t in faces:
            tris.append(np.array(t, dtype=np.float64))
            cols.append(np.array([0.13, 0.13, 0.14]))
    if not tris:
        return np.zeros((0, 3, 3)), np.zeros((0, 3))
    return np.array(tris), np.array(cols)


def main() -> int:
    glb = HERE / "gnssbike.glb"
    if not glb.exists():
        print("gnssbike.glb nao existe: rode o kicad-cli pcb export glb antes")
        return 1
    j, bina = ler_glb(glb)
    tris, cols = triangulos(j, bina)
    extra_t, extra_c = caixas_das_pecas()
    n_corpos = len(extra_t)
    silk_t, silk_c = serigrafia()
    if len(silk_t):
        extra_t = np.concatenate([extra_t, silk_t])
        extra_c = np.concatenate([extra_c, silk_c])
    if len(extra_t):
        # the boxes are in millimetres with y growing downward, as the board
        # file has them; the GLB is in metres with y already up
        extra_t = np.stack([extra_t[..., 0], -extra_t[..., 1], extra_t[..., 2]],
                           axis=-1) / 1000.0
        tris = np.concatenate([tris, extra_t])
        cols = np.concatenate([cols, extra_c])
    print(f"{len(tris)} triangulos, {len(j.get('meshes', []))} malhas, "
          f"{n_corpos} corpos e {len(silk_t)} tracos de serigrafia "
          "desenhados aqui")

    for nome, az, el, w, h in (("gnssbike-3d-frente.png", 0.0, 90.0, 1100, 1800),
                               ("gnssbike-3d-angulo.png", 28.0, 38.0, 1600, 1300),
                               ("gnssbike-3d-tras.png", 180.0, -90.0, 1100, 1800)):
        img = render(tris, cols, w, h, az, el)
        img.save(HERE / nome)
        print(f"  {nome}: {w} x {h}")

    # and the stack: display, board, cell, pulled apart so the three are all
    # visible at once. The board alone never showed what it has to fit
    # between, which is the thing that decides its size.
    caixa_t, caixa_c = pecas_da_caixa(afastar=16.0)
    caixa_t = np.stack([caixa_t[..., 0], -caixa_t[..., 1], caixa_t[..., 2]],
                       axis=-1) / 1000.0
    montagem_t = np.concatenate([tris, caixa_t])
    montagem_c = np.concatenate([cols, caixa_c])
    img = render(montagem_t, montagem_c, 1500, 1500, 24.0, 26.0)
    img.save(HERE / "gnssbike-3d-montagem.png")
    print(f"  gnssbike-3d-montagem.png: 1500 x 1500 "
          f"({len(MONTAGEM)} pecas da caixa)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
