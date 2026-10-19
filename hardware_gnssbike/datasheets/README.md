# Fichas técnicas

**Os arquivos deste diretório não vão para o repositório.** São documentos do
fabricante, e o repositório é público: aqui ficam só os endereços de onde
baixá-los e o que foi lido em cada um. O `.gitignore` ao lado cuida disso.

Baixe o que precisar e trabalhe localmente.

## MinewSemi ME54BS13 — o módulo de rádio

| Arquivo | O que é | Onde |
|---|---|---|
| `ME54BS13-nRF54LM20A_Datasheet_K_EN_v1.0.0.pdf` | **a ficha boa**: V1.0.0, 2026-06-23, 16 páginas, com números de pino, desenho mecânico cotado e as regras de PCB | <https://store.minewsemi.com/wp-content/uploads/2026/07/ME54BS13-nRF54LM20A_Datesheet_K_EN.pdf> |
| `ME54BS13-nRF54LM20A_Datasheet_K_EN_Brief.pdf` | versão anterior, V0.5.0 de 2026-03-06, 12 páginas. É a que a DigiKey serve | <https://en.minewsemi.com/file/ME54BS13-nRF54LM20A_Datasheet_K_EN_Brief.pdf> |
| `ME54BS13_3rdparty_girishji.kicad_mod` | footprint KiCad de terceiro, **não oficial**; a geometria bate com as cotas da ficha | <https://github.com/girishji/tmr-keyboard> |

Páginas: [produto](https://en.minewsemi.com/bluetooth-module/nrf54lm20a-me54bs13)
· [loja, US$ 6,00](https://store.minewsemi.com/product/bluetooth-modules-nrf54lm20a-me54bs13/).
Na página do produto o download pede login; pela URL direta do arquivo, não.

O que saiu dessas fichas e entrou no projeto está em
[`cad/parts.py`](../cad/parts.py) (`PADS_ME54BS13`, os 80 pads) e em
[`cad/footprints.py`](../cad/footprints.py) (`me54bs13()`, as cotas do desenho
mecânico). **A Minew não publica land pattern** — a ficha manda pedir o dela.

## As outras peças

As pinagens das peças do desenho de hoje estão transcritas em
[`cad/parts.py`](../cad/parts.py), cada uma com o documento e a revisão de
onde veio, no comentário logo acima: nPM1300, MAX17262, ADP5091 (Rev. A),
TPS7A02, TLV7031 (SLVSE13J), TPS610997 (SLVSD88M), PESD24VS1UL,
TPD4E05U06, PESD3V3S1UB, BAT54WS, MAX-F10S, a antena Unictron
H2UJ4U1H2Q0100, o u.FL da Hirose, MX25R6435F, BMP585, BMI270, MMC5603NJ
(Rev. B), OPT3001, o USB-C HRO TYPE-C-31-M-12, os JST SH e ZH, os três
Molex 503480 (SD-503480-001), o DMG1012T, a tecla XunPu TS-1088R, o buzzer
Same Sky CPT-12123, o LED RGB TUOZHAN S4-3528RGBTA-A (S35210052), o LED
Kingbright APT1608SURCK, o Tag-Connect TC2030 (`TC2030-CTX_1.pdf`) e as
duas telas: a JDI LPM027M128C (spec ver.02) e a Sharp LS027B7DH01A
(LCP-2110015A e LD-28305A), com o desenho T7 do Azumo 11103-06.

Algumas fichas guardadas aqui são de peças que saíram do desenho e ficam
como registro do que foi lido: a do AEM10900 (o colhedor de antes do
ADP5091), do ESD761, do TXU0204, do MMC5633NJL, do Hirose FH28, do Molex
2036150003, do JST GH, da TE L000670, da tecla Omron, do LED APTF1616 e do
buzzer CPT-1117. Das peças que entraram a partir de 2026-09-24, só a do
ADP5091 está aqui; as outras se baixam do fabricante ou da página da LCSC
do código que `cad/parts.py` traz.
