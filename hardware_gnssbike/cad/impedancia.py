"""Impedance of the board's lines on the fabricator's stack-up.

A quasi-static 2D field solution of the cross-section by finite
differences: div(eps grad phi) = 0 on a uniform grid, the ground plane at
y = 0, each conductor at a fixed potential, and the capacitance per length
taken from the field energy with and without the dielectrics, so that
Z0 = 1 / (c sqrt(C C_air)). Two grids, 5 and 2.5 um, and a Richardson step
between them. It counts what the closed forms leave out and what moved the
number on this board: the copper thickness (35 um over 0.0994 mm), the
solder mask over and between the tracks, and the ground pour 0.2 mm beside
them.

Checked on 2026-10-19 against Wheeler (a thin strip in air, w = h: 121.1
against 121.9 ohm) and Hammerstad-Jensen (0.2 mm over 0.1 mm, Dk 4.05,
35 um: 47.3 against 46.9 ohm). It needs numpy and scipy; each line takes a
few seconds:

    python impedancia.py
"""
from __future__ import annotations

import math

import numpy as np
import scipy.sparse as sp
import scipy.sparse.linalg as spl

C0 = 299792458.0
E0 = 8.8541878128e-12

# JLCPCB JLC04081H-3313, 0.8 mm, four layers (their impedance page,
# 2026-10-19): what lies between F.Cu and the plane on In1.Cu
H_PREPREG = 0.0994      # mm, 3313 prepreg
DK_PREPREG = 4.05       # 4.1 on another page of theirs; both are computed
T_COBRE = 0.035         # mm, outer copper
DK_MASCARA = 3.8
MASCARA_COBRE = 0.0127  # mm of mask over a track (0.5 mil)
MASCARA_LAMINADO = 0.0203   # mm of mask over the bare laminate (0.8 mil)
VAO_TERRA = 0.2         # mm from a line to the ground pour (the zones' clearance)


def _energia(conds, diel, x0, x1, y1, d):
    """The sum of eps_r |grad phi|^2 over the cross-section (so 2 W / eps0),
    with the dielectrics and with eps_r = 1 everywhere."""
    nx = int(round((x1 - x0) / d))
    ny = int(round(y1 / d))
    X, Y = np.meshgrid(x0 + d * np.arange(nx + 1), d * np.arange(ny + 1), indexing="ij")
    fixo = np.zeros((nx + 1, ny + 1), bool)
    val = np.zeros((nx + 1, ny + 1))
    fixo[:, 0] = True                       # the plane
    for a, b, c, e, v in conds:
        m = (X >= a - 1e-9) & (X <= b + 1e-9) & (Y >= c - 1e-9) & (Y <= e + 1e-9)
        fixo |= m
        val[m] = v
    XC, YC = np.meshgrid(x0 + d * (np.arange(nx) + 0.5), d * (np.arange(ny) + 0.5), indexing="ij")
    livres = ~fixo
    idx = -np.ones((nx + 1, ny + 1), int)
    idx[livres] = np.arange(livres.sum())
    n = int(livres.sum())
    saida = []
    for com_dieletrico in (True, False):
        eps = diel(XC, YC) if com_dieletrico else np.ones_like(XC)
        borda = np.zeros((nx + 2, ny + 2))
        borda[1:-1, 1:-1] = eps
        ah = 0.5 * (borda[1:-1, :-1] + borda[1:-1, 1:])    # edges (i, j) - (i + 1, j)
        av = 0.5 * (borda[:-1, 1:-1] + borda[1:, 1:-1])    # edges (i, j) - (i, j + 1)
        linhas, colunas, dados = [], [], []
        diag = np.zeros(n)
        rhs = np.zeros(n)

        def acopla(i1, j1, i2, j2, a):
            sel = a > 0
            i1, j1, i2, j2, a = i1[sel], j1[sel], i2[sel], j2[sel], a[sel]
            m1, m2 = idx[i1, j1], idx[i2, j2]
            for p, q, vq in ((m1, m2, val[i2, j2]), (m2, m1, val[i1, j1])):
                ok = p >= 0
                np.add.at(diag, p[ok], a[ok])
                ambos = ok & (q >= 0)
                linhas.append(p[ambos])
                colunas.append(q[ambos])
                dados.append(-a[ambos])
                fx = ok & (q < 0)
                np.add.at(rhs, p[fx], a[fx] * vq[fx])

        I, J = np.meshgrid(np.arange(nx), np.arange(ny + 1), indexing="ij")
        acopla(I.ravel(), J.ravel(), I.ravel() + 1, J.ravel(), ah.ravel())
        I, J = np.meshgrid(np.arange(nx + 1), np.arange(ny), indexing="ij")
        acopla(I.ravel(), J.ravel(), I.ravel(), J.ravel() + 1, av.ravel())
        linhas.append(np.arange(n))
        colunas.append(np.arange(n))
        dados.append(diag)
        A = sp.csr_matrix((np.concatenate(dados), (np.concatenate(linhas), np.concatenate(colunas))),
                          shape=(n, n))
        phi = val.copy()
        phi[livres] = spl.spsolve(A.tocsc(), rhs)
        saida.append(np.sum(ah * (phi[1:, :] - phi[:-1, :]) ** 2)
                     + np.sum(av * (phi[:, 1:] - phi[:, :-1]) ** 2))
    return saida


def _secao(w, s, modo, d, h, t, dk, g, mascara, largura=1.4, altura=0.8):
    def q(v):
        return None if v is None else round(v / d) * d
    w, s, g, h, t = q(w), q(s), q(g), q(h), q(t)
    m_cobre, m_lam = max(q(MASCARA_COBRE), d), max(q(MASCARA_LAMINADO), d)
    largura = q(largura)
    if s is None:
        trilhas = [(-w / 2, w / 2, 1.0)]
        borda = w / 2
    else:
        trilhas = [(-s / 2 - w, -s / 2, 1.0),
                   (s / 2, s / 2 + w, -1.0 if modo == "impar" else 1.0)]
        borda = s / 2 + w
    conds = [(a, b, h, h + t, v) for a, b, v in trilhas]
    if g is not None:
        conds += [(-largura, -borda - g, h, h + t, 0.0), (borda + g, largura, h, h + t, 0.0)]
    cobre = [(a, b) for a, b, _c, _e, _v in conds]

    def diel(XC, YC):
        eps = np.ones_like(XC)
        eps[YC < h] = dk
        if mascara:
            sobre = np.zeros_like(XC, bool)
            for a, b in cobre:
                sobre |= (XC >= a) & (XC <= b)
            eps[(~sobre) & (YC >= h) & (YC < h + m_lam)] = DK_MASCARA
            eps[sobre & (YC >= h + t) & (YC < h + t + m_cobre)] = DK_MASCARA
        return eps

    w1, w0 = _energia(conds, diel, -largura, largura, altura, d)
    # a single line at 1 V: C = eps0 sum; a pair at +-1 V or +1 +1: the
    # energy W is the capacitance of each line, so C = eps0 sum / 2
    fator = 1.0 if s is None else 0.5
    return 1.0 / (C0 * math.sqrt(fator * E0 * w1 * fator * E0 * w0))


def z(w, s=None, modo="simples", h=H_PREPREG, t=T_COBRE, dk=DK_PREPREG, g=VAO_TERRA,
      mascara=True) -> float:
    """Impedance in ohm of one line (s None) or of one line of a pair in
    `modo` "impar" or "par", Richardson between the 5 and 2.5 um grids."""
    a = _secao(w, s, modo, 0.005, h, t, dk, g, mascara)
    b = _secao(w, s, modo, 0.0025, h, t, dk, g, mascara)
    return 2 * b - a


def main() -> int:
    print(f"JLC04081H-3313: {H_PREPREG} mm de prepreg (Dk {DK_PREPREG}) ate o plano, "
          f"cobre de {T_COBRE * 1000:.0f} um, mascara Dk {DK_MASCARA}, terra a {VAO_TERRA} mm\n")
    for w in (0.196, 0.15):
        print(f"  linha de RF de {w:.3f} mm: {z(w):5.1f} ohm", flush=True)
    for w, s in ((0.15, 0.2), (0.15, 0.35)):
        zd = 2 * z(w, s, "impar")
        zc = z(w, s, "par") / 2
        print(f"  par USB de {w:.3f} mm com {s:.2f} de vao: {zd:5.1f} ohm diferencial, "
              f"{zc:4.1f} de modo comum", flush=True)
    print(f"\n  na JLC7628 (0,2104 mm, Dk 4,6), a linha de 0,15 mm daria "
          f"{z(0.15, h=0.2104, dk=4.6):5.1f} ohm")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
