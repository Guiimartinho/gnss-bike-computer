---
name: fw-hardware
description: Trabalhar com a placa myStravaB V3 e o devicetree do port Zephyr do GNSS Bike Computer - pinagem das revisões v1/v2/v3, overlay nrf52840dk_nrf52840.overlay sobre o nRF52840-DK, polaridades de GPIO, nós do DK que colidem com a placa, alimentação e latch pelo STC3100, esquema e placa Eagle. Use ao mexer em zephyr_app/boards/, em pinos, em GPIO, ao criar uma board própria ou ao investigar comportamento elétrico.
---

# Placa e devicetree

Referência completa: `docs/02-hardware.md`. Fonte de verdade da pinagem: `hardware/myStravaB_V3.sch` e `legacy/custom_board_v3.h` (que batem entre si, exceto o LED em P1.04, que não existe na placa).

## Pinos da V3

| Sinal | Pino | Polaridade / detalhe |
|---|---|---|
| Botões esquerdo, centro, direito | P0.14, P0.13, P0.11 | ativos baixos com pull-up; o centro também liga a placa |
| I2C SDA / SCL | P1.00 / P1.01 | pull-ups de 4,7 kΩ; BME280 0x76, FXOS 0x1E, STC3100 0x70, FRAM 0x50 |
| GPS TX / RX | P0.05 / P0.07 | UART 9600 |
| GPS reset (HW_R) | P0.03 | **ativo baixo** |
| GPS standby (HW_S) | P1.15 | **baixo = standby** |
| GPS FIX | P1.14 | alto com fix |
| LCD SCK / MOSI / CS | P0.15 / P0.16 / P0.17 | CS **ativo alto** |
| SD MOSI / CS / SCK / MISO | P0.25 / P0.26 / P0.27 / P0.28 | CS ativo baixo, sem card-detect |
| FXOS INT1 / RST | P1.02 / P1.03 | RST **ativo alto**, sem resistor na placa |
| NeoPixel | P1.13 | via BSS138 para 5 V |
| SWD | pads SWDIO/SWDCLK no J1 (soquete microSD) | — |

## Regras do overlay

1. **Flags de GPIO dizem a verdade elétrica**: `GPIO_ACTIVE_LOW` quando o nível baixo liga a função; o código trabalha com nível lógico (`gpio_pin_set_dt(…, 1)` = ativo). Nunca inverta no C o que o devicetree já inverte.
2. **Nó do DK em pino da placa é desligado**, inclusive os filhos (`&qspi` e `&mx25r64`, `&spi3`, `&pwm0` já estão). Confira no `build/zephyr_app/zephyr/zephyr.dts` e no `.config` que o driver saiu.
3. **Pinctrl herdado do DK**: um grupo do DK pode deixar propriedades (`bias-pull-up`) no seu grupo; use `/delete-property/`. O `uart0` do console ficou só com TX/RX.
4. **Sensores e receptor com driver do Zephyr**: configure pelo devicetree e pelo Kconfig do driver, não por registradores no app. O `reset-gpios` do FXOS garante o reset antes do `main()`.
5. **`gpio-keys` só para teclas**: com o subsistema de entrada, todo nó `gpio-keys` vira teclado. Não use `gpio-keys` para dar nome a pinos (os nós falsos da V3 saíram em 2026-09-19).
6. **O código não cita instâncias do SoC** (`uart1`, `i2c0`, `spi1`): os serviços acham os dispositivos pelos aliases `gnss`, `baro0`, `imu0`, `mag0`, `light0`, `watchdog0`, `backlight` (PWM da luz da tela), pelo disco `SD` (`zephyr,sdmmc-disk`), pela tela escolhida (`zephyr,display`), pelas teclas (nó com rótulo `longpress`, `zephyr,input-longpress` sobre os `gpio-keys`) e, na energia, por `pmic-regulators`. Cada placa define esses nomes no overlay dela; um alias ausente deixa o serviço sem o dispositivo.
7. **Tela** (`docs/05-arquitetura-zephyr.md#tela`): nó `jdi,lpm027m128b` (ou `sharp,ls027b7dh01`) no SPI, com `cs-gpios` **ativo alto**, `spi-cs-setup-delay-ns`/`spi-cs-hold-delay-ns` da ficha, `disp-gpios`, `extcomin-gpios` (sem ele, VCOM pelo SPI) e `power-gpios` quando a placa corta a alimentação do painel. O JDI quer os sinais em 3,0 V.
7. Depois de mexer: build, `grep` no `zephyr.dts` gerado para cada pino alterado e registro em `docs/02-hardware.md`.

## Alimentação e latch

- O botão central liga os reguladores (SWON → M2 → POW_EN). O firmware precisa escrever `REG_CONTROL = 0x02` no STC3100 (IO0 em 0) para manter a placa ligada (o port fazia isso em `stc3100_init()`, que saiu em 2026-09-19 com os drivers da V3).
- Para desligar: `REG_MODE = 0` e `REG_CONTROL = 0x01`. **Qualquer escrita em `REG_CONTROL` com o bit 0 em 1 desliga a placa na hora.**
- No port de hoje, quem desliga é a máquina de sistema do serviço de energia (`src/svc/power/sys_fsm.c`): ship mode do nPM1300 na placa nova (alias `pmic-regulators`), System OFF com VBUS ou sem PMIC. As regras dos 15 min vêm do `legacy/source/scheduling/power_scheduler.cpp`.

## Board própria

**Decidido em 2026-09-18: o produto terá board própria com o nRF54LM20A** e esquemático próprio (GNSS, bateria e display melhores, display colorido de 2,7", painel solar pequeno na caixa). A V3 existe só como esquema: não há placa física para testar. Comparação dos MCUs em `docs/02-hardware.md#próxima-placa`; a proposta de componentes (display, GNSS e antena, energia e painel solar, sensores, pinos) está em `docs/13-placa-nova.md`, a especificação (trilhos, lista de materiais, endereços I2C, pinos, PCB de 55 × 97 mm e empilhamento) em `docs/14-hardware-placa-nova.md`, e a avaliação que escolheu cada componente (carga por USB-C e painel solar, GNSS MAX-M10N-10B, display com o Sharp como plano B no mesmo conector) em `docs/15-avaliacao-componentes.md`.

- Enquanto a placa própria não existe, há dois alvos: o nRF52840-DK com o overlay da V3 e o nRF54LM20 DK (`nrf54lm20dk/nrf54lm20a/cpuapp`, com `boards/nrf54lm20dk_nrf54lm20a_cpuapp.overlay` e `.conf`; o DK vem com o nRF54LM20B, igual ao A mais a NPU). Mudança no devicetree ou no Kconfig compila nos dois.
- **Pinos do nRF54LM20A** (ficha 4539_001 v1.0): o SCL do TWIM e o SCK do SPIM só funcionam nos pinos de clock da tabela 79 (P0.03, P0.04, P0.06, P0.07, P1.03, P1.04, P1.07, P1.13, P1.14, P1.17, P1.18, P1.23, P1.24, P2.01, P2.06, P3.03 e P3.04), com o dado num pino vizinho; P1.01 (NFC1) e P1.02 (NFC2) saem do reset como antena NFC, com o GPIO desligado, e só viram GPIO com `nfct-pins-as-gpios` no nó `uicr`, que desliga o NFC. Módulo com cristal de 32,768 kHz embutido deixa 64 dos 66 GPIO (P1.20 e P1.21 são o XL1 e o XL2).
- Ao portar para o nRF54L: UARTE, SPIM e TWIM têm outras instâncias (`uart20`, `uart21`, `spi00`, `i2c22`...), o tempo vem do GRTC, a NVM é RRAM (settings no ZMS, não no NVS), o WDT é `wdt30`/`wdt31`, a causa do reset fica no periférico RESET e não há QSPI.
- A board entra em `zephyr_app/boards/<vendor>/<board>/` no modelo de hardware v2 do Zephyr (`board.yml`, `Kconfig.<board>`, `<board>_<soc>.dts`, pinctrl, `_defconfig`, `board.cmake`), sem nós que não existem na placa, com console por RTT ou USB CDC.
- A placa já vem do `-b` e o overlay de cada alvo tem o nome automático (`boards/<placa>.overlay`): a board própria entra sem mexer no `CMakeLists.txt`.
- O MCU precisa estar na lista do `sdk-ant` (ANT+ é obrigatório).

## Cuidados com a placa real

- O console do DK (P0.06/P0.08) não existe na V3: use RTT pelo J1.
- Para gravar a placa, o 3,3 V precisa estar ligado: segure o botão central ou alimente o trilho.
- P0.18 é o nRESET e também o SCK da NOR: nunca habilite `CONFIG_GPIO_AS_PINRESET`.
- Shunt do STC3100: o esquema diz 20 mΩ, o código usa 100 mΩ; meça antes de confiar na corrente.
- Os Gerbers da pasta `hardware/` são da V2.
