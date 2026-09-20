"""Converte um percurso de GPX ou TCX para o formato `.RTE` do projeto.

O Strava, o Komoot e o RideWithGPS exportam GPX (XML). O aparelho lê o
`.RTE` binário descrito em `zephyr_app/include/model/route_file.h`: cabeçalho
com o que o menu precisa, pontos de 10 bytes e, quando o arquivo de origem
traz, a lista de curvas com o nome da rua. A conversão tira 2/3 do tamanho
(100 km com ponto a cada 10 m: 303 KB em texto, 100 KB aqui) e acrescenta o
CRC-32 que o firmware confere antes de seguir a rota.

Uso:

    python tools/route_convert.py entrada.gpx saida.RTE [--name "Serra"]
    python tools/route_convert.py --selftest

Depois é só mandar o `.RTE` para o aparelho pelo aplicativo (mcumgr, grupo
de arquivos), como descrito em `docs/09-armazenamento-usb.md`.
"""
from __future__ import annotations

import argparse
import math
import struct
import sys
import zlib
import xml.etree.ElementTree as ET
from pathlib import Path

MAGIC = b"RTE1"
VERSION = 1
HEADER_SIZE = 64
POINT_SIZE = 10
CUE_SIZE = 28
NAME_MAX = 20
STREET_MAX = 22
FLAG_CUES = 0x0001

# enum route_turn de route_file.h
TURNS = {
    "straight": 0,
    "left": 1,
    "right": 2,
    "sharp_left": 3,
    "sharp_right": 4,
    "easy_left": 5,
    "easy_right": 6,
    "roundabout": 7,
    "uturn": 8,
    "arrive": 9,
}

# como os serviços nomeiam as curvas nos arquivos que exportam
TURN_WORDS = {
    "left": "left",
    "esquerda": "left",
    "right": "right",
    "direita": "right",
    "sharp left": "sharp_left",
    "sharp right": "sharp_right",
    "slight left": "easy_left",
    "slight right": "easy_right",
    "roundabout": "roundabout",
    "rotatoria": "roundabout",
    "rotatória": "roundabout",
    "u-turn": "uturn",
    "uturn": "uturn",
    "retorno": "uturn",
    "arrive": "arrive",
    "chegada": "arrive",
    "destination": "arrive",
}


def strip_ns(tag: str) -> str:
    """Tira o namespace do XML: `{http://...}trkpt` vira `trkpt`."""
    return tag.rsplit("}", 1)[-1]


def distance_m(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    """Equiretangular, a mesma do firmware (`vecteur.c`), raio 6.371.008 m."""
    dphi = math.radians(lat2 - lat1)
    dlmb = math.radians(lon2 - lon1)
    cosm = math.cos(math.radians((lat1 + lat2) / 2.0))
    return 6371008.0 * math.hypot(dphi, dlmb * cosm)


def turn_of(text: str) -> int:
    """A curva que um texto de instrução descreve."""
    low = (text or "").strip().lower()
    for words, name in TURN_WORDS.items():
        if words in low:
            return TURNS[name]
    return TURNS["straight"]


def read_points(root: ET.Element) -> tuple[list[tuple[float, float, float]], str]:
    """Pontos e nome de um GPX ou TCX, na ordem em que estão no arquivo."""
    track: list[tuple[float, float, float]] = []
    route: list[tuple[float, float, float]] = []
    name = ""

    for el in root.iter():
        tag = strip_ns(el.tag)

        if tag == "name" and not name and (el.text or "").strip():
            name = el.text.strip()

        # GPX: trkpt (traçado) e rtept (rota com instruções); TCX: Trackpoint
        if tag in ("trkpt", "rtept"):
            try:
                lat = float(el.attrib["lat"])
                lon = float(el.attrib["lon"])
            except (KeyError, ValueError):
                continue
            alt = 0.0
            for child in el:
                if strip_ns(child.tag) == "ele":
                    try:
                        alt = float(child.text or 0.0)
                    except ValueError:
                        alt = 0.0
            if tag == "trkpt":
                track.append((lat, lon, alt))
            else:
                route.append((lat, lon, alt))

        elif tag == "Trackpoint":
            lat = lon = None
            alt = 0.0
            for child in el.iter():
                ctag = strip_ns(child.tag)
                if ctag == "LatitudeDegrees":
                    lat = float(child.text or 0.0)
                elif ctag == "LongitudeDegrees":
                    lon = float(child.text or 0.0)
                elif ctag == "AltitudeMeters":
                    alt = float(child.text or 0.0)
            if lat is not None and lon is not None:
                track.append((lat, lon, alt))

    # o traçado manda; uma rota só com `rtept` (sem `trk`) também serve
    return (track if track else route), name


def read_cues(root: ET.Element,
              points: list[tuple[float, float, float]]) -> list[tuple[int, int, str]]:
    """Curvas do arquivo, ligadas ao ponto mais próximo do percurso.

    Pega o que os serviços exportam: `rtept` com `<name>`/`<cmt>` (o padrão
    do GPX para rota com instruções) e `CoursePoint` do TCX.
    """
    cues: list[tuple[int, int, str]] = []
    raw: list[tuple[float, float, str]] = []
    has_track = any(strip_ns(el.tag) in ("trkpt", "Trackpoint") for el in root.iter())

    for el in root.iter():
        tag = strip_ns(el.tag)
        if (tag == "rtept") and has_track:
            try:
                lat = float(el.attrib["lat"])
                lon = float(el.attrib["lon"])
            except (KeyError, ValueError):
                continue
            text = ""
            for child in el:
                if strip_ns(child.tag) in ("name", "cmt", "desc") and (child.text or "").strip():
                    text = child.text.strip()
                    break
            if text:
                raw.append((lat, lon, text))
        elif tag == "CoursePoint":
            lat = lon = None
            text = ""
            for child in el.iter():
                ctag = strip_ns(child.tag)
                if ctag == "LatitudeDegrees":
                    lat = float(child.text or 0.0)
                elif ctag == "LongitudeDegrees":
                    lon = float(child.text or 0.0)
                elif ctag in ("PointType", "Notes", "Name"):
                    text = text or (child.text or "").strip()
            if lat is not None and lon is not None and text:
                raw.append((lat, lon, text))

    for lat, lon, text in raw:
        best = 0
        best_d = float("inf")
        for i, (plat, plon, _) in enumerate(points):
            d = distance_m(lat, lon, plat, plon)
            if d < best_d:
                best_d = d
                best = i
        # uma curva longe demais do traçado não é deste percurso
        if best_d <= 50.0:
            cues.append((best, turn_of(text), text))

    cues.sort(key=lambda c: c[0])
    return cues


def build_rte(points: list[tuple[float, float, float]],
              cues: list[tuple[int, int, str]],
              name: str) -> bytes:
    """Monta os bytes do `.RTE`, com o CRC-32 do corpo no cabeçalho."""
    if len(points) < 2:
        raise ValueError("um percurso precisa de pelo menos dois pontos")

    body = bytearray()
    for lat, lon, alt in points:
        body += struct.pack("<iih", round(lat * 1e7), round(lon * 1e7),
                            max(-32768, min(32767, round(alt))))

    for at, turn, street in cues:
        text = street.encode("utf-8")[:STREET_MAX]
        body += struct.pack("<IBB", at, turn, 0) + text.ljust(STREET_MAX, b"\0")

    dist = sum(distance_m(points[i - 1][0], points[i - 1][1], points[i][0], points[i][1])
               for i in range(1, len(points)))
    climb = sum(max(0.0, points[i][2] - points[i - 1][2]) for i in range(1, len(points)))

    lats = [round(p[0] * 1e7) for p in points]
    lons = [round(p[1] * 1e7) for p in points]

    header = bytearray(HEADER_SIZE)
    header[0:4] = MAGIC
    struct.pack_into("<HHIIII", header, 4, VERSION, FLAG_CUES if cues else 0,
                     len(points), len(cues), round(dist), round(climb))
    struct.pack_into("<iiii", header, 24, min(lats), max(lats), min(lons), max(lons))
    header[40:40 + NAME_MAX] = name.encode("utf-8")[:NAME_MAX].ljust(NAME_MAX, b"\0")
    struct.pack_into("<I", header, 60, zlib.crc32(bytes(body)) & 0xFFFFFFFF)

    return bytes(header) + bytes(body)


def describe(data: bytes) -> str:
    """Lê de volta o que foi escrito e descreve, para conferência."""
    if len(data) < HEADER_SIZE or data[0:4] != MAGIC:
        raise ValueError("não é um arquivo RTE")

    version, flags, points, cues, dist, climb = struct.unpack_from("<HHIIII", data, 4)
    crc = struct.unpack_from("<I", data, 60)[0]
    name = data[40:40 + NAME_MAX].split(b"\0", 1)[0].decode("utf-8", "replace")
    body = data[HEADER_SIZE:]

    if version != VERSION:
        raise ValueError(f"versão {version} não é a {VERSION} deste conversor")
    if len(body) != (points * POINT_SIZE) + (cues * CUE_SIZE):
        raise ValueError("o corpo não tem o tamanho que o cabeçalho diz")
    if (zlib.crc32(body) & 0xFFFFFFFF) != crc:
        raise ValueError("o CRC não confere")
    if cues and not (flags & FLAG_CUES):
        raise ValueError("curvas sem a marca no cabeçalho")

    return (f"{name or '(sem nome)'}: {points} pontos, {cues} curvas, "
            f"{dist / 1000.0:.1f} km, {climb} m de subida, {len(data)} bytes")


def selftest() -> int:
    """Gera um GPX, converte, lê de volta e confere tudo."""
    gpx = ["<?xml version='1.0'?><gpx xmlns='http://www.topografix.com/GPX/1/1'>",
           "<trk><name>Serra do Mar</name><trkseg>"]
    for i in range(1000):
        lat = -23.5 + (i * 0.0001)
        alt = 700.0 + (i * 0.5 if i < 500 else (500 - i) * 0.5 + 500)
        gpx.append(f"<trkpt lat='{lat:.7f}' lon='-46.6'><ele>{alt:.1f}</ele></trkpt>")
    gpx.append("</trkseg></trk>")
    gpx.append("<rte><rtept lat='-23.4800000' lon='-46.6'><name>Turn right onto Rua das Flores"
               "</name></rtept>"
               "<rtept lat='-23.4500000' lon='-46.6'><name>Arrive at destination</name></rtept>"
               "</rte></gpx>")

    root = ET.fromstring("".join(gpx))
    points, name = read_points(root)
    cues = read_cues(root, points)
    data = build_rte(points, cues, name)

    problems = []
    if len(points) != 1000:
        problems.append(f"pontos lidos: {len(points)}")
    if name != "Serra do Mar":
        problems.append(f"nome: {name!r}")
    if len(cues) != 2:
        problems.append(f"curvas: {len(cues)}")
    elif cues[0][1] != TURNS["right"] or cues[1][1] != TURNS["arrive"]:
        problems.append(f"tipos das curvas: {[c[1] for c in cues]}")
    if len(data) != HEADER_SIZE + (1000 * POINT_SIZE) + (2 * CUE_SIZE):
        problems.append(f"tamanho: {len(data)}")

    # o texto do legacy do mesmo percurso, para comparar
    legacy = sum(len(f"{p[0]:.8f} {p[1]:.8f} {p[2]:.1f}\r\n") for p in points)

    try:
        print(describe(data))
    except ValueError as err:
        problems.append(str(err))

    # um byte trocado tem que ser pego pelo CRC
    broken = bytearray(data)
    broken[HEADER_SIZE + 3] ^= 0x01
    try:
        describe(bytes(broken))
        problems.append("CRC não pegou um byte trocado")
    except ValueError:
        pass

    print(f"texto do legacy: {legacy} bytes; RTE: {len(data)} bytes "
          f"({100 * len(data) / legacy:.0f} %)")

    if problems:
        for p in problems:
            print("FALHOU:", p)
        return 1

    print("selftest ok")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("entrada", nargs="?", help="arquivo GPX ou TCX")
    parser.add_argument("saida", nargs="?", help="arquivo .RTE a escrever")
    parser.add_argument("--name", default="", help="nome do percurso (senão, o do arquivo)")
    parser.add_argument("--selftest", action="store_true", help="testa o conversor consigo mesmo")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    if not args.entrada or not args.saida:
        parser.error("informe a entrada e a saída, ou use --selftest")

    root = ET.parse(args.entrada).getroot()
    points, name = read_points(root)
    if not points:
        print("nenhum ponto no arquivo de entrada", file=sys.stderr)
        return 1

    cues = read_cues(root, points)
    data = build_rte(points, cues, args.name or name or Path(args.entrada).stem)
    Path(args.saida).write_bytes(data)
    print(describe(data))

    return 0


if __name__ == "__main__":
    sys.exit(main())
