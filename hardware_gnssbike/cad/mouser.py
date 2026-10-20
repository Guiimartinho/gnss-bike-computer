"""The parts order for the board, Mouser first, from the BOM make_fab.py wrote.

Each BOM line goes to its manufacturer part number (mpn.py), the Mouser
search API gives the Mouser number, the stock and the price breaks, and a
CSV for Mouser's BOM tool comes out with the quantity for the boards asked
and the price at that quantity. What Mouser does not sell, or has no stock
of, goes to a CSV for LCSC's BOM tool, by the LCSC code the BOM already
carries, and LCSC rounds each line up to its own minimum; the few parts
that come from DigiKey or elsewhere are printed with their codes.

The API key is personal: it is read from MOUSER_API_KEY or from
.claude/local/mouser_api_key, which git does not see, and it is never
printed.

    python make_fab.py                      # first: the BOMs
    python mouser.py [jdi|sharp] [boards]   # default: jdi, 2 boards
"""
from __future__ import annotations

import csv
import json
import os
import pathlib
import re
import sys
import time
import urllib.request

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import mpn as MPN  # noqa: E402

SAIDA = HERE.parent / "fabricacao"
API = "https://api.mouser.com/api/v1/search/partnumber?apiKey="
# small passives are cheap at ten and easy to lose: a few spares each
PASSIVOS = ("C", "R", "L", "FB", "JP")


def chave() -> str:
    k = os.environ.get("MOUSER_API_KEY", "").strip()
    if not k:
        arq = HERE.parents[1] / ".claude" / "local" / "mouser_api_key"
        k = arq.read_text(encoding="utf-8").strip() if arq.exists() else ""
    if not k:
        raise SystemExit("sem a chave da Mouser: MOUSER_API_KEY ou .claude/local/mouser_api_key")
    return k


def consultar(mpns: list[str]) -> dict[str, dict]:
    """MPN (upper case) -> the Mouser part, ten numbers a call, as the API
    takes them, and slow enough for its limit of calls a minute."""
    k = chave()
    achados: dict[str, dict] = {}
    for i in range(0, len(mpns), 10):
        corpo = {"SearchByPartRequest": {"mouserPartNumber": "|".join(mpns[i:i + 10]),
                                         "partSearchOptions": "Exact"}}
        req = urllib.request.Request(API + k, data=json.dumps(corpo).encode(),
                                     headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=60) as r:
            d = json.load(r)
        if d.get("Errors"):
            raise SystemExit(f"Mouser: {d['Errors']}")
        for p in (d.get("SearchResults") or {}).get("Parts", []):
            if p.get("MouserPartNumber") not in (None, "", "N/A"):
                achados.setdefault(p["ManufacturerPartNumber"].upper(), p)
        time.sleep(2.5)
    return achados


def preco(p: dict, qtd: int) -> float:
    """Unit price at the break the quantity reaches, in the API's currency."""
    melhor = None
    for b in p.get("PriceBreaks") or []:
        if b["Quantity"] <= qtd:
            melhor = b
    if melhor is None and p.get("PriceBreaks"):
        melhor = p["PriceBreaks"][0]
    if melhor is None:
        return 0.0
    return float(re.sub(r"[^0-9.]", "", melhor["Price"].replace(",", ".")) or 0)


def quantidade(refs: list[str], placas: int, p: dict | None) -> int:
    q = len(refs) * placas
    if all(re.match(r"(%s)\d" % "|".join(PASSIVOS), r) for r in refs):
        q = max(q + 5, 10)
    if p:
        minimo = int(p.get("Min") or 1)
        mult = int(p.get("Mult") or 1)
        q = max(q, minimo)
        q = -(-q // mult) * mult
    return q


def main() -> int:
    variante = sys.argv[1] if len(sys.argv) > 1 else "jdi"
    placas = int(sys.argv[2]) if len(sys.argv) > 2 else 2
    bom = SAIDA / f"bom-{variante}.csv"
    if not bom.exists():
        raise SystemExit(f"falta {bom.name}: rode make_fab.py antes")
    linhas = list(csv.DictReader(open(bom, encoding="utf-8")))
    pedidos, fora = [], []
    for ln in linhas:
        refs = ln["Designator"].split(",")
        lcsc = ln["LCSC Part #"]
        if not lcsc:
            for ref in refs:
                if ref in MPN.SEM_LCSC:
                    fab, num, onde = MPN.SEM_LCSC[ref]
                    fora.append((ref, fab, num, len(refs) * placas, onde, ""))
            continue
        if lcsc in MPN.FORA_DA_MOUSER:
            fab, num = MPN.MPN[lcsc]
            fora.append((",".join(refs), fab, num, len(refs) * placas, MPN.FORA_DA_MOUSER[lcsc], lcsc))
            continue
        fab, num = MPN.MPN[lcsc]
        nota = ""
        if lcsc in MPN.MOUSER:
            fab, num, nota = MPN.MOUSER[lcsc]
        pedidos.append({"refs": refs, "fab": fab, "mpn": num, "lcsc": lcsc, "nota": nota})
    achados = consultar([p["mpn"] for p in pedidos])
    total, n_mouser = 0.0, 0
    lcsc, digikey = [], []
    saida = SAIDA / f"mouser-{variante}.csv"
    with open(saida, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Mouser No", "Mfr No", "Manufacturer", "Quantity", "Unit Price (USD)",
                    "Ext. Price (USD)", "Stock", "Lead Time", "Customer No", "Description", "Note"])
        for p in pedidos:
            m = achados.get(p["mpn"].upper())
            q = quantidade(p["refs"], placas, m)
            estoque = int(m.get("AvailabilityInStock") or 0) if m else 0
            if estoque < q:
                # a backorder is months: the part comes from elsewhere, and
                # as the project names it, not as Mouser's swap does
                motivo = (f"Mouser sem estoque ({estoque}, prazo {m.get('LeadTime') or '?'})" if m
                          else "nao achado na Mouser")
                fab, num = MPN.MPN[p["lcsc"]]
                if p["lcsc"] in MPN.DIGIKEY:
                    codigo, nota = MPN.DIGIKEY[p["lcsc"]]
                    digikey.append((",".join(p["refs"]), codigo, fab, num, len(p["refs"]) * placas,
                                    f"{motivo}; {nota}"))
                else:
                    lcsc.append((p["lcsc"], quantidade(p["refs"], placas, None), fab, num,
                                 ",".join(p["refs"]), motivo))
                continue
            un = preco(m, q)
            total += un * q
            n_mouser += 1
            w.writerow([m["MouserPartNumber"], m["ManufacturerPartNumber"], m.get("Manufacturer", p["fab"]), q,
                        f"{un:.4f}", f"{un * q:.2f}", estoque, m.get("LeadTime") or "", ",".join(p["refs"]),
                        (m.get("Description") or "")[:80], p["nota"]])
    for refs, fab, num, q, onde, codigo in fora:
        if codigo:
            lcsc.append((codigo, quantidade(refs.split(","), placas, None), fab, num, refs, onde))
        else:
            digikey.append((refs, "", fab, num, q, onde))
    saida_lcsc = SAIDA / f"lcsc-{variante}.csv"
    with open(saida_lcsc, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["LCSC Part Number", "Quantity", "Manufacturer", "Manufacturer Part Number",
                    "Customer Part Number", "Note"])
        for linha in sorted(lcsc, key=lambda x: x[4]):
            w.writerow(linha)
    print(f"{saida.name}: {n_mouser} linhas para {placas} placa(s), {variante}; "
          f"US$ {total:.2f} nos precos da quantidade")
    print(f"{saida_lcsc.name}: {len(lcsc)} linhas, o que a Mouser nao vende ou nao tem em estoque "
          f"(a LCSC arredonda cada uma ao lote minimo dela):")
    for codigo, q, fab, num, refs, motivo in sorted(lcsc, key=lambda x: x[4]):
        print(f"    {refs}: {codigo} {fab} {num} x{q} - {motivo}")
    print("  DigiKey e outras lojas:")
    for refs, codigo, fab, num, q, onde in digikey:
        print(f"    {refs}: {fab} {num} x{q}{' (' + codigo + ')' if codigo else ''} - {onde}")
    for refs, fab, num, q, onde in MPN.FORA_DA_PLACA:
        print(f"    {refs} (fora da placa): {fab} {num} x{q * placas} - {onde}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
