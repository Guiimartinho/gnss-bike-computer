# Corpos 3D das peças

Das 168 footprints da placa, contadas em 2026-10-21:
- **121** usam o modelo da própria biblioteca do KiCad;
- **3** usam o STEP do fabricante que está em `real/`: o USB-C `J101`, o conector da bateria `J102` e o do painel `J103`;
- **24** recebem uma **caixa desenhada por este projeto**, com a medida do encapsulamento, porque nem o KiCad nem `real/` têm modelo para elas;
- **20** não têm corpo, e isso está certo: os furos, os pontos de teste, as ilhas do Tag-Connect e os dois jumpers de três ilhas (`JP301` e `JP401`). O 0 Ω dos jumpers, só o `make_3d.py` desenha, no par de ilhas da montagem.

> [!NOTE]
> Os `.wrl` desta pasta são **gerados** por
> [`../footprints.py`](../footprints.py). Não os edite: a próxima execução os
> reescreve.

## Como pôr um modelo de verdade

1. Baixe o STEP no site do fabricante ou na página da peça na LCSC, que
   exporta o modelo da EasyEDA. Salve em **`real/`** com o **nome do
   footprint** da tabela abaixo. Os sites da Molex, da LCSC/EasyEDA e da
   u-blox recusam download automático (conferido em 2026-10-21): o arquivo
   tem de vir pelo navegador.
2. Rode `python ../aplicar_modelos.py`. Ele troca só o bloco do modelo de
   cada peça na placa roteada. **Nunca rode o `make_pcb.py`**: ele coloca a
   placa de novo e desfaz o roteamento.
3. Meça o giro e o deslocamento do corpo contra o `F.Fab` do footprint,
   numa exportação de teste. Escreva em `footprints.MODELO_GIRADO`, ou em
   `MODELO_GIRADO_VERSO` para peça no verso, e rode o `aplicar_modelos.py` de
   novo. Meça, não calcule: o STEP de fabricante pode vir girado 180° ou não
   ([armadilhas](../../../.claude/contexto/armadilhas-hardware.md)).
4. Rode `python ../dry_run_pcb.py`, cujas regras `ME4` a `ME6` medem o corpo
   contra as ilhas e o `F.Fab`, e depois `python ../make_3d.py`.

Aceita `.step`, `.stp` e `.wrl`, nessa ordem. **STEP é melhor**: é o único
formato que a exportação GLB e STEP do KiCad lê, então só ele aparece na
imagem gerada por `make_3d.py`. Um `.wrl` aparece apenas no visualizador 3D
do KiCad.

> [!WARNING]
> **`real/` não vai para o repositório.** Este repositório é público, e um
> modelo 3D de fabricante vem com os termos do fabricante: é a mesma razão
> que mantém as fichas fora de [`../../datasheets/`](../../datasheets/). O
> `.gitignore` da pasta cuida disso. Os `.wrl` gerados também ficam de fora,
> como todo arquivo gerado neste repositório. O que é versionado é o
> **gerador**, em [`../footprints.py`](../footprints.py), com a altura de
> cada caixa e a origem dela (`ALTURA` e `PACOTE`).

## As 24 caixas, e o que procurar

O nome do footprint é o nome do arquivo a salvar em `real/`.

| Footprint | Peças | Peça a procurar | A caixa representa bem? |
|---|---|---|---|
| `Molex_503480-1000_1x10_P0.50mm` | `J401` | Molex 503480-1000 (LCSC C127355) | não: o conector tem atuador e contatos |
| `Molex_503480-0540_1x05_P0.50mm` | `J402` | Molex 503480-0540 (C5527934) | não |
| `Molex_503480-0440_1x04_P0.50mm` | `J403` | Molex 503480-0440 (C3170007) | não |
| `SW_TS-1088R_3.9x3mm` | `SW601` a `SW603` | XunPu TS-1088R-02026 (C455280) | não: falta o êmbolo |
| `LED_RGB_3528_3.5x2.8mm` | `D601` | TUOZHAN S4-3528RGBTA-A (C2827321) | não: falta a lente |
| `SOT-523` | `Q401`, `Q601` a `Q603` | Diodes DMG1012T-7 (C20512) | não: faltam as pernas |
| `Buzzer_CPT-12123-81-SMT_12x12mm` | `LS601` | Same Sky CPT-12123-81-SMT-TR (C3812179) | quase: falta o furo de som |
| `Antena_Unictron_H2UJ4U1H2Q0100_5x3mm` | `E301` | Unictron H2UJ4U1H2Q0100 (C6569550) | sim: é um bloco de cerâmica |
| `MinewSemi_ME54BS13_16.5x12mm` | `U201` | MinewSemi ME54BS13 | sim: desenhado das cotas da ficha V1.0.0, com a blindagem e a ponta da antena aberta; a MinewSemi não publica STEP |
| `u-blox_MAX_LCC-18_9.7x10.1mm` | `U301` | u-blox MAX-F10S (C30005563) | sim: substrato e blindagem; a u-blox não publica STEP |
| `MAX17262_WLP-9_1.4x1.4mm_P0.4mm` | `U102` | Analog MAX17262REWL+T (C5328777) | sim: CI em pastilha |
| `BMP585_LGA-8_3.25x3.25mm` | `U502` | Bosch BMP585 (C18184976) | sim |
| `MMC5603_WLP-4_0.82x0.82mm` | `U504` | MEMSIC MMC5603NJ (C404328) | sim |
| `OPT3001_USON-6_2x2mm_P0.65mm` | `U505` | TI OPT3001DNPR (C90462) | sim |
| `TPD4E05U06_USON-10_1x2.5mm_P0.5mm` | `D102` | TI TPD4E05U06DQAR (C138714) | sim |
| `Texas_X2SON-4_1x1mm_P0.65mm` | `U104` | TI TPS7A0218PDQNR (C2862166) | sim |
| `L_Murata_DFE201610P` | `L101`, `L102`, `L401` | Murata DFE201610E-2R2M=P2 (C337891) | sim |
| `Texas_DSBGA-6_0.855x1.255mm_Layout2x3_P0.4mm_LevelB` | `U402` | TI TPS610997YFFR (C2072359) | sim |

O `L401` e o `U402` são da montagem da Sharp e não aparecem nas imagens, que
mostram a da JDI.

## Prioridade, se for baixar

Valem mais os três Molex da tela, as teclas, o LED RGB, os MOSFETs em SOT-523
e o buzzer: neles a caixa fica longe da peça de verdade. Os CI, os módulos, a
antena e os indutores são blocos também na vida real, e a caixa com a medida
da ficha já mostra o que importa: o contorno e a altura que a tampa tem de
acomodar.
