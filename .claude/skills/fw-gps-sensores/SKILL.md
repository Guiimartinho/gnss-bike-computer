---
name: fw-gps-sensores
description: Trabalhar com o GNSS e os sensores do GNSS Bike Computer no port Zephyr - serviço GNSS pela API de GNSS do Zephyr (u-blox M10 por UBX na placa nova, NMEA genérico nos DKs), serviço de sensores pela API de sensores (BMP585, BMI270, MMC5633NJL, OPT3001; BME280 e FXOS8700 da V3), inclinação, rumo e rugosidade (tilt.c), e o que o legacy fazia com o MediaTek M10578-A3 (PMTK, EPO), o STC3100 e a FRAM. Use ao mexer em zephyr_app/src/svc/gnss/, src/svc/sensors/ ou na leitura de sensores do modelo.
---

# GPS e sensores

## GNSS no port

- O serviço (`src/svc/gnss/gnss_svc.c`) usa o receptor do alias `gnss` pela API de GNSS do Zephyr: `GNSS_DATA_CALLBACK_DEFINE` e `GNSS_SATELLITES_CALLBACK_DEFINE`. Os callbacks rodam no workqueue próprio do modem (`CONFIG_MODEM_DEDICATED_WORKQUEUE`, 2048 B) e só convertem e publicam `gnss_fix` e `gnss_sky`: uma posição por época.
- No alvo da placa nova o receptor é o **u-blox MAX-M10N por UBX**, com driver próprio em `zephyr_app/modules/gnss_drivers/drivers/gnss/` (compatível `u-blox,max-m10`, sobre o `modem_ubx`); o Zephyr do NCS só traz M8 e F9P, e o M10 não aceita as mensagens `UBX-CFG-*` antigas. No alvo da V3 continua o `gnss-nmea-generic` (o M10578-A3 fala NMEA).
- Documentos da u-blox: "M10 SPG 5.30 Interface description" (UBXDOC-304424225-20395) e "MAX-M10N Integration manual" (UBXDOC-304424225-19802). Nunca invente identificador de chave nem deslocamento de campo: confira no documento (uma chave errada foi pega assim).
- Configuração por `UBX-CFG-VALSET` nas camadas **RAM e BBR**: o standby por software apaga a RAM do receptor, inclusive a configuração.
- LEAP é `CFG-PM-OPERATEMODE = 2`, no máximo 2 Hz e sem pulso de tempo; nele o receptor pode perder mensagens do host, então comando vai com repetição e o lote de configuração passa antes pela potência plena. Dormir é `UBX-RXM-PMREQ` com backup e force, acordando pela linha RX.
- Nada que espere resposta do receptor pode rodar no workqueue do modem: a resposta chega num item de trabalho dele. Quem manda comando é a thread `gnss`.
- A máquina de energia do receptor (`src/svc/gnss/gnss_power.c`, `test_gnss_power`) é pura: backup, aquisição, LEAP, potência plena, e o reinício por silêncio (10 s reconfigura, 30 s puxa o RESET_N).
- O modo manda: o receptor fica ativo em CRS, PRC e DBG e dorme em FEC e Zwift, como o legacy (`BoucleCRS.cpp:38`, `BoucleFEC.cpp:45`).
- Modelo dinâmico da u-blox: o `BIKE` é de motocicleta; para bicicleta vale o `PORT` (`docs/13-placa-nova.md`).

## GNSS da V3: Antenova M10578-A3 (MediaTek MT3333)

- UART 9600 no boot; saída padrão GGA, GSA, GSV, RMC, VTG por época (1 Hz); multi-constelação (pode mandar `GN`/`GL`).
- Pinos: reset e standby **ativos baixos**, FIX **alto com fix**.
- Backup (BV) na bateria: configurações feitas por PMTK (inclusive baud) sobrevivem ao desligamento.
- O port teve parser e gestão próprios (`gps_mgmt.c`, `nmea_parser.c`, `gps_epo.c`) até 2026-09-19; saíram para a API de GNSS do Zephyr.

### PMTK do legacy (`libraries/GlobalTop/LocusCommands.h`)

| Comando | Uso no legacy |
|---|---|
| `$PMTK010` (recebido) | módulo pronto: dispara a troca de baud |
| `$PMTK251,115200` | troca para 115200; WDT de baud alterna 9600 e 115200 se nada chega por 3 s |
| `$PMTK741,lat,lon,alt,AAAA,MM,DD,hh,mm,ss` | host aiding com a posição do celular (LNS) |
| `$PMTK721,...` | EPO, desativado no legacy |
| `$PMTK253,1` | **modo binário**: nunca envie no fluxo NMEA (o `gps_epo.c` do port envia: defeito aberto) |
| `$PMTK220,<ms>` | intervalo de fix (no legacy com bug, sem uso) |

Antes de mandar PMTK novo: checksum XOR entre `$` e `*`, com `*` fora da conta (o `gps_mgmt_set_rate()` do port inclui o `*`: defeito).

## Sensores no port

- O serviço (`src/svc/sensors/sensors_svc.c`) acha os sensores pelos aliases `baro0`, `imu0`, `mag0` e `light0` e lê pela API de sensores do Zephyr (`sensor_sample_fetch` e `sensor_channel_get`); um alias ausente deixa o sensor de fora. Placa nova: BMP585 (`bosch,bmp581`), BMI270, MMC5633NJL (`memsic,mmc56x3`), OPT3001. V3: BME280 e FXOS8700.
- Ritmo do legacy: acelerômetro a 50 Hz com média de 50 amostras (`fxos.cpp:600`, `:72`), barômetro a 10 Hz; magnetômetro e luz a 1 Hz.
- `src/svc/sensors/tilt.c` (teste `test_tilt`): inclinação e rolagem da média pelas equações do AN4248 nos eixos da placa (X à frente, Y à esquerda, Z para cima); rumo compensado da inclinação (o legacy não compensava: `fxos.cpp:796-821`); rugosidade como desvio médio absoluto, na unidade do legacy (contagens de ±4 g, 2048 por g). A montagem dos sensores na placa nova e a calibração do magnetômetro ainda faltam.
- A API de sensores dá a pressão em kPa, a aceleração em m/s² e o campo em gauss.

## BME280, FXOS8700 e STC3100 (V3)

- BME280: legacy com pressão ×16, temperatura ×1, IIR ×16, standby 62,5 ms, média de 10 leituras; o driver do Zephyr tem outros padrões de Kconfig (`CONFIG_BME280_STANDBY_62MS`, `CONFIG_BME280_FILTER_16`, `CONFIG_BME280_TEMP_OVER_1X` aproximam).
- FXOS8700: ±4 g, 50 Hz por interrupção no legacy; `reset-gpios` ativo alto no overlay.
- STC3100: sem driver no Zephyr; o do port saiu em 2026-09-19 com a V3, e o nRF52840 DK ficou sem leitura de bateria. Conversões do legacy em `docs/06-algoritmos.md#bateria`.

## FRAM FM24CL16B

2 KB em 0x50 a 0x57 (página nos bits baixos do endereço). O legacy guarda nela as configurações (24 B em 0x0000, versão 0x0002, CRC). O port usa NVS; para ler a FRAM há o binding `fujitsu,mb85rcxx` do Zephyr (`size = <2048>`, `address-width = <8>`).

## Antes de terminar

Testes de host verdes, build sem aviso novo e, se mudou pino ou polaridade, o `zephyr.dts` gerado conferido. Hardware: diga explicitamente o que não foi testado na placa.
