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

As pinagens do nPM1300, MAX17262, AEM10900, TPS7A02, ESD761, TPD4E05U06,
MAX-F10S, TXU0204, MX25R6435F, BMP585, BMI270, MMC5633NJL, OPT3001, do
receptáculo USB-C, do JST GH, do Hirose FH28, do LED da Kingbright, da tecla
Omron e do painel JDI estão transcritas em [`cad/parts.py`](../cad/parts.py),
cada uma com o documento e a revisão de onde veio, no comentário logo acima.
