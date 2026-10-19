# Ferramentas da revisão do esquemático

Scripts que preparam o material de uma revisão do esquemático da placa nova e mantêm o registro dos achados. A revisão em si é feita por agentes, folha por folha e peça por peça, a partir deste material. O método e os scripts vêm do projeto `35.esp32-s3_power_meter`, do mesmo autor (lá sob MIT), adaptados ao KiCad 8 e à cadeia de `hardware_gnssbike/cad/`.

## Uso

```sh
python tools/review/prepare.py            # cria .claude/reviews/<data>-esquematico/
python tools/review/prepare.py <pasta>    # ou numa pasta escolhida
```

O `prepare.py` não muda nada no repositório: copia o projeto do KiCad para a pasta da revisão e roda o `kicad-cli` sobre a cópia. A pasta `.claude/reviews/` fica fora do git. O resultado final da revisão vai para a documentação de `hardware_gnssbike/`. Rode de novo depois de mudar o esquemático: tudo é refeito, menos o que os revisores escreveram.

| Script | O que faz |
|---|---|
| `prepare.py` | copia o projeto e as fontes do gerador; exporta o netlist em XML, o PDF e uma imagem por página; roda o ERC com todas as severidades e o DRC com paridade de esquemático; chama os outros scripts; copia as fichas de `hardware_gnssbike/datasheets/` |
| `digest.py` | transforma o netlist em resumos: um por folha (peças, ligações pino a pino, notas), um por tipo de peça, a lista de redes, a lista de materiais e o `lint.md` (pinos sem ligação, redes sem quem as acione, pinos de alimentação de cada CI com os capacitores da rede) |
| `extras.py` | escreve `placement.csv` (posição de cada peça na placa) e `parts.csv` (número da LCSC e se a pinagem foi lida na ficha, de `parts.py`) |
| `known.py` | junta os achados de `out/findings/*.json` e os vereditos num registro com identificador estável (`register.json`) e numa lista curta (`known-findings.md`), para que nenhuma rodada repita o que já está registrado |
| `finding.py` | imprime achados do registro pelo identificador |
| `pdftool.py` | lê ficha em PDF: informações, texto com marcas de página, busca por expressão regular, página ou recorte em PNG |
| `spice.py` | roda um circuito no ngspice que vem com o KiCad (`KICAD_ROOT`, padrão `D:/KiCAD`) |

## Pasta de uma revisão

| Caminho | Conteúdo |
|---|---|
| `kicad/project/`, `kicad/generator/` | cópia do projeto do KiCad e das fontes do gerador (`parts.py`, `nets.py`, `footprints.py`, `blocos.py`, `sheets.py`) |
| `kicad/netlist.xml`, `schematic.pdf`, `pages/` | o netlist que vale para a conectividade, o PDF e as páginas em PNG |
| `kicad/erc.json`, `erc.rpt`, `drc.json` | ERC com todas as severidades e DRC com paridade de esquemático |
| `out/sheets/`, `out/parts/`, `nets.md`, `bom.md`, `lint.md`, `components.csv`, `placement.csv`, `parts.csv` | os resumos |
| `out/cards/`, `out/blocks/`, `out/findings/`, `out/verdicts/` | o que os revisores escrevem: cartão de cada peça, relatório de cada folha e cenário, achados e vereditos |
| `ds/_repo/`, `ds/<chave>/` | as fichas do repositório (só leitura) e as que os revisores baixam |

## Dependências

Python 3.11 do sistema com PyMuPDF (já instalado), `kicad-cli` do KiCad 8 (`KICAD_CLI`, padrão `D:\KiCAD\bin\kicad-cli.exe`) e o `ngspice.dll` do KiCad.
