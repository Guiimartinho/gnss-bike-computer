# Ambiente, build, gravação e console

Como montar o ambiente do nRF Connect SDK no Windows, compilar o port Zephyr, gravar o nRF52840-DK, ver o log, rodar os testes de host e validar a documentação. Tudo aqui foi executado nesta máquina em 2026-09-18, salvo onde está dito o contrário.

**Nesta página:** [Ferramentas](#ferramentas) · [Scripts](#scripts) · [Build do firmware](#build-do-firmware) · [Gravar e ver o log](#gravar-e-ver-o-log) · [Testes e análise estática](#testes-e-análise-estática) · [Documentação](#documentação) · [CI](#ci) · [Legacy](#legacy) · [Problemas conhecidos](#problemas-conhecidos)

## Ferramentas

| Ferramenta | Versão | Onde | Uso |
|---|---|---|---|
| nRF Connect SDK | v3.3.0 (Zephyr 4.3.99) | `C:\ncs\v3.3.0` | firmware |
| Toolchain do NCS | `936afb6332`: Zephyr SDK 0.17.0, GCC 12.2.0, CMake 4.2.1, Ninja, west 1.5.0, Python 3.12.4, dtc 1.4.7 | `C:\ncs\toolchains\936afb6332` | firmware |
| nrfutil | 8.1.1 com o comando `device` 2.17.5 | no toolchain (`nrfutil\bin`) | gravar, recover, listar placas |
| SEGGER J-Link | V8.76, V8.96, V9.24a | `C:\Program Files\SEGGER` | driver do J-Link OB do DK |
| MinGW-w64 GCC | 15.2.0 (com gcov) | `C:\ProgramData\mingw64\mingw64\bin` | testes de host |
| CMake / Ninja | 4.1.2 / 1.13 | `C:\Program Files\CMake`, pip do Python 3.11 | testes de host |
| cppcheck | 2.20.0 | scoop | análise estática |
| clang-format / clang-tidy | LLVM do scoop | scoop | formatação e lint (sem configuração no projeto ainda) |
| Node.js | 22.18.0 | `C:\Program Files\nodejs` | mermaid-cli (cache do npx) e `tools/zpm` |
| Python | 3.11.9 | usuário | scripts de documentação |

O NCS v3.1.0 e o toolchain `b8b84efebd`, usados até dezembro de 2025, **não existem mais** nesta máquina. Os builds gerados com eles, em outras pastas (`C:\Users\...\Documents\88.Personal\1.Projects\gnss-bike-computer` e `...\stravaV10`), foram apagados em 2026-09-18.

## Scripts

```mermaid
flowchart LR
    subgraph CMD["cmd / duplo clique"]
        BUILD["build.bat [pristine]"]
        BNCS["build_ncs.bat"]
        FLASH["flash.bat [keep]"]
        REC["recover.bat"]
        SER["serial.bat [COMx] [baud]"]
    end
    subgraph BASH["Git Bash"]
        FW["tools/fw/fw.sh build · flash · recover · devices · size"]
        HT["tools/fw/host_tests.sh"]
        MC["tools/docs/mermaid_check.py"]
        LC["tools/docs/links_check.py"]
    end
    BUILD --> ENVB["tools/fw/ncs_env.bat"]
    BNCS --> BUILD
    FLASH --> ENVB
    REC --> ENVB
    SER --> ENVB
    FW --> ENVS["tools/fw/ncs_env.sh"]
```

| Variável | Onde vale | Efeito |
|---|---|---|
| `NCS_ROOT`, `NCS_VERSION`, `NCS_TOOLCHAIN` | `ncs_env.*` | trocam o SDK (padrão `C:\ncs`, `v3.3.0`, `936afb6332`; o `.sh` descobre o toolchain pelo `toolchains.json`) |
| `BUILD_DIR` | `build.bat`, `flash.bat`, `fw.sh` | outra pasta de build (padrão `zephyr_app/build`); relativa, vale a partir da pasta atual |
| `BOARD` | `build.bat`, `flash.bat`, `recover.bat`, `fw.sh` | outro alvo (padrão `nrf52840dk/nrf52840`; o nRF54LM20 DK é `nrf54lm20dk/nrf54lm20a/cpuapp`); o overlay `zephyr_app/boards/<placa>.overlay` entra pelo nome |
| `ANT` | `build.bat`, `fw.sh` | `1` compila com o ANT: o add-on `sdk-ant` em `SDK_ANT_DIR`, `zephyr_app/modules/ant_ncs33_compat` e `zephyr_app/ant.conf`; ao trocar, use `pristine` ou outra `BUILD_DIR` |
| `SDK_ANT_DIR` | `build.bat`, `fw.sh` | pasta do add-on (padrão `C:\ncs\sdk-ant`) |
| `FAMILY` | `flash.bat`, `recover.bat`, `fw.sh` | família do `nrfutil`; sem ela, vem da `BOARD` (`nrf54l` para as placas nRF54L, `nrf52` no resto) |
| `NRF_SERIAL` | `flash.bat`, `recover.bat`, `fw.sh` | escolhe o J-Link pelo número de série |
| `SERIAL_PORT` | `serial.bat` | porta padrão do console (padrão `COM11`) |
| `NOPAUSE` | todos os `.bat` | não espera tecla no fim |

Os `ncs_env.*` reproduzem o `environment.json` do toolchain (PATH, `PYTHONPATH`, `NRFUTIL_HOME`, `ZEPHYR_TOOLCHAIN_VARIANT=zephyr`, `ZEPHYR_SDK_INSTALL_DIR`) e definem `ZEPHYR_BASE`. Eles **não** definem `TOOLCHAIN_ROOT` (ver [problemas conhecidos](#problemas-conhecidos)).

## Build do firmware

```sh
bash tools/fw/fw.sh build            # incremental
bash tools/fw/fw.sh build pristine   # do zero
```

Ou `build.bat` / `build.bat pristine` no cmd. Por baixo:

```sh
source tools/fw/ncs_env.sh
cd zephyr_app
python -m west build -p auto -b nrf52840dk/nrf52840 -d build --sysbuild .
```

- **Alvo:** `nrf52840dk/nrf52840`, com os pinos da placa myStravaB em `zephyr_app/boards/nrf52840dk_nrf52840.overlay`, que o Zephyr aplica pelo nome da placa. Outro alvo: `BOARD=<placa> bash tools/fw/fw.sh build` ou `set BOARD=<placa>` antes do `build.bat`.
- **nRF54LM20 DK:** `BOARD=nrf54lm20dk/nrf54lm20a/cpuapp BUILD_DIR=zephyr_app/build_54 bash tools/fw/fw.sh build`. Usa `boards/nrf54lm20dk_nrf54lm20a_cpuapp.overlay` (pinos no conector de expansão do DK) e `boards/nrf54lm20dk_nrf54lm20a_cpuapp.conf` (configurações no ZMS). Detalhes em [05](05-arquitetura-zephyr.md#devicetree-e-alvos).
- **ANT:** `ANT=1 bash tools/fw/fw.sh build pristine` (ou `set ANT=1` antes do `build.bat`), com `BOARD` para o nRF54LM20 DK. Precisa do add-on em `C:\ncs\sdk-ant` ([07](07-radio-ant-ble.md#ant-no-ncs-v330)); mostra um aviso esperado a mais, `Deprecated symbol SOC_SERIES_NRF52X is enabled` (ou `SOC_SERIES_NRF54LX`).
- **Sysbuild** é o fluxo padrão do NCS. O `zephyr_app/sysbuild.conf` desliga o Partition Manager (`SB_CONFIG_PARTITION_MANAGER=n`), depreciado no NCS 3.3; o layout vem do devicetree.
- **Saída:** `zephyr_app/build/zephyr_app/zephyr/zephyr.hex` (e `.elf`, `.map`, `.config`, `zephyr.dts`). Sem MCUboot não há `merged.hex`.

### Mapa de memória

| Região | Endereço | Tamanho | Conteúdo |
|---|---|---|---|
| flash: aplicação | `0x00000` | até 992 KB | firmware (`CONFIG_FLASH_LOAD_OFFSET=0`) |
| flash: `storage_partition` | `0xF8000` | 32 KB | settings/NVS: bonds BLE e configurações do usuário |
| RAM | `0x20000000` | 256 KB | tudo |

### Resultado de referência

Build com sysbuild da `develop` em 2026-09-19 (atualizado a cada commit que muda o tamanho):

| Item | Valor |
|---|---|
| FLASH | 473.224 B (45,13 % de 1 MB) |
| RAM | 219.200 B (83,62 % de 256 KB) |
| Avisos | 0 |
| Erros | 0 |
| Tempo | cerca de 1 min do zero (pelos horários dos logs) |
| nRF54LM20 DK | FLASH 484.668 B (24,40 % de 1.940 KB), RAM 247.800 B (47,36 % de 511 KB), 0 avisos |
| Com `ANT=1` | nRF52840 DK: FLASH 501.832 B, RAM 223.808 B; nRF54LM20 DK: FLASH 514.608 B, RAM 252.384 B; só o aviso do símbolo obsoleto |

Maiores consumidores de RAM (`bash tools/fw/fw.sh size`): o heap do LVGL 32.768 B (`CONFIG_LV_Z_MEM_POOL_SIZE`), `seg_runtime` 22.000 B, o buffer de desenho do LVGL 19.200 B, o heap do sistema 16.384 B (`CONFIG_HEAP_MEM_POOL_SIZE`), o quadro da tela (12.482 B da Sharp no nRF52840, 36.482 B do JDI no nRF54LM20), `points` 8.000 B, a pilha da thread `ui` (6.144 B), o pool do controlador BLE 5.247 B, as outras pilhas ([05](05-arquitetura-zephyr.md#pilhas)) e quatro cópias do retrato da tela (`ui_model_t`, 2.404 B: a do modelo, a do canal, a da thread `ui` e a da interface).

A interface ocupa no nRF54LM20 DK cerca de 151 KB de flash: LVGL 92.467 B, telas 28.753 B, fontes 26.904 B e o driver da tela 2.972 B ([18](18-interface-telas.md#memória)).

## Gravar e ver o log

```sh
bash tools/fw/fw.sh devices      # precisa aparecer um dispositivo com o trait jlink
bash tools/fw/fw.sh flash        # apaga tudo (ERASE_ALL) e grava
bash tools/fw/fw.sh flash keep   # apaga só as faixas do firmware: mantém a storage_partition
bash tools/fw/fw.sh recover      # chip protegido: apaga tudo e libera o APPROTECT
```

- Os scripts filtram `--traits jlink` e a família do chip (`nrf52`, ou `nrf54l` quando a `BOARD` é uma placa nRF54L): nesta máquina costuma haver um ST-LINK e outras seriais USB conectados.
- **Console:** `zephyr,console` é o `uart0` do DK (TX P0.06, RX P0.08, 115200 baud), exposto pela porta VCOM do J-Link. `serial.bat COMx` abre o miniterm do Python do toolchain (pyserial 3.5); `Ctrl+]` sai. O log usa o backend UART; o RTT está desligado no `prj.conf`.
- Em 2026-09-18 não havia nRF52840-DK conectado: **nada foi gravado nem testado em placa** nesta revisão.

## Testes e análise estática

```sh
bash tools/fw/host_tests.sh             # configura, compila e roda todos
bash tools/fw/host_tests.sh -R segment  # filtra pelo nome
```

Os testes compilam os módulos de lógica de `zephyr_app/src` com o GCC do PC e shims mínimos do Zephyr; detalhes em [12-ferramentas-testes.md](12-ferramentas-testes.md) e na skill `fw-testes`.

```sh
cppcheck --enable=warning,style,performance,portability --std=c11 --inline-suppr --quiet \
  --suppress=missingIncludeSystem --suppress=missingInclude --suppress=unusedFunction \
  -I zephyr_app/include zephyr_app/src
```

Sem os headers do Zephyr, o cppcheck acusa `syntaxError` nas macros de GATT e devicetree (`BT_GATT_*`, `DT_*`): são falsos positivos. Os achados reais estão em [11-qualidade-misra.md](11-qualidade-misra.md).

## Documentação

```sh
python tools/docs/mermaid_check.py   # extrai, procura diagramas em texto e renderiza cada Mermaid
python tools/docs/links_check.py     # links relativos e âncoras
```

O `mermaid_check.py` usa o mermaid-cli que já está no cache do npx e o Chrome headless do puppeteer; não instala nada. Padrão de escrita na skill `docs-gnss`.

## CI

O `.github/workflows/ci.yml` existe, mas está **desligado** por decisão do dono (2026-09-18): o único gatilho é `workflow_dispatch`, então nada roda sozinho no GitHub Actions. Para rodar, use "Run workflow" na aba Actions.

| Job | O que faz |
|---|---|
| `host-tests` | os testes de host no Ubuntu com o GCC do sistema |
| `docs` | `links_check.py` e `mermaid_check.py` com o mermaid-cli do npm (`.github/puppeteer-ci.json` passa `--no-sandbox` ao Chrome) |
| `firmware` | build com sysbuild no container `ghcr.io/nrfconnect/sdk-nrf-toolchain:v3.3.0`, com `west init` do sdk-nrf v3.3.0 |

Nenhum job rodou no GitHub até agora; o Linux pode acusar avisos que o MinGW não acusa. Ligar gatilhos automáticos (`push`, `pull_request`) só com pedido do dono.

## Legacy

O firmware original não compila nesta máquina: exige **nRF5 SDK 16.0.0**, **SoftDevice S340 6.1.1** e **GCC 6 2017-q2-update**, que não estão instalados. Os comandos originais (`make`, `make flash_softdevice`, `make flash`, DFU segurando o botão direito) estão em [`legacy/README.md`](../legacy/README.md). O `nrfutil` antigo do pip (para pacotes DFU do nRF5 SDK), em `Python311\Scripts`, está quebrado.

## Problemas conhecidos

| Sintoma | Causa e solução |
|---|---|
| `ValueError: path is on mount 'F:', start on mount 'C:'` | o `west` calcula caminhos relativos a partir do diretório atual; com o NCS em `C:` e o projeto em `F:`, rode de dentro do `zephyr_app` (os scripts fazem isso). Os `.bat` antigos faziam `cd C:\ncs\...` e só funcionavam com o projeto em `C:` |
| `include could not find requested file: ...\936afb6332\cmake\toolchain\zephyr\generic.cmake` | a variável de ambiente `TOOLCHAIN_ROOT` está definida: o Zephyr a lê como raiz das definições de toolchain. Não a defina; os `.bat` antigos a definiam |
| `Build directory ... is for application ...` | build antigo de outra pasta; `-p auto` (padrão dos scripts) refaz do zero |
| aviso `SB_CONFIG_PARTITION_MANAGER is enabled ... deprecated` | falta o `zephyr_app/sysbuild.conf` |
| `multiple definition of 'ant_stack_init'` | o add-on já define `ant_stack_init()`; no código do port use nomes fora do prefixo `ant_` (`rf_ant_init()`) |
| `lib/soft-float/libant.a ... missing` | o `ant_ncs33_compat` não entrou no build: compile pelos scripts com `ANT=1` |
| `--no-sysbuild` | ainda funciona para imagem única, mas está depreciado desde o NCS 2.7 |
| `JLink.exe` abre a ferramenta do Java | o `JLink.exe` do PATH é do Eclipse Adoptium; o da SEGGER fica em `C:\Program Files\SEGGER\JLink_V924a\` |
| pino da placa se comportando como outro periférico no DK | um nó do DK voltou a ocupar o pino; o overlay desliga `qspi` e `mx25r64`, `spi3` e `pwm0` e tira RTS/CTS do `uart0` (ver [02-hardware.md](02-hardware.md#alvo-híbrido-no-dk)) |
| cppcheck com `syntaxError` em `ble_*.c` e `neopixel.c` | macros do Zephyr sem os headers; falso positivo |
| diagnósticos do clangd nos testes de host | o clangd usa o `compile_commands.json` do firmware; `zephyr_app/tests/host/.clangd` aponta para o dos testes depois do primeiro `host_tests.sh` |
