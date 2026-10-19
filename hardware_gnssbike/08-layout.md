# Plano de layout

O que o layout tem de fazer, na ordem em que tem de fazer, e por quê. Fica
entre o [esquemático](01-esquematico.md), que diz o que liga em quê, e o
CAD, que desenha o cobre. A geometria — contorno, posicionamento e zonas
proibidas — está em [04](04-pcb-e-caixa.md); aqui é o **roteamento**.

**Nesta página:** [Regras de projeto](#regras-de-projeto) · [Ordem de roteamento](#ordem-de-roteamento) · [Terra e retorno](#terra-e-caminhos-de-retorno) · [Os nós críticos](#os-nós-críticos-um-a-um) · [Térmica](#térmica-no-cobre) · [Fabricação](#fabricação) · [Subida da primeira placa](#subida-da-primeira-placa) · [O que o layout não decide](#o-que-o-layout-não-decide)

> [!WARNING]
> **Nenhuma placa foi fabricada.** O layout, porém, deixou de ser só projeto
> em 2026-09-23: o arquivo de CAD existe em [`cad/`](cad/), com as peças
> posicionadas, o plano de terra e parte das trilhas. Este documento
> continua sendo **as regras**; quem diz o que a placa é de fato são
> [`cad/check_pcb.py`](cad/check_pcb.py), a verificação de regras do KiCad e
> o [dry-run das regras das fichas](09-dry-run-da-pcb.md).

## Regras de projeto

A pilha é de **quatro camadas em 0,8 mm** ([04](04-pcb-e-caixa.md#camadas)),
e a espessura vem do conector USB-C, não de escolha. Os valores abaixo são
os que um fabricante comum de protótipo entrega sem custo extra; **quem os
confirma é a folha de processo do fabricante escolhido**, que ainda não
existe.

| Regra | Valor de partida | Onde aperta |
|---|---|---|
| Trilha e afastamento mínimos | 0,127 mm (5 mil) | sob o MAX17262 (WLP de 0,4 mm). **Não** sob o módulo: a matriz LGA do ME54BS13 é folgada, ilhas de 0,6 mm com passo de 1,5 × 1,2 mm (desenho mecânico da ficha V1.0.0) |
| Via padrão | furo 0,2 mm, ilha 0,45 mm | em qualquer lugar |
| Via na ilha (*via-in-pad*) | tampada e aplainada | nas 9 vias térmicas do pad central do nPM1300, que o layout de referência da Nordic desenha em matriz de 3 × 3 (PS v1.1, figuras 61 a 64); proibida sob os pads térmicos do TPS7A02, do ADP5091 e do OPT3001 (regra `AL6` do dry run) |
| Anel anular mínimo | 0,125 mm | — |
| Afastamento ao contorno | 0,25 mm | borda de cima, onde a antena chega perto |
| Máscara entre ilhas | 0,1 mm | LGA do módulo. As vias saem cobertas de máscara (*tented*: no KiCad 8 nenhuma via fica nas camadas de máscara, e a plotagem não as abre), então entre via e ilha vale só o isolamento de 0,127 mm (`route.FOLGA_MASCARA`) |

> [!IMPORTANT]
> **O MAX17262 não precisa mais de via na ilha.** Ele é um WLP de 1,5 ×
> 1,5 mm com passo de 0,4 mm, e a única bola sem saída pela face é a do
> meio, o `ALRT`. Até 2026-10-19 ele tinha um pull-up, e só chegaria lá por
> uma via dentro da ilha, que num WLP precisa ser tampada e aplainada para
> não sugar a solda. Desde então o `ALRT` vai ao GND, como a ficha pede
> quando o alerta não é usado, e chega à bola de GND vizinha (`C3`) por uma
> ponte de 0,1 mm, que deixa 0,133 mm até as bolas `B3` e `C2`; o
> `gnssbike.kicad_dru` aceita essa largura só dentro do `U102`. As vias
> térmicas do pad central do nPM1300 continuam dentro de uma ilha.

## Ordem de roteamento

Roteamento é uma sequência de compromissos, e quem roteia por último fica
com o que sobrou. A ordem abaixo põe primeiro o que não admite
compromisso.

```mermaid
flowchart TD
    A["1 · Antena GNSS e a linha de 50 Ω<br/>não admite desvio nem via"] --> B
    B["2 · Laços de chaveamento<br/>BUCK1, BUCK2, SWDCDC do AEM10900"] --> C
    C["3 · O sensor do medidor, em Kelvin<br/>e o caminho de potência da célula"] --> D
    D["4 · Par diferencial do USB, 90 Ω"] --> E
    E["5 · Trilhos de potência<br/>VBUS, VSYS, VBAT: largura, não roteamento"] --> F
    F["6 · SPI da flash, 8 MHz<br/>curto e em camada interna"] --> G
    G["7 · O resto: I²C, SPI do display,<br/>teclas, PWM, console"] --> H
    H["8 · Preenchimento de terra e costura de vias"]
```

**Por que nesta ordem.** A antena e os laços de chaveamento são os dois
únicos blocos em que o cobre *é* o circuito: uma trilha errada ali não
degrada, **quebra**. O sensor do medidor vem logo depois porque a queda em
qualquer junta dentro dele entra direto na leitura. O USB é par casado e
precisa de espaço contínuo. Os trilhos de potência, ao contrário do que
parece, são os mais fáceis: o que eles exigem é **largura**, e largura se
resolve com cobre, não com posição.

## Terra e caminhos de retorno

**Uma camada de terra inteira, sem cortes.** A camada 2 é terra contínua de
borda a borda, e nada — nem trilha de sinal, nem trilha de alimentação —
atravessa ela. É a regra que mais barato compra desempenho nesta placa, e a
que mais fácil se perde quando falta espaço na camada 1.

**Por que não há divisão de terra.** A tentação clássica é separar "terra
analógica" e "terra digital". Aqui isso seria errado nos dois blocos que
importam: a antena GNSS precisa de um plano **contínuo** sob e ao redor da
linha de 50 Ω, e qualquer divisão obriga a corrente de retorno a contornar,
o que é exatamente o laço que se quer evitar. O mesmo vale para o par do
USB.

| O que | Regra |
|---|---|
| Camada 2 | terra contínua, sem divisão e sem trilha |
| Camada 3 | alimentação, com os trilhos largos como planos parciais |
| Troca de camada de sinal rápido | **via de retorno de terra a menos de 1 mm** da via de sinal |
| Costura de vias | ao longo de todo o contorno e em volta da zona da antena, a cada 3 a 5 mm |
| Sob a antena GNSS e sob a área da antena do ME54BS13 | **cobre nenhum em camada nenhuma** ([04](04-pcb-e-caixa.md#zonas-proibidas)) |

## Os nós críticos, um a um

### Antena GNSS, e a linha de 50 Ω

O que mais importa nesta placa, e o que menos perdoa.

- Linha de **50 Ω** do `RF_IN` do MAX-F10S à rede em π e daí à antena. A
  largura sai da pilha do fabricante; sem a pilha **não há como desenhar
  esta trilha**.
- **Sem via, sem curva viva, sem passar por baixo de nada.** Se a linha
  precisar trocar de camada, o projeto do posicionamento está errado.
- A rede em π (três posições) fica **junto da antena**, não junto do
  módulo.
- A zona livre de cobre da antena vale em **todas as camadas**, inclusive
  na de terra.
- **A antena escolhida não cabe na zona** ([04](04-pcb-e-caixa.md#zonas-proibidas)):
  10,75 mm contra os 8 mm reservados. Isto é decisão pendente e **bloqueia
  o layout da borda de cima**.

### Os três laços de chaveamento

Um conversor chaveado não é um circuito no esquemático: é um **laço de
corrente** no cobre, e a área desse laço é o que ele irradia.

| Conversor | Laço a minimizar | Cuidado próprio |
|---|---|---|
| `BUCK1` (1,8 V, GNSS) | entrada → indutor → capacitor de saída → terra → entrada | é o trilho do receptor: a ondulação dele **vira ruído no C/N0** |
| `BUCK2` (3,0 V, MCU) | idem | é o trilho que liga o aparelho |
| AEM10900, nó `SWDCDC` | indutor e capacitores colados nos pinos | **sem plano de terra por baixo** (seção 13 da ficha) |

**Regras dos três:** o capacitor de entrada encosta no pino, com o retorno
de terra na mesma face; o indutor fica o mais perto possível; o laço fecha
em área mínima; e o nó de chaveamento (o lado do indutor que oscila) é
**pequeno**, porque ele é a antena involuntária do circuito.

**E os três ficam longe da antena do rádio.** A ficha do ME54BS13 (7.3)
pede **20 mm** entre a antena do módulo e qualquer conversor chaveado ou
indutor, e a geometria de hoje **não dá isso**: entre a zona de energia e a
área livre da antena sobram cerca de 11,5 mm
([04](04-pcb-e-caixa.md#zonas-proibidas)). Isso não é regra de cobre e
**nenhum DRC a pega**: quem a cumpre é quem posiciona.

> [!CAUTION]
> **O `SWDCDC` do AEM10900 é a exceção que contradiz a regra geral.** Em
> todo o resto da placa a camada de terra é contínua e vem logo abaixo; ali
> a ficha pede o contrário, para não formar capacitância no nó de
> chaveamento. É o tipo de instrução que um roteador automático desfaz sem
> avisar.

### O sensor do medidor de bateria

O sensor de 7 mΩ é **interno** ao MAX17262, entre os pinos `BATT` e `SYS`
([02](02-calculos.md#medidor-de-bateria)). Isso simplifica o layout e
transfere o problema para as **juntas**:

- O caminho da célula ao `BATT` e do `SYS` ao resto é **potência**: 600 mA
  de carga, e cada miliohm de trilha entra no erro de medida.
- O `JP101`, o jumper de medição de corrente, fica **nesse caminho**
  ([06](06-conectores-e-pontos-de-teste.md#jp101--jumper-de-medição-de-corrente)):
  1206 e ≤ 50 mΩ, e mesmo assim são 30 mV a 600 mA.
- **Nenhuma outra carga pendura entre o `BATT` e o `SYS`.**

### Par diferencial do USB

`D+` e `D−` a **90 Ω diferencial**, casados em comprimento, sem via se der,
e com o plano de terra contínuo embaixo o caminho inteiro. Os dois lados do
conector são unidos na placa, como manda a norma para USB 2.0
([06](06-conectores-e-pontos-de-teste.md#j101--usb-c)).

### SPI da flash, e o harmônico de L1

O 197º harmônico de 8 MHz cai a **0,58 MHz do centro de L1**
([04](04-pcb-e-caixa.md)), e a flash é a única coisa rápida perto do
receptor.

- Trilha **curta**: a flash está a cerca de 67 mm da zona da antena, e essa
  distância é a defesa principal.
- **Camada interna**, entre planos, sempre que possível.
- Os **33 Ω** em série ficam junto de quem **aciona**: `SCK` e `MOSI` no
  pino do MCU, **`MISO` no pino `SO` da flash**
  ([02](02-calculos.md#resistor-de-série-no-spi-da-flash)).
- Sem trilha de SPI passando pela borda de cima.

### Os barramentos lentos

O SPI do display (2 MHz no máximo) e os dois I²C (400 kHz) não têm regime
crítico. O que eles pedem é banal e costuma ser esquecido: **os pull-ups do
I²C perto do mestre**, e o FPC do display saindo **pela esquerda**, longe
das duas antenas ([04](04-pcb-e-caixa.md#posicionamento)).

### Largura de trilha

O [cálculo](02-calculos.md#corrente-por-trilho-e-largura-de-trilha) fixa a
corrente de cada nó. Desde 2026-10-19 o `route.py` dá a largura por essa
corrente, em três classes, e a regra `AL2` do `dry_run_pcb.py` confere cada
trecho pela IPC-2221 (10 °C de subida, cobre de 35 µm, que ainda é
suposição até a pilha do fabricante):

| Classe | Largura | Nós | Por quê |
|---|---|---|---|
| energia | 0,25 mm, só nas faces de fora | `VBUS`, `VSYS`, `VBAT`, `VBAT_SYS`, `VBAT_CELULA` | de 0,5 a 0,8 A. Pela IPC-2221, 0,25 mm levam 0,87 A por fora e 0,44 A numa camada interna: por fora bastam de 0,12 a 0,22 mm, por dentro seriam precisos de 0,30 a 0,57 mm, e por isso esses nós não entram na `In2.Cu` |
| média | 0,25 mm | `3V0` e os ramos `3V0_*` | 123 mA no pico de partida do receptor; 0,25 mm levam 0,44 A numa camada interna |
| sinal | 0,15 mm | `1V8`, `3V3BL`, `SD3V0`, `SD3V0_FLASH`, `VBCKP`, `VBUSOUT` | de microampères a 16 mA; a `VBUSOUT` passa por uma chave de 7,5 Ω dentro do nPM1300 e só alimenta a detecção de USB do módulo e um divisor |

Até 2026-10-19 todo trilho saía com 0,4 mm, o backup de 28 µA também, e
eram esses trilhos que os sinais achavam no caminho ao atravessar a zona de
energia. Os 0,4 mm eram a largura que uma camada interna pede para os
trilhos de carga; na face de fora, 0,25 mm já cumprem a norma com folga.

## Térmica no cobre

O carregador do nPM1300 dissipa até **1,50 W**, e a caixa inteira chega a
44,1 °C a 25 °C de ambiente ([02](02-calculos.md#calor-da-caixa-inteira)) —
**empate com o corte do JEITA a 45 °C, antes de qualquer sol**. O layout é
uma das duas alavancas para isso (a outra é baixar a corrente de carga):

- Preenchimento de cobre generoso no `VSYS` e no terra sob o nPM1300, nas
  quatro camadas, costurado com vias térmicas. O pad central leva as 9 vias
  em matriz de 3 × 3 do layout de referência da Nordic (PS v1.1, figuras 61
  a 64), que o `route.py` põe desde 2026-10-19; até então a regra `AL6` do
  dry run proibia via sob ele, e o pad só achava o terra se o cobre da
  frente passasse entre os pinos.
- **A célula fica longe do carregador.** Hoje o desenho a põe atrás da
  placa ([04](04-pcb-e-caixa.md#posicionamento)); o nPM1300 não pode ficar
  do outro lado dela.
- O NTC que o JEITA lê fica **na célula**, não perto do carregador, senão
  ele corta a carga por causa do calor do próprio chip.

## Fabricação

- **Duas passagens pelo forno, no máximo**, com o lado do módulo **por
  último** ([04](04-pcb-e-caixa.md#montagem)).
- O ME54BS13 é híbrido: **20 pads castelados na borda, numerados de 1 a 20,
  mais 60 ilhas LGA por baixo**, de `A0` a `F9`. Os castelados se inspecionam
  e, no limite, se retocam com ferro; as 60 ilhas do meio, não. Quem manda é
  a matriz: **estêncil e forno**, sem retrabalho manual do que fica embaixo.
- **Fiduciais**: três globais na placa e um par local junto do módulo e do
  MAX17262.
- Os **16 pontos de teste** ([06](06-conectores-e-pontos-de-teste.md#pontos-de-teste))
  ficam acessíveis com a placa montada, na face oposta à da célula.
- Painelização com abas perfuradas; a borda de cima, onde vai a antena,
  **não leva aba**.

## Subida da primeira placa

Numa placa que nunca existiu, a ordem de montar e ligar vale tanto quanto o
layout. **Nada disto foi feito.**

| Passo | O que montar | O que conferir antes de ligar |
|---|---|---|
| 0 | nada | continuidade e curto de **cada trilho contra o terra**, com a placa nua |
| 1 | só o caminho de energia: nPM1300, MAX17262, passivos, `RVSET1` e `RVSET2` | **medir os dois `RVSET` com o multímetro**: um errado e o MCU nunca liga, ou queima |
| 2 | — | fonte de bancada no lugar da célula, **com limite de corrente**; conferir `VSYS` |
| 3 | — | `3V0` = 3,0 V e `1V8` = 1,8 V, e a **rampa do `1V8` no osciloscópio**: tem de ficar entre 25 e 35.000 µs/V ([07](07-sequencias-e-protecao.md#as-três-restrições-de-partida)) |
| 4 | o módulo ME54BS13 | console no `uart20`, e o aparelho tem de **partir** |
| 5 | o receptor GNSS | **só depois** de a rampa do passo 3 estar confirmada |
| 6 | sensores e flash | barramentos, endereços, e a `LDSW1` de fato cortando |
| 7 | display, teclas, buzzer, LED | — |
| 8 | painéis e antena | `UBX-MON-SPAN` com a flash trabalhando, e o S11 da antena **dentro da caixa** |

> [!CAUTION]
> **O passo 3 vem antes do passo 5, e não é formalidade.** O `V_IO` do
> MAX-F10S tem máximo absoluto de 1,98 V e rampa limitada; a ficha diz que
> fora da faixa o módulo **pode ser danificado**. Soldar o receptor antes
> de medir o trilho é arriscar a peça mais cara da placa depois do módulo.

## O que o layout não decide

| Em aberto | Quem resolve |
|---|---|
| **Pilha do fabricante** — sem ela não há 50 Ω, nem 90 Ω, nem largura de trilha | o fabricante escolhido |
| **Via tampada e aplainada** — sem ela o MAX17262 não é montável | idem |
| **A antena não cabe na zona** — 10,75 mm contra 8 mm | decisão de mecânica, **bloqueia a borda de cima** |
| **Espelhamento do mapa de pads do ME54BS13** — o de-para dos 80 pads já existe ([03](03-netlist.md#como-ler)), mas a ficha V1.0.0 e a V0.5.0 discordam de qual lado é qual | um módulo real na bancada, antes de fabricar |
| **Ordem das cinco vias do conector da luz** — a ficha já diz que são **5 vias, passo 0,5 mm**, num conector à parte do de 10 vias dos sinais; falta qual é anodo e qual é catodo | ficha em mãos ou amostra |
| **Peça do conector de 5 vias** do `J402` | escolha de componente |
| **Orientação do divisor do `DIS_STO_CH`** | ficha do AEM10900 |
| **Onde fica o NTC do JEITA na célula** | fabricante do pack |
