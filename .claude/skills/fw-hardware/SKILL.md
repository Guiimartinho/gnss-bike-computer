---
name: fw-hardware
description: Trabalhar com a placa myStravaB V3 e o devicetree do port Zephyr do GNSS Bike Computer - pinagem das revisões v1/v2/v3, overlay nrf52840_strava.overlay sobre o nRF52840-DK, polaridades de GPIO, nós do DK que colidem com a placa, alimentação e latch pelo STC3100, esquema e placa Eagle. Use ao mexer em zephyr_app/boards/, em pinos, em GPIO, ao criar uma board própria ou ao investigar comportamento elétrico.
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
6. Depois de mexer: build, `grep` no `zephyr.dts` gerado para cada pino alterado e registro em `docs/02-hardware.md`.

## Alimentação e latch

- O botão central liga os reguladores (SWON → M2 → POW_EN). O firmware precisa escrever `REG_CONTROL = 0x02` no STC3100 (IO0 em 0) para manter a placa ligada; isso está em `stc3100_init()`.
- Para desligar: `REG_MODE = 0` e `REG_CONTROL = 0x01`. **Qualquer escrita em `REG_CONTROL` com o bit 0 em 1 desliga a placa na hora.**
- O port ainda não desliga nem tem o auto-off de 15 min do legacy (`legacy/source/scheduling/power_scheduler.cpp`).

## Board própria

O alvo atual é o DK com overlay. Uma board `mystravab_v3` (em `zephyr_app/boards/<vendor>/mystravab_v3/`, hardware model v2 do Zephyr) removeria de vez os nós do DK, levaria o console para RTT ou USB CDC e deixaria de exigir `DTC_OVERLAY_FILE` fixo no `CMakeLists.txt`. Está na fase 1 do roteiro; combine com o dono antes.

## Cuidados com a placa real

- O console do DK (P0.06/P0.08) não existe na V3: use RTT pelo J1.
- Para gravar a placa, o 3,3 V precisa estar ligado: segure o botão central ou alimente o trilho.
- P0.18 é o nRESET e também o SCK da NOR: nunca habilite `CONFIG_GPIO_AS_PINRESET`.
- Shunt do STC3100: o esquema diz 20 mΩ, o código usa 100 mΩ; meça antes de confiar na corrente.
- Os Gerbers da pasta `hardware/` são da V2.
