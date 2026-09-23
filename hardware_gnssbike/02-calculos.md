# Cálculos

Cada valor de componente do esquemático, com a conta que o justifica e a
origem de cada número de entrada. Conta feita aqui vem marcada como
**conta**; número de ficha técnica vem marcado como **ficha**; e o que
depende de medida está dito como tal.

**Nesta página:** [Corrente de cada trilho](#corrente-de-cada-trilho) · [Orçamento do USB](#orçamento-do-usb-e-tempo-de-carga) · [Autonomia](#autonomia) · [Corrente por trilho](#corrente-por-trilho-e-largura-de-trilha) · [Pull-ups do I²C](#pull-ups-do-i²c) · [Luz do display](#luz-do-display) · [LED RGB](#led-rgb) · [Calor do carregador](#calor-do-carregador) · [Calor da caixa](#calor-da-caixa-inteira) · [Colheita solar](#colheita-solar) · [Medidor de bateria](#medidor-de-bateria) · [Conversores do nPM1300](#conversores-do-npm1300) · [Desacoplamento](#desacoplamento) · [Capacitor de volume](#capacitor-de-volume-no-módulo) · [Proteção das teclas](#proteção-das-teclas) · [Buzzer](#buzzer-piezo) · [Série no SPI](#resistor-de-série-no-spi-da-flash) · [Rampa do V_IO](#rampa-do-v_io-na-partida) · [Trilho de 5 V](#trilho-de-5-v-com-a-sharp) · [O que não foi calculado](#o-que-não-foi-calculado)

> [!WARNING]
> Nada foi medido. Não existe placa.

## Corrente de cada trilho

### `3V0`, do BUCK2 (limite de 200 mA, **ficha**)

| Carga | Pior caso | Origem |
|---|---|---|
| Rádio do BM20C a +8 dBm | 10,9 mA | ficha do nRF54LM20A, a 3,0 V |
| CPU do BM20C (CoreMark da RRAM com cache) | 2,6 mA | ficha |
| Sensores (quatro, lendo) | cerca de 1 mA | estimativa de [13](../docs/13-placa-nova.md#orçamento-de-energia) |
| Buzzer piezo | cerca de 5 mA | estimativa |
| Flash NOR apagando um setor, no modo de baixo consumo | 3,1 mA | ficha da MX25R6435F |
| Display (JDI, 1 quadro/s) | 10 µA | 30 µW ÷ 3,0 V, **conta** |
| TXU0204 `VCCA` e `I2C_VDD` do AEM10900 | dezenas de µA | ficha |
| REG710, só na montagem com a Sharp | cerca de 0,2 mA | 65 µA parado mais o dobro da corrente da tela |
| **Soma** | **cerca de 23 mA** | **conta** |

**Ocupação: 23 mA de 200 mA, 11,5 %** — ou seja, folga de 177 mA, quase 9×.

> [!NOTE]
> O [`docs/14`](../docs/14-hardware-placa-nova.md#alimentação) estima "perto
> de 120 mA" neste trilho. Aquele número é de quando o armazenamento era um
> **cartão microSD**, que puxa até 100 mA pela chave `LDSW1`. Com a flash
> NOR soldada, decidida em 2026-09-20, o armazenamento passou de até 100 mA
> para **3,1 mA**, e o pico do trilho caiu de cerca de 120 mA para cerca de
> 23 mA. Os 100 mA da `LDSW1` deixaram de ser o que dimensiona qualquer
> coisa.

### `1V8`, do BUCK1 (limite de 200 mA, **ficha**)

| Carga | Aquisição | Rastreio | Origem |
|---|---|---|---|
| MAX-F10S `VCC`, medido a 3,0 V | 21 mA | 16 mA | ficha, tabelas 15 e 16 |
| MAX-F10S `V_IO`, medido a 3,0 V | 3 mA | 3 mA | ficha |
| TXU0204 `VCCB` | dezenas de µA | idem | ficha |
| **Total no trilho de 1,8 V** | **34 mA** | **26 mA** | [14](../docs/14-hardware-placa-nova.md#gnss) |

A última linha **não é a soma das de cima**, e a razão não é equivalência
de potência: o receptor **gasta menos** a 1,8 V do que a 3,0 V — 46,8 mW
contra 57 mW em rastreio, cerca de 18 % menos. Os 26 mA saem de
46,8 mW ÷ 1,8 V, e os 34 mA da aquisição vêm da mesma fonte
([14](../docs/14-hardware-placa-nova.md#gnss)), não de uma conta feita aqui.

> [!NOTE]
> Quem aplicar equivalência de potência crua chega a outro número:
> (21 + 3) mA × 3,0 V = 72 mW, e 72 ÷ 1,8 = 40 mA. **Está errado**, porque
> supõe que o consumo não muda com a tensão, e a ficha diz que muda. O
> mesmo erro em rastreio daria 31,7 mA em vez de 26.

**Pico de partida: até 100 mA** (ficha). Folga de 2× contra os 200 mA do
BUCK1 no pico, e de quase 8× em regime.

### `VSYS`, a entrada de tudo

Com a bateria a 3,7 V e reguladores a 90 % (**conta**):

| Fonte | Conta | Corrente do `VSYS` |
|---|---|---|
| BUCK2 | 23 mA × 3,0 V ÷ 3,7 V ÷ 0,9 | 20,7 mA |
| BUCK1 | 34 mA × 1,8 V ÷ 3,7 V ÷ 0,9 | 18,4 mA |
| `LDSW2` como LDO, para a luz | 16 mA (LDO não transforma corrente) | 16 mA |
| LED RGB e LED de carga | 3 canais de até 3,6 mA, mais 5 mA | até 16 mA |
| **Pior caso somado** | | **cerca de 71 mA** |

## Orçamento do USB e tempo de carga

O carregador do nPM1300 é **linear**: o que entra pelo cabo é a carga da
célula **mais** o consumo do sistema, sem transformação.

```
I(VBUS) = I(carga) + I(sistema) = 600 mA + 71 mA = 671 mA
```

O que a fonte oferece decide se esses 600 mA existem (**conta**):

| Fonte USB-C | Oferece | Sobra para a carga | Resultado |
|---|---|---|---|
| Default USB (`Rp` de 56 kΩ) | 500 mA | 429 mA | **a carga cai para 429 mA** |
| 1,5 A | 1500 mA | 1429 mA | os 600 mA cabem |
| 3,0 A | 3000 mA | 2929 mA | cabem |

É por isso que o nPM1300 começa em **100 mA** e o firmware só sobe o limite
**depois** de ler o `CC1` e o `CC2` — o devicetree parte de
`vbus-limit-microamp = <500000>`. Uma fonte comum de celular é "Default
USB", e nela o aparelho carrega mais devagar, não pior.

Tempo de carga de uma célula de 2000 mAh, com 80 % da capacidade na fase de
corrente constante e cerca de uma hora de cauda em tensão constante
(**conta**):

| Corrente | Corrente constante | Total aproximado |
|---|---|---|
| 600 mA (fonte de 1,5 A) | 2,7 h | **cerca de 3,7 h** |
| 429 mA (fonte comum) | 3,7 h | **cerca de 4,7 h** |

> [!NOTE]
> O [calor do carregador](#calor-do-carregador) pode alongar os dois: a
> regulação térmica reduz a corrente sozinha, e o corte por temperatura da
> célula pode disparar antes do fim. Não medido.

## Autonomia

Energia útil da célula, com 90 % do nominal aproveitável (**conta**):

```
2000 mAh × 3,7 V × 0,9 = 6,66 Wh
```

| Regime | Consumo | Autonomia |
|---|---|---|
| **Pior caso deste esquemático** (os 71 mA do `VSYS`, tudo ligado ao mesmo tempo) | 263 mW | **25 h** |
| Típico de [13](../docs/13-placa-nova.md#orçamento-de-energia) | 58 mW | 115 h |
| Econômico de [13](../docs/13-placa-nova.md#orçamento-de-energia) | 19 mW | 350 h |

Os 115 h do meio batem com o orçamento de energia de
[`docs/13`](../docs/13-placa-nova.md#orçamento-de-energia) — **e tinham de
bater**: os 58 mW vêm de lá e a divisão é a mesma. Não é conferência
cruzada, é a mesma conta escrita duas vezes. A coluna que falta aqui é a
de uso pesado daquele documento, 74 mW, que dá cerca de **90 h** e é a mais
próxima de um pedal de verdade.

O pior caso de 25 h não é um regime de uso: é rádio transmitindo, receptor
adquirindo, luz acesa e LED aceso ao mesmo tempo. Serve para dimensionar
trilho e regulador, não para prometer autonomia.

## Corrente por trilho e largura de trilha

Qual nó carrega quanto, que é o que decide a largura no layout (**conta**,
a partir das seções acima):

| Nó | Pior caso | Classe | Quando |
|---|---|---|---|
| `VBUS` | até 1500 mA | **larga** | com uma fonte de 1,5 A |
| `VSYS` | 671 mA | **larga** | sistema mais carga |
| `VBAT` | 600 mA | **larga** | carga |
| `1V8` | 100 mA | média | pico de partida do receptor |
| `3V0` | 23 mA | sinal | — |
| `3V3BL` | 16 mA | sinal | luz acesa |
| `SD3V0` | 3,1 mA | sinal | apagando um setor |
| `VBCKP` | 28 µA | sinal | backup do receptor |

> [!IMPORTANT]
> **A largura em milímetros não sai daqui.** Ela depende da espessura do
> cobre e da pilha do fabricante, que [04](04-pcb-e-caixa.md#camadas) ainda
> não tem. O que este documento fixa é a **corrente**; a largura sai da
> IPC-2152 quando a pilha existir. Os três nós marcados como "larga" são
> os que não podem sair com largura de sinal.

## Pull-ups do I²C

Dois barramentos em modo rápido (400 kHz), os dois a 3,0 V.

**Limite de cima** — o resistor tem de carregar a capacitância do
barramento dentro do tempo de subida que a norma permite:

```
Rp(máx) = t_r / (0,8473 × Cb)
```

Com `t_r` = 300 ns (norma I²C, modo rápido) e `Cb` = 60 pF (quatro
dispositivos de cerca de 10 pF mais cerca de 20 pF de trilha, **estimativa**):

```
Rp(máx) = 300e-9 / (0,8473 × 60e-12) = 5,9 kΩ
```

**Limite de baixo** — o dispositivo tem de conseguir puxar a linha para o
nível baixo com a corrente que ele aguenta:

```
Rp(mín) = (VDD − VOL) / IOL = (3,0 − 0,4) / 3 mA = 867 Ω
```

**Escolhido: 4,7 kΩ**, que é o valor que a [especificação](../docs/14-hardware-placa-nova.md#barramentos-e-endereços)
e a [lista de compras](../docs/19-lista-de-compras.md#passivos) já usam.
Confere dentro da faixa de 0,87 a 5,9 kΩ:

| Resistor | Tempo de subida | Corrente no nível baixo |
|---|---|---|
| 2,2 kΩ | 112 ns | 1,18 mA |
| **4,7 kΩ** | **239 ns** | **0,55 mA** |
| limite | 300 ns | 3 mA |

Os dois passam. O primeiro rascunho deste documento escolheu 2,2 kΩ, por
ter mais folga no tempo de subida — e estava errado em escolher: **4,7 kΩ
cumpre a norma, já está na lista validada e já é o valor da especificação**,
e trocá-lo obrigaria a comprar um resistor a mais para ganhar margem que
não falta. Divergir de uma lista validada é o tipo de coisa que se paga na
montagem.

O que vale registrar é a margem: com 4,7 kΩ sobram 61 ns dos 300, e a
capacitância de 60 pF é **estimativa**. Se o barramento sair mais
carregado do que isso no layout — trilha longa, mais de quatro peças —,
`C_b` passa de 75 pF e o tempo de subida estoura. **A conferir quando o
layout existir**; se estourar, aí sim 2,2 kΩ resolve.

O barramento da energia tem três dispositivos em vez de quatro, o que só
aumenta a folga. **Mesmo valor nos dois, 4,7 kΩ.**

> [!IMPORTANT]
> **Falta saber de onde o nPM1300 alimenta os seus pinos digitais.** O
> `PWR_SDA`, o `PWR_SCL` e o `PMIC_INT` são compartilhados entre o MCU, que
> vive em 3,0 V, e o nPM1300, cujo domínio de entrada e saída esta
> especificação não registra em lugar nenhum. Se ele for referenciado ao
> `VSYS`, que **com o USB chega a 5,5 V**, os limiares de entrada dele
> deixam de casar com os 3,0 V do MCU, e o `PMIC_INT` poderia chegar acima
> do trilho do MCU — o mesmo tipo de problema que o LED RGB tinha. Fechar
> contra a ficha do nPM1300 **antes do layout**. Não conferido aqui.

## Luz do display

O trilho `3V3BL` sai da `LDSW2` do nPM1300 em 3,3 V, e o MOSFET de canal N
liga o catodo ao terra sob PWM.

### Com o JDI LPM027M128**C**

LED de **16 mA a 2,67 V** (ficha). **Só o C tem luz**: o B é a mesma tela
sem backlight ([01](01-esquematico.md#folha-4--display)), e com ele este
resistor, o `Q401` e o trilho `3V3BL` não têm o que acionar. Com `V_DS` do MOSFET em cerca de 50 mV:

```
R_BL = (3,3 − 2,67 − 0,05) / 16 mA = 0,58 / 0,016 = 36,3 Ω
```

**Escolhido: 39 Ω**, o valor E24 acima (nunca o abaixo: um resistor menor
passa mais corrente que a ficha permite). Confere:

```
I = 0,58 / 39 = 14,9 mA        (93 % do nominal)
P = 0,58 × 0,0149 = 8,6 mW     (0402 aguenta 63 mW)
```

### Com a Sharp e o filme Azumo

LED de **10 mA típicos, 25 mA máximo** (ficha), com a tensão direta a
medir na amostra. Com `V_f` = 3,0 V como hipótese:

```
R_BL = (3,3 − 3,0 − 0,05) / 10 mA = 25 Ω
```

> [!CAUTION]
> **Este valor não pode ser fechado sem a amostra.** Com `V_f` de 3,2 V o
> resistor cai para 5 Ω e a corrente vira refém da tolerância do LED; com
> `V_f` de 2,8 V sobe para 45 Ω. E a `LDSW2` sai de regulação quando o
> `VSYS` chega a cerca de 3,4 V, de modo que a luz enfraquece com a
> bateria baixa qualquer que seja o resistor. Medir a tensão direta do
> filme antes de fechar o esquemático desta montagem.

## LED RGB

> [!CAUTION]
> **O catodo não pode ir direto ao pino do MCU.** O anodo comum fica no
> `VSYS`, e com o cabo USB ligado o `VSYS` acompanha o `VBUS`: vai a até
> **5,5 V**. Um pino de 3,0 V amarrado a um catodo cujo anodo está em 5,5 V
> fica com o diodo de proteção polarizado, e o pino é quem paga. Por isso
> vai **um N-MOSFET de canal N por cor** (Diodes DMG1012T-7), com o pino do
> MCU na porta. É a correção 18 da [lista de compras](../docs/19-lista-de-compras.md),
> e a primeira versão deste documento a tinha errado.

Arranjo, e a ordem importa: o LED é de **anodo comum**, de modo que o
anodo vai **direto ao `VSYS`** e o resistor fica do lado do catodo —
`VSYS` → anodo comum → cada die → `R_LED` → dreno do MOSFET → fonte no
`GND`, com o pino do MCU na porta.

> [!CAUTION]
> **Não há resistor por cor no anodo de um LED de anodo comum**: os três
> dies dividem um pino só, e três resistores ali seriam 333 Ω em paralelo,
> sem corrente independente por cor. Pior, com o resistor no anodo e o
> catodo no dreno, o resistor fica **em paralelo com o die** e o die vê o
> `VSYS` nu — até 5,5 V com cabo USB sobre um die de 1,83 V. Um rascunho
> deste documento desenhou assim; a tabela de correntes abaixo só vale com
> o resistor **entre cada catodo e o seu MOSFET**. **O resistor é de 1 kΩ**, o valor que a
[especificação](../docs/14-hardware-placa-nova.md#componentes-principais) e a [lista de
compras](../docs/19-lista-de-compras.md#passivos) já fixaram.

Tensão direta da peça escolhida (Kingbright APTF1616SEEZGKQBKC, **ficha**,
a 2 mA): **1,83 V no vermelho** e **2,66 V no verde e no azul**.

Corrente em cada ponto da faixa do `VSYS` (**conta**, com `V_DS` do MOSFET
em 50 mV):

| `VSYS` | Quando | Vermelho | Verde e azul |
|---|---|---|---|
| 5,5 V | USB no limite | 3,62 mA | 2,79 mA |
| 4,2 V | bateria cheia | 2,32 mA | 1,49 mA |
| 3,7 V | meio | 1,82 mA | 0,99 mA |
| 3,3 V | quase vazia | 1,42 mA | 0,59 mA |
| 3,0 V | vazia | 1,12 mA | 0,29 mA |

Os valores a 3,7 V batem com os "cerca de 1,9 mA no vermelho e 1,0 mA no
verde e no azul" da lista de compras, o que confirma a conta dos dois lados.

**O brilho varia com a tensão do `VSYS`**, e de forma diferente em cada
cor: **3,2 vezes no vermelho** (3,62 a 1,12 mA) e **9,6 vezes no verde e no
azul** (2,79 a 0,29 mA), porque a tensão direta maior come mais da margem.
Nenhum canal apaga: o verde e o azul chegam ao fim da bateria com 0,29 mA, fracos mas
acesos. Isso é consequência de o anodo estar num trilho não regulado, e não
tem conserto no resistor.

> [!NOTE]
> Um rascunho anterior deste documento usou tensão direta de 2,0 V e 3,0 V,
> que **não vieram de fonte nenhuma**, e chegou a 680 Ω e 390 Ω e à
> conclusão de que "o verde e o azul apagam". Com a ficha da peça na mão, a
> conclusão cai: eles não apagam, só enfraquecem. Foi o tipo de erro que
> inventar um número produz — e a especificação já trazia o valor certo.

## Calor do carregador

O carregador do nPM1300 é **linear**: o que não vira carga vira calor.

```
P = (V_BUS − V_BAT) × I_CHG
```

Com `I_CHG` = 600 mA (0,3 C da célula de 2000 mAh):

O `VBUS` vai de 4,0 a **5,5 V** ([03](03-netlist.md#nós-de-alimentação)), e
o pior caso é o limite de cima, não os 5,0 V nominais:

| `V_BAT` | Com `VBUS` de 5,0 V | Com `VBUS` de **5,5 V** |
|---|---|---|
| 3,0 V (célula vazia) | 1,20 W | **1,50 W** |
| 3,7 V (meio) | 0,78 W | 1,08 W |
| 4,2 V (fim) | 0,48 W | 0,78 W |

Com um QFN32 de 5 × 5 mm em quatro camadas, `θ_JA` fica perto de **32 °C/W**
(valor típico deste encapsulamento; a ficha do nPM1300 traz o dele). No
pior caso:

```
ΔT = 1,50 W × 32 °C/W = 48 °C
```

Numa caixa **vedada**, a 25 °C de ambiente, isso põe a junção perto de
73 °C. O CI tem regulação térmica e reduz a corrente sozinho, então não há
risco de dano; o que há é **carga mais lenta do que a conta promete** e
calor perto da célula.

> [!NOTE]
> A célula não deve ser carregada acima de 45 °C, e o JEITA do nPM1300 corta
> nisso lendo o NTC do pack. Com o carregador dissipando mais de 1 W a
> poucos milímetros da célula, dentro de uma caixa fechada, **é plausível
> que o corte por temperatura dispare antes de a carga terminar**. Medir na
> bancada; se disparar, baixar `I_CHG` para 400 mA (0,2 C) resolve, ao custo
> de carga mais demorada. Não medido.

## Calor da caixa inteira

O [calor do carregador](#calor-do-carregador) é a junção de um chip. A
outra pergunta é o que acontece com a **caixa**, que é vedada e fica no
guidão.

Área externa (**conta**, caixa de 62 × 104 × 19 mm):

```
2×(62×104) + 2×(62×19) + 2×(104×19) = 19.204 mm² = 192 cm² = 0,0192 m²
```

Dissipação no pior caso, que é **carregando**. E aqui a conta não é "o
carregador mais a perda dos reguladores": é **tudo o que entra pelo cabo e
não vira química na célula** (**conta**):

```
entra pelo VBUS:      0,671 A × 5,5 V = 3,691 W
vai para a célula:    0,600 A × 3,0 V = 1,800 W
vira calor na caixa:                    1,891 W
```

Com convecção natural em ar parado, cujo coeficiente fica entre 5 e
10 W/m²·K (**conta**):

| `h` | Elevação da caixa | Caixa a 25 °C de ambiente |
|---|---|---|
| 5 W/m²·K (pior) | 19,7 °C | **44,7 °C** |
| 7 W/m²·K | 14,1 °C | 39,1 °C |
| 10 W/m²·K (melhor) | 9,8 °C | 34,8 °C |

**Pedalando, o problema não existe:** sem carga o aparelho dissipa 263 mW
no pior caso, e a caixa sobe de 1,4 a 2,7 °C. O calor é inteiramente do
caminho de carga, e só com o cabo ligado.

> [!CAUTION]
> **No pior caso a caixa já encosta no corte do JEITA sem nenhum sol.** O
> corte de carga da célula é **45 °C** e a conta acima dá **44,7 °C** com
> 25 °C de ambiente e o pior coeficiente. Não é margem: é empate. E as duas
> saídas já estão escritas e nenhuma foi tomada — baixar o `I_CHG` para
> 400 mA (0,2 C), o que alonga a carga, ou dar caminho térmico do
> carregador à caixa. **Decisão do dono, e antes do layout**, porque a
> segunda muda o posicionamento.

> [!NOTE]
> **A junção do carregador é mais quente do que os 73 °C da seção
> anterior.** Aqueles 73 °C usam 25 °C de ambiente, e o ar que rodeia o
> chip **é o interior da caixa**, que esta seção acabou de mostrar subir
> até 19,7 °C. Compondo, a junção vai a cerca de **93 °C**. O `θ_JA` de
> 32 °C/W supõe ar livre, então compor os dois é aproximação grosseira nos
> dois sentidos — mas o sinal é claro, e o número de 73 °C é otimista.

## Colheita solar

### Arranjo

Seis módulos de três células, 23 × 8 mm cada (**ficha** da classe
KXOB25-05X3F). Área física:

```
6 × 23 mm × 8 mm = 1.104 mm² = 11,0 cm²
```

Área **equivalente**, porque quatro dos seis ficam em chanfros de 45° e
não recebem luz de frente: os 7 a 9 cm² de [13](../docs/13-placa-nova.md#orçamento-de-energia).

### MPPT

O AEM10900 fixa o ponto de máxima potência numa razão da tensão em aberto,
escolhida pelos pinos `R_MPP[2:0]`. Para silício monocristalino:

```
V_mp / V_oc = 1,67 V / 2,07 V = 0,81
```

**Escolhido: 80 %** (`R_MPP[2:0]` no `VINT`), o degrau mais próximo.

### Série ou paralelo

Seis módulos de três células. A escolha não é livre: a entrada do AEM10900
é de fonte de baixa tensão, e **seis em série dariam 12,4 V em aberto**
(6 × 2,07 V), muito acima do que ele recebe. Logo, **paralelo**: 2,07 V em
aberto e 1,67 V no ponto de máxima potência. A **soma aritmética** dos seis
módulos seria 6 × 18,4 mA = **110 mA**; o número que a escolha do indutor
usa, **88 mA**, é o pico ponderado pelo ângulo de cada face
([15](../docs/15-avaliacao-componentes.md#carga-usb-c-e-painel-solar)),
porque quatro dos seis ficam em chanfros de 45°. **Com o sol perpendicular
aos chanfros os 110 mA são possíveis**, e é contra eles que o indutor tem
de ser conferido, não contra os 88.

**Sem diodo de bloqueio**, e vale explicar por quê, porque a pergunta é
legítima: quatro dos seis módulos ficam em chanfros de 45°, de modo que
**sombreamento parcial é o estado normal**, não a exceção. Num arranjo em
paralelo, um módulo sombreado pode virar carga dos outros. A conta diz que
aqui não vira (**conta**):

```
módulo sombreado, com os outros em 1,67 V:
1,67 V ÷ 3 células = 0,56 V por célula
```

0,56 V fica **abaixo do joelho de cerca de 0,6 V** de uma célula de
silício: o módulo sombreado conduz pouco e a perda é modesta. Um Schottky
por módulo custaria de 0,2 a 0,3 V sobre os 1,67 V — de 12 a 18 % da
tensão de trabalho —, que é perda certa para evitar uma perda incerta e
menor. **Medir na bancada, com um módulo tapado, antes de fechar.**

> [!NOTE]
> O AEM10900 faz **um** ponto de máxima potência para o arranjo inteiro, e
> os módulos da faceta frontal e os dos chanfros veem iluminações muito
> diferentes. Um único MPPT para seis módulos desiguais é compromisso, não
> otimização — é o preço de ter um colhedor só.

### Indutor

Aqui a ficha se contradiz, e vale registrar em vez de escolher calado:

| Fonte | Com 6,8 µH | Com 3,3 µH | Com 4,7 µH |
|---|---|---|---|
| Tabela 6 da ficha | 65,5 mA | 175,5 mA | cerca de 95 mA, escalando |
| Fórmula da seção 6.7.2 (`579 / L`) | 85 mA | 175 mA | 123 mA |

O pico estimado do arranjo é de **cerca de 88 mA** ao meio-dia. Os dois
caminhos de conta dão, com 4,7 µH, de 95 a 123 mA — acima dos 88 mA
necessários pelas duas contas, que é por isso que **4,7 µH é a escolha**
(TDK VLS252012HBX-4R7M-1, 1,4 A de saturação, 240 mΩ).

O de 6,8 µH é o valor das curvas de rendimento publicadas e fica no
footprint para comparação na bancada. **A divergência entre a tabela e a
fórmula é pergunta em aberto para a e-peas.**

## Medidor de bateria

**O sensor de 7 mΩ é interno ao CI.** A peça é a **MAX17262REWL+T**, e o
"R" do código é justamente a versão com o resistor de medida integrado,
entre os pinos `BATT` e `SYS` ([13](../docs/13-placa-nova.md#energia),
[14](../docs/14-hardware-placa-nova.md#carga), [15](../docs/15-avaliacao-componentes.md#medição-da-carga)).

> [!CAUTION]
> **Não existe resistor de medida externo nesta placa.** Um rascunho
> anterior deste esquemático criou um, com pinos `CSP` e `CSN` que o CI nem
> tem. Montado assim, a resistência do caminho dobraria e o medidor erraria
> **toda** corrente e toda estimativa de carga — e o erro é do tipo que não
> aparece na bancada, porque o medidor continua respondendo.

Queda no sensor interno, na carga a 600 mA (**conta**):

```
V = 0,6 A × 7 mΩ = 4,2 mV
P = 0,6² × 0,007 = 2,5 mW
```

No pior pico de descarga, que é a partida do receptor (100 mA):

```
V = 0,1 A × 7 mΩ = 0,7 mV
```

Desprezível nos dois casos. O CI aguenta **1,7 A contínuos**
([15](../docs/15-avaliacao-componentes.md#medição-da-carga)), muito acima de
tudo que a placa faz.

## Conversores do nPM1300

Os dois indutores de **2,2 µH com DCR abaixo de 400 mΩ em 0806** e a lista
de capacitores (dois de 1,0 µF/10 V, nove de 10 µF/25 V, um de 2,2 µF/16 V
e um de 100 nF) **vêm da lista de referência da Nordic**, tabelas 39 e 40
da ficha, configuração 1. A peça escolhida é a Murata
DFE201610E-2R2M=P2, com 140 mΩ.

As tensões saem de resistores, não de firmware:

| Trilho | Resistor | Valor | Por quê |
|---|---|---|---|
| BUCK1, 1,8 V | `RVSET1` | 47 kΩ, 5 % no máximo | a tabela 18 do VSET1 vai de 1,0 a 2,7 V |
| BUCK2, 3,0 V | `RVSET2` | 150 kΩ, 5 % no máximo | **a tabela do VSET1 não tem 3,0 V; a do VSET2 tem** (tabela 19) |

> [!CAUTION]
> **Nenhum `VSET` pode ficar aberto** (ficha), e é o BUCK2 que alimenta o
> MCU: se o `RVSET2` estiver errado ou ausente, o aparelho não liga. O
> BUCK1 está travado em 1,8 V também no devicetree, porque o `V_IO` do
> receptor tem máximo absoluto de **1,98 V** e o registrador aceitaria até
> 3,3 V.

**A ondulação dos conversores não foi recalculada aqui**: depende da
frequência de chaveamento interna do nPM1300, que a ficha traz e que não
foi lida nesta sessão. O indutor e os capacitores são os da referência, o
que é a forma de o projeto herdar essa conta pronta.

## Desacoplamento

Regra seguida, sem conta nova: **100 nF junto de cada pino de alimentação
de cada CI**, mais o volume que a ficha de cada peça pede.

| Onde | Valor | Origem |
|---|---|---|
| Cada pino de alimentação de CI | 100 nF, 0402, X7R | prática, e a lista da Nordic |
| `VBUS` | 10 µF, 25 V | ficha do nPM1300 (o CI tolera 22 V em transitório) |
| `1V8`, junto do módulo GNSS | ferrite + 10 µF | ficha do MAX-F10S, ondulação abaixo de 50 mV |
| `MMC5633NJL` `VDD` | pelo menos 2,2 µF | ficha |
| `SD3V0`, junto da flash | 22 µF | [14](../docs/14-hardware-placa-nova.md#alimentação) |
| `CSRC` e `CINT` do AEM10900 | 22 µF, 6,3 V, 0402 | lista mínima da e-peas |
| `CSTO` do AEM10900 | 22 µF, 10 V, 0603 | dá cerca de 9 µF com 4 V aplicados, acima dos 5 µF efetivos que a ficha pede |
| Saída do TPS7A02 | pelo menos 0,5 µF efetivos | ficha |
| Bombeamento do REG710 | 0,22 µF, 25 V | ficha |
| Entrada **e** saída do REG710 | 10 µF cada | [14](../docs/14-hardware-placa-nova.md#componentes-principais), "como na V3"; a entrada tinha sido esquecida |

## Capacitor de volume no módulo

O `3V0` já tem os 100 nF por pino, e o BM20C é a carga com o transitório
mais rápido do trilho: o rádio puxa 10,9 mA em rajada a +8 dBm. O buck
responde, mas não instantaneamente; entre a borda e a resposta dele quem
segura a tensão é o capacitor local.

Com uma resposta de laço da ordem de 10 µs e 50 mV de queda aceita
(**conta**):

```
C = I × t / ΔV = 10,9 mA × 10 µs / 50 mV = 2,2 µF
```

**Escolhido: 4,7 µF**, junto do pino de alimentação do módulo — não por
ser o degrau E-series acima de 2,2 µF (não é; E12 dá 2,7 e E6 dá 3,3), mas
porque **a lista de compras já traz essa linha** (4,7 µF, 16 V, X5R, 0603,
para o `VDD` do MMC5633NJL) e ela cobre os dois usos. Os outros trilhos já tinham volume declarado (22 µF
no `SD3V0`, 10 µF no `1V8`, 10 µF na saída do REG710) e o `3V0` do MCU
não tinha.

## Proteção das teclas

As três teclas são o que o ciclista toca. Até o dry-run de 2026-09-23 iam
direto ao pino do MCU: o USB tinha TVS e elas, nada.

Rede adotada: **100 Ω em série** e **1 nF ao `GND`** em cada uma
([03](03-netlist.md#pinos-de-configuração-amarrados-em-cobre),
[05](05-materiais.md#folha-6--interface)).
Confere (**conta**, com o pull-up interno do nRF em cerca de 13 kΩ):

```
τ = (13 kΩ + 100 Ω) × 1 nF = 13,1 µs
queda no nível baixo = 3,0 V × 100 / (13.000 + 100) = 22,9 mV
```

Os 13 µs são rápidos para a leitura de tecla e lentos para uma descarga; os
23 mV de queda não tiram o nível baixo do lugar. O capacitor ainda ajuda no
ressalto do contato.

> [!CAUTION]
> **A tecla central é a pior das três.** Uma descarga nela entra também no
> `SHPHLD` do nPM1300 e pode religar o aparelho. Por isso a derivação para
> o `SHPHLD` sai **depois** dos 100 Ω, e não do contato: assim uma rede só
> cobre os dois ramos. Contra o pull-up interno de 50 kΩ do PMIC, os 100 Ω
> dão 11 mV de erro a 5,5 V e o 1 nF dá τ de 50 µs — nada para um botão nem
> para o toque longo de 10 s. Enquanto a
> [ligação dela](03-netlist.md#interface) não for decidida, a proteção
> dela tem de cobrir os dois caminhos.

## Buzzer piezo

O Same Sky CPT-1117-83-SMT-TR dá 83 dB a 10 cm com 5 Vpp; os dois pinos em
contrafase entregam 6 Vpp sem nenhuma fonte a mais.

Um piezo é carga **capacitiva** (dezenas de nF), e a contrafase põe
**6 Vpp** sobre ele — 3,0 V de cada lado, em oposição. A corrente média é
pequena; o que precisa de resistor é o **pico da borda** (**conta**, com
15 nF típicos e 4 kHz):

```
I(média) = 2 × C × V × f = 2 × 15 nF × 6,0 V × 4 kHz = 0,72 mA
```

O laço que carrega o piezo passa pelos **dois** resistores em série, de
modo que o pico é `6 V ÷ 2R` e a constante de tempo é `2R × C`:

| `R` em cada pino | Pico (`6 V ÷ 2R`) | Borda (`2R × C`) | Fração do período |
|---|---|---|---|
| 100 Ω | 30,0 mA | 3,0 µs | 1,2 % |
| **330 Ω** | **9,1 mA** | 9,9 µs | 4,0 % |
| 470 Ω | 6,4 mA | 14,1 µs | 5,6 % |

**Escolhido: 330 Ω** em cada pino. Os 30 mA de pico com 100 Ω já passam do
que um GPIO entrega, e o resistor **não custa volume**: o piezo é
capacitivo, logo a tensão final é a mesma, só a borda amolece 4 % do
período.

Os "cerca de 5 mA" do [orçamento de energia](../docs/13-placa-nova.md#orçamento-de-energia)
são estimativa conservadora; a conta capacitiva dá 0,72 mA, e o valor
real depende da cavidade ressonante. Não medido.

## Resistor de série no SPI da flash

A flash roda a **8 MHz**, e o 197º harmônico de 8 MHz cai a 0,58 MHz do
centro de L1 (1575,42 MHz): é o único sinal rápido perto do receptor
([04](04-pcb-e-caixa.md)). A mitigação é amaciar a borda com um resistor
em série junto do pino do MCU — mas amaciar demais quebra a temporização.

Com a capacitância de entrada da flash mais a trilha entre 10 e 20 pF
(**estimativa**), e meio período de 62,5 ns a 8 MHz (**conta**):

| `R` | `C` = 10 pF | `C` = 20 pF |
|---|---|---|
| **33 Ω** | 0,33 ns (0,5 %) | 0,66 ns (1,1 %) |
| 100 Ω | 1,00 ns (1,6 %) | 2,00 ns (3,2 %) |

**Escolhido: 33 Ω**, nas três linhas — mas **não todas no mesmo lugar**: o
`SCK` e o `MOSI` levam o resistor junto do pino do MCU, que é quem os
aciona, e o `MISO` junto do pino `SO` **da flash**, que é quem aciona
aquela linha. Um resistor na ponta que recebe não amacia borda nenhuma:
faz um passa-baixas na entrada e deixa a trilha irradiando igual. A constante
de tempo fica em cerca de 1 % do meio período no pior caso: amolece a
borda sem chegar perto de atrapalhar o relógio. Os 100 Ω também caberiam,
e amaciariam mais; 33 Ω é o compromisso conservador, e o valor final sai da
medida do espectro na banda (`UBX-MON-SPAN`) com a flash trabalhando.

> [!NOTE]
> Trocar os 8 MHz por 16 ou 21,33 MHz tiraria o harmônico da banda de vez
> ([13](../docs/13-placa-nova.md#antena-gnss-dentro-da-caixa) escolheu essas
> frequências justamente por isso), e é **uma linha do devicetree**. Mas
> `docs/14` amarra os 8 MHz ao **modo de baixo consumo** da MX25R6435F, que
> é de onde vêm os 3,1 mA da [corrente do `3V0`](#corrente-de-cada-trilho).
> Subir a frequência é trocar ruído por consumo: **decisão do dono**, não
> deste documento.

## Rampa do V_IO na partida

O `V_IO` do MAX-F10S aceita rampa entre **25 e 35.000 µs/V** (máximo
absoluto, tabela 12 da ficha). Fora disso, a ficha diz que o módulo pode
ser danificado — é restrição de partida, e quem a cumpre é a partida suave
do BUCK1 (**conta**):

| Partida do BUCK1 | Rampa | Dentro da faixa? |
|---|---|---|
| 0,5 ms | 278 µs/V | sim |
| 1 ms | 556 µs/V | sim |
| 5 ms | 2.778 µs/V | sim |

Qualquer partida entre meio milissegundo e cinco milissegundos cabe com
folga nas duas pontas. E o valor real **está na ficha e já tinha sido
levantado**: o buck do nPM1300 parte em cerca de **1,2 ms** (3,3 V com
10 µF), perto de **360 µs/V**
([15](../docs/15-avaliacao-componentes.md#gnss)) — dentro da faixa, com
mais de uma década de folga para cada lado. **Confirmar com osciloscópio**
na primeira energização, antes de soldar o receptor.

## Trilho de 5 V, com a Sharp

O REG710 entrega 30 mA. A carga é só a tela (**conta**):

```
Sharp a 1 quadro/s: 175 µW ÷ 5,0 V = 35 µA
REG710 parado:      65 µA
total:              cerca de 100 µA, de 30.000 µA
```

Folga de 300×. A luz **não** sai daqui: ela vem do `3V3BL`, pela `LDSW2`.

## O que não foi calculado

Dito aqui para não passar por esquecimento.

| Não calculado | Por quê | Quando vira necessário |
|---|---|---|
| Ondulação e resposta transitória dos bucks | os valores vêm da lista de referência da Nordic, tabelas 39 e 40 | se algum componente sair dessa lista |
| Largura de trilha em milímetros | depende da pilha do fabricante, que [04](04-pcb-e-caixa.md#camadas) ainda não tem; a [corrente](#corrente-por-trilho-e-largura-de-trilha) está fixada | no layout |
| Impedância da linha da antena GNSS e o par de 90 Ω do USB | idem | no layout |
| Integridade de sinal do SPI do display | 2 MHz no máximo, trilha curta: não é regime crítico | se a FPC ficar longa |
| Comportamento térmico da caixa fechada | precisa do material e da geometria reais | no protótipo |
| Brown-out do nRF54LM20A e `VSYSPOF` do nPM1300 | fichas não lidas nesta rodada ([07](07-sequencias-e-protecao.md)) | antes do primeiro protótipo |
| Domínio de tensão dos pinos digitais do nPM1300 | nenhum documento do projeto registra ([pull-ups](#pull-ups-do-i²c)) | **antes do layout** |
| Tolerância do cristal de 32,768 kHz do módulo | a ficha do BM20C não informa, e o ANT+ pede ±50 ppm | antes de confiar no ANT+ |
| Capacitância efetiva dos cerâmicos sob tensão | só o caso do `CSTO` foi considerado; um 10 µF de 25 V perde metade a 5 V | ao fechar o volume de cada trilho |
