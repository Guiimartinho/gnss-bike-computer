# Cálculos

Cada valor de componente do esquemático, com a conta que o justifica e a
origem de cada número de entrada. Conta feita aqui vem marcada como
**conta**; número de ficha técnica vem marcado como **ficha**; e o que
depende de medida está dito como tal.

**Nesta página:** [Corrente de cada trilho](#corrente-de-cada-trilho) · [Pull-ups do I²C](#pull-ups-do-i²c) · [Luz do display](#luz-do-display) · [LED RGB](#led-rgb) · [Calor do carregador](#calor-do-carregador) · [Colheita solar](#colheita-solar) · [Medidor de bateria](#medidor-de-bateria) · [Conversores do nPM1300](#conversores-do-npm1300) · [Desacoplamento](#desacoplamento) · [O que não foi calculado](#o-que-não-foi-calculado)

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

**Folga: 23 mA contra 200 mA, 11 %.**

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

As duas últimas linhas não são a soma das de cima: as correntes da ficha
são medidas a 3,0 V, e o mesmo consumo a 1,8 V pede **mais** corrente. O
receptor gasta 46,8 mW a 1,8 V contra 57 mW a 3,0 V, e 46,8 mW ÷ 1,8 V dá
os 26 mA do rastreio.

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

### Com o JDI LPM027M128B/C

LED de **16 mA a 2,67 V** (ficha). Com `V_DS` do MOSFET em cerca de 50 mV:

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

Arranjo: `VSYS` → resistor → LED → dreno do MOSFET → fonte no `GND`, com o
pino do MCU na porta. **O resistor é de 1 kΩ**, o valor que a
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

**O brilho varia cerca de três vezes ao longo da faixa**, e nenhum canal
apaga: o verde e o azul chegam ao fim da bateria com 0,29 mA, fracos mas
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

## O que não foi calculado

Dito aqui para não passar por esquecimento:

| Não calculado | Por quê | Quando vira necessário |
|---|---|---|
| Ondulação e resposta transitória dos bucks | os valores vêm da lista de referência da Nordic | se algum componente sair da lista |
| Impedância da linha da antena GNSS | precisa da pilha de camadas do fabricante | no layout |
| Par diferencial de 90 Ω do USB | idem | no layout |
| Integridade de sinal do SPI do display | 2 MHz no máximo, trilha curta: não é regime crítico | se a FPC ficar longa |
| Comportamento térmico da caixa fechada | precisa do material e da geometria reais | no protótipo |
| Tolerância do cristal de 32,768 kHz do módulo | a ficha do BM20C não informa, e o ANT+ pede ±50 ppm | antes de confiar no ANT+ |
