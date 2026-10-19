# A caixa

A proposta de caixa em volta da placa real, desenhada por programa e medida
por um dry run próprio. **Nada foi impresso.** As decisões, o que cada regra
mede e o que ficou para o dono estão em
[04 · Placa e caixa](../04-pcb-e-caixa.md#a-placa-de-hoje-dentro-da-caixa-do-conceito);
o que ela mudou na placa, em
[10 · Dry-run de 2026-09-26](../10-dry-run-2026-09-26.md#adendo-da-tarde-a-caixa-medida-e-o-que-ela-mudou-na-placa).

![A caixa aberta, com a placa, o display e a célula dentro](../../docs/img/hardware/gnssbike-3d-caixa-aberta.png)

| Arquivo | O que é |
|---|---|
| [`make_caixa.py`](make_caixa.py) | desenha a caixa em volta de `cad/gnssbike.kicad_pcb` (o courtyard, a altura e a face de cada peça, e o GLB da placa): o PDF, as três vistas 3D e os STL |
| [`dry_run_caixa.py`](dry_run_caixa.py) | mede a mesma `Caixa` com 13 regras: folga sob a tampa e sobre a célula, o teto do display, capas sobre as chaves, janela e vidro, entalhe e porta do USB-C, bossas e pilares, bossas da tampa contra as antenas, janelas do LED e do sensor, furos do buzzer e do barômetro, bolsos, coberturas e fendas de fio dos módulos, o chanfro assentando na parede e a folga da face interna dele, o berço da antena externa; uma regra que não acha o que medir **falha** |
| [`gnssbike-caixa.pdf`](gnssbike-caixa.pdf) | três páginas: frente com a tampa e a porta, por dentro com o berço, cortes A-A e B-B, a tecla e o bolso de um módulo, as premissas, o que não bate e as peças |
| `caixa-concha.stl`, `caixa-tampa.stl` | a concha e a tampa (PETG ou ASA, 0,2 mm; a tampa de cabeça para baixo, com suporte sob os chanfros) |
| `caixa-tecla-1.stl`, `-2`, `-3` | as três capas das teclas, cada uma com a sua aba |
| `caixa-membrana-teclas.stl` | a membrana de TPU de 0,3 mm sobre as teclas |
| `caixa-cobertura-faceta.stl`, `caixa-cobertura-chanfro.stl` | as coberturas transparentes dos módulos solares (a do chanfro, quatro vezes) |
| `caixa-porta-usb.stl` | a porta do USB-C (o pino de ø1,5 à parte) |

Os STL são sopas de triângulos de caixas sobrepostas, que o fatiador une: servem
para a primeira prova impressa, não para usinar. As vistas 3D ficam em
[`docs/img/hardware/`](../../docs/img/hardware/) (`gnssbike-3d-caixa-aberta.png`,
`-frente.png`, `-explodida.png`).

## Como rodar

Depois da cadeia da placa ([`cad/README.md`](../cad/README.md#verificação)),
porque os dois leem a placa colocada e o `make_caixa.py` precisa do GLB que o
`dry_run_pcb.py` exporta:

```sh
python hardware_gnssbike/caixa/dry_run_caixa.py   # 13 regras; código 1 se alguma falhar
python hardware_gnssbike/caixa/make_caixa.py      # PDF e STL aqui, vistas em docs/img/hardware/
```

Resultado de 2026-10-19, com a placa daquele dia: **12 regras medidas e
cumpridas, 0 violadas, 1 não medida** (a passagem dos fios dos módulos, as
juntas, o aperto das capas e os parafusos auto-atarraxantes só uma prova
impressa mede). A revisão parte por parte, com o que mudou e o que fica para
o dono decidir (a vedação do USB-C, dos furos do fundo e da linha de
partição), está em [12](../12-revisao-da-caixa.md).

## O que a caixa é

| Item | Valor |
|---|---|
| Caixa | 64 × 106 × 18,2 mm, raio 7, paredes 2, fundo e tampa 1,5. Os bolsos dos módulos solares na tolerância da ficha pedem o milímetro a mais desde 2026-10-19 ([12](../12-revisao-da-caixa.md#1--painéis-solares)). Era 62 × 106 × 17 até 2026-10-18: a JDI tem 2,2 mm de espessura e não 1,0, e a união dos contornos das duas telas (43,02 mm) precisa caber entre os dois chanfros com 0,5 mm de parede atrás de cada bolso de módulo |
| Placa | 34 × 95, centrada, a 0,5 mm da parede de baixo; duas bossas M2 e quatro pilares |
| Pilha | célula colada no fundo entre nervuras (z 1,8 a 8,8); 1,2 mm de peças do verso; placa em z 10,5 a 11,3; 3,0 mm sob o display; display em z 14,3 a 16,5, colado sob a tampa em volta da janela |
| Telas | a JDI LPM027M128C **ou** a Sharp LS027B7DH01A, nua ou como o módulo Azumo 11103-06, no mesmo `J401` da placa ([11](../11-revisao-do-esquematico.md)). As áreas ativas coincidem, e a janela fica centrada nelas, 1,67 mm à direita do meio da placa; o bolso é a união dos contornos, 43,02 × 64,57 mm. A barra de trás do Azumo desce a 0,59 mm da placa, e o CX3 confere que não há peça sob ela |
| Teclas | três capas de 5 em furos de 5,6, exatamente sobre as chaves da placa (passo 13,4), sob membrana de TPU **natural, translúcida**: as janelas do LED RGB (`D601`) e dos LEDs de carga (`D103`, `D104`, entre a primeira e a segunda tecla) ficam no rebaixo da membrana, e desde 2026-10-18 são cortadas também no fundo dele, que antes as fechava. Cada tecla para 0,15 mm abaixo da membrana (o curso da chave é de 0,2 ± 0,1 mm); a membrana é filme cortado, não impresso |
| Painéis | dois módulos de 23 × 8 na faceta abaixo das teclas e dois em cada chanfro de 45° e 7,5 mm, sob coberturas de PC ou PET de 0,5 mm coladas; bolsos de 23,8 × 8,6 × 2,35 mm, pela tolerância de ±0,3 da ficha e a fita de 0,15; passagens de fio sob os terminais |
| Sensor de luz | janela de 3,0 mm vedada por um disco transparente num rebaixo de 5,0 × 0,6, fora da membrana e da faceta |
| USB-C | entalhe na parede de baixo e na aba da tampa, com porta articulada por fora |
| Antena GNSS externa | berço com duas nervuras e lábio contra a parede de cima, para uma patch de 12 × 12 × 4 em pé (opção) |
| Tampa | quatro parafusos M2 nos cantos |
