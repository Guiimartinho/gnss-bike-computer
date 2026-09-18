# Changelog

Mudanças relevantes do projeto. Formato inspirado no [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/); a versão do firmware é a de `APP_VERSION_*` em `zephyr_app/include/app_types.h`. Toda mudança entra em **Não lançado** no mesmo commit do código.

## [Não lançado]

Revisão completa de 2026-09-18: análise do legacy e do port, migração para o NCS v3.3.0, correções críticas, testes de host e documentação. Nada foi testado na placa nem no nRF52840-DK.

### Corrigido

- Parser NMEA: o caminho caractere a caractere nunca reconhecia uma sentença; milissegundos lidos errado (`.200` virava 2000 ms); coordenadas perdiam precisão ao virar `float` antes da divisão; a posição válida nunca voltava a falso (`src/drivers/gps/nmea_parser.c`).

### Adicionado

- Testes de host do port (`zephyr_app/tests/host/`, rodados por `tools/fw/host_tests.sh`): Unity 2.6.1 + CTest com o GCC do PC e shims do Zephyr; 4 conjuntos, 32 casos (`vecteur`, `power_zone`, `suffer_score`, `nmea_parser`), com oráculo do legacy.
- `zephyr_app/sysbuild.conf` com `SB_CONFIG_PARTITION_MANAGER=n`: build com sysbuild, sem o Partition Manager depreciado.
- `.gitattributes` (LF no repositório, CRLF nos `.bat`, `hardware/` e os dados de teste de `tools/TDD/` byte a byte) e `.editorconfig`.

### Alterado

- `.gitignore`: builds, caches do clangd, `__pycache__`, `node_modules` e `.claude/settings.local.json`.

### Removido

- Template vazio de app da raiz (`CMakeLists.txt`, `prj.conf`, `src/main.c`), que vinha do commit inicial e não era o firmware.

## [2.0.0] - 2025-12-01

Port inicial para Zephyr, criado entre 2025-11-26 e 2025-11-28 e compilado pela última vez em 2025-12-01 com o NCS v3.1.0 (versão definida em `app_types.h`, sem tag).

### Adicionado

- `zephyr_app/` com HAL (GPIO, I2C, SPI, UART), drivers (LS027, BME280 e FXOS8700 sobre os drivers nativos, STC3100, GPS, parser NMEA, EPO, NeoPixel em stub), modelo (boucle, attitude, Kalman de 3 estados, locator, segmentos, listas de pontos, vetores, zonas de potência, suffer score, zonas RR, configurações em NVS, recuperação de falha, percurso, log no SD, Zwift), BLE (NUS, LNS, BAS, DIS e clientes HRS, CSC, FTMS e Komoot) e interface (9 páginas, menu, telas de rolo).
- Documentos de arquitetura, revisão MISRA, ambiente e gap analysis (25 e 26 de novembro de 2025), em `docs/`.
- Scripts `.bat` para o NCS v3.1.0.

## Legacy

O stravaV10 original (Vincent Gollé, 2015 a 2020, CC BY-NC 4.0) foi copiado para `legacy/`, `libraries/` e `tools/` em 2025-11-25 e o projeto da placa para `hardware/` em 2025-11-30. Essas pastas não são versionadas aqui como produto: são referência.
