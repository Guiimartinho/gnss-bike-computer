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
4. **Sensores com driver nativo** (BME280, FXOS8700): configure pelo devicetree e pelo Kconfig do driver, não por registradores no app. O `reset-gpios` do FXOS garante o reset antes do `main()`.
5. **Sem binding** (`st,stc3100`): o nó é aceito e ignorado; o driver próprio acessa pelo `hal_i2c`.
6. **O código não cita instâncias do SoC** (`uart1`, `i2c0`, `spi1`): usa os aliases `gps-uart`, `sensor-i2c`, `lcd-spi`, `sdc-spi`, `sw0`–`sw2`, `led0` e os rótulos da aplicação (`gps_reset`, `gps_stdby`, `gps_fix`, `imu_int1`, `imu_reset`, `neo_data`, `baro`, `fxos`). Cada placa define esses nomes no overlay dela.
7. Depois de mexer: build, `grep` no `zephyr.dts` gerado para cada pino alterado e registro em `docs/02-hardware.md`.

## Alimentação e latch

- O botão central liga os reguladores (SWON → M2 → POW_EN). O firmware precisa escrever `REG_CONTROL = 0x02` no STC3100 (IO0 em 0) para manter a placa ligada; isso está em `stc3100_init()`.
- Para desligar: `REG_MODE = 0` e `REG_CONTROL = 0x01`. **Qualquer escrita em `REG_CONTROL` com o bit 0 em 1 desliga a placa na hora.**
- No port: `stc3100_shutdown()` faz isso; o `power_scheduler` (porta de `legacy/source/scheduling/power_scheduler.cpp`) chama depois de 15 min sem posição processada em CRS/PRC, e o menu tem "Power Off". Na USB a placa não apaga; o agendador tenta de novo 15 min depois.
- Não escreva em `REG_CONTROL` fora do `stc3100.c`: um bit 0 em 1 por engano desliga a placa em campo.

## Board própria

**Decidido em 2026-09-18: o produto terá board própria com o nRF54LM20A** e esquemático próprio (GNSS, bateria e display melhores, display colorido de 2,7", painel solar pequeno na caixa). A V3 existe só como esquema: não há placa física para testar. Comparação dos MCUs em `docs/02-hardware.md#próxima-placa`.

- Enquanto a placa própria não existe, há dois alvos: o nRF52840-DK com o overlay da V3 e o nRF54LM20 DK (`nrf54lm20dk/nrf54lm20a/cpuapp`, com `boards/nrf54lm20dk_nrf54lm20a_cpuapp.overlay` e `.conf`; o DK vem com o nRF54LM20B, igual ao A mais a NPU). Mudança no devicetree ou no Kconfig compila nos dois.
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
