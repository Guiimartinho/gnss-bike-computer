# Placa e caixa

O contorno, a espessura, as camadas, onde cada bloco fica e onde nada pode
ficar. Fecha o que as [folhas do esquemático](01-esquematico.md) deixam em
aberto por natureza: um esquemático diz o que se liga a quê, e é o layout
que decide se a antena
enxerga o céu, se o harmônico da flash cai dentro de L1 e se a placa cabe na
caixa. As posições vêm da tabela de zonas de [`docs/14`](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso)
e do desenho da caixa em [`tools/docs/case_drawing.py`](../tools/docs/case_drawing.py),
que gera [`docs/img/placa-nova-caixa.svg`](../docs/img/placa-nova-caixa.svg).
Conta feita aqui vem marcada como **conta**; número de ficha ou de desenho
vem marcado como **ficha** ou **desenho**.

**Nesta página:** [O contorno](#o-contorno) · [Orçamento de área](#orçamento-de-área) · [Camadas](#camadas) · [Posicionamento](#posicionamento) · [Zonas proibidas](#zonas-proibidas) · [As duas antenas](#as-duas-antenas) · [Montagem](#montagem) · [O que só a bancada decide](#o-que-só-a-bancada-decide)

> [!WARNING]
> **Não existe placa, não existe layout e nada foi fabricado nem medido.**
> Não há arquivo de CAD, não há gerber, não há pilha de camadas de
> fabricante, nenhuma peça foi colocada e nenhuma trilha foi roteada. O que
> segue é o conjunto de restrições que o layout terá de respeitar, e as
> contas que mostram quais delas ainda não fecham.

## O contorno

| Item | Medida | Origem |
|---|---|---|
| Caixa | 62 × 104 × 19 mm, mais 3 mm do engate de quarto de volta | [`case_drawing.py`](../tools/docs/case_drawing.py) (`W, H, T`, `MOUNT`) |
| Raio de canto da caixa | 7 mm | idem (`R`) |
| PCB | **55 × 97 mm**, espessura de 0,8 mm | desenho (`rect(3.5, 3.5, W-7, H-7)`) e [14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso) |
| Raio de canto da placa | **3 mm no desenho, 4 mm em [14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso)** | as duas fontes discordam, ver abaixo |
| Recuo da placa em relação à caixa | 3,5 mm em cada lado | **conta**: (62 − 55) ÷ 2 = 3,5 e (104 − 97) ÷ 2 = 3,5 |
| Caixa da V3, para comparar | cerca de 60 × 85 mm | legenda do desenho |
| Placa da V3, para comparar | 52,35 × 77,47 mm | [13](../docs/13-placa-nova.md#o-que-cabe-numa-caixa-pequena) |

A placa nova tem **31,6 % mais área** que a da V3 (**conta**: 5.335 ÷ 4.055,6).

> [!IMPORTANT]
> **O raio de canto da placa não bate entre as duas fontes.** O desenho da
> caixa traça a placa com **3 mm** de raio — `xv.rect(3.5, 3.5, W - 7, H - 7, 3, …)`
> em [`case_drawing.py:339`](../tools/docs/case_drawing.py), onde o quarto
> argumento é o raio —, e [14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso)
> escreve "cantos com raio de 4 mm". Um milímetro num canto não muda nada
> elétrico, mas muda o contorno que vai para o fabricante e a folga contra
> a parede de 7 mm de raio da caixa. **Qual dos dois vale ainda não foi
> decidido**, e esta página não escolhe por ninguém: registra os dois.

**Sistema de coordenadas** desta página, o mesmo de [14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso):
origem no canto de cima à esquerda da **placa**, vista pela frente, x para a
direita (0 a 55 mm) e y para baixo (0 a 97 mm). O desenho da caixa usa
coordenadas da **caixa**; a conversão é `x_placa = x_caixa − 3,5` e
`y_placa = y_caixa − 3,5`.

> [!IMPORTANT]
> **A folga entre a placa e a caixa não bate com o texto de [14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso).**
> Lá está escrito "paredes de cerca de 2 mm e folga de 0,5 mm", que somam
> 2,5 mm por lado e dariam uma placa de 57 × 99 mm. O desenho põe a placa
> com 3,5 mm de recuo, o que com parede de 2 mm deixa **1,5 mm de folga
> mecânica por lado**, não 0,5 mm (**conta**: 3,5 − 2,0). Um milímetro por
> lado é a diferença entre apertar e sobrar; **qual dos dois vale é decisão
> do projeto mecânico, que não existe.** Esta página segue o desenho, que é
> executável, e registra a divergência.

## Orçamento de área

A placa tem **5.335 mm²** (**conta**: 55 × 97). A pergunta é se o que
precisa entrar cabe, e a resposta tem duas partes: a área de planta que as
zonas consomem, e a área que fica com **restrição de altura** por causa do
display na frente e da bateria atrás.

### O que cada zona consome em planta

Áreas calculadas a partir dos retângulos de [14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso).
Todas são **conta**, menos a linha marcada como **falta**, que não tem
retângulo em documento nenhum.

| Zona | Retângulo (x, y em mm) | Lados | Área (mm²) | % dos 5.335 |
|---|---|---|---|---|
| Antena GNSS (área livre) | 0–55, 0–8 | 55 × 8 | 440,0 | 8,2 % |
| GNSS (MAX-F10S e blindagem) | 20–35, 2–16 | 15 × 14 | 210,0 | 3,9 % |
| LED RGB | 50–53, 0–3 | 3 × 3 | 9,0 | 0,2 % |
| IMU e magnetômetro | 8–15, 20–23 | 7 × 3 | 21,0 | 0,4 % |
| FPC do display | 3,7–7,1, 29,5–39,5 | 3,4 × 10 | 34,0 | 0,6 % |
| Conector do filme de luz (Molex 5034800440) | **sem retângulo em [14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso)** | — | **falta** | — |
| Buzzer | 10,5–21,5, 46,5–55,5 | 11 × 9 | 99,0 | 1,9 % |
| Energia (nPM1300, AEM10900, MAX17262, indutores, conectores) | 16–38, 67–90 | 22 × 23 | 506,0 | 9,5 % |
| Armazenamento (flash NOR) | 0–14, 67–83 | 14 × 16 | 224,0 | 4,2 % |
| Barômetro | 4,5–8, 83–86,5 | 3,5 × 3,5 | 12,3 | 0,2 % |
| BM20C **mais a zona proibida da sua antena** | 45–55, 71,5–87,7 unido a 40–55, 82–91 | ver a conta abaixo | 240,0 | 4,5 % |
| Botões | 8–47, 88–95 | 39 × 7 | 273,0 | 5,1 % |
| USB-C | 23–32, 94–97 | 9 × 3 | 27,0 | 0,5 % |
| Luz ambiente (OPT3001) | 0,4–2,4, 88–90 | 2 × 2 | 4,0 | 0,1 % |
| **Soma bruta** | | | **2.099,3** | **39,3 %** |
| Sobreposições contadas duas vezes | | | −120,0 | −2,2 % |
| **Soma líquida** | | | **1.979,3** | **37,1 %** |

O BM20C entra com a **zona proibida**, não só com o retângulo do módulo: é
a zona que proíbe cobre, e é ela que o layout tem de respeitar
([Zonas proibidas](#zonas-proibidas)). Ela passa 5 mm à esquerda do módulo,
e por isso a área é a **união** dos dois retângulos (**conta**):

```
módulo:         x 45 a 55, y 71,5 a 87,7  = 10   × 16,2 = 162,0 mm²
zona proibida:  x 40 a 55, y 82   a 91    = 15   ×  9   = 135,0 mm²
comum aos dois: x 45 a 55, y 82   a 87,7  = 10   ×  5,7 =  57,0 mm²
união = 162,0 + 135,0 − 57,0 = 240,0 mm²        (240,0 ÷ 5.335 = 4,5 %)
```

A soma bruta fecha (**conta**): 440,0 + 210,0 + 9,0 + 21,0 + 34,0 + 99,0 +
506,0 + 224,0 + 12,3 + 240,0 + 273,0 + 27,0 + 4,0 = **2.099,3 mm²**.

As três sobreposições (**conta**) são exatamente os três pontos que o
layout terá de resolver:

| Sobreposição | Retângulo comum | Área |
|---|---|---|
| GNSS dentro da área livre da antena | 20–35, 2–8 | 15 × 6 = 90 mm² |
| LED RGB dentro da área livre da antena | 50–53, 0–3 | 3 × 3 = 9 mm² |
| Botão da direita dentro da **zona proibida** do BM20C | 40–47, 88–91 | 7 × 3 = **21 mm²** |

A terceira sai de cruzar a faixa dos botões com a zona proibida inteira, e
não com o retângulo do módulo (**conta**):

```
botões:        x  8 a 47, y 88 a 95
zona proibida: x 40 a 55, y 82 a 91
em x: de max(40; 8) = 40 a min(55; 47) = 47, ou seja 7 mm
em y: de max(88; 82) = 88 a min(95; 91) = 91, ou seja 3 mm
área = 7 × 3 = 21 mm²
```

Total das sobreposições: 90 + 9 + 21 = **120 mm²**.

**Sobram 3.355,7 mm², 62,9 % da placa** (**conta**: 5.335 − 1.979,3), para
os passivos espalhados, as vias, os furos M2, os pontos de teste, o
footprint Tag-Connect TC2030-NL, os dois pads do console (`uart20`, `TP201` e
`TP202` de [06](06-conectores-e-pontos-de-teste.md#pontos-de-teste)) e as trilhas. **Pela planta,
fecha com folga.**

> [!NOTE]
> Os 6 módulos solares de 23 × 8 mm **não consomem área de placa**: ficam na
> face inclinada da caixa e nos dois chanfros de 45°, e chegam à placa por
> pads de mola ou FPC em três grupos. A antena GNSS também não: é um
> elemento na parede da caixa, com contatos de mola. **A área dos contatos e
> dos conectores dos painéis não está dimensionada em lugar nenhum** e não
> entra na soma acima — é um número que falta.

> [!IMPORTANT]
> **O conector do filme de luz não tem zona nem área.**
> [14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso) manda
> pôr o Molex 5034800440, de 4 vias e passo de 0,5 mm, "ao lado" do FPC do
> display na borda esquerda, mas a tabela de zonas de lá **não lhe dá
> retângulo**, e a ficha dele não está resumida em nenhum documento deste
> repositório: não há como calcular a área aqui sem inventar uma medida.
> Ele só existe na montagem com a Sharp
> ([05](05-materiais.md#folha-4--display)), o que não o torna opcional no
> layout, porque a placa tem de aceitar as duas montagens. **É um número
> que falta**, e a soma acima está subestimada por ele.

### As duas sombras: display e bateria

O display fica sobre a face da frente e a bateria, sob a face de trás. Não
competem por área de planta, mas **limitam a altura** das peças embaixo
delas: até 2,6 mm sob o display e até 1,2 mm sob a bateria
([14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso)).

| Sombra | Retângulo (coordenadas da placa) | Área (mm²) | % dos 5.335 | Altura permitida |
|---|---|---|---|---|
| Display JDI LPM027M128B (contorno 40,08 × 61,8) | 7,46–47,54, 5,1–66,9 | 2.476,9 | 46,4 % | 2,6 mm |
| Display Sharp LS027B7DH01A (contorno 42,82 × 62,8) | — | 2.689,1 | 50,4 % | 2,6 mm |
| Área ativa (35,28 × 58,8), só para referência | 9,86–45,14, 6,6–65,4 | 2.074,5 | 38,9 % | — |
| Bateria LiPo (36 × 60), na face de trás | 9,5–45,5, 22,5–82,5 | 2.160,0 | 40,5 % | 1,2 mm |

Contas de conferência, com o JDI:

```
interseção das duas sombras = 36,0 mm × 44,4 mm = 1.598,4 mm²  (30,0 %)
união das duas sombras = 2.476,9 + 2.160,0 − 1.598,4 = 3.038,5 mm²  (57,0 %)
área sem restrição de altura dos dois lados = 5.335 − 3.038,5 = 2.296,5 mm²  (43,0 %)
```

Ou seja: **30 % da placa tem teto de 2,6 mm na frente e de 1,2 mm atrás ao
mesmo tempo**, e só 43 % está livre das duas. É apertado, mas o inventário
de peças altas é curto: o módulo GNSS (2,5 mm, sob o display, dentro do
limite de 2,6 mm), o BM20C (2,0 mm, fora das duas sombras), os dois
indutores dos bucks em 0806 e o do AEM10900, o conector USB-C e o soquete
FPC — todos na faixa de baixo ou na borda, fora da sombra da bateria.

> [!CAUTION]
> **O BM20C não pode ir para a face de trás onde está desenhado.** Ele
> ocupa x 45 a 55 e a sombra da bateria vai até x 45,5: são
> **0,5 × 11 = 5,5 mm² de sobreposição** (**conta**, com y 71,5 a 82,5),
> onde o teto é 1,2 mm e o módulo tem 2,0 mm de altura. Duas saídas, as
> duas de layout: o módulo vai na **face da frente** (o que também decide a
> ordem do forno, ver [Montagem](#montagem)), ou o módulo anda 0,5 mm para
> a direita e encosta na borda. Não medido: a folga real depende do pack de
> bateria comprado.

### O aperto que não fecha: a antena GNSS

Entre a borda de cima da placa e o contorno do display sobram **5,1 mm**
(**conta**: 8,6 − 3,5, do desenho), e a zona da antena de
[14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso) reserva
8 mm. A peça do protótipo, a **TE L000670, mede 14 × 10,75 × 1 mm**
([15](../docs/15-avaliacao-componentes.md#antena-gnss)).

```
na orientação mais favorável, a peça pede 10,75 mm de profundidade
contra a zona reservada de 8 mm    faltam 10,75 − 8,00 = 2,75 mm
contra a faixa livre de 5,1 mm     faltam 10,75 − 5,10 = 5,65 mm
```

Os 2,75 mm que faltam contra a zona já são um problema; os 5,65 mm contra a
faixa livre são pior, porque o que sobra do outro lado **fica sob o
display**, e [13](../docs/13-placa-nova.md#o-que-cabe-numa-caixa-pequena) é
explícito: "Debaixo do LCD não", porque o LCD deforma o diagrama da antena e
emite ruído de banda larga. Três saídas, nenhuma decidida:

1. **Antena na parede da caixa**, com contatos de mola (a opção A de
   [13](../docs/13-placa-nova.md#o-que-cabe-numa-caixa-pequena), que é o que
   Garmin, COROS e Wahoo fazem): a peça sai da placa e o aperto some. Pede
   fornecedor de antena sob medida, que ainda não existe
   ([14](../docs/14-hardware-placa-nova.md#pendências)).
2. **Display mais para baixo**, roubando dos botões: cada milímetro de
   descida é um milímetro a menos na faixa de baixo, que já tem os botões
   (y 88 a 95) e o USB-C (y 94 a 97).
3. **Caixa mais alta**, que é o que [13](../docs/13-placa-nova.md#o-que-cabe-numa-caixa-pequena)
   já previa como consequência da antena.

Com a Sharp, que é 1,0 mm mais alta que o JDI (**conta**: 62,8 − 61,8), a
faixa livre cai para cerca de **4,6 mm** se o centro for mantido — o aperto
piora.

## Camadas

**4 camadas, 0,8 mm**, com controle de impedância, como já registrado em
[14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso).

```mermaid
flowchart TB
    L1["L1 · frente<br/>peças da face do display, sinais rápidos e curtos,<br/>rede em π da antena GNSS, linha de 50 Ω até o elemento"]
    L2["L2 · GND sólido<br/>referência de L1 e de L3, e o plano que a antena usa para irradiar"]
    L3["L3 · alimentação e sinais lentos<br/>3V0 e 1V8 em áreas; display e USB entre L2 e o cobre de L4"]
    L4["L4 · trás<br/>peças da face da bateria, sinais, cobre de retorno"]
    L1 --- L2 --- L3 --- L4
```

### Por que 4 e não 2

| Motivo | O número por trás |
|---|---|
| **O plano de terra é a antena.** Numa antena linear quem irradia é o plano; [13](../docs/13-placa-nova.md#o-que-cabe-numa-caixa-pequena) traz o mesmo chip com 43,4 dB-Hz num plano de 80 × 40 mm e **34,7 dB-Hz** num de 24 × 15 mm | 8,7 dB-Hz de diferença, ficha |
| Em 2 camadas o plano da face de baixo é rasgado pelo roteamento: os 31 sinais de [03](03-netlist.md#nós-do-mcu) mais os 12 nós de alimentação teriam de passar por ali | 31 e 12, do netlist |
| **Camada interna para display e USB.** [13](../docs/13-placa-nova.md#regras-de-projeto) e [14](../docs/14-hardware-placa-nova.md#regras-de-layout) pedem essas linhas entre planos de terra; em 2 camadas não existe camada interna | — |
| **Referência contínua para as duas impedâncias controladas:** 50 Ω do `RF_IN` do MAX-F10S até o elemento e 90 Ω diferencial do par `USB_DM`/`USB_DP` | [03](03-netlist.md#dedicados-do-módulo) |
| **Área.** 46,4 % da frente está sob o display e 40,5 % da traseira sob a bateria: sobram poucas regiões boas para peças, e o roteamento tem de descer | **conta**, acima |
| [13](../docs/13-placa-nova.md#regras-de-projeto), regra 6, já manda "placa de 4 camadas, com planos sólidos e moldura de terra com vias" | — |

A linha de 50 Ω fica curta por construção: a zona do GNSS começa em y 2 e a
zona da antena vai de y 0 a 8, de modo que o percurso é de **poucos
milímetros**. O comprimento exato não existe até o footprint do MAX estar
posicionado e o pino `RF_IN` ter coordenada.

### 0,8 mm, e a tensão que isso cria

A espessura de 0,8 mm **não é escolha de layout**: o desenho do receptáculo
USB-C Molex 2036150003 a recomenda, e a especificação do projeto já desceu
de 1,0 para 0,8 mm por causa dele ([15](../docs/15-avaliacao-componentes.md#usb-c-e-proteção)).
Com 4 camadas dentro de 0,8 mm, os dielétricos ficam finos:

```
cobre: 4 folhas de 35 µm (1 oz) = 0,14 mm
dielétrico disponível = 0,80 − 0,14 = 0,66 mm, repartido em 3 vãos
repartição uniforme = 0,66 ÷ 3 = 0,22 mm por vão
```

E aqui a conta para. A largura de uma trilha de 50 Ω em L1 depende do vão
**L1–L2**, e as duas repartições plausíveis de 0,66 mm são muito diferentes:
uma pilha uniforme dá 0,22 mm de vão, e uma pilha com prepreg fino por fora
e núcleo grosso no meio dá algo como 0,10 mm fora e 0,46 mm no meio. **A
largura de 50 Ω e a de 90 Ω diferencial não existem como número até o
fabricante mandar a pilha dele**, e [02](02-calculos.md#o-que-não-foi-calculado)
já registra as duas como não calculadas.

> [!IMPORTANT]
> **Confirmar com o fabricante, antes do layout:** que ele fabrica 4 camadas
> em 0,8 mm acabados, com que repartição de dielétrico, com que cobre
> (35 µm ou 18 µm) e com que largura de trilha para 50 Ω e 90 Ω
> diferencial nessa pilha. Sem esses quatro números o roteamento de RF e de
> USB não começa. **Nenhum fabricante foi consultado.**

Uma placa de 0,8 mm também **empena mais** que uma de 1,6 mm no forno e sob
o calor do carregador, que dissipa até 1,50 W a poucos milímetros da célula
([02](02-calculos.md#calor-do-carregador)). Não medido.

## Posicionamento

Os retângulos abaixo são os de [14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso),
nas coordenadas da placa, e batem com o desenho da caixa. As três faixas de
y correspondem à borda de cima (antena e GNSS), ao meio (sob o display, com
a bateria atrás) e à base (energia, rádio, botões e USB).

```mermaid
flowchart TB
    subgraph PCB["PCB 55 × 97 mm · vista pela frente · origem no canto de cima à esquerda"]
        direction TB
        subgraph Y1["y 0 a 20 · borda de cima"]
            direction LR
            A1["área livre da antena GNSS<br/>x 0 a 55 · y 0 a 8<br/>sem cobre em nenhuma camada"]
            A2["MAX-F10S sob blindagem<br/>x 20 a 35 · y 2 a 16<br/>frente, até 2,6 mm"]
            A3["LED RGB<br/>x 50 a 53 · y 0 a 3"]
        end
        subgraph Y2["y 20 a 67 · meio, sob o display"]
            direction LR
            B1["FPC do display, 10 vias<br/>x 3,7 a 7,1 · y 29,5 a 39,5<br/>sai pela esquerda"]
            B2["BMI270 e MMC5633NJL<br/>x 8 a 15 · y 20 a 23"]
            B3["buzzer piezo<br/>x 10,5 a 21,5 · y 46,5 a 55,5"]
            B4["LiPo 36 × 60 × 7 mm, atrás<br/>x 9,5 a 45,5 · y 22,5 a 82,5<br/>teto de 1,2 mm embaixo"]
        end
        subgraph Y3["y 67 a 97 · base"]
            direction LR
            C1["flash MX25R6435F<br/>x 0 a 14 · y 67 a 83"]
            C2["energia: nPM1300, MAX17262,<br/>AEM10900, indutores<br/>x 16 a 38 · y 67 a 90"]
            C3["BM20C 10,0 × 16,2 × 2 mm<br/>x 45 a 55 · y 71,5 a 87,7<br/>antena nos últimos 5,5 mm"]
            C4["BMP585 no respiro, atrás<br/>x 4,5 a 8 · y 83 a 86,5"]
            C5["3 teclas Omron B3S-1002P<br/>6 × 6 × 4,3 mm<br/>x 8 a 47 · y 88 a 95"]
            C6["USB-C IPX8<br/>x 23 a 32 · y 94 a 97"]
            C7["OPT3001<br/>x 0,4 a 2,4 · y 88 a 90"]
        end
    end
    ANT["elemento linear L1 e L5<br/>na parede de cima da caixa,<br/>fora da placa, por contatos de mola"] -.->|"linha de 50 Ω, poucos mm"| A2
    Y1 --> Y2 --> Y3
    PV["6 módulos solares 23 × 8 mm<br/>2 na face inclinada e 2 em cada chanfro,<br/>na caixa, fora da placa"] -.->|"3 grupos, mola ou FPC"| C2
```

O que o arranjo garante, e por quê:

| Escolha | Razão |
|---|---|
| GNSS e sua antena na borda de **cima**, rádio no canto de **baixo à direita** | é a borda que aponta para o céu com o aparelho inclinado no guidão, e põe as duas antenas em pontas opostas ([13](../docs/13-placa-nova.md#antena-gnss-dentro-da-caixa)) |
| FPC do display pela **esquerda** | longe das duas antenas ([14](../docs/14-hardware-placa-nova.md#regras-de-layout)); as 10 vias estão na [folha 4](01-esquematico.md#folha-4--display) |
| Energia (bucks e boost) na base, na diagonal da antena GNSS | laços de chaveamento longe da antena ([13](../docs/13-placa-nova.md#regras-de-projeto), regra 5) |
| BMP585 na face de trás, junto do respiro, em y 83 a 86,5 | fora da sombra da bateria, que termina em y 82,5 (**conta**) |
| OPT3001 embaixo à esquerda | sob a janela de luz ambiente, longe das antenas |
| USB-C no meio da borda de baixo | o anel veda contra a parede, sem tampa |

## Zonas proibidas

Quatro zonas, com quatro proibições diferentes. **Nenhuma foi verificada em
cobre.**

| Zona | Retângulo (placa) | Área | O que é proibido | Origem |
|---|---|---|---|---|
| **Antena do BM20C** | x 40–55, y 82–91 | 135 mm² (**conta**: 15 × 9) | cobre, trilha, plano e via **em todas as camadas**; nenhum componente | ficha Fanstel (p. 17), via [14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso) |
| **Antena GNSS** | x 0–55, y 0–8 | 440 mm² (**conta**) | cobre em todas as camadas sob a antena; nada metálico mais alto que 3 mm num raio de 10 mm; a caixa a 3 a 5 mm | [13](../docs/13-placa-nova.md#regras-de-projeto), [14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso) |
| **Sombra da bateria** | x 9,5–45,5, y 22,5–82,5 | 2.160 mm² (**conta**) | peças acima de 1,2 mm na face de trás; e a bolsa metálica não pode ficar entre a antena GNSS e o céu | [13](../docs/13-placa-nova.md#regras-de-projeto), [14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso) |
| **Conector USB-C** | x 23–32, y 94–97 | 27 mm² (**conta**) | o anel de vedação passa da borda da placa; nada entre o corpo do conector e a parede | desenho Molex 2036150003 |

### A zona do BM20C, em detalhe

O módulo tem **10,0 × 16,2 × 2 mm** e **os últimos 5,5 mm são a área da
antena**. A ficha da Fanstel (p. 17) pede que essa área fique **fora da
placa ou numa região sem terra e sem trilhas em todas as camadas**, com
cerca de 5 mm livres para o lado, que o módulo **nunca** fique no meio da
placa e que metal externo fique a pelo menos 30 mm para o melhor alcance.

```
módulo:              x 45 a 55, y 71,5 a 87,7   (16,2 mm de comprimento)
área da antena:      x 45 a 55, y 82,2 a 87,7   (os últimos 5,5 mm)
zona proibida:       x 40 a 55, y 82   a 91     (a área da antena, mais 5 mm
                                                 para o lado e a folga de 14)
```

Os 30 mm de metal externo **não são cumpridos** e
[14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso) já
registra por quem: a bateria, o botão da direita e o parafuso M2 de baixo à
direita. O parafuso é o único que sai de graça — basta movê-lo para fora da
zona, como o próprio documento manda. A faixa dos botões invade **21 mm²**
da zona (**conta**: x de 40 a 47 e y de 88 a 91, 7 × 3, no
[orçamento de área](#orçamento-de-área)) — é o botão da direita, cujo
centro fica em x 42,5 — e a bateria fica a poucos milímetros atrás.
**Consequência medível: alcance menor do que o módulo promete.** Não
medido.

### A zona da antena GNSS, em detalhe

Além do cobre, três coisas não podem ficar entre a antena e o céu: a **bolsa
metálica da bateria**, os **parafusos** e o **painel solar**
([13](../docs/13-placa-nova.md#regras-de-projeto), regra 2). Na geometria
atual a bateria termina em y 22,5, a 14,5 mm da zona da antena (**conta**:
22,5 − 8), o que resolve a bateria. Os dois parafusos de cima estão em
(6,5; 12,5) e (55,5; 12,5) nas coordenadas da caixa, ou seja, (3,0; 9,0) e
(52,0; 9,0) na placa: **1 mm abaixo da zona**, com cabeça e ilha que
certamente entram nela. Os módulos solares mais altos são os dos chanfros
laterais, e começam em **y 21,5 na caixa**, ou **y 18 na placa**; a faceta
chanfrada que os recebe começa antes, em **y 17,5 na caixa**, ou **y 14 na
placa** ([`case_drawing.py`](../tools/docs/case_drawing.py), o bisel
`rect(0.6, 17.5, 6.2, 55.0, …)` e os módulos `module(fv, x0, 21.5, 5.5, 23.0, True)`):

```
módulo mais alto:  y 21,5 − 3,5 = 18,0 na placa, a 18,0 − 8 = 10,0 mm da zona
faceta chanfrada:  y 17,5 − 3,5 = 14,0 na placa, a 14,0 − 8 =  6,0 mm da zona
```

Uma versão anterior desta página dizia "y 18 na caixa (y 14,5 na placa)":
os 18 já eram a coordenada da **placa**, e o recuo de 3,5 mm foi descontado
duas vezes. Nenhum dos três foi checado contra o raio de 10 mm que a regra
pede.

### A zona do conector USB-C

O conector é o Molex 2036150003 da [folha 1](01-esquematico.md#folha-1--energia).
O desenho da Molex dá: **placa de 0,8 mm**, furo de **9,54 × 3,76 mm** numa
parede de **pelo menos 1,2 mm**, e a **borda da placa 2,73 mm atrás da
frente do conector**. Com o recuo de 3,5 mm:

```
folga da borda da placa à face externa da caixa = 3,5 mm
a boca do conector fica 3,5 − 2,73 = 0,77 mm atrás da face externa
com parede de 1,2 mm, o conector entra 2,73 − (3,5 − 1,2) = 0,43 mm no furo
com parede de 2,0 mm, o conector entra 2,73 − (3,5 − 2,0) = 1,23 mm no furo
```

Fecha nos dois casos (**conta**), com o anel apertado contra a face interna
da parede. O que sobra em aberto é o **plugue**: ele precisa vencer 0,77 mm
de furo antes de alcançar a boca do conector, e um plugue de capa grossa
pode não entrar. **Nenhum cabo foi provado contra nenhuma caixa.**

## As duas antenas

Uma placa com **duas antenas ligadas a rádios que trabalham ao mesmo
tempo** é o risco de RF deste projeto, e já está registrado como pendência
em [10](../docs/10-status-do-port.md), na
[proposta](../docs/13-placa-nova.md#riscos), nas
[pendências de 14](../docs/14-hardware-placa-nova.md#pendências) e na
[bancada antes do layout de 15](../docs/15-avaliacao-componentes.md#bancada-antes-do-layout).
O que este documento acrescenta é a distância que a geometria dá.

| Antena | Onde | Frequência |
|---|---|---|
| 2,4 GHz (BLE e ANT+) | no BM20C, últimos 5,5 mm, x 45–55, y 82–88 | 2,40 a 2,48 GHz |
| GNSS L1 e L5 | elementos na parede de cima, fora da placa, x 2,5–25 (L1) e x 30–52,5 (L5), cerca de 2,2 mm à frente da borda de cima ([folha 3](01-esquematico.md#folha-3--gnss)) | 1.575,42 MHz e 1.176,45 MHz |

Distância entre a antena do rádio e o ponto mais próximo do elemento de L5
(**conta**, do centro (50; 85) ao ponto (52,5; −2,2)):

```
d = raiz de ((52,5 − 50)² + (85 + 2,2)²) = raiz de (6,25 + 7.603,84) = 87,2 mm
```

Cerca de **90 mm em números redondos**, que é o máximo que uma placa de
55 × 97 mm permite: as duas antenas estão em pontas opostas da diagonal. Em
comprimentos de onda (**conta**, com c = 3 × 10⁸ m/s):

| Banda | λ | Distância |
|---|---|---|
| 2,44 GHz | 123,0 mm | 0,71 λ |
| L1, 1.575,42 MHz | 190,4 mm | 0,46 λ |

> [!CAUTION]
> **Menos de um comprimento de onda não é muita separação.**
> [13](../docs/13-placa-nova.md#regras-de-projeto) dá a faixa esperada num
> aparelho pequeno: **isolação de 6 a 20 dB**, o que com os +8 dBm do
> nRF54LM20A põe **de −12 a +2 dBm na entrada do GNSS**. A defesa deste
> projeto é o MAX-F10S ter SAW, LNA e SAW **dentro do módulo** (a entrada
> aguenta 0 dBm, e a ficha não abre exceção fora da banda). **A isolação
> desta placa não existe como número: ninguém mediu o S21, porque não há
> placa.** O ensaio já está previsto: S21 em 2,44 GHz e C/N0 com o rádio
> transmitindo, antes de ligar o rádio na potência cheia
> ([15](../docs/15-avaliacao-componentes.md#bancada-antes-do-layout)).

### O harmônico de 8 MHz da flash

[13](../docs/13-placa-nova.md#regras-de-projeto) escolheu as frequências do
barramento de armazenamento **de propósito** para fugir de L1: 16 ou
21,33 MHz, "nunca a 8 ou 25 MHz", porque os harmônicos de 1, 2, 4, 8 e
25 MHz caem a menos de 0,6 MHz do centro de L1. Quando o cartão saiu e a
flash NOR entrou, o devicetree ficou com
[`spi-max-frequency = <8000000>`](../zephyr_app/boards/gnss/gnssbike/)
(`gnssbike_nrf54lm20a_cpuapp.dts:463`) — **8 MHz, um dos que a regra
mandava evitar**. A conta confirma:

```
1.575,42 ÷ 8 = 196,93, e o harmônico vizinho é 197 × 8 = 1.576,00 MHz
distância ao centro de L1 = 1.576,00 − 1.575,42 = 0,58 MHz
```

O barramento sai da flash (x 0 a 14, y 67 a 83) e vai ao MCU; a antena está
na borda de cima. **Risco a mitigar no layout**, nas três frentes que
[13](../docs/13-placa-nova.md#regras-de-projeto) e
[14](../docs/14-hardware-placa-nova.md#regras-de-layout) já indicam:

1. **Borda lenta:** **33 Ω** em série no `NOR_SCK` e nos dados, junto do
   pino do MCU, para tirar energia dos harmônicos altos. A conta de
   [02](02-calculos.md#resistor-de-série-no-spi-da-flash) mostra que 33 Ω
   custa cerca de 1 % do meio período a 8 MHz, longe de atrapalhar o
   relógio, e que 100 Ω também caberiam. **O valor final sai de medida**,
   com o espectro na banda (`UBX-MON-SPAN`) e a flash trabalhando: a
   capacitância real da trilha ainda não existe.
2. **Trilha curta:** a flash está a cerca de **67 mm** da área livre da
   antena (**conta**: do meio da zona de armazenamento, y 75, à borda da
   zona da antena, y 8), o que ajuda; manter o laço `SCK`/retorno o mais
   curto possível.
3. **Camada interna** entre planos de terra, como o display e o USB.

Uma quarta saída é de firmware e não de layout: **16 MHz tira o harmônico do
centro de L1** (o valor que [13](../docs/13-placa-nova.md#regras-de-projeto)
tinha escolhido para o mesmo barramento). Trocar o número é uma linha do
devicetree; **a decisão é do dono e não é deste documento**. O que a bancada
tem de fazer é o ensaio: `UBX-MON-SPAN` com a flash escrevendo e apagando,
com e sem os resistores.

## Montagem

Três restrições de processo mandam no layout, e todas vêm do BM20C:

| Restrição | Consequência |
|---|---|
| Pinos **LGA, não castelados** | montagem por estêncil e forno; **não se solda à mão e não se retrabalha com ferro**. Um erro sob o módulo custa uma estação de ar quente ou a placa |
| **No máximo duas passagens pelo forno** | uma face por passagem; nada de retrabalho térmico "de brinde" |
| **O lado do módulo por último** | a face do BM20C é a **segunda** passagem |

Combinando com a conta da sombra da bateria, que põe o BM20C na **face da
frente** (acima), a ordem fica:

```mermaid
flowchart LR
    P1["1ª passagem · face de trás<br/>BMP585 no respiro, conector da bateria,<br/>peças até 1,2 mm sob a célula"] --> P2["2ª passagem · face da frente<br/>BM20C, MAX-F10S e blindagem,<br/>nPM1300, AEM10900, MAX17262,<br/>indutores, FPC, USB-C, teclas"]
    P2 --> P3["sem 3ª passagem:<br/>o que falhar vai a ar quente,<br/>peça a peça"]
```

O que isso impõe:

- **Tudo o que é pesado vai na segunda passagem**, ou é colado. Na segunda
  passagem a face de trás fica de cabeça para baixo e só a tensão
  superficial da solda a segura: o conector USB-C, o soquete FPC, a
  blindagem do GNSS e os indutores **não** podem ficar na face de trás sem
  cola. A geometria já ajuda: todos esses estão na frente.
- **A face de trás fica com pouca coisa**: o BMP585 junto do respiro, o
  conector da bateria e passivos baixos sob a célula. É o inverso do
  costume, e é consequência direta do módulo.
- **MSL.** O MAX-F10S é MSL 4 e o BMP585, MSL 3
  ([14](../docs/14-hardware-placa-nova.md#regras-de-layout)). Duas passagens
  consomem o orçamento de exposição das duas peças; se a placa parar entre
  as passagens, elas voltam para a estufa.
- **Peças WLP**, o MAX17262 e o MMC5633NJL, pedem estêncil fino e inspeção
  por raio X ou por ótica lateral, que **não estão disponíveis aqui**.

Estas conclusões vêm da regra da ficha e da geometria calculada acima;
**nada foi montado, e a escolha de face de cada peça é do arquivo de
montagem, que não existe.**

## O que só a bancada decide

| Em aberto | Por que não fecha aqui | Onde |
|---|---|---|
| **A pilha do fabricante** | sem as espessuras de dielétrico não há largura de trilha para 50 Ω nem para 90 Ω diferencial; 0,66 mm repartido em 3 vãos admite pilhas muito diferentes | [Camadas](#camadas), [02](02-calculos.md#o-que-não-foi-calculado) |
| **4 camadas em 0,8 mm** | o USB-C manda a espessura e o resto manda as camadas; falta confirmar que o fabricante faz a combinação | [Camadas](#camadas) |
| **A antena GNSS de verdade** | a TE L000670 não cabe: faltam 2,75 mm contra a zona e 5,65 mm contra a faixa livre acima do display | [Orçamento de área](#orçamento-de-área) |
| **Isolação entre as duas antenas** | a conta dá 87,2 mm, 0,71 λ em 2,44 GHz; a isolação real depende do plano, da caixa e das correntes de retorno. Medir o S21 antes de ligar o rádio na potência cheia | [As duas antenas](#as-duas-antenas), [15](../docs/15-avaliacao-componentes.md#bancada-antes-do-layout) |
| **Harmônico de 8 MHz da flash** | 0,58 MHz do centro de L1; só o `UBX-MON-SPAN` com a flash trabalhando diz se aparece | [As duas antenas](#as-duas-antenas) |
| **Alcance do rádio** | a regra dos 30 mm de metal externo não é cumprida pela bateria, pelo botão da direita — que invade 21 mm² da zona proibida — e pelo parafuso de baixo | [Zonas proibidas](#zonas-proibidas) |
| **Folga entre placa e caixa** | 3,5 mm no desenho contra os 2,5 mm que [14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso) descreve | [O contorno](#o-contorno) |
| **Raio de canto da placa** | 3 mm no [desenho](../tools/docs/case_drawing.py) contra os 4 mm de [14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso); as duas fontes discordam e nenhuma foi confirmada | [O contorno](#o-contorno) |
| **Face de cada peça e ordem do forno** | a conta põe o BM20C na frente; o arquivo de montagem não existe | [Montagem](#montagem) |
| **Plugue do USB-C na caixa** | a boca do conector fica 0,77 mm atrás da face externa; um plugue de capa grossa pode não entrar | [Zonas proibidas](#zonas-proibidas) |
| **Contatos dos painéis e da antena** | a área dos pads de mola e dos conectores dos três grupos de painéis não está dimensionada em lugar nenhum | [Orçamento de área](#orçamento-de-área) |
| **Zona do conector do filme de luz** | [14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso) põe o Molex 5034800440 ao lado do FPC do display, mas não lhe dá retângulo, e a medida do corpo dele não está em nenhum documento daqui | [Orçamento de área](#orçamento-de-área) |
| **Empenamento e calor** | 0,8 mm empena mais, e o carregador linear dissipa até 1,50 W dentro de uma caixa vedada | [02](02-calculos.md#calor-do-carregador) |
| **Altura real das peças** | o empilhamento de [14](../docs/14-hardware-placa-nova.md#empilhamento-mecânico) fecha em 17,1 mm dentro de 19 mm, com 1,9 mm de folga, a partir de fichas e não de peças medidas | [14](../docs/14-hardware-placa-nova.md#empilhamento-mecânico) |
