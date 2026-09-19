# Arquitetura do port Zephyr

Como o `zephyr_app/` está organizado desde 2026-09-19: um serviço por assunto, cada um com a sua thread, eventos no zbus, máquinas de estado no SMF e o hardware pelas APIs do Zephyr, achado por aliases do devicetree. É a base da arquitetura planejada em [16-arquitetura-firmware.md](16-arquitetura-firmware.md); o que cada serviço já faz, e o que falta, está em [10-status-do-port.md](10-status-do-port.md).

> [!IMPORTANT]
> Build verificado nos dois alvos (nRF52840 DK e nRF54LM20 DK, com e sem ANT) e testes de host; **nada rodou em placa**.

**Nesta página:** [Camadas](#camadas) · [Boot](#boot) · [Threads](#threads) · [Eventos](#eventos) · [Máquinas de estado](#máquinas-de-estado) · [Watchdog](#watchdog) · [Fluxo de dados](#fluxo-de-dados) · [Pilhas](#pilhas) · [Módulos](#módulos) · [Devicetree e alvos](#devicetree-e-alvos) · [Configuração](#configuração) · [Regras de concorrência](#regras-de-concorrência)

## Camadas

```mermaid
flowchart TB
    subgraph APP["src/app"]
        MAIN["main.c<br/>boot"]
        CHAN["app_channels.c<br/>canais do zbus"]
        SVCF["app_svc.c<br/>watchdog e caixas de entrada"]
    end
    subgraph SVC["src/svc: um serviço, uma thread"]
        PWR["power<br/>máquina de sistema"]
        GNS["gnss"]
        SNS["sensors<br/>tilt"]
        RAD["radio"]
        STO["storage"]
        MOD["model<br/>máquina de modos"]
        UIS["ui"]
    end
    subgraph MODEL["src/model: algoritmos do legacy"]
        ALG["attitude, Kalman, locator, segmentos,<br/>percurso, zonas, log, FDIR, configurações"]
    end
    subgraph RF["src/rf"]
        BLE["ble_manager e clientes"]
        ANTN["ant (só com ANT=1)"]
    end
    subgraph Z["Zephyr e NCS v3.3.0"]
        ZAPI["zbus, SMF, task_wdt, GNSS, sensores, FatFs,<br/>settings, BT host e SoftDevice Controller"]
    end
    MAIN --> SVC
    SVC --> CHAN
    MOD --> MODEL
    STO --> MODEL
    RAD --> RF
    SVC --> ZAPI
    RF --> ZAPI
```

- Um serviço nunca chama outro: publica num canal, e quem precisa escuta. As chamadas só descem, para o modelo e para o Zephyr.
- Os nomes em francês vêm do legacy (`parcours` = percurso, `liste_points`, `vecteur`); o glossário está em [04-arquitetura-legacy.md](04-arquitetura-legacy.md#glossário).

## Boot

```mermaid
sequenceDiagram
    participant M as main()
    participant S as serviços
    participant P as energia
    M->>M: alimenta o WDT que sobreviveu a um reset por software
    M->>M: crash_recovery_init (causa do reset, FDIR)
    M->>M: user_settings_init e enforce (NVS ou ZMS)
    M->>M: task_wdt sobre o watchdog0
    M->>M: model_svc_init (attitude, segmentos, percurso, zonas)
    M->>S: storage, power, sensors, gnss, radio, model, ui
    M->>P: power_svc_ready
    P->>S: system_state Ligado
    M-->>M: retorna (a thread main termina)
```

- Os drivers do Zephyr (sensores, receptor GNSS, cartão) iniciam antes do `main()`; um que não responde fica fora, e o serviço dele segue sem ele.
- O `model_svc_init()` roda antes das threads porque o `segment_init()` zera os segmentos: se rodasse depois, apagaria o que o armazenamento acabou de carregar.
- O armazenamento monta o cartão, carrega os segmentos e publica o que achou; o modelo só usa os segmentos depois dessa mensagem.

## Threads

Prioridade no Zephyr: número menor ganha. Pilhas medidas (ver [Pilhas](#pilhas)).

| Thread | Arquivo | Prioridade | Pilha | Acorda com | Faz |
|---|---|---|---|---|---|
| `sensors` | `src/svc/sensors/sensors_svc.c` | 4 | 2048 B | a cada 20 ms | acelerômetro a 50 Hz com média de 1 s, barômetro a 10 Hz, magnetômetro e luz a 1 Hz, como o legacy (`fxos.cpp:600`, `Attitude.cpp`) |
| `model` | `src/svc/model/model_svc.c` | 5 | 4096 B | caixa de entrada; ao menos 1 vez por segundo | o único escritor do modelo: `attitude` e o Kalman, segmentos, zonas, percurso, máquina de modos; publica o retrato da tela e o ponto do log |
| `gnss` | `src/svc/gnss/gnss_svc.c` | 6 | 2048 B | caixa de entrada | segue o modo (dorme em FEC e Zwift) e o desligamento |
| `radio` | `src/svc/radio/radio_svc.c` | 6 | 3072 B | caixa de entrada | sobe o ANT (com `ANT=1`) e o BLE, liga os clientes aos canais, atualiza o nível da bateria no BLE |
| `ui` | `src/svc/ui/ui_svc.c` | 7 | 2048 B | caixa de entrada | guarda o último retrato; o LVGL e o driver da tela chegam no passo da interface |
| `storage` | `src/svc/storage/storage_svc.c` | 8 | 3584 B | caixa de entrada | monta o cartão, carrega segmentos, lista percursos, grava o log de atividade |
| `power` | `src/svc/power/power_svc.c` | 9 | 2048 B | caixa de entrada; tique de 1 s | máquina de sistema, desligamento automático, ship mode ou System OFF, `power_status` |
| workqueue do modem | Zephyr (`CONFIG_MODEM_DEDICATED_WORKQUEUE`) | do sistema | 2048 B | bytes do receptor | o driver GNSS interpreta e chama os callbacks do serviço, que publicam `gnss_fix` e `gnss_sky` |
| RX do BT | Zephyr | cooperativa | 3072 B | pacotes do rádio | os clientes BLE chamam os callbacks do serviço de rádio, que publicam `ext_sensor` e `link_status` |

Ficam por conta do Zephyr e do NCS: o log, o TX do BT, o MPSL, o workqueue do sistema e o idle.

## Eventos

Cada serviço tem uma **caixa de entrada**, uma `k_msgq` estática que um listener do zbus enche com cópias das mensagens que o serviço usa (`ZBUS_CHAN_ADD_OBS()` no arquivo do serviço). A thread dorme na caixa com um tempo máximo de 1 s, que também dá o trabalho periódico e a alimentação do watchdog (`app_inbox_get()`). Assim nenhum evento se perde entre dois processamentos, nada é alocado depois do boot, e cada thread tem um ponto só de espera. O retrato da tela e as listas grandes (satélites, pareamento, cartão) não vão na caixa: o listener só avisa, e a thread lê o canal. Com a caixa cheia, a mensagem é descartada e contada no log.

| Canal | Mensagem | Publica | Escutam |
|---|---|---|---|
| `chan_gnss_fix` | posição, velocidade, rumo, altitude, hora UTC, satélites, modo do receptor, uma por época | callback do GNSS | modelo, energia |
| `chan_gnss_sky` | satélites em vista | callback do GNSS | modelo |
| `chan_baro` | pressão e temperatura, 10 Hz | sensores | modelo |
| `chan_imu` | inclinação, rolagem e rugosidade, 1 Hz | sensores | modelo |
| `chan_mag` | rumo magnético compensado, 1 Hz | sensores | modelo |
| `chan_ambient` | luz ambiente, 1 Hz | sensores | interface |
| `chan_ext_sensor` | um dado de sensor externo | rádio | modelo, energia |
| `chan_link_status` | sensor conectado, perdido, procurando ou sem par | rádio | modelo |
| `chan_pair_list` | dispositivos achados no pareamento | rádio | modelo |
| `chan_phone_nav` | próxima curva do celular | rádio | modelo |
| `chan_power_status` | bateria e carga, a cada 60 s e quando muda | energia | modelo, rádio |
| `chan_system_cmd` | os pedidos da interface (modo, desligar, parear, FTP, zoom, formatar, MSC) | interface e serviços | cada serviço filtra os seus |
| `chan_system_state` | Partida, Ligado, MSC, Desligando, Desligado | energia | todos |
| `chan_mode` | modo em vigor | modelo | energia, GNSS |
| `chan_shutdown_ack` | um serviço terminou a sua parte do desligamento | serviços | energia |
| `chan_notif` | notificação da tela | qualquer um | interface |
| `chan_log_point` | um ponto do log por época | modelo | armazenamento |
| `chan_input` | tecla e tipo de toque | teclado | interface |
| `chan_storage_info` | cartão montado, segmentos carregados, percursos | armazenamento | modelo |
| `chan_model_state` | o retrato da tela (`ui_model_t`, [18](18-interface-telas.md#implementação)) | modelo | interface |

As mensagens estão em `zephyr_app/include/app/app_events.h`, em C sem tipos do Zephyr.

## Máquinas de estado

| Máquina | Arquivo | Estados | Teste |
|---|---|---|---|
| Sistema e energia | `src/svc/power/sys_fsm.c` | Partida, Ligado e MSC sob um pai comum que aceita o desligamento; Desligando espera a resposta de cada serviço por até 5 s; Desligado corta a energia (ship mode do nPM1300 sem VBUS, System OFF com VBUS ou sem PMIC) | `test_sys_fsm` (16 casos), com o `lib/smf/smf.c` do Zephyr |
| Modo | `src/svc/model/model_svc.c` | CRS, PRC, FEC, Zwift, DBG, com as entradas e saídas de `boucle__change_mode()` (`legacy/source/model/Boucle.cpp:101-143`): PRC inicia o percurso carregado e o para ao sair, FEC zera as zonas e o score | pelo build; sem teste de host |

O desligamento automático é o do legacy (`legacy/source/scheduling/power_scheduler.cpp`): cada posição com fix em CRS, PRC e DBG, e cada dado do rolo em FEC, reinicia a contagem de 15 min (`src/model/power_scheduler.c`, `test_power_scheduler`). Diagramas das máquinas em [16](16-arquitetura-firmware.md#máquinas-de-estado).

## Watchdog

O `task_wdt` do Zephyr dá a cada thread de serviço o seu canal de 4 s, o tempo do WDT do legacy (que tinha um canal só, alimentado pelo `boucle` e pelo LCD). São 7 canais (`CONFIG_TASK_WDT_CHANNELS=8`); o WDT do nRF fica por trás e pega um travamento em que nem o timer do kernel roda.

- Cada thread alimenta o seu canal antes de cada espera na caixa, e a espera dura no máximo 1 s: um serviço preso num evento por mais de 4 s reinicia o aparelho, com o nome dele no log (`LOG_PANIC` no expirar).
- O armazenamento carrega os segmentos antes de criar o seu canal, porque a carga pode passar de 4 s com muitos arquivos no cartão.
- `CONFIG_TASK_WDT_MIN_TIMEOUT=4000`: o timer do kernel não acorda a CPU a cada 100 ms só para alimentar o WDT.
- O WDT do nRF52 continua contando depois de um `sys_reboot()`: o `main()` o alimenta (`app_wdt_feed_if_running()`) até os canais existirem. O WDT é o `watchdog0` do devicetree (`wdt0` no nRF52840, `wdt31` no nRF54LM20).
- O timer do kernel não pausa num breakpoint: para depurar passo a passo, compile com `-DCONFIG_TASK_WDT=n`.
- Não testado na placa.

## Fluxo de dados

```mermaid
flowchart LR
    RX["receptor GNSS"] -->|"API GNSS<br/>workqueue do modem"| GS["callbacks do gnss_svc"]
    SEN["barômetro, IMU,<br/>magnetômetro, luz"] -->|"API de sensores"| SS["sensors_svc<br/>tilt: 50 amostras"]
    BLEC["clientes BLE e ANT"] --> RS["radio_svc"]
    GS -->|"gnss_fix, gnss_sky"| MOD["model_svc<br/>attitude, Kalman, segmentos,<br/>zonas, percurso"]
    SS -->|"baro, imu, mag"| MOD
    RS -->|"ext_sensor, link_status"| MOD
    MOD -->|"model_state"| UI["ui_svc"]
    MOD -->|"log_point"| ST["storage_svc<br/>sd_logger"]
    ST --> FS[("FatFs no cartão")]
    ST -->|"storage_info"| MOD
    GS -->|"gnss_fix"| PW["power_svc<br/>máquina de sistema"]
    RS -->|"ext_sensor"| PW
    PW -->|"system_state"| MOD
    PW -->|"power_status"| MOD
```

## Pilhas

Medidas com `CONFIG_STACK_USAGE=y` (arquivos `.su` do GCC) em 2026-09-19, no build do nRF54LM20 DK. A soma segue a cadeia de chamadas mais funda e acrescenta o quadro de exceção com FPU (~100 B) e uma chamada de log (~250 B).

| Thread | Cadeia mais funda | Total | Pilha | Folga |
|---|---|---|---|---|
| `model` | `model_thread` 344 + `attitude_update_baro` 104 + **`measurement_update` 1672** + `udmat_invert` 352 | ~2,8 KB | 4096 B | ~1,3 KB |
| `storage` | `storage_thread` 416 + `segment_load_all` 656 + FatFs (`f_open` 96, `follow_path` 56, `dir_find` 64) + SD por SPI (`sdhc_spi_request` 136, `sdhc_spi_send_cmd` 56, `spi_nrfx_transceive` 128) | ~2,2 KB | 3584 B | ~1,4 KB |
| `radio` | `bt_enable` 24 + `bt_init` 168 + `bt_hci_cmd_send_sync` 96 + `settings_zms_load` 144 e o ZMS | ~1,5 KB | 3072 B | ~1,5 KB |
| `sensors` | `sensors_thread` 168 + leitura I2C (`i2c_nrfx_twim_msg_transfer` 56) + publicação no zbus (`zbus_chan_pub` 56, `_zbus_vded_exec` 104, listener até 80, `k_msgq_put`) | ~0,9 KB | 2048 B | ~1,1 KB |
| `power` | estado novo pelo SMF (`smf_set_state` 64) + publicação no zbus (~0,4 KB) + log; ou o `LOG_PANIC` do desligamento (`z_cbvprintf_impl` 160) | ~0,85 KB | 2048 B | ~1,2 KB |
| `gnss` | publicação no zbus e log | ~0,7 KB | 2048 B | ~1,3 KB |
| `ui` | leitura do canal e log | ~0,6 KB | 2048 B | ~1,4 KB |
| workqueue do modem | interpretação NMEA (`modem_chat_process_handler` 64, `gnss_nmea0183_parse_rmc`) + `gnss_data_cb` 72 + publicação | ~0,9 KB | 2048 B | ~1,1 KB |
| `main` | `settings_zms_save` 144 e o ZMS, ou `zms_mount` 232, com log | ~0,8 KB | 2048 B | ~1,2 KB |

- O workqueue do sistema, de 1024 B, ficaria com ~170 B de folga com os callbacks do GNSS: por isso o modem ganhou workqueue próprio.
- A thread RX do BT subiu de 2200 para 3072 B, pelos ~0,45 KB que a publicação dos clientes acrescenta.
- A tela com o LVGL muda a pilha da `ui` no passo da interface.
- Na placa, confirme com `CONFIG_THREAD_ANALYZER=y`.

```sh
source tools/fw/ncs_env.sh
cd zephyr_app
python -m west build -p always -b nrf54lm20dk/nrf54lm20a/cpuapp -d build_su --no-sysbuild . -- -DCONFIG_STACK_USAGE=y
find build_su -name "*.su" -exec cat {} + | sort -t$'\t' -k2 -rn | head -40
```

## Módulos

| Pasta | Arquivos | Papel |
|---|---|---|
| `src/app/` | `main.c`, `app_channels.c`, `app_svc.c` | boot, canais, watchdog e caixas de entrada |
| `src/svc/power/` | `power_svc.c`, `sys_fsm.c` | serviço de energia e máquina de sistema |
| `src/svc/gnss/` | `gnss_svc.c` | receptor pela API GNSS do Zephyr |
| `src/svc/sensors/` | `sensors_svc.c`, `tilt.c` | sensores pela API de sensores; inclinação, rumo e rugosidade |
| `src/svc/radio/` | `radio_svc.c` | ANT e BLE, clientes para eventos |
| `src/svc/storage/` | `storage_svc.c` | cartão, log, segmentos, percursos |
| `src/svc/model/` | `model_svc.c`, `model_ui.c` | a thread do modelo e o retrato da tela |
| `src/svc/ui/` | `ui_svc.c` | a thread da tela |
| `src/model/` | `attitude`, `kalman_altitude`, `udmatrix`, `kalman`, `locator`, `loc_source`, `segment`, `liste_points`, `vecteur`, `parcours`, `power_zone`, `suffer_score`, `rr_zone`, `sd_logger`, `crash_recovery`, `user_settings`, `power_scheduler` | os algoritmos do legacy |
| `src/rf/` | `ble/ble_manager.c`, `ble_nus.c`, `ble_lns.c`, `ble_*_client.c`, `ant/ant.c` | BLE (scan ainda não iniciado) e ANT (só com `ANT=1`) |
| `src/ui/`, `include/ui/` | interface LVGL da placa nova ([18](18-interface-telas.md)) | ainda só no renderizador de host (`tests/ui`) |

Saíram em 2026-09-19, substituídos pelas APIs do Zephyr ou pelos serviços: o HAL próprio (`src/hal`), os drivers da V3 (`ls027`, `baro`, `fxos`, `stc3100`, `gps_mgmt`, `nmea_parser`, `gps_epo`, `neopixel`), a interface em paisagem (`src/vue`), a USB da pilha antiga (`src/usb`), os stubs do sistema de arquivos (`src/utils`), o `boucle`, o `model_lock`, o `zwift` (protocolo próprio, não o do legacy) e o `baro_drift` (duplicado).

## Devicetree e alvos

A placa vem do `-b` (variável `BOARD` dos scripts); o Zephyr aplica `boards/<placa>.overlay` pelo nome. O código acha os dispositivos pelos aliases abaixo: uma placa nova só precisa defini-los no overlay dela. Um alias ausente deixa o serviço sem aquele dispositivo.

| Alias | Serviço | nRF54LM20 DK (periféricos da placa nova) | nRF52840 DK (pinos da V3) |
|---|---|---|---|
| `gnss` | GNSS | `gnss-nmea-generic` no `uart21` (TX P1.04, RX P1.05), até o driver UBX do M10 | `gnss-nmea-generic` no `uart1` (TX P0.05, RX P0.07), o M10578-A3 |
| `baro0` | sensores | BMP585 (`bosch,bmp581`) em 0x47 no `i2c23` (SDA P1.02, SCL P1.03) | BME280 em 0x76 no `i2c0` (SDA P1.00, SCL P1.01) |
| `imu0` | sensores | BMI270 em 0x68, INT1 em P3.04 | FXOS8700 em 0x1E |
| `mag0` | sensores | MMC5633NJL (`memsic,mmc56x3`) em 0x30 | FXOS8700 |
| `light0` | sensores | OPT3001 em 0x44 | — |
| `watchdog0` | watchdog | `wdt31`, ligado no overlay | `wdt0` |
| disco `SD` | armazenamento | `sdhc-spi-slot` no `spi00` (SCK P2.01, MOSI P2.02, MISO P2.04, CS P2.03); a flash MX25R64 do DK sai do devicetree | `sdhc-spi-slot` no `spi2` (MOSI P0.25, CS P0.26, SCK P0.27, MISO P0.28) |
| `zephyr,console` | log | `uart20`, VCOM0 do DK | `uart0` (TX P0.06, RX P0.08), só no DK |

- O nRF52840 DK não tem leitura de bateria: o STC3100 da V3 não tem driver no Zephyr. A V3 existe só como esquema.
- Os três botões da V3 estão como `gpio-keys` com códigos de tecla, para o subsistema de entrada do passo da interface; os nós falsos de `gpio-keys` que davam nomes a pinos do GPS, do IMU e do NeoPixel saíram, porque o subsistema de entrada os trataria como teclas.
- Nós do DK desligados no overlay da V3 porque ocupam pinos da placa: `qspi` e `mx25r64`, `spi3`, `pwm0`; o `uart0` perdeu RTS/CTS. Detalhes em [02-hardware.md](02-hardware.md).
- `boards/nrf54lm20dk_nrf54lm20a_cpuapp.conf` troca o NVS pelo ZMS: a NVM do nRF54L é RRAM, e a Nordic recomenda o ZMS nela.
- Build do nRF54LM20 DK: `BOARD=nrf54lm20dk/nrf54lm20a/cpuapp bash tools/fw/fw.sh build`.
- Nada disso rodou num DK: é build verificado, não teste em placa.

## Configuração

| Arquivo | O que define |
|---|---|
| `prj.conf` | zbus e SMF, `task_wdt` com 8 canais, `CONFIG_POWEROFF`, GNSS com satélites e o workqueue próprio do modem, sensores, FatFs com nomes longos em buffer estático, BLE central e periférico (4 conexões, RX do BT com 3072 B), settings em NVS, log por UART, `CONFIG_RESET_ON_FATAL_ERROR`, otimização de tamanho |
| `boards/*.conf` | o ZMS no nRF54LM20 |
| `sysbuild.conf` | `SB_CONFIG_PARTITION_MANAGER=n` |
| `CMakeLists.txt` | fontes por camada, `-Wall -Wextra` |

## Regras de concorrência

1. **ISR não processa**: só copia dados para um buffer e acorda quem processa. Nada de `k_mutex_lock`, arquivo, `snprintf` com float ou trigonometria em ISR. As interrupções dos barramentos e do receptor ficam nos drivers do Zephyr.
2. **Um dono por dado**: o modelo é escrito só pela thread `model`; as outras recebem cópias pelos canais. Serviço que precisa mudar o modelo publica um comando ou um dado.
3. **Callbacks** (BT, GNSS, entrada) rodam em threads do Zephyr: só convertem e publicam. Nada de chamar o modelo, esperar o rádio ou segurar mutex de lá.
4. **Listener do zbus** roda na thread de quem publica, com o canal travado: só copia para a caixa de entrada e retorna.
5. **Pilha**: toda thread nova ou cadeia pesada nova passa pela medição com `CONFIG_STACK_USAGE` e deixa pelo menos 1 KB de folga.

Procedimento completo na skill `fw-threads`.
