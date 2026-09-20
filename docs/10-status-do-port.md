# Status do port

Onde o port Zephyr (`zephyr_app/`) está em relação ao firmware original (`legacy/`), o que foi corrigido na revisão de 2026-09-18, a migração para serviços de 2026-09-19, os defeitos que continuam abertos e a ordem proposta para o que falta. Substitui o gap analysis de novembro de 2025 (`historico/2025-11/`), que listava como ausentes módulos portados logo depois e trazia constantes erradas.

**Nesta página:** [Resumo](#resumo) · [Matriz por área](#matriz-por-área) · [Correções de 2026-09-18](#correções-de-2026-09-18) · [Migração de 2026-09-19](#migração-de-2026-09-19) · [Defeitos abertos](#defeitos-abertos) · [Roteiro](#roteiro) · [Decisões do dono](#decisões-do-dono)

## Resumo

- **Compila** no NCS v3.3.0 sem aviso (nRF52840 DK: FLASH 474.268 B, RAM 220.672 B; nRF54LM20 DK: FLASH 498.256 B, RAM 250.336 B) e **passa em 16 conjuntos de testes de host** (169 casos). **Nada foi testado na placa** nem nos DKs.
- Desde 2026-09-19 o firmware é a base da arquitetura de [16](16-arquitetura-firmware.md): sete serviços com thread própria, eventos no zbus, máquinas de sistema e de modo no SMF, hardware pelas APIs do Zephyr ([05](05-arquitetura-zephyr.md)). O HAL próprio, os drivers da V3 e a interface em paisagem saíram.
- Os algoritmos do legacy continuam no `src/model` e rodam na thread do modelo; alguns **não funcionariam** ainda (segmentos, formatos de arquivo, BLE central), e a interface nova, em LVGL, está no firmware com o driver próprio da tela (JDI LPM027M128B e Sharp LS027B7DH01, em retrato), as teclas com toque longo e a luz; foi testada no PC (29 telas em 2 temas), mas nunca vista em tela de verdade ([18](18-interface-telas.md)).
- ANT+: a pilha do add-on `sdk-ant` entra no build com `ANT=1` e sobe no boot (`rf_ant_init()`), mas os perfis (HRM, BSC, FE-C) ainda não foram portados; os clientes BLE ainda não funcionam de ponta a ponta.

```mermaid
pie showData
    title Áreas do legacy no port (20 áreas da matriz)
    "fiel" : 1
    "parcial ou diferente" : 14
    "stub ou não ligado" : 1
    "ausente ou quebrado" : 4
```

## Matriz por área

Estados: **fiel** (mesmo comportamento), **diferente** (existe com regras ou constantes diferentes), **parcial**, **não ligado** (código existe, ninguém chama), **stub**, **ausente**, **quebrado** (ligado, mas falha).

| Área | Legacy | Port | Estado | Detalhe |
|---|---|---|---|---|
| Execução | task manager cooperativo, eventos, tick de 5 ms | sete threads preemptivas de serviço, cada uma dormindo na sua caixa de entrada; eventos no zbus; a thread `model` é a única escritora do modelo | diferente | preemptivo, com cópias no lugar de estado compartilhado ([05](05-arquitetura-zephyr.md#threads)) |
| Watchdog | 4 s, alimentado por boucle e LCD | `task_wdt`: um canal de 4 s por thread de serviço, sete no total, sobre o WDT do nRF | diferente | mais estrito que o legacy (cada thread responde pelo seu canal); não testado na placa ([05](05-arquitetura-zephyr.md#watchdog)) |
| Energia | latch pelo IO0 do STC3100, auto-off 15 min | máquina de sistema no SMF (`sys_fsm.c`): auto-off de 15 min com os pings do legacy (posição em CRS, PRC e DBG; rolo em FEC), desligamento em etapas com a resposta de cada serviço, ship mode do nPM1300 ou System OFF; medidor MAX17262 por driver próprio, com bateria fraca (10 %) e crítica (0 %); nPM1300 com os trilhos travados, o limite do VBUS pela fonte USB-C e a máquina de carga; AEM10900 por driver próprio, em 4,05 V com a placa ligada e de volta aos pinos antes de desligar | parcial | a potência do painel em mW espera o fator θ·L da e-peas; `test_sys_fsm`, `test_max17262`, `test_battery`, `test_charge`, `test_aem10900`; não testado na placa |
| Modos | CRS, PRC, FEC, Zwift, MSC com `init`/`invalidate` | máquina de modos no SMF na thread do modelo: CRS, PRC, FEC, Zwift, DBG; o GNSS dorme em FEC e Zwift | parcial | o laço do Zwift (`$LOC`) e o MSC ainda não existem |
| GPS e NMEA | TinyGPS++ com checksum, PMTK010→PMTK251, WDT de baud | API de GNSS do Zephyr pelo alias `gnss`: driver próprio do u-blox M10 por UBX no alvo da placa nova (`u-blox,max-m10`, com LEAP, standby e reinício por silêncio) e `gnss-nmea-generic` no alvo da V3, uma posição por época | parcial | falta EPO/AssistNow e o teste com receptor de verdade |
| Sensores (inclinação, rumo, rugosidade) | FXOS8700 a 50 Hz, média de 50 amostras, rugosidade por desvio médio (`fxos.cpp`) | serviço de sensores pela API do Zephyr (aliases `baro0`, `imu0`, `mag0`, `light0`) e `tilt.c`: mesmo ritmo e janela, rugosidade na unidade do legacy, rumo compensado pelo AN4248 | parcial | `test_tilt`; os eixos dos sensores na placa nova a confirmar; sem calibração do magnetômetro |
| Altitude (Kalman 3 estados) | `Attitude::computeFusion` | `kalman_altitude.c` + `udmatrix.c` | fiel | corrigidos em 2026-09-19 o P0 (900 em todos os elementos), o `bound` (valor absoluto) e a soma de matrizes que zerava o destino, além da taxa (uma fusão por época, com a pressão média de 1 s); `test_kalman_altitude`, `test_udmatrix` ([06](06-algoritmos.md#altitude-kalman-de-3-estados)) |
| Distância | equiretangular com 6.371.008 m, posições brutas, descarte dos primeiros 25 m, instantâneo a cada 15 m | a mesma fórmula (`vecteur.c`) e a mesma regra (`distance.c`) | fiel | `test_vecteur`, `test_distance`; o `locator.c` deixou de ter uma segunda fórmula |
| Drift barômetro/GPS | τ = 800/801 por fix | igual em `attitude.c` | fiel | uma vez por época; o `baro_drift.c` duplicado saiu |
| Potência estimada | `1.025·(9.81·W·vz + 0.004·9.81·W·v + 0.204·v³)` | a mesma fórmula em `power_estimate.c`, com a velocidade da posição anterior e o peso só do ciclista | fiel | `test_power_estimate` contra a transcrição do legacy; satura em vez de dar a volta no `int16_t` |
| Zonas de potência e suffer score | PowerZone, SufferScore | `power_zone.c`, `suffer_score.c`, alimentados como no legacy | diferente | zonas de potência com o rolo em FEC (`BoucleFEC.cpp:75`); o score a cada 1 s com a FC do momento (o legacy, a cada volta do laço, `Model.cpp:377`) |
| Zonas RR | RRZone | `rr_zone.c`, a cada dado da cinta com RR | diferente | o legacy chama a cada volta do laço e repete o último RR ([06](06-algoritmos.md#zonas)) |
| Segmentos Strava | carga por distância, ativação, desempenho, até 2 na tela | `segment.c`, `liste_points.c`, `vecteur.c`, com os arquivos do legacy lidos por `segment_file.c` | parcial | a varredura, a carga e o alocador são os do legacy (nome com a posição, 300 m para alocar, texto `lat ; lon ; rtime ; alt`, alocador a cada época no serviço do modelo), com os pontos num pool de 3 × 256 e decimação do que passa disso; `test_segment` (14 casos) e `test_segment_file` nos 138 arquivos reais; falta mostrar o segmento na tela e testar com cartão de verdade |
| Parcours (GPX) | `.PAR` texto, seleção no menu | `parcours.c` com `.CRS` | não ligado | `parcours_load` sem chamador; loader quebra com CRLF |
| Log no SD | `@DDMMYY.txt`, 19 campos | o mesmo arquivo e os mesmos 19 campos (`sd_logger.c`), gravados de 15 em 15 m em lotes de 5 | fiel | `test_sd_logger`; não testado com cartão |
| Configurações | FRAM 0x50, versão 0x0002 | NVS no nRF52840 e ZMS no nRF54LM20 (`user_settings.c`) | diferente | lidas e validadas no boot; FTP e peso gravados pelos comandos da interface |
| Recuperação de falha (FDIR) | `.noinit` + CRC-8, restauração por data ao achar a referência do nível do mar, uma vez, com notificação | `crash_recovery.c` e `attitude.c` | fiel | corrigidos em 2026-09-19 o CRC (cobria o próprio campo), o `has_data` (exigia falha registrada) e o momento da restauração (era no `attitude_init`, antes de saber a data); `test_crash_recovery` |
| BLE | só central (NUS→stravaAP, LNS, CPS, Komoot) | periférico + central (HRS, CSC, FTMS) | quebrado | scan nunca iniciado; `bt_gatt_subscribe` com `ccc_handle=0` faria `memset(NULL)` ([07](07-radio-ant-ble.md)) |
| ANT+ | HRM, BSC, FE-C, busca em background | pilha do `sdk-ant` com `ANT=1` (`src/rf/ant/ant.c`), sem perfis | parcial | compila no NCS v3.3.0; mapa de integração em [07](07-radio-ant-ble.md#ant-no-ncs-v330) |
| Interface | retrato, `Org_01`, cadrans, menu, notificações | LVGL em retrato, 29 telas (`src/ui`) testadas no PC; no firmware, a thread `ui` com o retrato do modelo, o driver `memlcd` (`modules/gnss_drivers`), as teclas por `zephyr,input-longpress` e a luz | parcial | falta projetar o mapa e os segmentos na tela (`nseg` e `route.n` vão em 0) e ver tudo num painel; a interface em paisagem (`src/vue`) saiu em 2026-09-19 ([18](18-interface-telas.md#diferenças-para-o-legacy)) |
| Comandos (`$LOC`, `$DWN`, `$QRY`) e USB | VParser via USB CDC e NUS; MSC | nada; os arquivos da pilha USB antiga saíram | ausente | a USB `device_next` é o passo da USB |

## Correções de 2026-09-18

Todas compiladas no NCS v3.3.0; as marcadas com teste têm teste de host que falha quando a correção é revertida (mutação conferida).

| # | Defeito | Correção | Verificação |
|---|---|---|---|
| 1 | Pilha da `main_loop` estourava: `measurement_update()` tem quadro de 1.672 B e a cadeia do Kalman soma ~2.200 B, acima dos 2.048 B | `MAIN_STACK_SIZE` 4096 e `DISPLAY_STACK_SIZE` 2048 (`src/main.c`) | `CONFIG_STACK_USAGE`: cadeias medidas em [05](05-arquitetura-zephyr.md#pilhas) |
| 2 | Todo o GPS e o modelo rodavam na ISR da UARTE1, por sentença, com `k_mutex_lock(K_FOREVER)` e escrita em arquivo no caminho | a ISR só enfileira bytes num `ring_buf`; `hal_uart_process()` monta as linhas na `main_loop` (`src/hal/hal_uart.c`, `gps_mgmt_process()`) | build; `test_gps_mgmt` |
| 3 | Callback de fix disparava 5 a 7 vezes por segundo com o mesmo ponto | um callback por época, no RMC válido; RMC `V` encerra o fix na hora (`src/drivers/gps/gps_mgmt.c`) | `test_gps_mgmt` (mutação: 5 callbacks por época) |
| 4 | Sentenças NMEA sem validação de checksum no caminho usado | `nmea_verify_checksum()` antes do parse | `test_gps_mgmt` |
| 5 | Caminho caractere a caractere do parser nunca reconhecia sentença (sem `$`), milissegundos errados (`.200` virava 2000 ms), coordenada convertida em `float` antes da divisão, `position_valid` nunca voltava a falso | `identify_sentence()` aceita com e sem `$`; fração de segundo; conversão em `double`; posição decidida por GGA/RMC (`src/drivers/gps/nmea_parser.c`) | `test_nmea_parser` (12 casos) |
| 6 | `sd_logger_add_entry()` escrevia além de `buffer[5]` quando o flush falhava (sempre, com o FS em stub): corrupção da `.bss` depois de ~90 m de atividade | tenta o flush de novo e descarta as entradas se o cartão continua indisponível (`src/model/sd_logger.c`) | `test_sd_logger` com canário (mutação conferida) |
| 7 | Botões invertidos duas vezes: em repouso pareciam pressionados e 1 s depois saía `LONG_CENTER`, que para e grava a atividade | leitura direta do nível lógico de `gpio_pin_get_dt()` (`src/hal/hal_gpio.c`) | build; revisão da semântica `GPIO_ACTIVE_LOW` |
| 8 | Reset e standby do GPS com polaridade invertida: o M10578 ficaria preso em reset e em standby | `GPIO_ACTIVE_LOW` em `gps_reset` e `gps_stdby` (overlay) | DTS gerado; `test_gps_mgmt` confere os níveis lógicos |
| 9 | Reset do FXOS8700 flutuando no boot (sem resistor na placa) e com semântica invertida | `reset-gpios` ativo alto no nó `fxos`; `imu_reset` ativo alto e inativo no HAL | DTS gerado |
| 10 | Nós do DK nos pinos da placa: QSPI (CSN = CS do LCD), `spi3` (NeoPixel, FIX, STDBY), `pwm0` (botão central), RTS/CTS do `uart0` (UART do GPS) | desligados no overlay; `uart0` só com TX/RX; `uart1` sem o pull-up herdado no TX | `.config` sem `NORDIC_QSPI_NOR`/`NRFX_QSPI`; FLASH −4,3 KB no build final |
| 11 | Erro fatal travava o aparelho | `CONFIG_RESET_ON_FATAL_ERROR=y` (loga e reinicia) | `.config` |
| 12 | O NCS v3.3.0 (Zephyr 4.3.99) trocou `CONFIG_SOC_SERIES_NRF52X` por `CONFIG_SOC_SERIES_NRF52`: a causa do reset era sempre "desconhecida" | `CONFIG_SOC_SERIES_NRF52` (`src/model/crash_recovery.c`) | build |
| 13 | Aparência BLE 1157 (sensor de velocidade e cadência) | 1153, Cycling Computer | `.config` |

Também nesta revisão: build com sysbuild e sem Partition Manager (`zephyr_app/sysbuild.conf`), scripts novos (`tools/fw/`), testes de host (`zephyr_app/tests/host/`) e validação da documentação (`tools/docs/`).

Os arquivos das correções 1 a 5, 7 e 9 saíram na migração de 2026-09-19 (o GPS passou para a API de GNSS do Zephyr, os botões para o subsistema de entrada, as threads para os serviços), junto com os testes `test_gps_mgmt` e `test_nmea_parser`; as correções 6 e 10 a 13 continuam no código.

## Migração de 2026-09-19

A base da arquitetura de [16](16-arquitetura-firmware.md), descrita em [05](05-arquitetura-zephyr.md).

| Item | O que mudou | Verificação |
|---|---|---|
| Serviços | sete threads (`sensors`, `model`, `gnss`, `radio`, `ui`, `storage`, `power`), cada uma dormindo na sua caixa de entrada, uma `k_msgq` enchida por listeners do zbus; nenhuma alocação depois do boot | build nos dois DKs, com e sem ANT, sem aviso |
| Eventos | 20 canais do zbus com as mensagens de `include/app/app_events.h` | build |
| Máquina de sistema | Partida, Ligado, MSC, Desligando (espera cada serviço por até 5 s) e Desligado, no SMF | `test_sys_fsm` (16 casos, com o `smf.c` do Zephyr); mutação: 6 de 6 mortas |
| Máquina de modos | CRS, PRC, FEC, Zwift, DBG, com as entradas e saídas de `boucle__change_mode()` | build |
| Desligamento automático | as regras do legacy na máquina de sistema; o `power_scheduler` só conta o tempo | `test_power_scheduler` (7 casos); mutação morta |
| Sensores | API de sensores do Zephyr por aliases; inclinação, rumo e rugosidade em `tilt.c`, no ritmo do legacy | `test_tilt` (8 casos, contra leituras construídas e o laço do legacy); mutação: 3 de 3 mortas |
| GNSS | API de GNSS do Zephyr com driver próprio do u-blox M10 (UBX, LEAP, standby, reinício por silêncio) e a máquina de energia em `gnss_power.c` | `test_ubx_m10`, `test_gnss_power`; build |
| Armazenamento | FatFs de verdade (saíram os stubs), log pelo `sd_logger`, carga de segmentos e lista de percursos | build; não testado com cartão |
| Zonas | potência com o rolo em FEC, score a cada 1 s com a FC do momento, como o legacy (antes: por posição e só com valor acima de zero) | build; leitura do legacy |
| Pilhas | medidas com `CONFIG_STACK_USAGE`, todas com pelo menos 1 KB de folga ([05](05-arquitetura-zephyr.md#pilhas)) | `.su` do GCC |
| Removidos | HAL próprio, drivers da V3 (LCD, BME280, FXOS8700, STC3100, GPS MediaTek, EPO, NeoPixel), `src/vue`, `src/usb`, stubs do sistema de arquivos, `boucle`, `model_lock`, `zwift` (protocolo próprio) e `baro_drift` (duplicado) | build |

## Defeitos abertos

Ordenados por gravidade. Linhas conferidas em 2026-09-18. Saíram com o código em 2026-09-19: o retrato errado do `ls027.c`, o EPO do `gps_epo.c`, o menu ilegível do `vue.c`, o score não inicializado do `vue_fec.c`, o campo sem uso do `hal_gpio.c` e o shunt do STC3100; as zonas que só recebiam amostra acima de zero foram corrigidas na migração.

| Gravidade | Onde | Defeito |
|---|---|---|
| crítico | `src/rf/ble_hrs_client.c:160`, `ble_bsc_client.c:268`, `ble_fec_client.c:270` | `bt_gatt_subscribe` com `ccc_handle=0` e `CONFIG_BT_GATT_AUTO_DISCOVER_CCC=y` sem `disc_params`: `memset(NULL)` no Zephyr 4.3 assim que o scan for ligado |
| crítico | `src/rf/ble/ble_manager.c:218` | `bt_conn_le_create` sem `bt_conn_unref`: o pool de 4 conexões esgota |
| alto | `src/model/udmatrix.c:64-67, 264-281` | `udmat_ones` gera identidade; `bound` com sinal zera covariâncias negativas: α0 nunca é estimado |
| alto | `src/model/crash_recovery.c:104-118` | CRC inclui o próprio campo `crc`; a restauração falha em 255 de 256 casos |
| alto | `src/rf/ble_fec_client.c:46-48, 166-168` | flags do FTMS erradas (cadência, tempo, energia): a potência sai do offset errado |
| alto | `src/rf/ble_bsc_client.c:136-137` | velocidade CSC 3600 vezes menor |
| alto | `src/model/parcours.c:216-218, 375-377` | loader para no CRLF; `OFF_ROUTE` não tem saída |

## Roteiro

Proposta de ordem; cada fase fecha com build, testes de host e, a partir da fase 1, teste na placa.

```mermaid
flowchart TD
    F0["0 · estabilização<br/>feito em 2026-09-18"]:::done --> F1
    F1["1 · base de execução<br/>feito: serviços, zbus, SMF de sistema e de modo,<br/>watchdog por serviço, auto-off do legacy<br/>falta: board própria (nRF54LM20A, esquemático próprio)"]:::partial --> F2
    F2["2 · fidelidade dos algoritmos<br/>Kalman (ones, bound, taxa), potência, distância,<br/>zonas, FDIR, testes diferenciais contra o legacy"]:::pending --> F3
    F3["3 · armazenamento<br/>SD e FAT montados, formatos do legacy,<br/>log @DDMMYY, loader e allocator de segmentos, liste_points"]:::pending --> F4
    F4["4 · rádio<br/>ANT+ pelo sdk-ant (HRM, BSC, FE-C) e BLE central,<br/>sensores no modelo, pareamento"]:::pending --> F5
    F5["5 · interface<br/>feito: telas LVGL testadas no PC, driver da tela,<br/>thread, teclas e luz no firmware<br/>falta: mapa e segmentos na tela, teste em painel"]:::partial --> F6
    F6["6 · comandos e USB<br/>VParser $LOC/$DWN/$QRY, USB device_next CDC e MSC,<br/>stravaAP e tools/zpm"]:::pending --> F7
    F7["7 · extras<br/>Komoot, LNS, EPO e host aiding, WS2812, FRAM"]:::pending
    classDef done fill:#2e7d32,color:#ffffff
    classDef partial fill:#f9a825,color:#000000
    classDef pending fill:#ef6c00,color:#ffffff
```

Tamanhos estimados pelos relatórios de análise: fase 1 M, fase 2 M, fase 3 G, fase 4 G (com ANT+) ou M (só BLE), fase 5 G, fase 6 M a G, fase 7 M a G.

### Andamento da fase 1

| Item | Estado | Onde |
|---|---|---|
| Thread de modelo única | feito em 2026-09-18 e refeito em 2026-09-19: a thread `model` é a única escritora do modelo e recebe tudo pela caixa de entrada | `src/svc/model/model_svc.c` |
| Trava do modelo | substituída em 2026-09-19: a tela recebe uma cópia do modelo pelo `chan_model_state` | `src/svc/model/model_ui.c` |
| Mensagens dos clientes BLE para o modelo | feito em 2026-09-19: os callbacks publicam `ext_sensor` e `link_status` | `src/svc/radio/radio_svc.c` |
| Watchdog | feito em 2026-09-18 e refeito em 2026-09-19: um canal de 4 s por thread de serviço, sete no total; não testado na placa | `src/app/app_svc.c`, `prj.conf` |
| Auto-off e desligamento | feito em 2026-09-18 pelo STC3100 e refeito em 2026-09-19 na máquina de sistema: 15 min sem posição em CRS, PRC e DBG, ou sem dado do rolo em FEC; desligamento em etapas; ship mode do nPM1300 ou System OFF (o latch do STC3100 saiu com a V3); não testado na placa | `src/svc/power/sys_fsm.c`, `test_sys_fsm` |
| Board própria | MCU escolhido em 2026-09-18 (nRF54LM20A) e esquemático próprio em projeto; o firmware compila para o nRF54LM20 DK com os periféricos da placa nova nos pinos de referência do docs/14 (GNSS, BMP585, BMI270, MMC5633NJL, OPT3001, cartão); não testado em placa | `boards/nrf54lm20dk_nrf54lm20a_cpuapp.overlay` e `.conf` |

### Andamento da fase 5

| Item | Estado | Onde |
|---|---|---|
| Telas | feito em 2026-09-19, no PC: 29 telas em LVGL nos temas de 8 cores e preto e branco, com os arranjos, os campos e os formatos do legacy e as diferenças registradas em [18](18-interface-telas.md#diferenças-para-o-legacy); o renderizador de host confere cores, textos e navegação | `src/ui/`, `include/ui/`, `tests/ui/`, `tools/ui/` |
| Formatação dos números | feito em 2026-09-19: `_fmkstr`, `_secjmkstr` e os limites do `cadran`, com duas diferenças de propósito ([06](06-algoritmos.md#formatação-dos-números)) | `src/ui/ui_fmt.c`, `test_ui_fmt` |
| Driver da tela | feito em 2026-09-19, no build: JDI LPM027M128B e C em 3 bits e Sharp LS027B7DH01 em 1 bit, retrato, a quantização do renderizador, só as linhas que mudaram, COM pelo EXTCOMIN ou pelo SPI, sequência de partida e de desligamento das fichas ([05](05-arquitetura-zephyr.md#tela)); `test_memlcd` (19 casos), mutação 14 de 14 mortas; não testado em painel | `modules/gnss_drivers/` |
| Thread da tela, teclas e ligação ao modelo | feito em 2026-09-19, no build: thread `ui` com o LVGL (6 KB de pilha, ~4,7 KB medidos), o retrato do modelo, notificações, telas de USB e de desligamento com o progresso dos serviços, teclas com toque longo, tema e luz guardados em `ui/prefs`; não testado na placa | `src/svc/ui/` |
| Luz da tela | feito em 2026-09-19: a máquina de [16](16-arquitetura-firmware.md#luz-do-display), com PWM pelo alias `backlight` e o COM a 120 Hz no JDI com a luz acesa; `test_backlight` (11 casos), mutação 8 de 8 mortas; limites a acertar na bancada | `src/svc/ui/backlight.c` |
| Mapa e segmentos na tela | a fazer: projetar o percurso e os segmentos no retrato (`afficheSegment`, `Zoom.cpp`); hoje `nseg` e `route.n` vão em 0 | `src/svc/model/model_ui.c` |

## Decisões do dono

Tomadas em 2026-09-18:

| Decisão | Escolha | Consequência |
|---|---|---|
| Rádio | **ANT+ e BLE juntos**: os sensores e equipamentos externos falam ANT+ | ANT+ pelo add-on **ANT for nRF Connect SDK** (`sdk-ant`); a v2.1.1 foi feita para o **sdk-nrf v3.2.4**, e o port a usa no NCS v3.3.0 com o módulo `ant_ncs33_compat` (ver a linha seguinte); exige aceitar o ANT+ Adopter Agreement e usar a chave de avaliação (`CONFIG_ANT_EVALUATION_KEY`) até haver licença comercial (ver [07](07-radio-ant-ble.md#decisão-ant-e-ble)) |
| ANT no NCS v3.3.0 | **obrigatório**: o `sdk-ant` v2.1.1 roda sobre o NCS v3.3.0, não sobre o v3.2.4 | add-on clonado em `C:\ncs\sdk-ant` e usado como módulo extra do Zephyr com `ANT=1`; compila nos dois alvos; não testado em placa ([07](07-radio-ant-ble.md#ant-no-ncs-v330)) |
| Placa | **board própria com o nRF54LM20A**, no lugar do DK com overlay, e **esquemático próprio**: GNSS, bateria e display melhores, painel solar pequeno na caixa e o que mais fizer sentido | a V3 existe só como esquema, não há placa física; o nRF54LM20 DK vem com o nRF54LM20B (a mesma peça com NPU) e o NCS v3.3.0 e o `sdk-ant` v2.1.x suportam os dois (ver [02](02-hardware.md#próxima-placa)) |
| Tela | **retangular, no formato do legacy**: 2,7", 400 × 240, em retrato | a interface nova já é retrato ([18](18-interface-telas.md)) e substitui a do `src/vue`, em paisagem; o display novo mantém o tamanho |
| CI | **desligado**: `.github/workflows/ci.yml` só roda à mão | não gasta minutos do GitHub Actions; ligar só com pedido do dono |
| Commits | um commit por item pronto e verificado, na `develop`; Conventional Commits em inglês, nunca atribuídos a IA | regra permanente deste projeto (skill `commit-gnss`); a `main` só recebe merge da `develop` quando o dono pedir |

Tomada em 2026-09-19:

| Decisão | Escolha | Consequência |
|---|---|---|
| Display | **JDI LPM027M128B**, achado no AliExpress; a Sharp LS027B7DH01A fica de reserva; peças do AliExpress têm preferência | o nRF54LM20 DK usa o LPM027M128B (`jdi,lpm027m128b`) e o tema de 8 cores; o B é refletivo e sem luz própria; a [lista de compras](19-lista-de-compras.md) não muda |

Ainda em aberto:

| Decisão | Opções | Consequência |
|---|---|---|
| Componentes da placa nova | proposta em [13](13-placa-nova.md), especificação em [14](14-hardware-placa-nova.md), avaliação em [15](15-avaliacao-componentes.md) e lista de compras validada em [19](19-lista-de-compras.md): nRF54LM20A no módulo Fanstel BM20C, display Sharp LS027B7DH01A com luz frontal (o JDI LPM027M128C de 8 cores no mesmo conector, sem canal autorizado de compra; a tela foi decidida em 2026-09-19: o JDI LPM027M128B, acima), GNSS u-blox MAX-M10N-10B (o MAX-F10S no mesmo footprint) com antena linear na borda de cima, nPM1300 com MAX17262 e carregador solar AEM10900, BMP585, BMI270, MMC5633NJL e OPT3001 | o dono aprova ou troca cada item; amostras e placas de avaliação antes do esquemático |
| Hardware de teste | nRF54LM20 DK para desenvolver até a placa própria existir, e as placas de avaliação da [proposta](13-placa-nova.md#próximos-passos) | sem placa, nada roda de verdade: hoje só há build e testes de host |
| Formatos no SD | compatíveis com o legacy (segmentos em texto com nome base36, `.PAR`, `@DDMMYY.txt`) ou formatos novos com conversor | há 138 segmentos e 2 percursos de exemplo em `tools/TDD/DB` no formato do legacy |
| Licença do projeto | o legacy é CC BY-NC 4.0; o port deriva dele | afeta uso comercial e a escolha da licença do repositório |
