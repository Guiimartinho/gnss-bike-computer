# Lista de nós

O esquemático como lista: cada nó, de onde sai e aonde chega. É esta a
forma que a ferramenta confere — `python hardware_gnssbike/net_check.py`
lê as tabelas desta página e as compara com o devicetree da placa em
[`zephyr_app/boards/gnss/gnssbike/`](../zephyr_app/boards/gnss/gnssbike/).

**Nesta página:** [Como ler](#como-ler) · [Alimentação](#nós-de-alimentação) · [MCU](#nós-do-mcu) · [Sem ligação ao MCU](#nós-sem-ligação-ao-mcu) · [Contagem](#contagem)

> [!WARNING]
> Nenhum destes nós existe em cobre. Não há layout nem placa.

## Como ler

- **Nó**: o nome do sinal, em maiúsculas, como aparecerá no esquemático.
- **Pino do MCU**: o pino do nRF54LM20A, exatamente como o devicetree o
  declara. O esquemático liga por **nome de sinal**; o de-para para o pad do
  módulo MinewSemi ME54BS13 sai da ficha V1.0.0 (p. 6 a 9) e está
  transcrito em [`cad/parts.py`](cad/parts.py) (`PADS_ME54BS13`, os 80
  pads: 20 castelados numerados de 1 a 20 e uma matriz LGA de 60, de `A0` a
  `F9`). **O espelhamento desse mapa ainda precisa ser conferido num módulo
  real** ([01](01-esquematico.md#em-aberto-nesta-folha-1)).
- **Tipo**: `alim` (alimentação), `dig` (digital), `ana` (analógico),
  `rf` (radiofrequência).
- Toda referência de terra é **GND**, plano contínuo (ver
  [04](04-pcb-e-caixa.md#camadas)).

## Nós de alimentação

| Nó | Tensão | Sai de | Chega em | Tipo |
|---|---|---|---|---|
| `VBUS` | 4,0 a 5,5 V | conector USB-C, pinos A4, A9, B4, B9 | nPM1300 `VBUS`; TVS ESD761; C 10 µF/25 V | alim |
| `VBUSOUT` | = `VBUS` | nPM1300 `VBUSOUT` | pad 9 (`VBUS`) do ME54BS13 (detecção e PHY do USB); topo do divisor `DIS_STO_CH`; C 1 µF | alim |
| `VBAT` | 3,0 a 4,2 V | positivo da célula, pelo `JP101` | `BATT` e `TH` do MAX17262 (o sensor de 7 mΩ é **interno** ao CI, entre `BATT` e `SYS`); 100 nF | alim |
| `VBAT_SYS` | = `VBAT` menos a queda no sensor | `SYS` do MAX17262 | nPM1300 `VBAT`; ADP5091 `BAT` e `SYS` (o colhedor carrega a célula por aqui, e a corrente passa pelo sensor); TPS7A02 `IN` e `EN`; 2 × 22 µF e 100 nF | alim |
| `VSYS` | `VBAT` ou `VBUS` | nPM1300 `VSYS` | entradas de `BUCK1`, `BUCK2` e `LDSW2`; anodos do LED RGB e do LED de carga | alim |
| `3V0` | 3,0 V | nPM1300 `BUCK2` | `VDD` do ME54BS13 (pad 19); BMP585, BMI270, MMC5633NJL e OPT3001; buzzer; entrada da `LDSW1`; o `VDD`/`VDDA` do display pelo `JP401`; o receptor GNSS pelo `JP103` (nós `3V0_BLOCO` e `3V0_GNSS`, abaixo); e o `IN` do REG710 **só no plano B** | alim |
| `3V0_BLOCO` | 3,0 V | `JP103`, do `3V0` | `FB301`: é o nó em que se mede a corrente do receptor | alim |
| `3V0_GNSS` | 3,0 V | `FB301`, ferrite de 600 Ω e 150 mΩ | MAX-F10S `VCC` **e** `V_IO` (opção 1 da tabela 35 do manual de integração: os dois juntos, `VIO_SEL` aberto); 10 µF e 100 nF | alim |
| `1V8` | 1,8 V | nPM1300 `BUCK1`, por filtro LC | **nada**, desde 2026-09-26: o receptor passou ao `3V0`. Montado, sem carga, desligado pelo firmware ([01](01-esquematico.md#folha-3--gnss)) | alim |
| `SD3V0` | 3,0 V | nPM1300 `LDSW1`, do `3V0` | MX25R6435F `VCC` | alim |
| `3V3BL` | 3,3 V | nPM1300 `LDSW2`, do `VSYS` | anodo da luz do painel, por `R_BL` de 39 Ω; o catodo volta ao dreno do `Q401`. Com o JDI o LED está **dentro do painel** e os dois fios chegam pelo **`J402`, um FPC próprio de 5 vias e passo 0,5 mm** ([06](06-conectores-e-pontos-de-teste.md#j402--luz-do-lpm027m128c)) | alim |
| `VBCKP` | 1,8 V | TPS7A02, do `VBAT` | MAX-F10S `V_BCKP` | alim |
| `5V0` | 5,0 V | REG710NA-5, do `3V0` | `VDD` e `VDDA` do display, pelo `JP401` — **não montado**: existe só no plano B, com a Sharp | alim |
| `GND` | 0 V | — | plano contínuo | alim |

> [!CAUTION]
> **O `5V0` deixou de existir na placa montada.** A decisão de 2026-09-23
> pelo **JDI LPM027M128C**, de 3,0 V, tirou o `U401` (REG710NA-5), o `C401`
> de bombeamento e os dois de 10 µF da montagem: o `JP401` vira um **0 Ω
> fixo na posição do 3,0 V** e a posição do 5 V fica sem peça
> ([01](01-esquematico.md#folha-4--display)). O nó continua na tabela porque
> o footprint continua no desenho, e é ele que faz o plano B — a Sharp
> LS027B7DH01A, de 5 V — ser uma troca de montagem.
>
> A regra antiga não muda de valor: `3V0` e `5V0` chegam ao mesmo pino do
> display e **nunca** podem alcançá-lo ao mesmo tempo, porque o JDI tem
> máximo absoluto de 3,6 V e queima com 5 V. O pad do meio do `JP401` é um
> só, e é isso que torna o caso impossível.

## Nós do MCU

Os 31 pinos que o [devicetree](../zephyr_app/boards/gnss/gnssbike/gnssbike-pinctrl.dtsi)
usa, mais os dois reservados. Confira com `python tools/fw/board_check.py`.

### Armazenamento · `spi00`

| Nó | Pino do MCU | Outro extremo | Tipo | Nota |
|---|---|---|---|---|
| `NOR_SCK` | P2.01 | MX25R6435F pino 6 (`SCLK`) | dig | P2.01 é pino de clock (tabela 79) |
| `NOR_MOSI` | P2.02 | MX25R6435F pino 5 (`SI/IO0`) | dig | — |
| `NOR_MISO` | P2.04 | MX25R6435F pino 2 (`SO/IO1`) | dig | — |
| `NOR_CS` | P2.05 | MX25R6435F pino 1 (`CS#`) | dig | ativo baixo, pull-up de 100 kΩ ao `SD3V0` |

### Display · `spi22` e `pwm20`

| Nó | Pino do MCU | Outro extremo | Tipo | Nota |
|---|---|---|---|---|
| `DISP_SCK` | P3.03 | FPC pino 1 (`SCLK`) | dig | pino de clock; 1 MHz típico, 2 MHz máximo |
| `DISP_MOSI` | P3.00 | FPC pino 2 (`SI`) | dig | — |
| `DISP_CS` | P3.02 | FPC pino 3 (`SCS`) | dig | **ativo alto**, ao contrário do costume |
| `DISP_EXTCOMIN` | P3.06 | FPC pino 4 (`EXTCOMIN`) | dig | inverte o VCOM; 1 Hz sem luz, cerca de 120 Hz com luz |
| `DISP_ON` | P3.05 | FPC pino 5 (`DISP`) | dig | liga a matriz |
| `DISP_PWR_EN` | P3.07 | REG710 `EN` | dig | **pino livre**: com o JDI o REG710 não é montado e nada o usa; volta a ter função só no plano B, cortando os 65 µA do conversor. Ver o aviso abaixo |
| `BL_PWM` | P3.08 | porta do MOSFET da luz | dig | `pwm20`, canal 0; a luz é o LED **de dentro** do painel |

> [!NOTE]
> **O `DISP_PWR_EN` ficou sem carga.** Com o JDI LPM027M128C não há REG710
> para habilitar, de modo que **P3.07 volta a ficar disponível**. O
> [devicetree](../zephyr_app/boards/gnss/gnssbike/gnssbike_nrf54lm20a_cpuapp.dts)
> continua trazendo `power-gpios = <&gpio3 7 ...>` no nó do display, e isso
> é de propósito: é o que faz o plano B, com a Sharp e os 5 V, funcionar sem
> mexer no firmware. Com o JDI montado o pino aciona um regulador que não
> existe, e o pull-down de 100 kΩ do `R403` segura o nível. Quem quiser o
> pino para outra coisa tira essa linha do devicetree; até lá, o nó fica
> nesta tabela para o `net_check.py` continuar batendo.

### GNSS · `uart21`

Receptor e MCU no mesmo `3V0` desde 2026-09-26: as quatro linhas vão de
pino a pino. O tradutor de nível TXU0204 que ficava aqui saiu com o trilho
de 1,8 V ([01](01-esquematico.md#folha-3--gnss)).

| Nó | Pino do MCU | Outro extremo | Tipo | Nota |
|---|---|---|---|---|
| `GNSS_TX` | P1.04 | `RXD` do MAX-F10S | dig | pino de clock, gasto de propósito com a UART |
| `GNSS_EXTINT` | P1.08 | `EXTINT` do MAX-F10S | dig | ligado na placa; **reservado no firmware**, que ainda não o usa |
| `GNSS_RX` | P1.05 | `TXD` do MAX-F10S | dig | — |
| `GNSS_TIMEPULSE` | P1.09 | `TIMEPULSE` do MAX-F10S | dig | ligado na placa; **reservado no firmware**; divide o pino com o `SAFEBOOT_N` por 1 kΩ interno: **sem pull-down** |
| `GNSS_RESET_N` | P1.06 | `RESET_N` do MAX-F10S | dig | o pino do MCU é dreno aberto e só puxa para baixo; o pull-up de 7 a 13 kΩ é interno ao módulo |

> [!CAUTION]
> **Nada de pull-down externo no `TIMEPULSE`.** Ele divide o pino com o
> `SAFEBOOT_N` por 1 kΩ interno, e o módulo entra em safeboot se o pino
> estiver baixo na partida. É justamente o resistor que alguém acrescenta
> no layout para "garantir o nível".

> [!NOTE]
> Um rascunho anterior deste documento passou o `RESET_N` pelo tradutor e
> concluiu que "o tradutor não cabia", cinco sinais para quatro canais, e
> deixou o `EXTINT` sem componente. **Era invenção**: a
> [especificação](../docs/14-hardware-placa-nova.md#gnss) já dizia que o
> `RESET_N` dispensa o tradutor. Pior, o rascunho punha o `RX` num canal
> A→B e o `RESET_N` num B→A, o que é **saída contra saída** nos dois pares:
> o módulo nunca teria falado com o MCU.

### Sensores · `i2c23`

| Nó | Pino do MCU | Outro extremo | Tipo | Nota |
|---|---|---|---|---|
| `SENS_SDA` | P1.29 | BMP585, BMI270, MMC5633NJL, OPT3001 | dig | pull-up de 4,7 kΩ ao `3V0` |
| `SENS_SCL` | P1.03 | os mesmos quatro | dig | **pino de clock**, como o TWIM exige; pull-up de 4,7 kΩ |
| `IMU_INT` | P1.10 | BMI270 `INT1` | dig | — |
| `BARO_INT` | P1.12 | BMP585 `INT` | dig | — |

Endereços: BMP585 `0x47` (SDO no `3V0`), BMI270 `0x68` (SDO no GND),
MMC5633NJL `0x30`, OPT3001 `0x44` (ADDR no GND). **Nada varre este
barramento**: `0x7E` poria o MMC5633NJL em I3C até faltar energia.

### Energia · `i2c30`

| Nó | Pino do MCU | Outro extremo | Tipo | Nota |
|---|---|---|---|---|
| `PWR_SDA` | P0.02 | nPM1300, MAX17262 | dig | pull-up de 4,7 kΩ ao `3V0`; o ADP5091 não tem I²C |
| `PWR_SCL` | P0.03 | os mesmos dois | dig | **pino de clock**; pull-up de 4,7 kΩ |
| `PMIC_INT` | P0.00 | nPM1300 `GPIO3` | dig | eventos de botão, carga e falha |

Endereços: nPM1300 `0x6B`, MAX17262 `0x36`. O ADP5091, que substituiu o
AEM10900 em 2026-09-25, não tem barramento nenhum: tudo nele é resistor.

### Interface

| Nó | Pino do MCU | Outro extremo | Tipo | Nota |
|---|---|---|---|---|
| `KEY_L` | P1.26 | tecla esquerda, ao GND | dig | pull-up interno, ativo baixo |
| `KEY_C` | P1.27 | `R605` de 100 Ω, o Schottky `D107` (anodo do lado do MCU) e daí o nó da tecla central, que também é o `SHPHLD` do nPM1300 | dig | pull-up interno; **o Schottky isola o pino do nó do PMIC**, que sobe até 5,5 V com o cabo (decisão do dono, 2026-09-26: ver o aviso abaixo) |
| `KEY_R` | P1.30 | tecla direita, ao GND | dig | — |
| `RGB_R` | P1.16 | **porta** do MOSFET do vermelho | dig | `pwm22` canal 0; o LED vai do `VSYS` ao dreno, por `R_LEDR` |
| `RGB_G` | P1.19 | **porta** do MOSFET do verde | dig | `pwm22` canal 1 |
| `RGB_B` | P1.22 | **porta** do MOSFET do azul | dig | `pwm22` canal 2 |
| `BUZ_A` | P1.25 | buzzer, lado A | dig | `pwm21` canal 0 |
| `BUZ_B` | P1.28 | buzzer, lado B | dig | `pwm21` canal 1, em contrafase |

> [!NOTE]
> **A tecla central está ligada em dois lugares de propósito, e desde
> 2026-09-26 isso é legítimo.** A especificação
> ([`docs/14`](../docs/14-hardware-placa-nova.md#componentes-principais))
> mandava a tecla só ao `SHPHLD`, porque o pull-up de 50 kΩ dele se refere
> ao maior entre `VBAT` e `VBUS` — até 5,5 V com o cabo — e um pino de
> 3,0 V amarrado nesse nó teria o diodo de proteção conduzindo; e em ship
> mode, com o `3V0` em 0 V, o mesmo diodo grampearia o `SHPHLD` perto de
> 0,7 V, que o nPM1300 pode ler como tecla presa. O dono decidiu manter a
> leitura por GPIO **e** resolver os dois problemas em cobre: o `D107`
> (BAT54WS) fica entre o `R605` e o nó da tecla, com o catodo no nó.
> Apertada, a tecla leva os dois nós ao GND e o MCU lê baixo (0,3 a 0,4 V
> de queda, abaixo dos 0,9 V de `V_IL`); solta, o catodo fica acima do
> anodo e o P1.27 só vê o próprio pull-up — e em ship mode o anodo, sem
> alimentação, não tem por onde grampear o nó ([01](01-esquematico.md#folha-6--interface)).
> O firmware continua lendo a tecla pelo GPIO; assinar
> `NPM13XX_EVENT_SHIPHOLD_PRESS` fica como melhoria ([10](../docs/10-status-do-port.md#defeitos-abertos)).

### Console · `uart20`

| Nó | Pino do MCU | Outro extremo | Tipo | Nota |
|---|---|---|---|---|
| `CON_TX` | P1.00 | ponto de teste `TP201` | dig | 115200 baud; não sai na caixa. O `TP203`, de `GND`, fica ao lado: sem ele não dá para ligar um conversor USB-serial ([06](06-conectores-e-pontos-de-teste.md#pontos-de-teste)) |
| `CON_RX` | P1.31 | ponto de teste `TP202` | dig | — |

### Dedicados do módulo

Estes não são GPIO: são pads dedicados do módulo, e vão pelo número
castelado da ficha ME54BS13 V1.0.0.

| Nó | Pad do ME54BS13 | Outro extremo | Tipo | Nota |
|---|---|---|---|---|
| `USB_DM` | 7 | USB-C A7 e B7 | dig | par de 90 Ω diferencial |
| `USB_DP` | 8 | USB-C A6 e B6 | dig | idem |
| `MOD_VBUS` | 9 | `VBUSOUT` | alim | detecção e PHY do USB. **A faixa aceita neste pad não foi levantada na ficha do ME54BS13**; os 4,4 a 5,5 V que este documento trazia eram do BM20C |
| `SWDIO` | 5 | Tag-Connect pino 2 e `J202` pino 2 | dig | — |
| `SWDCLK` | 6 | Tag-Connect pino 4 e `J202` pino 4 | dig | — |
| `MOD_RESET` | 4 | Tag-Connect pino **3** e `J202` pino 10 | dig | corrigido em 2026-09-25: a ficha `TC2030-CTX_1.pdf` põe o `nRESET` no contato 3, e o 6 é o `SWO`. O `J202` (conector Cortex de 10 vias, desde 2026-09-26) segue a pinagem da ARM: `nRESET` no 10 |
| `3V0_MOD` | 19 | `JP102`, do `3V0` | alim | com o 100 nF e o 4,7 µF de volume ao lado |
| — | 2 | **aberto** | rf | saída para antena externa; o módulo já traz a antena de PCB |
| `GND` | 1, 3, 10, 11, 20, `D0`, `E0`, `F0` | plano | alim | oito pads de terra, cinco castelados e três da matriz |

## Nós sem ligação ao MCU

| Nó | Sai de | Chega em | Nota |
|---|---|---|---|
| `CC1`, `CC2` | USB-C A5, B5 | nPM1300 `CC1`, `CC2`, com o **TPD4E05U06** | o Rd de 5,1 kΩ é interno ao nPM1300; o TPD4E05U06 protege as linhas de CC e de dados |
| `NTC_BAT` | NTC do pack | nPM1300 `NTC` | 10 kΩ, B3380, JEITA |
| `NTC_SOLAR` | **`RT101`, o NTC da face de trás da placa**, sobre `R106` de 4,87 kΩ, alimentado pelo `SRC` | `IN+` do comparador `U105` (TLV7031) | o corte térmico da carga solar, em cobre: quente, o divisor passa da metade do `SRC`. A [lista de materiais](05-materiais.md#folha-1--energia) escolhe este NTC; o conector reserva a via 4 para o do pack, e **os dois nunca são montados juntos** ([01](01-esquematico.md#folha-1--energia)) |
| `REF_TERM` | `R124`/`R125`, 100 kΩ cada, do `SRC` | `IN−` do `U105` | a metade do `SRC`: o limiar de 45 °C |
| `TERM_QUENTE` | saída do `U105` | anodo do `D106` (BAT54WS) | alto acima de 45 °C |
| `DIS_SW` | `R104` de 100 kΩ, do `VBUSOUT`; catodo do `D106`; `R105` de 180 kΩ ao `AGND` | ADP5091 `DIS_SW` | **o USB bloqueia a carga solar em hardware**, sem pino do MCU: com o cabo o pino vê 5,5 × 180 ÷ 280 = 3,5 V (**conta**), acima do 1 V de nível alto e abaixo dos 6,0 V de máximo absoluto; o comparador entra no mesmo nó em OU de diodos ([07](07-sequencias-e-protecao.md#o-corte-térmico-da-carga-solar)) |
| `SW_DCDC` | ADP5091 `SW` | `L103`, 22 µH | nó curto, sem plano embaixo; a outra ponta do indutor vai ao `SRC` (boost) |
| `SRC` | painéis solares, por `R113`/`R114`/`R115` de 0 Ω (um por grupo) | ADP5091 `VIN`; `C115`; `D105` (ESD); topo do divisor do NTC e da referência; `R117` do MPPT | 6 módulos de 3 células em paralelo, 2,07 V em aberto |
| `MPPT` | `R117` de 4,3 MΩ do `SRC` | ADP5091 `MPPT`, com `R116` de 18 MΩ ao `AGND` | ponto de máxima potência a 18 ÷ 22,3 = 81 % da tensão em aberto (**conta**) |
| `ADP_REF` | ADP5091 `REF` | topo de `R118` (`TERM`) e `R120` (`SETSD`) | figura 42 da ficha: os divisores penduram no `REF`, não no `BAT`; corrigido em 2026-09-26 |
| `TERM` | `R118` de 4,32 MΩ, do `REF` | ADP5091 `TERM`, com `R119` de 2,67 MΩ ao `AGND` | fim de carga em 3/2 × 1,011 × (1 + 4,32/2,67) = 3,97 V (equação 6, **conta**) |
| `SETSD` | `R120` de 6,65 MΩ, do `REF` | ADP5091 `SETSD`, com `R121` de 3,32 MΩ ao `AGND` | corte de descarga em 1,011 × (1 + 6,65/3,32) = 3,04 V (equação 8, **conta**) |
| `MINOP`, `VID`, `CBP` | `R122` de 402 kΩ, `R123` de 111 kΩ, `C119` de 10 nF | ADP5091 | boost parado com o painel abaixo de 1,0 V; regulador desabilitado com estado definido; a amostra da tensão em aberto |
| `LED_CHG` | nPM1300 `LED1` | LED de carga, anodo no `VSYS` | acende com o aparelho desligado |
| `LED_ERR` | nPM1300 `LED0` | LED de erro, anodo no `VSYS` | — |
| `SHPHLD` | tecla central | nPM1300 `SHPHLD` | pull-up interno de 50 kΩ; liga fora do ship mode |
| `RGB_R_D`, `RGB_G_D`, `RGB_B_D` | catodo de cada cor do LED | `R_LEDR`, `R_LEDG` ou `R_LEDB`, e daí ao dreno do seu MOSFET | o LED é de **anodo comum**: o anodo vai direto ao `VSYS` e **o resistor fica do lado do catodo**, um por cor. Pôr o resistor no anodo o deixaria em paralelo com o die, que veria o `VSYS` nu — até 5,5 V com cabo ([02](02-calculos.md#led-rgb)) |
| `ALRT` do MAX17262 | pull-up de 10 kΩ ao `3V0` | **nenhum pino do MCU** | dreno aberto; o firmware não usa o alerta, e o pull-up existe para o pino não flutuar ([14](../docs/14-hardware-placa-nova.md#ligações-fixas-dos-cis)) |
| `INT` do OPT3001 | pull-up de 10 kΩ ao `3V0` | **nenhum pino do MCU** | idem |
| `JP102` a `JP106` | cada trilho de bloco | 0 Ω em série, um por bloco: módulo de rádio, GNSS, display, sensores e armazenamento | para medir a corrente de **cada bloco** com o PPK2, e não só o total ([14](../docs/14-hardware-placa-nova.md#placa-de-circuito-impresso)); o `JP101` da célula mede o conjunto |
| `JP101` | `VBAT+` do `J102` | `BATT` do MAX17262 | jumper de 0 Ω, 1206, para abrir o caminho da célula e pôr o amperímetro; a resistência dele entra na tensão que o medidor lê, então ≤ 50 mΩ ([06](06-conectores-e-pontos-de-teste.md#jp101--jumper-de-medição-de-corrente)) |

> [!NOTE]
> **O divisor do `DIS_SW` fechou com o ADP5091.** O `R104` de 100 kΩ fica
> em série, do `VBUSOUT`, e o `R105` de 180 kΩ vai ao `AGND`: com o cabo o
> pino vê 3,5 V, acima do 1 V de nível alto e abaixo dos 6,0 V de máximo
> absoluto do pino (**ficha**). É o **único intertravamento da placa que
> não passa por firmware**, e existe justamente para o caso de o firmware
> travar com as duas fontes de carga ativas; desde 2026-09-26 o comparador
> do corte térmico entra no mesmo nó, pelo `D106`
> ([01](01-esquematico.md#folha-1--energia)).

### Pinos de configuração, amarrados em cobre

Não passam por firmware: quem os deixa abertos muda o comportamento da
placa sem que nada avise. Um pino de configuração aberto do AEM10900
**lê alto**.

| Pino | Vai a | Por quê |
|---|---|---|
| ADP5091 `SETBK` | `AGND` | sem célula primária no `BACK_UP`: "connect the SETBK pin to the AGND pin without the BACK_UP storage element" (ficha, tabela 5) |
| ADP5091 `REG_D0`, `REG_D1` | `GND` | o regulador de saída fica desabilitado, o estado de menor consumo (510 nA típicos) |
| ADP5091 `REG_FB` | `REG_OUT` | a configuração de saída fixa (ficha, pino 15); é o que impede o nó de realimentação de flutuar com o regulador desligado |
| ADP5091 `VID` | `R123` de 111 kΩ ao `AGND` | estado definido do regulador desabilitado |
| ADP5091 `MINOP` | `R122` de 402 kΩ ao `AGND` | o boost para com o painel abaixo de 1,0 V |
| ADP5091 `REF` | topo dos divisores de `TERM` (`R118`/`R119`) e `SETSD` (`R120`/`R121`) | figura 42 da ficha; até 2026-09-26 os divisores pendiam do `BAT` e o pino ficava aberto — a equação 6 não valia e a histerese do `SETSD` não existia |
| ADP5091 `SETPG`, `SETHYST`, `PGOOD`, `LLD`, `BACK_UP` | abertos | `PGOOD` e `LLD` são saídas sem consumidor; `SETPG`/`SETHYST` são os ajustes do `PGOOD`; `BACK_UP` é a célula primária que não existe. **Conferir na ficha se `SETPG`/`SETHYST` abertos são aceitos** ([README](README.md#fichas-que-precisam-ser-lidas)) |
| MAX-F10S **`VIO_SEL`** | **aberto** | é isto que põe o `V_IO` na faixa de 2,7 a 3,6 V (manual de integração, 4.1.2 e tabela 35, opção 1). Até 2026-09-26 ia ao `GND`, para a faixa de 1,76 a 1,98 V do projeto de 1,8 V; **no GND com o `3V0` aplicado o máximo absoluto do `V_IO` é 1,98 V e o módulo queima** |
| MAX17262 `TH` | `BATT` | a temperatura vem do próprio CI; não há termistor neste barramento |
| TPS7A02 `EN` | `IN` | o LDO do `VBCKP` está sempre ligado |
| BMP585 `CSB` e `SDO` | `VDDIO` | I²C ligado e endereço `0x47`; com o `CSB` baixo na partida o I²C só volta no próximo corte de energia |
| BMI270 pinos 2 e 3 | `VDDIO` | **a Bosch proíbe o GND** |
| BMI270 pino 12 (`CS`) | `VDDIO` | seleciona I²C |
| BMI270 pino 1 | `GND` | endereço `0x68` |
| BMI270 pinos 10 e 11 | abertos | — |
| Display `EXTMODE` | `VDD` da tela | VCOM invertido pelo `EXTCOMIN` |
| Display `VSS`, `VSSA` | `GND` | — |
| Portas dos quatro MOSFET (`BL_PWM`, `RGB_R`, `RGB_G`, `RGB_B`) | 100 kΩ ao `GND` | o DMG1012T-7 não tem pull-down interno e os GPIO saem do reset em alta impedância: sem isto a luz pode acender sozinha e o MOSFET ficar na região linear |
| `DISP_PWR_EN`, `DISP_ON`, `DISP_CS` | 100 kΩ ao `GND` | os três são ativos altos e flutuam do reset até o firmware; o `SCS` flutuando alto com o relógio indefinido escreve lixo no painel. Com o JDI o `DISP_PWR_EN` não aciona nada, e o pull-down é o que o mantém definido |
| `NOR_SCK` e `NOR_MOSI` junto do pino do MCU; **`NOR_MISO` junto do pino `SO` da flash** | 33 Ω em série | amacia a borda de 8 MHz, cujo 197º harmônico cai a 0,58 MHz do centro de L1; a constante de tempo fica em cerca de 1 % do meio período, longe de atrapalhar o relógio ([02](02-calculos.md#resistor-de-série-no-spi-da-flash)) |
| `BUZ_A`, `BUZ_B` | **330 Ω** em série | o piezo é carga capacitiva: sem resistor o pico da borda passa de 30 mA, e 330 Ω o põe em 9 mA sem tirar volume ([02](02-calculos.md#buzzer-piezo)) |
| `KEY_L`, `KEY_C`, `KEY_R` | **100 Ω** em série e **1 nF** ao `GND` | as teclas saem para a caixa e não tinham proteção nenhuma; τ de 13 µs com o pull-up interno e só 23 mV de queda no nível baixo ([02](02-calculos.md#proteção-das-teclas)) |
| Derivação da tecla central para o `SHPHLD` | sai **depois** dos 100 Ω, não do contato | é o que faz a rede proteger os dois ramos: contra o pull-up interno de 50 kΩ do PMIC, 100 Ω dão 11 mV de erro a 5,5 V e o 1 nF dá τ de 50 µs, inócuos para um botão e para o toque longo de 10 s |
| `3V0`, junto do pino de alimentação do módulo (pad 19) | **4,7 µF** | o rádio puxa 10,9 mA em rajada e o buck leva cerca de 10 µs para responder ([02](02-calculos.md#capacitor-de-volume-no-módulo)) |
| Pull-ups de `WP` e `HOLD` da flash | 47 kΩ ao **`SD3V0`** | ao `3V0` a flash se alimentaria pelos pinos com a `LDSW1` cortada |
| Tag-Connect pino 1 (`VTref`) | `3V0` | sem ele a maioria das sondas recusa conectar |
| Tag-Connect pino 5 | `GND` | — |
| `J202` pino 1 (`VTref`) | `3V0` | o mesmo `VTref`, no conector Cortex de 10 vias |
| `J202` pinos 3 e 5 | `GND` | os dois terras do conector Cortex; o 9 (`GNDDetect`) fica aberto, o 7 é a chave e o 8 é TDI, só JTAG |

> [!CAUTION]
> **Nada de pull-down externo no `TIMEPULSE` do receptor**: com ele o
> módulo entra em safeboot e não parte.

## Contagem

Números conferidos contando as linhas deste arquivo e rodando
`python hardware_gnssbike/net_check.py` e `python tools/fw/board_check.py`.

| | |
|---|---|
| Pinos do MCU usados pelo firmware | **31** de 66 |
| Pinos ligados na placa e **reservados** no firmware | 2 (`GNSS_EXTINT` P1.08 e `GNSS_TIMEPULSE` P1.09) |
| Total de pinos do MCU neste esquemático | **33** |
| Pinos de clock usados | **5**: `NOR_SCK` P2.01, `DISP_SCK` P3.03, `SENS_SCL` P1.03, `PWR_SCL` P0.03 e `GNSS_TX` P1.04 |
| Pinos de clock livres | 12 |
| Nós de alimentação | 12 na tabela, **11 montados**: o `5V0` só existe no plano B, com a Sharp |
| Nós sem ligação ao MCU | 12 |
| Pinos de configuração amarrados em cobre | 26 |
