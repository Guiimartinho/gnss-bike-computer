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


def caixas_das_pecas() -> tuple[np.ndarray, np.ndarray]:
    """Bodies for the parts whose footprint was generated here.

    KiCad's GLB carries only the STEP models of the library footprints, so the
    ten parts drawn in footprints.py - the radio module, the GNSS receiver,
    the level translator, the sensors - would appear as bare pads. Their
    package outline and height are in footprints.CORPO, and the box is built
    here at the position, rotation and face the board gives them.
    """
    sys.path.insert(0, str(HERE))
    import footprints as FPS
    import fp_load
    import make_pcb as MP

    arv = fp_load.parse((HERE / "gnssbike.kicad_pcb").read_text(encoding="utf-8"))
    tris: list[np.ndarray] = []
    cols: list[np.ndarray] = []
    for f in fp_load.kids(arv, "footprint"):
        nome = f[1]
        if nome not in FPS.CORPO:
            continue
        w, h, alt = FPS.CORPO[nome]
        at = fp_load.kid(f, "at")
        x, y = float(at[1]), float(at[2])
        ang = math.radians(float(at[3])) if len(at) > 3 else 0.0
        atras = fp_load.kid(f, "layer")[1] == "B.Cu"
        z0 = -0.8 if atras else 0.0      # the board is 0.8 mm thick
        z1 = z0 - alt if atras else alt
        ca, sa = math.cos(ang), math.sin(ang)
        cantos = []
        for dx, dy in ((-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)):
            cantos.append((x + dx * ca + dy * sa, y - dx * sa + dy * ca))
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
    if len(extra_t):
        # the boxes are in millimetres with y growing downward, as the board
        # file has them; the GLB is in metres with y already up
        extra_t = np.stack([extra_t[..., 0], -extra_t[..., 1], extra_t[..., 2]],
                           axis=-1) / 1000.0
        tris = np.concatenate([tris, extra_t])
        cols = np.concatenate([cols, extra_c])
    print(f"{len(tris)} triangulos, {len(j.get('meshes', []))} malhas, "
          f"{len(extra_t) // 12} corpos desenhados aqui")

    for nome, az, el, w, h in (("gnssbike-3d-frente.png", 0.0, 90.0, 1100, 1800),
                               ("gnssbike-3d-angulo.png", 28.0, 38.0, 1600, 1300),
                               ("gnssbike-3d-tras.png", 180.0, -90.0, 1100, 1800)):
        img = render(tris, cols, w, h, az, el)
        img.save(HERE / nome)
        print(f"  {nome}: {w} x {h}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
