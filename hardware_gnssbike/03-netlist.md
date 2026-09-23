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
  declara. O **pad LGA do módulo BM20C ainda não foi levantado**: o
  esquemático liga por nome de sinal e o layout precisará do de-para.
- **Tipo**: `alim` (alimentação), `dig` (digital), `ana` (analógico),
  `rf` (radiofrequência).
- Toda referência de terra é **GND**, plano contínuo (ver
  [04](04-pcb-e-caixa.md#camadas)).

## Nós de alimentação

| Nó | Tensão | Sai de | Chega em | Tipo |
|---|---|---|---|---|
| `VBUS` | 4,0 a 5,5 V | conector USB-C, pinos A4, A9, B4, B9 | nPM1300 `VBUS`; TVS ESD761; C 10 µF/25 V | alim |
| `VBUSOUT` | = `VBUS` | nPM1300 `VBUSOUT` | pad `VBUS` do BM20C (detecção e PHY do USB); topo do divisor `DIS_STO_CH`; C 1 µF | alim |
| `VBAT` | 3,0 a 4,2 V | positivo da célula, pelo `BATT` do MAX17262 | `SYS` do MAX17262 (o sensor de 7 mΩ é **interno** ao CI, entre `BATT` e `SYS`); nPM1300 `VBAT`; AEM10900 `STO`; TPS7A02 `IN` | alim |
| `VSYS` | `VBAT` ou `VBUS` | nPM1300 `VSYS` | entradas de `BUCK1`, `BUCK2` e `LDSW2`; anodos do LED RGB e do LED de carga | alim |
| `3V0` | 3,0 V | nPM1300 `BUCK2` | BM20C `VDD`; BMP585, BMI270, MMC5633NJL e OPT3001; TXU0204 `VCCA`; AEM10900 `I2C_VDD`; buzzer; entrada da `LDSW1`; REG710 `IN`; e o `VDD`/`VDDA` do display **só na montagem com o JDI**, pelo `JP401` | alim |
| `1V8` | 1,8 V | nPM1300 `BUCK1`, por filtro LC | MAX-F10S `VCC` e `V_IO`; TXU0204 `VCCB` | alim |
| `SD3V0` | 3,0 V | nPM1300 `LDSW1`, do `3V0` | MX25R6435F `VCC` | alim |
| `3V3BL` | 3,3 V | nPM1300 `LDSW2`, do `VSYS` | dreno do MOSFET da luz, por `R_BL` | alim |
| `VBCKP` | 1,8 V | TPS7A02, do `VBAT` | MAX-F10S `V_BCKP` | alim |
| `5V0` | 5,0 V | REG710NA-5, do `3V0` | `VDD` e `VDDA` do display **só na montagem com a Sharp**, pelo `JP401` | alim |
| `VINT` | interno | AEM10900 `VINT` | `R_MPP[2:0]`, `T_MPP[1:0]`, `STO_CFG[2]`, `STO_CFG[0]`, `KEEP_ALIVE` | alim |
| `GND` | 0 V | — | plano contínuo | alim |

> [!CAUTION]
> `3V0` e `5V0` chegam ao mesmo pino do display e **nunca** podem alcançá-lo ao mesmo tempo: o JDI
> tem máximo absoluto de 3,6 V e queima com 5 V. A escolha é um jumper de
> resistor de posição única ([01](01-esquematico.md#folha-4--display)).

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
| `DISP_PWR_EN` | P3.07 | REG710 `EN` | dig | corta os 65 µA do REG710; só na montagem com a Sharp |
| `BL_PWM` | P3.08 | porta do MOSFET da luz | dig | `pwm20`, canal 0 |

### GNSS · `uart21`

O TXU0204 tem **direção fixa**: `A1` e `A2` vão do lado A (3,0 V) para o
lado B (1,8 V), e `B3` e `B4` vão de B para A
([15](../docs/15-avaliacao-componentes.md#gnss)). Os quatro canais atendem
exatamente os quatro sinais que precisam de tradução, dois em cada sentido.

| Nó | Pino do MCU | Canal | Outro extremo | Tipo | Nota |
|---|---|---|---|---|---|
| `GNSS_TX` | P1.04 | `A1` → `B1` | `RXD` do MAX-F10S | dig | pino de clock, gasto de propósito com a UART |
| `GNSS_EXTINT` | P1.08 | `A2` → `B2` | `EXTINT` do MAX-F10S | dig | ligado na placa; **reservado no firmware**, que ainda não o usa |
| `GNSS_RX` | P1.05 | `B3` → `A3` | `TXD` do MAX-F10S | dig | — |
| `GNSS_TIMEPULSE` | P1.09 | `B4` → `A4` | `TIMEPULSE` do MAX-F10S | dig | ligado na placa; **reservado no firmware** |
| `GNSS_RESET_N` | P1.06 | **nenhum** | `RESET_N` do MAX-F10S, direto | dig | **não passa pelo tradutor**: o pino do MCU é dreno aberto e só puxa para baixo, e o pull-up de 7 a 13 kΩ é interno ao módulo |

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
| `PWR_SDA` | P0.02 | nPM1300, MAX17262, AEM10900 | dig | pull-up de 4,7 kΩ ao `3V0` |
| `PWR_SCL` | P0.03 | os mesmos três | dig | **pino de clock**; pull-up de 4,7 kΩ |
| `PMIC_INT` | P0.00 | nPM1300 `GPIO3` | dig | eventos de botão, carga e falha |

Endereços: nPM1300 `0x6B`, MAX17262 `0x36`, AEM10900 `0x41` (`I2C_ADDR`
no `I2C_VDD`).

### Interface

| Nó | Pino do MCU | Outro extremo | Tipo | Nota |
|---|---|---|---|---|
| `KEY_L` | P1.26 | tecla esquerda, ao GND | dig | pull-up interno, ativo baixo |
| `KEY_C` | P1.27 | tecla central, ao GND | dig | **em conflito com a especificação**: ver o aviso abaixo |
| `KEY_R` | P1.30 | tecla direita, ao GND | dig | — |
| `RGB_R` | P1.16 | **porta** do MOSFET do vermelho | dig | `pwm22` canal 0; o LED vai do `VSYS` ao dreno, por `R_LEDR` |
| `RGB_G` | P1.19 | **porta** do MOSFET do verde | dig | `pwm22` canal 1 |
| `RGB_B` | P1.22 | **porta** do MOSFET do azul | dig | `pwm22` canal 2 |
| `BUZ_A` | P1.25 | buzzer, lado A | dig | `pwm21` canal 0 |
| `BUZ_B` | P1.28 | buzzer, lado B | dig | `pwm21` canal 1, em contrafase |

> [!CAUTION]
> **A tecla central está ligada em dois lugares, e a especificação diz que
> não pode.** O [`docs/14`](../docs/14-hardware-placa-nova.md#componentes-principais)
> ("o central vai **só** ao SHPHLD"), a mesma página na tabela de pinos
> ("botão central ao GND, **sem outra ligação**"), a
> [avaliação](../docs/15-avaliacao-componentes.md#detalhes-para-o-esquemático)
> e a [lista de compras](../docs/19-lista-de-compras.md) dizem os quatro a
> mesma coisa: a tecla central vai só ao `SHPHLD` do nPM1300, e **o MCU
> fica sabendo dela pelo `GPIO3`**, que já está aqui como `PMIC_INT`.
> O devicetree do firmware, porém, declara `key_centre` em P1.27
> (`gnssbike_nrf54lm20a_cpuapp.dts`), e foi de lá que este esquemático
> copiou.
>
> **Por que a especificação tem razão:** o pull-up de 50 kΩ do `SHPHLD`
> precisa funcionar **em ship mode**, com todos os reguladores desligados,
> logo ele se refere ao `VSYS`. Daí saem dois problemas. Com o USB, o nó
> vai a 5,5 V e o diodo de proteção de P1.27 conduz — o mesmo mecanismo
> que obrigou os três MOSFET do LED RGB. E em ship mode o `3V0` está em
> 0 V, de modo que o diodo de P1.27 grampeia o `SHPHLD` perto de 0,7 V, que
> o nPM1300 pode ler como **tecla presa**: o aparelho não fica desligado e,
> com o reset de toque longo ligado de fábrica, entra em ciclo de
> reinício.
>
> **Isto não foi resolvido aqui**, porque resolver muda o firmware: sem
> P1.27 a interface perde a tecla de confirmação e passa a depender do
> evento `NPM13XX_EVENT_SHIPHOLD_PRESS` do PMIC. É decisão do dono.

### Console · `uart20`

| Nó | Pino do MCU | Outro extremo | Tipo | Nota |
|---|---|---|---|---|
| `CON_TX` | P1.00 | pad de teste TP1, com um pad de GND ao lado | dig | 115200 baud; não sai na caixa |
| `CON_RX` | P1.31 | pad de teste TP2 | dig | — |

### Dedicados do módulo

| Nó | Pad do BM20C | Outro extremo | Tipo | Nota |
|---|---|---|---|---|
| `USB_DM` | G6 | USB-C A7 e B7 | dig | par de 90 Ω diferencial |
| `USB_DP` | G7 | USB-C A6 e B6 | dig | idem |
| `MOD_VBUS` | H7 | `VBUSOUT` | alim | 4,4 a 5,5 V |
| `SWDIO` | J3 | Tag-Connect pino 2 | dig | — |
| `SWDCLK` | K3 | Tag-Connect pino 4 | dig | — |
| `MOD_RESET` | G2 | Tag-Connect pino 6 | dig | — |

## Nós sem ligação ao MCU

| Nó | Sai de | Chega em | Nota |
|---|---|---|---|
| `CC1`, `CC2` | USB-C A5, B5 | nPM1300 `CC1`, `CC2`, com o **TPD4E05U06** | o Rd de 5,1 kΩ é interno ao nPM1300; o TPD4E05U06 protege as linhas de CC e de dados |
| `NTC_BAT` | NTC do pack | nPM1300 `NTC` | 10 kΩ, B3380, JEITA |
| `TH_MON` | segundo NTC | AEM10900 `TH_MON` | com `TH_REF` e `RDIV` de 22 kΩ |
| `DIS_STO_CH` | divisor 100 kΩ / 1 MΩ do `VBUSOUT` | AEM10900 `DIS_STO_CH` | **o USB bloqueia a carga solar em hardware**, sem pino do MCU |
| `SWDCDC` | AEM10900 | indutor de 4,7 µH | nó curto, sem plano embaixo |
| `SRC` | painéis solares | AEM10900 `SRC` | 6 módulos de 3 células |
| `LED_CHG` | nPM1300 `LED1` | LED de carga, anodo no `VSYS` | acende com o aparelho desligado |
| `LED_ERR` | nPM1300 `LED0` | LED de erro, anodo no `VSYS` | — |
| `SHPHLD` | tecla central | nPM1300 `SHPHLD` | pull-up interno de 50 kΩ; liga fora do ship mode |
| `RGB_R_D`, `RGB_G_D`, `RGB_B_D` | `VSYS`, por `R_LEDR`, `R_LEDG` e `R_LEDB` | catodo de cada cor, e daí ao dreno do seu MOSFET | **o catodo não vai ao pino do MCU**: com o USB o `VSYS` chega a 5,5 V ([02](02-calculos.md#led-rgb)) |
| `ST_STO` | AEM10900 | ponto de teste TP3 | — |

### Pinos de configuração, amarrados em cobre

Não passam por firmware: quem os deixa abertos muda o comportamento da
placa sem que nada avise. Um pino de configuração aberto do AEM10900
**lê alto**.

| Pino | Vai a | Por quê |
|---|---|---|
| AEM10900 `STO_CFG[2]`, `STO_CFG[0]` | `VINT` | com o `[1]` no GND, dá H, L, H: carga até **3,90 V** e corte em **3,01 V**, o perfil Li-ion long life |
| AEM10900 **`STO_CFG[1]`** | **`GND`** | idem — **é o único dos três que vai ao terra**; aberto, o limiar de carga da célula muda ao sol, sem firmware no meio |
| AEM10900 `R_MPP[2:0]` | `VINT` | MPPT a 80 % da tensão em aberto |
| AEM10900 `T_MPP[1:0]` | `VINT` | tempo entre medidas do MPP, o padrão |
| AEM10900 `KEEP_ALIVE` | `VINT` | a configuração por I²C sobrevive ao `3V0` desligado |
| AEM10900 `I2C_ADDR` | **`I2C_VDD`** | endereço `0x41` |
| MAX-F10S **`VIO_SEL`** | **`GND`** | é isto que põe o `V_IO` na faixa de 1,76 a 1,98 V. **Todo o trilho de 1,8 V e o máximo absoluto de 1,98 V dependem deste pino**; com ele aberto o módulo esperaria `V_IO` igual ao `VCC` |
| MAX17262 `TH` | `BATT` | a temperatura vem do próprio CI; não há termistor neste barramento |
| TPS7A02 `EN` | `IN` | o LDO do `VBCKP` está sempre ligado |
| TXU0204 `OE` | `VCCA` | as saídas ficam habilitadas sempre que o lado de 3,0 V existe |
| BMP585 `CSB` e `SDO` | `VDDIO` | I²C ligado e endereço `0x47`; com o `CSB` baixo na partida o I²C só volta no próximo corte de energia |
| BMI270 pinos 2 e 3 | `VDDIO` | **a Bosch proíbe o GND** |
| BMI270 pino 12 (`CS`) | `VDDIO` | seleciona I²C |
| BMI270 pino 1 | `GND` | endereço `0x68` |
| BMI270 pinos 10 e 11 | abertos | — |
| Display `EXTMODE` | `VDD` da tela | VCOM invertido pelo `EXTCOMIN` |
| Display `VSS`, `VSSA` | `GND` | — |
| Portas dos quatro MOSFET (`BL_PWM`, `RGB_R`, `RGB_G`, `RGB_B`) | 100 kΩ ao `GND` | o DMG1012T-7 não tem pull-down interno e os GPIO saem do reset em alta impedância: sem isto a luz pode acender sozinha e o MOSFET ficar na região linear |
| `DISP_PWR_EN`, `DISP_ON`, `DISP_CS` | 100 kΩ ao `GND` | os três são ativos altos e flutuam do reset até o firmware; o `SCS` flutuando alto com o relógio indefinido escreve lixo no painel |
| `NOR_SCK`, `NOR_MOSI`, `NOR_MISO` | 33 Ω em série, junto do pino do MCU | amacia a borda de 8 MHz, cujo 197º harmônico cai a 0,58 MHz do centro de L1 ([04](04-pcb-e-caixa.md)) |
| `BUZ_A`, `BUZ_B` | 100 Ω em série | o piezo é carga capacitiva e o pico da borda fica limitado só pelo driver do GPIO |
| Pull-ups de `WP` e `HOLD` da flash | 47 kΩ ao **`SD3V0`** | ao `3V0` a flash se alimentaria pelos pinos com a `LDSW1` cortada |
| Tag-Connect pino 1 (`VTref`) | `3V0` | sem ele a maioria das sondas recusa conectar |
| Tag-Connect pino 5 | `GND` | — |

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
| Nós de alimentação | 12 |
| Nós sem ligação ao MCU | 11 |
| Pinos de configuração amarrados em cobre | 24 |
