# Changelog

Mudanças relevantes do projeto. Formato inspirado no [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/); a versão do firmware é a de `APP_VERSION_*` em `zephyr_app/include/app_types.h`. Toda mudança entra em **Não lançado** no mesmo commit do código.

## [Não lançado]

Revisão completa de 2026-09-18: análise do legacy e do port, migração para o NCS v3.3.0, correções críticas, testes de host e documentação. Nada foi testado na placa nem no nRF52840-DK.

### Corrigido

- `BUILD_DIR` relativo no `fw.sh` e no `build.bat` era resolvido a partir do `zephyr_app`, onde o build roda: o `BUILD_DIR=zephyr_app/build_54` da documentação criava `zephyr_app/zephyr_app/build_54`. Agora ele vale a partir da pasta de quem chama; o binário não muda.
- Estouro de pilha na thread `main_loop`: a cadeia do Kalman de altitude usa ~2.200 B e a pilha tinha 2.048 B; agora 4.096 B (`src/main.c`). A thread `display` passou de 1.024 para 2.048 B.
- O GPS e todo o modelo rodavam dentro da ISR da UARTE1, uma vez por sentença NMEA: a ISR agora só enfileira bytes num `ring_buf` e `hal_uart_process()` monta as linhas na `main_loop` (`src/hal/hal_uart.c`, `src/drivers/gps/gps_mgmt.c`).
- O callback de fix disparava 5 a 7 vezes por segundo com o mesmo ponto: agora uma vez por época, no RMC válido, e um RMC `V` encerra o fix na hora.
- Linhas NMEA sem validação de checksum no caminho usado pelo `gps_mgmt`.
- Parser NMEA: o caminho caractere a caractere nunca reconhecia uma sentença; milissegundos lidos errado (`.200` virava 2000 ms); coordenadas perdiam precisão ao virar `float` antes da divisão; a posição válida nunca voltava a falso (`src/drivers/gps/nmea_parser.c`).
- `sd_logger_add_entry()` escrevia além do buffer quando o cartão não estava disponível, corrompendo a memória depois de ~90 m de atividade (`src/model/sd_logger.c`).
- Botões lidos com inversão dupla: em repouso pareciam pressionados e, 1 s depois do boot, um `LONG_CENTER` parava e gravava a atividade (`src/hal/hal_gpio.c`).
- Reset e standby do GPS com polaridade invertida no overlay (o módulo ficaria em reset e em standby).
- Reset do FXOS8700 flutuando durante a inicialização do driver e com semântica invertida: `reset-gpios` ativo alto no nó e `imu_reset` corrigido.
- Nós do nRF52840-DK nos pinos da placa: `qspi` e `mx25r64` (CS do LCD), `spi3` (NeoPixel, FIX e standby do GPS) e `pwm0` (botão central) desligados; `uart0` sem RTS/CTS (pinos do GPS); `uart1` sem o pull-up herdado no TX.
- Erro fatal travava o aparelho: `CONFIG_RESET_ON_FATAL_ERROR=y`.
- Causa do reset sempre "desconhecida" no NCS v3.3.0: o Zephyr 4.3.99 trocou `CONFIG_SOC_SERIES_NRF52X` por `CONFIG_SOC_SERIES_NRF52` (`src/model/crash_recovery.c`).
- Aparência BLE 1157 (sensor de velocidade e cadência) trocada por 1153 (Cycling Computer).
- Scripts `.bat` apontavam para o NCS v3.1.0 e o toolchain `b8b84efebd`, que não existem mais, para caminhos de uma pasta antiga, e definiam `TOOLCHAIN_ROOT`, que quebra o CMake do Zephyr; o `build.bat` rodava o `west` no drive `C:` com o projeto em `F:`.
- Corrida entre threads no modelo: a thread `sensor` repetia a cada 1 s as leituras I2C que o `boucle_process()` já faz e escrevia os caches dos drivers lidos pela `main_loop`, e a `display` lia o modelo sem trava. A `sensor` saiu, a `main_loop` é a única escritora do modelo e a `display` compõe o quadro sob `model_lock()`; o nível de bateria vai ao BLE só quando muda (`src/main.c`, `src/model/model_lock.c`, `src/vue/vue.c`). RAM −1.280 B.
- Nenhum watchdog (o legacy tinha um de 4 s): `task_wdt` com um canal de 4 s para a `main_loop` e outro para a `display`, sobre o WDT do nRF; ao expirar, loga a thread travada e reinicia. O boot alimenta o WDT quando ele sobrevive a um reset por software (`src/main.c`, `prj.conf`). FLASH +1.116 B.
- O aparelho nunca desligava (o legacy desliga pelo STC3100 depois de 15 min sem atividade e pelo menu): `power_scheduler` portado do legacy, com ping a cada posição processada em CRS/PRC, `stc3100_shutdown()` (`REG_MODE = 0`, `REG_CONTROL = 0x01`), a atividade salva zerada antes e o item "Power Off" no menu; na USB, nova tentativa 15 min depois (`src/model/power_scheduler.c`, `src/drivers/sensors/stc3100.c`, teste `test_power_scheduler` com 8 casos).

### Adicionado

- `docs/img/placa-nova-caixa.svg` e a seção "Como fica o aparelho" do `docs/13`: desenho em escala do aparelho proposto, a partir da caixa da V3 (62 × 104 × 19 mm, JDI colorido, 6 módulos solares na frente inclinada e nos chanfros laterais, antenas GNSS no topo, BM20C no canto oposto, USB-C na base), gerado por `tools/docs/case_drawing.py`. A área de painel do `docs/13` passou a ser a que cabe no desenho (11 cm² de módulos, cerca de 7 a 9 cm² equivalentes), no lugar dos 10 a 15 cm² estimados antes.
- Build com ANT (`ANT=1` no `fw.sh` e no `build.bat`): o add-on `sdk-ant` v2.1.1, clonado em `C:\ncs\sdk-ant`, roda sobre o NCS v3.3.0 como módulo extra do Zephyr, com `zephyr_app/modules/ant_ncs33_compat` (religa `SOC_SERIES_NRF52X` e `SOC_SERIES_NRF54LX`, obsoletos no NCS v3.3.0) e `zephyr_app/ant.conf`; `rf_ant_init()` (`src/rf/ant/ant.c`, port do `ant_stack_init()` do legacy) sobe a pilha e grava a chave ANT+ antes do BLE. Compila nos dois alvos (+28,6 KB de FLASH e +4,6 KB de RAM no nRF52840); o build sem `ANT` não muda; não testado em placa. `docs/07` ganhou o mapa do que o ANT e o ANT+ podem integrar ao port.
- Testes de host do port (`zephyr_app/tests/host/`, rodados por `tools/fw/host_tests.sh`): Unity 2.6.1 + CTest com o GCC do PC, shims do Zephyr, sistema de arquivos em memória e HAL falso do GPS; 6 conjuntos, 42 casos (`vecteur`, `power_zone`, `suffer_score`, `nmea_parser`, `sd_logger`, `gps_mgmt`), com oráculo do legacy e mutação conferida nas correções.
- `tools/fw/`: `ncs_env.sh` e `ncs_env.bat` (ambiente do NCS a partir do `environment.json` do toolchain) e `fw.sh` (build, flash, recover, devices, size).
- `tools/docs/`: `mermaid_check.py` (extrai e renderiza os diagramas com o mermaid-cli local e aponta diagramas em texto puro) e `links_check.py`.
- `zephyr_app/sysbuild.conf` com `SB_CONFIG_PARTITION_MANAGER=n`: build com sysbuild, sem o Partition Manager depreciado.
- `CONFIG_RING_BUFFER=y` e a função `hal_uart_process()`.
- Contexto para assistentes de IA: `CLAUDE.md`, `AGENTS.md` e dez skills em `.claude/skills/` (`fw-build`, `fw-testes`, `fw-threads`, `fw-port-legacy`, `fw-hardware`, `fw-gps-sensores`, `fw-radio`, `fw-vue`, `docs-gnss`, `commit-gnss`).
- Documentação numerada em `docs/` (01 a 12, com índice), `README.md` e `legacy/README.md` (origem, licença CC BY-NC 4.0 e diferenças em relação ao upstream).
- `.gitattributes` (LF no repositório, CRLF nos `.bat`, `hardware/` e os dados de teste de `tools/TDD/` byte a byte) e `.editorconfig`.
- CI em `.github/workflows/ci.yml`, **desligado** (só `workflow_dispatch`): testes de host, documentação e build do firmware no container `sdk-nrf-toolchain:v3.3.0`. O `mermaid_check.py` aceita `PUPPETEER_CONFIG` para o Chrome do runner.
- Decisões do dono registradas em `docs/10`, `docs/07`, `docs/02` e nas skills: ANT+ e BLE juntos, pelo add-on `sdk-ant`, e board própria com MCU da Nordic.
- Alvo **nRF54LM20 DK** (`nrf54lm20dk/nrf54lm20a/cpuapp`), para desenvolver a placa própria: overlay com os nomes da aplicação em pinos do conector de expansão, `wdt31` ligado, configurações no ZMS (a NVM é RRAM) e a família do `nrfutil` escolhida pela `BOARD` no `fw.sh`, no `flash.bat` e no `recover.bat`. Compila com os 7 avisos conhecidos; não testado em placa.
- `docs/13-placa-nova.md`: proposta de hardware da placa própria, com pesquisa de mercado e de datasheets de 2026-09-18: nRF54LM20A no módulo Fanstel BM20C, display JDI LPM027M128C (MIP de 8 cores do tamanho do LS027; e-paper colorido descartado por levar de 11 a 20 s por quadro), GNSS u-blox MAX-F10S (L1 + L5) ou MAX-M10N no mesmo footprint, com antena linear na borda de cima como Garmin, COROS e Wahoo fazem (fotos internas do FCC), nPM1300 com o medidor MAX17262 e o carregador solar AEM10900 com painel ANYSOLAR, BMP585, LSM6DSV16X, LIS2MDL e OPT3001, orçamentos de pinos e de energia, riscos e próximos passos. Nada comprado nem testado; consumos e autonomia estimados.
- Segunda rodada de decisões: nRF54LM20A com esquemático próprio (GNSS, bateria e display melhores, painel solar pequeno na caixa), tela no formato do legacy (2,7", em retrato) e um commit por item verificado como regra do projeto; os links dos acordos do ANT+ entraram em `docs/07`.

### Alterado

- `build.bat`, `build_ncs.bat`, `flash.bat`, `recover.bat` e `serial.bat` reescritos para o NCS v3.3.0: ambiente em `tools/fw/ncs_env.bat`, `west` rodando no drive do projeto, sysbuild, `-p auto`, gravação filtrada por J-Link (`--traits jlink` ou `NRF_SERIAL`), opção de preservar a partição de settings, porta serial por parâmetro.
- `.gitignore`: builds, caches do clangd, `__pycache__`, `node_modules` e `.claude/settings.local.json`.
- Imagens de `docs/` movidas para `docs/img/`; os quatro documentos de novembro de 2025 arquivados sem alteração em `docs/historico/2025-11/`.
- Tamanho: FLASH 294.796 B (−4,3 KB sem o driver QSPI do DK), RAM 118.080 B (+2,9 KB pelas pilhas maiores).
- A alimentação do WDT no boot, depois de um reset por software, usa o `watchdog0` do devicetree em vez do `NRF_WDT` fixo do nRF52: vale também para o `wdt31` do nRF54LM20; o binário do nRF52840 não mudou.
- O `crash_recovery` lê a causa do reset pela API `hwinfo` do Zephyr, que vale para o nRF52 (POWER) e o nRF54L (RESET), e decodifica o CFSR em qualquer núcleo ARMv7-M ou ARMv8-M Mainline (Cortex-M4 e M33), não só no M4. FLASH do nRF52840 +112 B.
- O código chega aos barramentos pelos aliases do devicetree (`gps-uart`, `sensor-i2c`, `lcd-spi`, `sdc-spi`), não pelas instâncias do nRF52 (`uart1`, `i2c0`, `spi1`, `spi2`): passo para compilar em outras placas; o binário do nRF52840 não mudou.
- O `CMakeLists.txt` não fixa mais a placa nem o overlay: a placa vem do `-b` (`BOARD` no `fw.sh` e no `build.bat`) e o overlay da V3 passou a `boards/nrf52840dk_nrf52840.overlay`, aplicado pelo nome; o overlay de teste de 2025-11 que ocupava esse nome e nunca entrava no build saiu. O binário do nRF52840 não mudou.
- `docs/07` e `docs/13`: a pilha ANT exige cristal de 32,768 kHz com no máximo ±50 ppm (os dois alvos do port já usam o cristal a 50 ppm; na placa nova, conferir o cristal do módulo), e a instalação do `sdk-ant` pela tag `v2.1.1` ou pelo índice de add-ons do VS Code.
- `docs/07` com o que o ANT+ Adopter Agreement (versão 20250103) e a página de downloads dizem de fato: nada dos perfis nem das ferramentas pode ser distribuído (regra nova no `CLAUDE.md`, porque o repositório é público), a marca ANT+ exige interoperabilidade testada por você, e os programas de membros e de certificação ANT+ terminaram em 2025-06-30.
- Repositório publicado no GitHub: `Guiimartinho/gnss-bike-computer`, público (o antigo `bike-computer-stravaV11`, renomeado), com `main` e `develop`; o histórico de 2025-11 do repositório antigo, sem ligação com o atual, ficou no ramo `archive/stravav11-2025-11`.
- Branches `main` (versões estáveis) e `develop` (trabalho) no lugar da `master`; regra registrada no `CLAUDE.md`, no `AGENTS.md` e na skill `commit-gnss`.
- Armadilhas de build registradas no `CLAUDE.md` e na skill `fw-build` (o build incremental guarda símbolos Kconfig antigos; caminhos de build longos demais) e menções aos builds antigos, já apagados, retiradas.

### Removido

- `include/rf/glasses.h`: stub sem uso que afirmava que o ANT+ não existia no Zephyr; o `include/rf/ant.h` deixou de ser stub.
- Template vazio de app da raiz (`CMakeLists.txt`, `prj.conf`, `src/main.c`), que vinha do commit inicial e não era o firmware.

## [2.0.0] - 2025-12-01

Port inicial para Zephyr, criado entre 2025-11-26 e 2025-11-28 e compilado pela última vez em 2025-12-01 com o NCS v3.1.0 (versão definida em `app_types.h`, sem tag).

### Adicionado

- `zephyr_app/` com HAL (GPIO, I2C, SPI, UART), drivers (LS027, BME280 e FXOS8700 sobre os drivers nativos, STC3100, GPS, parser NMEA, EPO, NeoPixel em stub), modelo (boucle, attitude, Kalman de 3 estados, locator, segmentos, listas de pontos, vetores, zonas de potência, suffer score, zonas RR, configurações em NVS, recuperação de falha, percurso, log no SD, Zwift), BLE (NUS, LNS, BAS, DIS e clientes HRS, CSC, FTMS e Komoot) e interface (9 páginas, menu, telas de rolo).
- Documentos de arquitetura, revisão MISRA, ambiente e gap analysis (25 e 26 de novembro de 2025), hoje em `docs/historico/`.
- Scripts `.bat` para o NCS v3.1.0.

## Legacy

O stravaV10 original (Vincent Gollé, 2015 a 2020, CC BY-NC 4.0) foi copiado para `legacy/`, `libraries/` e `tools/` em 2025-11-25 e o projeto da placa para `hardware/` em 2025-11-30. Essas pastas não são versionadas aqui como produto: são referência.
