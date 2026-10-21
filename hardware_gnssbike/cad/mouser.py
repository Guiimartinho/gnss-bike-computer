"""The parts order for the board, Mouser first, from the BOM make_fab.py wrote.

Each BOM line goes to its manufacturer part number (mpn.py), the Mouser
search API gives the Mouser number, the stock and the price breaks, and a
CSV for Mouser's BOM tool comes out with the quantity for the boards asked
and the price at that quantity. What Mouser does not sell, or has no stock
of, goes to a spreadsheet for LCSC's BOM tool, by the LCSC code the BOM
already carries, and LCSC rounds each line up to its own minimum; what is
bought at DigiKey goes to one for DigiKey's, by its code. Both are .xlsx,
which the two tools take and Excel opens the same in any language: a CSV
opens in one column where the list separator is ';', as in Brazil. What
none of the three sells is printed.

Each Mouser line carries the lifecycle status the API gives, and the parts
on their way out (end of life, obsolete, not for new designs) are listed
at the end, to be swapped or sent elsewhere in mpn.py. The API is not the
last word: on 2026-10-19 Mouser's BOM tool flagged two parts obsolete that
it gave no status at all (mpn.py, above DIGIKEY).

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

import openpyxl
from openpyxl.styles import Font

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import mpn as MPN  # noqa: E402

SAIDA = HERE.parent / "fabricacao"
API = "https://api.mouser.com/api/v1/search/partnumber?apiKey="
# small passives are cheap at ten and easy to lose: a few spares each
PASSIVOS = ("C", "R", "L", "FB", "JP")
# what the search API's LifecycleStatus says of a part on its way out
SAINDO = ("end of life", "obsolete", "not recommended", "last time")


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


def planilha(caminho: pathlib.Path, cabecalho: list[str], linhas: list[list]) -> None:
    """An .xlsx with the header in bold and columns wide enough to read."""
    wb = openpyxl.Workbook()
    ws = wb.active
    ws.title = "GNSS Bike"
    ws.append(cabecalho)
    for c in ws[1]:
        c.font = Font(bold=True)
    for ln in linhas:
        ws.append(ln)
    for col in ws.columns:
        ws.column_dimensions[col[0].column_letter].width = min(60, max(len(str(c.value or "")) for c in col) + 2)
    wb.save(caminho)


def main() -> int:
    variante = sys.argv[1] if len(sys.argv) > 1 else "jdi"
    placas = int(sys.argv[2]) if len(sys.argv) > 2 else 2
    bom = SAIDA / f"bom-{variante}.csv"
    if not bom.exists():
        raise SystemExit(f"falta {bom.name}: rode make_fab.py antes")
    linhas = list(csv.DictReader(open(bom, encoding="utf-8")))
    pedidos, pela_lcsc, digikey, outras = [], [], [], []
    for ln in linhas:
        refs = ln["Designator"].split(",")
        lcsc = ln["LCSC Part #"]
        if not lcsc:
            for ref in refs:
                if ref in MPN.SEM_LCSC:
                    fab, num, codigo, nota = MPN.SEM_LCSC[ref]
                    (digikey if codigo else outras).append((ref, codigo, fab, num, placas, nota))
            continue
        if lcsc in MPN.FORA_DA_MOUSER:
            fab, num = MPN.MPN[lcsc]
            pela_lcsc.append((lcsc, quantidade(refs, placas, None), fab, num, ",".join(refs),
                              MPN.FORA_DA_MOUSER[lcsc]))
            continue
        if lcsc in MPN.DIGIKEY:
            fab = MPN.MPN[lcsc][0]
            codigo, num, nota = MPN.DIGIKEY[lcsc]
            digikey.append((",".join(refs), codigo, fab, num, len(refs) * placas, nota))
            continue
        fab, num = MPN.MPN[lcsc]
        nota = ""
        if lcsc in MPN.MOUSER:
            fab, num, nota = MPN.MOUSER[lcsc]
        # BOM lines that differ only by footprint name are one part to buy,
        # and Mouser's BOM tool joins them anyway
        igual = next((p for p in pedidos if p["mpn"] == num), None)
        if igual:
            igual["refs"] += refs
            continue
        pedidos.append({"refs": refs, "fab": fab, "mpn": num, "lcsc": lcsc, "nota": nota})
    for refs, fab, num, por_placa, codigo, nota, telas in MPN.FORA_DA_PLACA:
        if variante in telas:
            (digikey if codigo else outras).append((refs, codigo, fab, num, por_placa * placas, nota))
    achados = consultar([p["mpn"] for p in pedidos])
    total, n_mouser = 0.0, 0
    saindo = []
    saida = SAIDA / f"mouser-{variante}.csv"
    with open(saida, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Mouser No", "Mfr No", "Manufacturer", "Quantity", "Unit Price (USD)",
                    "Ext. Price (USD)", "Stock", "Lead Time", "Lifecycle", "Customer No", "Description",
                    "Note"])
        for p in pedidos:
            m = achados.get(p["mpn"].upper())
            q = quantidade(p["refs"], placas, m)
            estoque = int(m.get("AvailabilityInStock") or 0) if m else 0
            if estoque < q:
                # a backorder is months: the part comes from LCSC, and as
                # the project names it, not as Mouser's swap does
                motivo = (f"Mouser sem estoque ({estoque}, prazo {m.get('LeadTime') or '?'})" if m
                          else "nao achado na Mouser")
                fab, num = MPN.MPN[p["lcsc"]]
                pela_lcsc.append((p["lcsc"], quantidade(p["refs"], placas, None), fab, num,
                                  ",".join(p["refs"]), motivo))
                continue
            un = preco(m, q)
            total += un * q
            n_mouser += 1
            ciclo = m.get("LifecycleStatus") or ""
            if any(s in ciclo.lower() for s in SAINDO):
                saindo.append((",".join(p["refs"]), m["ManufacturerPartNumber"], ciclo))
            w.writerow([m["MouserPartNumber"], m["ManufacturerPartNumber"], m.get("Manufacturer", p["fab"]), q,
                        f"{un:.4f}", f"{un * q:.2f}", estoque, m.get("LeadTime") or "", ciclo,
                        ",".join(p["refs"]), (m.get("Description") or "")[:80], p["nota"]])
    # the customer number goes on the order and on the bags: the project's
    # name and the designators
    pela_lcsc.sort(key=lambda x: x[4])
    saida_lcsc = SAIDA / f"lcsc-{variante}.xlsx"
    planilha(saida_lcsc, ["LCSC Part Number", "Quantity", "Manufacturer", "Manufacturer Part Number",
                          "Customer Part Number", "Note"],
             [[codigo, q, fab, num, f"GNSS Bike {refs}", motivo]
              for codigo, q, fab, num, refs, motivo in pela_lcsc])
    saida_dk = SAIDA / f"digikey-{variante}.xlsx"
    planilha(saida_dk, ["Digi-Key Part Number", "Manufacturer Part Number", "Manufacturer", "Quantity",
                        "Customer Reference", "Reference Designator", "Note"],
             [[codigo, num, fab, q, f"GNSS Bike {refs}", refs, nota]
              for refs, codigo, fab, num, q, nota in digikey])
    print(f"{saida.name}: {n_mouser} linhas para {placas} placa(s), {variante}; "
          f"US$ {total:.2f} nos precos da quantidade")
    for refs, num, ciclo in saindo:
        print(f"    ATENCAO {refs}: {num} esta '{ciclo}' na Mouser; troque ou mande para outra loja em mpn.py")
    print(f"{saida_lcsc.name}: {len(pela_lcsc)} linhas, o que a Mouser nao vende ou nao tem em estoque "
          f"(a LCSC arredonda cada uma ao lote minimo dela):")
    for codigo, q, fab, num, refs, motivo in pela_lcsc:
        print(f"    {refs}: {codigo} {fab} {num} x{q} - {motivo}")
    print(f"{saida_dk.name}: {len(digikey)} linhas:")
    for refs, codigo, fab, num, q, nota in digikey:
        print(f"    {refs}: {codigo} {fab} {num} x{q}{' - ' + nota if nota else ''}")
    print("  Outras lojas:")
    for refs, codigo, fab, num, q, nota in outras:
        print(f"    {refs}: {fab} {num} x{q} - {nota}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
