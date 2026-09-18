# Arquitetura de hardware da placa nova

Especificação técnica preliminar da placa própria do GNSS Bike Computer: decisões, arquitetura, alimentação, componentes principais, barramentos, alocação de pinos, placa de circuito impresso, empilhamento mecânico, regras de layout e bring-up. A pesquisa de mercado e as alternativas estão em [13-placa-nova.md](13-placa-nova.md); o aparelho desenhado, em [Como fica o aparelho](13-placa-nova.md#como-fica-o-aparelho).

> [!IMPORTANT]
> Especificação de conceito, anterior ao esquemático. Nenhum componente foi montado nem medido. Tensões, correntes e endereços vêm dos datasheets citados em [Referências](#referências); potências médias e autonomia são estimativas de [13](13-placa-nova.md#orçamento-de-energia).

**Nesta página:** [Estado das decisões](#estado-das-decisões) · [Arquitetura](#arquitetura) · [Alimentação](#alimentação) · [Componentes principais](#componentes-principais) · [GNSS](#gnss) · [Barramentos e endereços](#barramentos-e-endereços) · [Alocação de pinos](#alocação-de-pinos) · [Placa de circuito impresso](#placa-de-circuito-impresso) · [Empilhamento mecânico](#empilhamento-mecânico) · [Regras de layout](#regras-de-layout) · [Teste e bring-up](#teste-e-bring-up) · [Pendências](#pendências) · [Referências](#referências)

## Estado das decisões

| Bloco | Escolha | Estado | Como fechar |
|---|---|---|---|
| MCU e rádio | nRF54LM20A no módulo Fanstel BM20C (antena em chip) | MCU decidido; módulo recomendado | datasheet do BM20C: pinagem, USB, NFC, guia de layout da antena e tolerância do cristal de 32,768 kHz (o ANT exige no máximo ±50 ppm) |
| Pilha de rádio | BLE do NCS v3.3.0 e ANT do `sdk-ant` v2.1.1 | decidido | — |
| Display | JDI LPM027M128C: MIP de 8 cores, 2,7", 400 × 240, com luz | recomendado | 3 a 5 amostras antes do layout; plano B LS027B7DH01 no mesmo conector |
| Painéis solares | 6 módulos de 3 células de 23 × 8 mm (classe ANYSOLAR KXOB25-05X3F) na frente inclinada e nos chanfros laterais | arranjo decidido | fixação, janela e vedação na mecânica |
| GNSS | footprint MAX da u-blox: MAX-F10S (L1 + L5) ou MAX-M10N-10B (L1, modo LEAP) | **em aberto** | teste A/B na caixa real ([GNSS](#gnss)) |
| Carga e medição | nPM1300 (USB e reguladores), AEM10900 (solar), MAX17262 (medidor na célula) | recomendado; **em aberto** até validar a carga dupla | bancada com o nPM1300 EK e a placa de avaliação do AEM10900 na mesma célula; alternativa TI BQ25798 ([13](13-placa-nova.md#dois-carregadores-na-mesma-célula)) |
| Bateria | LiPo de 1 célula, 2000 mAh, 60 × 36 × 7 mm, com proteção (PCM) e NTC de 10 kΩ | recomendado | especificação com o fornecedor do pack (NTC de 3 fios, UN38.3) |
| Sensores | BMP585, LSM6DSV16X, LIS2MDL, OPT3001 | recomendado | estoque na compra (alternativas LIS2DW12 e MMC5603NJ) |
| Armazenamento | microSD (Hirose DM3AT-SF-PEJM5) no `spi00` | recomendado | — |
| USB | USB-C (GCT USB4105-GF-A) com proteção ESD, USB High Speed | recomendado | vedação na mecânica |

O display é um LCD de memória refletivo (MIP), legível ao sol como o e-paper, e não e-paper: o e-paper colorido leva de 11 a 20 s por quadro em cor e gasta cerca de 144 mJ por atualização, contra menos de 0,2 s e cerca de 30 µJ do MIP, e o aparelho redesenha dados a cada segundo ([13](13-placa-nova.md#display)).

## Arquitetura

```mermaid
flowchart LR
    subgraph PWR["Energia"]
        USBC["USB-C"] --> PMIC["nPM1300<br/>carregador, BUCK1, BUCK2,<br/>chaves de carga, ship mode"]
        PV["6 módulos solares"] --> HARV["AEM10900<br/>MPPT"]
        PMIC --- NODE(("nó da bateria"))
        HARV --- NODE
        NODE --- GAUGE["MAX17262"] --- CELL["LiPo 1S 2000 mAh"]
        NODE --> BCK["TPS7A02 1,8 V<br/>backup do GNSS"]
    end
    subgraph CORE["Módulo BM20C"]
        MCU["nRF54LM20A<br/>BLE e ANT+"]
    end
    subgraph GNSSB["GNSS"]
        ANT["antena L1/L5"] --> GMOD["u-blox MAX"]
        GMOD --- XLAT["TXU0204<br/>1,8 V para 3,0 V"]
    end
    subgraph HMI["Interface"]
        LCD["JDI LPM027M128C"]
        KEYS["3 botões"]
        BUZ["buzzer piezo"]
        LED["LED RGB"]
        ALS["OPT3001"]
    end
    subgraph SENS["Sensores"]
        BARO["BMP585"]
        IMU["LSM6DSV16X"]
        MAG["LIS2MDL"]
    end
    SD["microSD"]
    XLAT ---|"uart21"| MCU
    LCD ---|"spi22"| MCU
    SD ---|"spi00"| MCU
    BARO ---|"i2c23"| MCU
    IMU ---|"i2c23"| MCU
    MAG ---|"i2c23"| MCU
    ALS ---|"i2c23"| MCU
    PMIC -.-|"i2c30"| MCU
    GAUGE -.-|"i2c30"| MCU
    HARV -.-|"i2c30"| MCU
    USBC ---|"USB HS"| MCU
    KEYS --> MCU
    MCU --> BUZ
    MCU --> LED
    BCK --> GMOD
```

## Alimentação

### Árvore

```mermaid
flowchart TD
    VBUS["USB-C VBUS 5 V<br/>TVS TPD1E10B06"] --> PMIC["nPM1300"]
    PMIC -->|"VBUSOUT"| NRFVBUS["VBUS do BM20C<br/>detecção e PHY do USB"]
    CELL["LiPo 1S"] --- GAUGE["MAX17262<br/>sensor de 7 mΩ"]
    GAUGE --- VBAT(("VBAT<br/>3,0 a 4,2 V"))
    VBAT --- PMIC
    PV["módulos solares"] --> AEM["AEM10900"]
    AEM -->|"STO"| VBAT
    VBAT --> LDO["TPS7A02 1,8 V"]
    LDO --> VBCKP["V_BCKP do GNSS"]
    PMIC -->|"BUCK1 3,0 V"| R3V0["3V0: BM20C, display, sensores,<br/>LED, luz, buzzer, lado A do TXU0204"]
    PMIC -->|"BUCK2 1,8 V"| R1V8["1V8: GNSS VCC e V_IO,<br/>lado B do TXU0204"]
    R3V0 --> LSW["chave LDSW1 do nPM1300"]
    LSW --> RSD["SD3V0: microSD"]
```

| Trilho | Fonte | Tensão | Limite da fonte | Cargas | Observação |
|---|---|---|---|---|---|
| VBUS | USB-C | 4,0 a 5,5 V (o nPM1300 tolera 22 V em transitório) | limite de entrada do nPM1300, de 100 mA a 1,5 A | nPM1300; VBUS do BM20C pela saída VBUSOUT | o USB do nRF54LM20A exige 5 V no pino VBUS além do VDD; a VBUSOUT tem proteção contra sobretensão e subtensão |
| VBAT | célula, através do MAX17262 | 3,0 a 4,2 V | proteção do pack | VBAT do nPM1300, STO do AEM10900, entrada do TPS7A02 | nenhuma carga da aplicação direto no VBAT: o datasheet do nPM1300 proíbe |
| 3V0 | BUCK1 do nPM1300 | 3,0 V | 200 mA | BM20C, display (VDD e VDDA), sensores, LED RGB, luz do display, buzzer, I2C_VDD do AEM10900, lado A do TXU0204 | tensão de partida pelo resistor de VSET1: o MCU depende dele para ligar |
| 1V8 | BUCK2 do nPM1300 | 1,8 V | 200 mA | VCC e V_IO do módulo GNSS, lado B do TXU0204 | filtro LC (ferrite e 10 µF) junto do módulo; ripple abaixo de 50 mV; partida pelo VSET2 |
| SD3V0 | chave LDSW1 do nPM1300, a partir do 3V0 | 3,0 V | 100 mA | microSD | 22 µF junto do soquete; se os picos medidos passarem de 100 mA, chave dedicada |
| VBCKP | TPS7A02, a partir do VBAT | 1,8 V | 200 mA, 25 nA de consumo próprio | V_BCKP do GNSS (28 µA em backup de hardware) | mantém efemérides e relógio do GNSS em ship mode, para partidas a quente |

Pico estimado no 3V0: rádio a +8 dBm (10,9 mA), CPU (2,6 mA), luz do display (16 mA), LED RGB (até 6 mA), sensores (cerca de 1 mA) e o microSD pela LDSW1 (até 100 mA), perto de 140 mA contra os 200 mA do BUCK1. No 1V8, o GNSS consome até 31 mA de VCC na aquisição (MAX-F10S a 1,8 V) e pode puxar até 100 mA de pico na partida, segundo o datasheet.

### Carga

| Caminho | Parâmetros | Origem |
|---|---|---|
| USB (nPM1300) | 600 mA de carga (0,3C da célula de 2000 mAh; o chip vai de 32 a 800 mA); término em 4,20 V, ou 4,10 V para vida longa; JEITA com o NTC do pack (0, 10, 45 e 60 °C, os padrões dos registradores); CC1 e CC2 ligados direto ao conector, com os resistores Rd internos do nPM1300 | datasheet do nPM1300 |
| Solar (AEM10900) | MPPT a 80 % da tensão em aberto (padrão); indutor de 3,3 µH, com até 175,5 mA de entrada (os 6 módulos somam cerca de 110 mA a 1 sol); limiar de carga de 3,90 V pelos pinos (perfil Li-ion long life) e cerca de 4,05 V por I2C com a placa ligada; corte fora de 0 a 45 °C por um segundo NTC colado na célula | datasheet do AEM10900 e [13](13-placa-nova.md#dois-carregadores-na-mesma-célula) |
| Medição (MAX17262) | corrente líquida de todas as fontes, inclusive a do painel com a placa desligada; 5,2 µA em hibernação | datasheet do MAX17262 |

### Estados de energia

| Estado | O que fica ligado | Consumo na célula | Entrada | Saída |
|---|---|---|---|---|
| Desligado (ship mode) | MAX17262 em hibernação, TPS7A02 e o backup do GNSS; o AEM10900 continua carregando com sol, sozinho | cerca de 34 µA (370 nA do nPM1300, 5,2 µA do MAX17262, 25 nA do TPS7A02 e 28 µA do backup do GNSS); sem o backup do GNSS, cerca de 6 µA | `regulator_parent_ship_mode()` pelo `power_scheduler` ou pelo menu | botão no pino SHPHLD ou VBUS |
| Ligado | todos os trilhos | cerca de 58 mW no uso típico (estimativa de [13](13-placa-nova.md#orçamento-de-energia)) | — | — |
| Ligado com USB | todos os trilhos, carga pelo nPM1300 | — | VBUS | o nPM1300 não entra em ship mode com VBUS presente: o firmware desliga os trilhos e põe o MCU em System OFF |

## Componentes principais

| Função | Peça | Encapsulamento | Dados principais | Driver no NCS v3.3.0 | Estado |
|---|---|---|---|---|---|
| MCU e rádio | Fanstel BM20C (nRF54LM20A) | módulo 10,0 × 14,8 × 2 mm | Cortex-M33 de 128 MHz, 2036 KB de RRAM e 512 KB de RAM; 66 GPIO; USB High Speed; cristais de 32 MHz e de 32,768 kHz; FCC, ISED, europeia e TELEC | NCS e `sdk-ant` | recomendado; produção prevista para 09/2026 |
| Display | JDI LPM027M128C | 61,8 × 40,08 × 1,39 mm, FPC de 10 vias | 400 × 240, 8 cores, 3,0 V, SPI até 2 MHz, 5 µW parado e 30 µW a 1 quadro/s; luz de 16 mA a 2,67 V | `jdi,lpm013m126`, com mudanças | recomendado; compra em risco |
| Conector do display | Hirose FH28-10S-0.5SH(05) | FPC de 10 vias, passo 0,5 mm | indicado no datasheet do display | — | recomendado |
| GNSS | u-blox MAX-F10S ou MAX-M10N-10B | LCC de 10,1 × 9,7 × 2,5 mm | ver [GNSS](#gnss) | não há (o `u-blox,m10` entra no Zephyr 4.5) | em aberto |
| Antena GNSS | TE L000670 (protótipo) | chip de 14 × 10,75 × 1 mm, na borda | L1 e L5 numa alimentação: 66 % e 56 % de eficiência num plano de 90 × 41 mm | — | recomendado para o protótipo |
| Tradutor de nível | TI TXU0204 | X2QFN de 12 pinos | 4 canais de direção fixa, 1,1 a 5,5 V de cada lado, 2,5 µA | — | recomendado |
| PMIC | Nordic nPM1300 | QFN32 de 5 × 5 mm | carregador de 32 a 800 mA, 2 bucks de 200 mA, 2 LDO de 50 mA ou chaves de 100 mA, ship mode de 370 nA, VBUSOUT, detecção USB-C | `nordic,npm1300*` | recomendado |
| Medidor de carga | Analog Devices MAX17262 | a definir (WLP ou TDFN) | sensor interno de 7 mΩ, ModelGauge m5 EZ, 5,2 µA em hibernação | `maxim,max17262` | recomendado |
| Carregador solar | e-peas AEM10900 | QFN28 de 4 × 4 mm | MPPT de 120 mV a 2,73 V, partida a frio com 250 mV, até 175,5 mA de entrada, I2C, NTC, medidor de energia | não há | recomendado; validar a carga dupla |
| Módulos solares | ANYSOLAR KXOB25-05X3F (6 unidades) | 23 × 8 × 1,8 mm | 3 células, 2,07 V em aberto, 30,7 mW e 18,4 mA a 1 sol | — | recomendado |
| Bateria | LiPo de 1 célula, 2000 mAh | 60 × 36 × 7 mm | proteção (PCM), NTC de 10 kΩ (B3380) de 3 fios | — | a especificar com o fornecedor |
| LDO do backup do GNSS | TI TPS7A02, 1,8 V | X2SON de 1 × 1 mm | 25 nA de consumo próprio, 1,5 a 6,0 V de entrada, 200 mA | — | recomendado |
| Barômetro | Bosch BMP585 | LGA de 3,25 × 3,25 × 1,96 mm | ruído abaixo de 0,1 Pa RMS, ±0,5 Pa/K, 1,3 µA a 1 Hz, tampa metálica | `bosch,bmp581` (chip ID 0x51 aceito, não testado) | recomendado |
| IMU | ST LSM6DSV16X | LGA-14 de 2,5 × 3,0 × 0,83 mm | acelerômetro e giroscópio, fusão interna, 0,65 mA em alto desempenho | `st,lsm6dsv16x` | recomendado; conferir estoque |
| Magnetômetro | ST LIS2MDL | LGA de 2,0 × 2,0 × 0,7 mm | I2C em 0x1E | `st,lis2mdl` | recomendado; conferir estoque |
| Luz ambiente | TI OPT3001 | USON de 2,0 × 2,0 × 0,65 mm | 0,01 a 83 mil lux, 1,8 µA, 1,6 a 3,6 V | `ti,opt3001` | recomendado |
| microSD | Hirose DM3AT-SF-PEJM5 | soquete de 1,68 mm de altura | push-push com detecção de cartão | `zephyr,sdhc-spi-slot` | recomendado; avaliar trava contra vibração |
| USB-C | GCT USB4105-GF-A | 16 pinos, SMD em ângulo reto | USB 2.0 | — | recomendado |
| ESD | TI TPD4E05U06 e TPD1E10B06 | USON-10 e 0402 | 0,5 pF e ±12 kV nas linhas de dados e CC; ±30 kV no VBUS | — | recomendado |
| Buzzer | Same Sky CPT-1117-83-SMT-TR | 11 × 9 mm | piezo em ponte por dois PWM | PWM | recomendado |
| LED | RGB de 1,6 × 1,6 mm (a escolher) | — | três canais PWM, cerca de 2 mA por canal | `pwm-leds` | a escolher |
| Botões | 3 chaves táteis seladas (a escolher) | — | a de ligar também no SHPHLD | GPIO | a escolher |
| Luz do display | N-MOSFET de sinal e resistor (a escolher) | SOT-723 ou similar | 16 mA com PWM | PWM | a escolher |
| Conector da bateria | JST SH de 3 vias (a confirmar) | passo 1,0 mm | VBAT, GND e NTC | — | a confirmar |

## GNSS

- **Footprint único:** o MAX-M10S, o MAX-M10N e o MAX-F10S têm a mesma pinagem de alimentação, UART, RESET_N, EXTINT, TIMEPULSE e RF_IN; o M10N não tem I2C (pinos 16 e 17 reservados), que a placa não usa.
- **Alimentação:** VCC de 1,76 a 3,6 V; V_IO nunca acima do VCC (com VIO_SEL em GND, de 1,76 a 1,98 V). A placa usa VCC e V_IO em 1,8 V, pelo BUCK2 (o MAX-F10S gasta cerca de 47 mW contra 57 mW em 3,0 V), com o TXU0204 entre os domínios de 1,8 V e 3,0 V. Alternativa sem tradutor: módulo inteiro em 3,0 V, pelo BUCK1.
- **Sinais:** RXD e EXTINT do MCU para o módulo; TXD e TIMEPULSE do módulo para o MCU, pelos quatro canais do TXU0204. O RESET_N (ativo baixo, pelo menos 1 ms) vem de um pino do MCU em dreno aberto, que só puxa para baixo e dispensa o tradutor.
- **Backup:** V_BCKP de 1,65 a 3,6 V; 28 µA em backup de hardware e cerca de 3 µA em operação normal, pelo TPS7A02.
- **Entrada de RF:** o MAX-F10S e o MAX-M10N-10B têm SAW, LNA e SAW internos e dispensam filtro externo; um MAX-M10S (LNA antes do SAW) pediria um SAW externo, por causa do BLE a +8 dBm do nRF54LM20A.
- **Antena:** no protótipo, a TE L000670 na borda de cima da placa, com área livre em todas as camadas e rede de casamento em π; numa segunda versão, elementos de L1 e L5 na parede da caixa com contatos de mola e um diplexador na RF_IN, como no [desenho](13-placa-nova.md#como-fica-o-aparelho) e nos produtos do mercado ([13](13-placa-nova.md#antena-gnss-dentro-da-caixa)).
- **Teste A/B para fechar o módulo:** MAX-F10S contra MAX-M10N-10B na mesma placa e na caixa real: C/N0 por banda, ruído na banda (`UBX-MON-SPAN`), partida a frio e a quente, trajeto em cidade e em mata contra uma referência, e consumo no PPK2.

## Barramentos e endereços

| Barramento | Instância | Velocidade | Nível | Dispositivos (endereço de 7 bits) | Observação |
|---|---|---|---|---|---|
| I2C dos sensores | `i2c23` | 400 kHz | 3,0 V, pull-ups de 4,7 kΩ | BMP585 0x47 (SDO alto; 0x46 com SDO em GND), LSM6DSV16X 0x6A (SA0 em GND), LIS2MDL 0x1E (fixo), OPT3001 0x44 (ADDR em GND) | sem conflito |
| I2C de energia | `i2c30` | até 400 kHz | 3,0 V | nPM1300 0x6B (fixo), MAX17262 0x36 (fixo), AEM10900 0x41 (I2C_ADDR em I2C_VDD; 0x40 em GND) | sem conflito; o AEM10900 segue pelos pinos quando o 3V0 cai |
| SPI do cartão | `spi00` | 16 MHz | 3,0 V | microSD | 16 MHz fica fora do lóbulo de L1 e dentro dos 25 MHz do modo SPI do cartão |
| SPI do display | `spi22` | 2 MHz (máximo do display) | 3,0 V | JDI LPM027M128C | CS ativo alto |
| UART do GNSS | `uart21` | a configurar por UBX (`CFG-UART1-BAUDRATE`) | 3,0 V no MCU, 1,8 V no módulo | u-blox MAX | pelo TXU0204 |
| USB | USBHS | 480 Mbit/s | par diferencial de 90 Ω | USB-C | VBUS do nRF pela VBUSOUT do nPM1300 |
| PWM | `pwm20` a `pwm22` | — | 3,0 V | luz do display, EXTCOMIN, LED RGB (3), buzzer (2) | — |

## Alocação de pinos

Provisória. Os blocos seriais seguem os domínios de pinos do nRF54LM20A (`spi00` na porta P2; blocos 20 a 24 nas portas P1 e P3; bloco 30 na porta P0), e as funções que já existem no port repetem os pinos do alvo nRF54LM20 DK ([05](05-arquitetura-zephyr.md#nrf54lm20-dk)). O mapeamento final depende da pinagem do BM20C.

| Função | Sinais | Periférico | Porta | Pino no DK (referência) |
|---|---|---|---|---|
| microSD | SCK, MOSI, MISO, CS, detecção de cartão | `spi00` e GPIO | P2 | SCK P2.01, MOSI P2.02, MISO P2.04, CS P2.03 |
| Display | SCK, MOSI, CS, DISP | `spi22` e GPIO | P3 | SCK P3.03, MOSI P3.00, CS P3.02 |
| Display | EXTCOMIN (cerca de 60 Hz com a luz acesa) e luz | `pwm20` | P1 | — |
| GNSS | TXD, RXD, RESET_N, EXTINT, TIMEPULSE, OE do tradutor | `uart21` e GPIO | P1 | TX P1.04, RX P1.05, RESET P1.06, EXTINT P1.07, TIMEPULSE P1.13 |
| Sensores | SDA, SCL, INT1 e INT2 do IMU, INT do barômetro | `i2c23` e GPIO | P1 e P3 | SDA P1.02, SCL P1.03, INT1 P3.04 |
| Energia | SDA, SCL, interrupção do nPM1300 (GPIO3), ALRT do MAX17262, IRQ e bloqueio de carga (DIS_STO_CH) do AEM10900 | `i2c30` e GPIO | P0 | — |
| Botões | 3 entradas com despertar | GPIO | P0 ou P1 | P1.26, P1.09, P1.08 |
| LED RGB | 3 canais | `pwm22` | P1 ou P3 | — |
| Buzzer | 2 canais em contrafase | `pwm21` | P1 ou P3 | — |
| Console | TX, RX em pads de teste | `uart30` | P0 | — |
| Dedicados | USB (D+, D−, VBUS), SWD, cristais internos do módulo | — | — | — |

Total: 38 GPIO dos 66 do módulo.

## Placa de circuito impresso

- **Contorno:** 55 × 97 mm, cantos com raio de 4 mm, dentro da caixa de 62 × 104 mm (paredes de cerca de 2 mm e folga de 0,5 mm). Origem no canto de cima à esquerda, com a placa vista pela frente.
- **Espessura e camadas:** 1,0 mm, 4 camadas, controle de impedância.

| Camada | Uso |
|---|---|
| L1 (frente) | componentes da face do display, sinais rápidos curtos, rede em π da antena GNSS |
| L2 | GND sólido, referência de L1 e L3 |
| L3 | 3V0 e 1V8 em áreas, sinais lentos |
| L4 (trás) | componentes da face da bateria, sinais |

As espessuras de dielétrico saem com o fabricante para 90 Ω diferencial no USB e 50 Ω na RF_IN do GNSS.

- **Zonas** (posições do [desenho](13-placa-nova.md#como-fica-o-aparelho), a fechar no layout):

| Zona | x (mm) | y (mm) | Conteúdo | Restrição |
|---|---|---|---|---|
| Antena GNSS | 0 a 55 | 0 a 8 | TE L000670 na borda de cima (protótipo) ou os contatos dos elementos na parede | sem cobre sob a antena em todas as camadas; nada metálico mais alto que 3 mm num raio de 10 mm |
| GNSS | 20 a 35 | 2 a 16 | módulo MAX e filtro do 1V8 | sob o display: altura até 2,6 mm (o módulo tem 2,5 mm) |
| LED | 50 a 53 | 0 a 3 | LED RGB sob o furo de luz | trilhas curtas, fora da área livre da antena |
| IMU e magnetômetro | 8 a 15 | 20 a 23 | LSM6DSV16X e LIS2MDL | longe de correntes altas e de ímãs |
| FPC do display | 3,7 a 7,1 | 29,5 a 39,5 | Hirose FH28 na borda esquerda | longe das antenas |
| Buzzer | 10,5 a 21,5 | 46,5 a 55,5 | piezo, com saída de som na caixa | — |
| Energia | 16 a 38 | 67 a 90 | nPM1300, AEM10900, MAX17262, indutores, conectores da bateria e dos painéis | cobre largo nos caminhos de corrente |
| microSD | 0 a 14 | 67 a 83 | soquete com a boca na borda esquerda | — |
| Barômetro | 4,5 a 8 | 83 a 86,5 | BMP585 na face de trás, junto do respiro | fora da sombra da bateria |
| BM20C | 45 a 55 | 73 a 88 | módulo e área livre da antena de 2,4 GHz (y 85 a 91) | guia de layout da Fanstel; longe dos painéis e da antena GNSS |
| Botões | 8 a 47 | 88 a 95 | 3 chaves táteis (centros em x 12,5, 27,5 e 42,5, y 91,5) | — |
| USB-C | 23 a 32 | 94 a 97 | conector na borda de baixo | TVS junto do conector |
| Luz ambiente | 0,4 a 2,4 | 88 a 90 | OPT3001 sob a janela de baixo | — |

- **Faces:** na face da frente, sob o display, só peças de até 2,6 mm; na face de trás, na área da bateria (x 9,5 a 45,5, y 22,5 a 82,5), só peças de até 1,2 mm, com fita isolante sobre elas.
- **Fixação:** 4 furos M2 nos cantos, alinhados aos parafusos da traseira; o furo de baixo à direita sai da área livre da antena do BM20C.
- **Conectores:** display (FPC de 10 vias) na borda esquerda; bateria (3 vias) na face de trás; painéis em três grupos (frente, esquerda, direita), por pads de mola ou FPC; USB-C na borda de baixo; microSD na borda esquerda.

## Empilhamento mecânico

Da frente para trás, na área do display:

| Camada | Espessura (mm) | Observação |
|---|---|---|
| Parede da frente e janela | 1,2 | policarbonato ou vidro sobre o display |
| Display JDI | 1,39 | a versão com luz (C) pode ser mais grossa: medir a amostra |
| Espuma e folga | 0,5 | — |
| Componentes da face da frente | até 2,6 | o módulo GNSS tem 2,5 mm |
| PCB | 1,0 | 4 camadas |
| Componentes da face de trás | até 1,2 | fora da área da bateria, até 2,0 mm |
| Bateria | 7,6 | 7,0 mm e cerca de 8 % de folga para inchaço |
| Parede de trás | 1,5 | o engate de quarto de volta soma 3 mm por fora |
| **Total** | **17,0** | a caixa tem 19 mm: sobram cerca de 2 mm para tolerâncias e para os chanfros com painéis |

## Regras de layout

1. **Antena GNSS:** área livre em todas as camadas, rede em π junto ao ponto de alimentação e sintonia com VNA na caixa final; bateria, soquete microSD, parafusos e painéis fora do caminho entre a antena e o céu ([13](13-placa-nova.md#regras-de-projeto)).
2. **Rádio de 2,4 GHz:** a antena do BM20C no canto oposto ao GNSS, com a área livre do guia do módulo; medir o S21 entre as duas antenas no protótipo.
3. **Clocks e ruído:** microSD a 16 MHz; linhas do display, do cartão e do USB em camada interna entre planos de terra e longe da zona do GNSS. Clocks até cerca de 2 MHz, como o SPI do display, têm harmônicos dentro de L1 e L5: bordas lentas (resistor em série), trilhas curtas e o FPC do display pela esquerda.
4. **Fontes chaveadas:** laços dos bucks do nPM1300 e do boost do AEM10900 curtos, indutores junto dos pinos, no lado oposto e na diagonal da antena GNSS; ripple do 1V8 abaixo de 50 mV no módulo GNSS.
5. **USB:** D+ e D− como par diferencial de 90 Ω, curto, sem vias se possível; TVS junto do conector.
6. **Bateria e temperatura:** NTC do pack no nPM1300 e segundo NTC colado na célula para o AEM10900; célula longe dos painéis e do carregador, porque a caixa ao sol passa de 45 °C.
7. **Vedação:** respiro com membrana para o barômetro, tampas no USB-C e no microSD, botões selados.

## Teste e bring-up

- **Pontos de teste:** VBUS, VBAT, VSYS, 3V0, 1V8, SD3V0, VBCKP e GND, com jumper de 0 Ω em série nos trilhos de cada bloco (BM20C, GNSS, display, sensores, microSD) para medir corrente com o PPK2.
- **Depuração:** SWD (SWDIO, SWDCLK, reset, VDD e GND) num footprint Tag-Connect TC2030 e console no `uart30`, em pads.

```mermaid
flowchart LR
    A["1 · energia<br/>BUCK1 e BUCK2 pelos VSET"] --> B["2 · MCU<br/>SWD, LED"]
    B --> C["3 · I2C de energia<br/>nPM1300, MAX17262, AEM10900"]
    C --> D["4 · display"]
    D --> E["5 · sensores"]
    E --> F["6 · GNSS<br/>UART, TIMEPULSE, C/N0"]
    F --> G["7 · microSD e USB"]
    G --> H["8 · BLE e ANT+"]
    H --> I["9 · carga solar e dupla<br/>PPK2 e temperatura"]
    I --> J["10 · consumo por trilho<br/>e estados de energia"]
```

## Pendências

| Pendência | Como fechar |
|---|---|
| Datasheet e pinagem do BM20C | pedir à Fanstel: pinos de USB e NFC, guia de layout da antena, tolerância do cristal de 32,768 kHz |
| Módulo GNSS | teste A/B do MAX-F10S e do MAX-M10N-10B; driver `u-blox,m10` do Zephyr 4.5 ou parser UBX próprio ([13](13-placa-nova.md#impacto-no-firmware)) |
| Carga dupla | nPM1300 e AEM10900 na mesma célula, em bancada: fim de carga, medição, temperatura |
| Display | amostras do JDI (espessura da versão com luz, lado do contato do FPC) e mudanças no driver |
| Antena GNSS de parede | fornecedor de antena sob medida para a segunda versão |
| Bateria | pack com NTC de 3 fios, proteção e UN38.3 |
| Mecânica | caixa, janela dos painéis, vedação, fixação da placa e dos painéis |
| Estoque | LSM6DSV16X e LIS2MDL; alternativas LIS2DW12 e MMC5603NJ |

## Referências

- [13-placa-nova.md](13-placa-nova.md): pesquisa, alternativas e fontes de cada bloco.
- Conferidos para esta especificação: u-blox MAX-F10S Data sheet R03 (faixas de VCC, V_IO e V_BCKP, corrente de backup), MAX-M10N-00B Data sheet R05 (pinos 16 e 17 reservados); Nordic nRF54LM20A/B Datasheet v1.0 (USBREG: VBUS de 5 V e VDD para o USB) e nPM1300 Product Specification v1.1 (VBUSOUT, CC1 e CC2 com Rd interno, VSET1 e VSET2, chaves de 100 mA, "VSYS must not be supplied from an external source"); e-peas AEM10900 (endereço 0x40 ou 0x41, corrente de entrada); TI [TXU0204](https://www.ti.com/product/TXU0204), [TPS7A02](https://www.ti.com/product/TPS7A02) e [OPT3001](https://www.ti.com/product/OPT3001).
- NCS v3.3.0: `zephyr/drivers/sensor/bosch/bmp581/bmp581.h` (endereços 0x46 e 0x47 da família BMP5) e os exemplos com `lis2mdl@1e`, `opt3001@44`, `npm1300@6b`, `max17262@36` e `lsm6dsv16x@6b`.
