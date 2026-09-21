<div align="center">

# GNSS Bike Computer

**Computador de bordo para ciclismo com GPS: segmentos do Strava em tempo real, percursos, altimetria por fusão de sensores e sensores sem fio. Portado do stravaV10 para Zephyr, do nRF52840 da myStravaB V3 para o nRF54LM20A de uma placa própria com GNSS de banda dupla e painel solar.**

![MCU](https://img.shields.io/badge/MCU-nRF52840%20%7C%20nRF54LM20A-00A9CE)
![SDK](https://img.shields.io/badge/nRF%20Connect%20SDK-v3.3.0-00A9CE)
![RTOS](https://img.shields.io/badge/Zephyr-4.3.99-7929D2)
![Rádio](https://img.shields.io/badge/r%C3%A1dio-BLE%20%2B%20ANT%2B-0082FC)
![Linguagem](https://img.shields.io/badge/C-C11-A8B9CC?logo=c&logoColor=white)
![Testes](https://img.shields.io/badge/testes%20de%20host-65%20casos-2E7D32)
![CI](https://img.shields.io/badge/CI-desligado-lightgrey)
![Estado](https://img.shields.io/badge/estado-port%20em%20andamento-EF6C00)
![Licença](https://img.shields.io/badge/licen%C3%A7a-a%20definir-lightgrey)

</div>

O projeto leva para o nRF Connect SDK o **stravaV10**, firmware aberto de Vincent Gollé para a placa **myStravaB V3**: um aparelho em retrato com LCD de memória Sharp, GNSS MediaTek, barômetro, acelerômetro, medidor de bateria, microSD e rádio BLE + ANT+. O código original fica em [`legacy/`](legacy/) como referência de comportamento; o port em C puro sobre Zephyr fica em [`zephyr_app/`](zephyr_app/) e compila para o nRF52840-DK e para o nRF54LM20 DK. A próxima placa é própria, com o nRF54LM20A, GNSS de banda dupla, tela colorida e painéis solares na caixa que estendem a autonomia, sem sustentar o aparelho sozinhos; a especificação e a avaliação dos componentes estão em [`docs/`](docs/README.md).

> [!WARNING]
> O port **compila e passa nos testes de host, mas nunca rodou em placa nem nos DKs**. Segmentos, percursos, sensores BLE e ANT+, log no SD e a interface do legacy ainda não funcionam de ponta a ponta. A placa nova ainda não existe: nenhum componente foi comprado nem medido. O estado de cada área está em [docs/10-status-do-port.md](docs/10-status-do-port.md).

## Índice

- [Visão geral](#visão-geral)
- [Placa nova](#placa-nova)
- [Funcionalidades](#funcionalidades)
- [Início rápido](#início-rápido)
- [Estrutura](#estrutura)
- [Documentação](#documentação)
- [Estado e próximos passos](#estado-e-próximos-passos)
- [Créditos e licenças](#créditos-e-licenças)

## Visão geral

```mermaid
flowchart LR
    subgraph DEV["myStravaB V3 · nRF52840"]
        MODEL["modelo<br/>altitude, potência,<br/>segmentos, percurso"]
        VUE["interface<br/>LCD LS027 + 3 botões"]
        LOG["log no microSD"]
    end
    GNSS["GNSS M10578-A3"] -->|NMEA| MODEL
    SENS["BME280 · FXOS8700 · STC3100"] -->|I2C| MODEL
    SENSORS["frequência cardíaca, cadência,<br/>potência, rolo"] -->|"ANT+ / BLE"| MODEL
    PHONE["celular<br/>Komoot, LNS"] -->|BLE| MODEL
    MODEL --> VUE
    MODEL --> LOG
    PC["PC"] <-->|"USB / stravaAP"| DEV
```

## Placa nova

![Proposta do aparelho: frente, lateral direita, traseira e arranjo interno](docs/img/placa-nova-caixa.svg)

Conceito em escala, a partir da caixa da V3: 62 × 104 × 19 mm, PCB de 55 × 97 mm em 4 camadas, 6 módulos solares na frente inclinada e nos chanfros laterais. A escolha de cada componente, com os números dos datasheets, está em [docs/15-avaliacao-componentes.md](docs/15-avaliacao-componentes.md); a lista de compras, validada peça a peça, em [docs/19-lista-de-compras.md](docs/19-lista-de-compras.md).

```mermaid
flowchart LR
    MCU["Fanstel BM20C<br/>nRF54LM20A, BLE e ANT+"]
    GNSS2["u-blox MAX-F10S<br/>L1 + L5, 46,8 mW"] -->|UART| MCU
    LCD2["JDI LPM027M128B de 8 cores<br/>ou Sharp LS027B7DH01A com luz"] ---|SPI| MCU
    PWR["nPM1300, AEM10900, MAX17262<br/>USB-C e painel solar"] -.->|I2C| MCU
    SENS2["BMP585, BMI270,<br/>MMC5633NJL, OPT3001"] -->|I2C| MCU
    MEM["SD NAND"] ---|SPI| MCU
```

| Bloco | Escolha | Por quê |
|---|---|---|
| Carga | nPM1300 no USB-C, AEM10900 no painel, MAX17262 na célula | o painel carrega com o aparelho desligado e corte térmico próprio; o USB bloqueia a carga solar no hardware |
| GNSS | u-blox MAX-F10S, banda dupla L1 + L5, com o MAX-M10N-10B (só L1) no mesmo footprint | 1 m de CEP contra 1,5 m, e o código do L5 contra o multipercurso de prédio e mata. Custa autonomia: o aparelho gasta cerca de 58 mW e dura cerca de 115 h sem sol, e o painel devolve de 23 a 46 min por hora de sol em vez de cobrir o consumo. Com o M10N em LEAP seriam cerca de 21 mW e 310 h (estimativas) |
| Tela | JDI LPM027M128B de 8 cores, sem luz própria (escolha do dono em 2026-09-19, no AliExpress); a Sharp LS027B7DH01A com luz frontal no mesmo conector, de reserva | o firmware atende as duas pelo mesmo driver, em retrato, com um tema para cada |
| USB e armazenamento | USB-C IPX8 e SD NAND soldado | caixa sem tampas e sem cartão solto na vibração |

## Funcionalidades

| Funcionalidade | Legacy | Port |
|---|---|---|
| Segmentos do Strava com avanço contra o recorde | sim | lógica portada, sem carga de dados |
| Percurso com mapa e zoom | sim | parcial, não ligado |
| Altitude por Kalman de 3 estados, subida, inclinação | sim | portado com diferenças |
| Potência estimada | sim | fórmula diferente |
| Sensores ANT+ (FC, velocidade e cadência, rolo FE-C) | sim | a pilha do add-on `sdk-ant` compila com o NCS v3.3.0 (`ANT=1`); perfis na fase 4 |
| BLE (potência, posição do celular, Komoot) | sim | parcial |
| Zonas de potência, suffer score, variabilidade da FC | sim | portados e testados, sem dados reais |
| Log no microSD e download pelo PC | sim | log em stub |
| USB serial e mass storage | sim | fora do build |
| Telas em retrato, menu, notificações, botões | sim | LVGL no firmware com driver próprio da tela, 31 telas testadas no PC; nunca vistas num painel |

## Início rápido

**Pré-requisitos** (Windows): nRF Connect SDK v3.3.0 em `C:\ncs` com o toolchain `936afb6332`, SEGGER J-Link, nRF52840-DK ou nRF54LM20 DK. Para o ANT, o add-on `sdk-ant` em `C:\ncs\sdk-ant`, depois de aceitar o acordo ANT+ ([07](docs/07-radio-ant-ble.md#ant-no-ncs-v330)). Para os testes de host: MinGW-w64 GCC, CMake e Ninja. Detalhes em [docs/03-ambiente-build.md](docs/03-ambiente-build.md).

Código: [github.com/Guiimartinho/gnss-bike-computer](https://github.com/Guiimartinho/gnss-bike-computer) (`git clone https://github.com/Guiimartinho/gnss-bike-computer.git`).

```sh
# Git Bash, na raiz do repositório
bash tools/fw/fw.sh build          # compila o zephyr_app (sysbuild) para o nRF52840-DK
bash tools/fw/fw.sh flash          # grava no DK pelo J-Link
BOARD=nrf54lm20dk/nrf54lm20a/cpuapp BUILD_DIR=zephyr_app/build_54 bash tools/fw/fw.sh build
ANT=1 bash tools/fw/fw.sh build pristine   # com a pilha ANT do sdk-ant
bash tools/fw/host_tests.sh        # 36 conjuntos de testes de host
python tools/ui/render_screens.py  # desenha as 31 telas no PC (docs/telas)
python tools/docs/mermaid_check.py # valida os diagramas da documentação
```

No `cmd` ou com duplo clique: `build.bat`, `flash.bat`, `recover.bat`, `serial.bat COMx`.

O CI (`.github/workflows/ci.yml`) está **desligado**: só roda à mão, pela aba Actions do GitHub ([detalhes](docs/03-ambiente-build.md#ci)).

## Estrutura

```mermaid
flowchart TB
    ROOT["gnss_bike_computer/"]
    ROOT --> ZA["zephyr_app/<br/>port Zephyr: src, include, boards, modules, tests/host"]
    ROOT --> LEG["legacy/<br/>stravaV10 original (referência)"]
    ROOT --> LIB["libraries/<br/>bibliotecas do legacy"]
    ROOT --> TOOLS["tools/<br/>fw e docs do projeto · TDD, zpm, MMD, jumper do legacy"]
    ROOT --> HW["hardware/<br/>Eagle da myStravaB V3 (Gerbers da V2)"]
    ROOT --> DOCS["docs/<br/>documentação numerada, img, historico"]
    ROOT --> AI["CLAUDE.md · AGENTS.md · .claude/skills/"]
    ROOT --> BAT["build.bat · flash.bat · recover.bat · serial.bat"]
```

## Documentação

| Documento | Assunto |
|---|---|
| [01 · Visão geral](docs/01-visao-geral.md) | produto, modos, legacy e port |
| [02 · Hardware](docs/02-hardware.md) | placa, pinagem, alimentação, alvo no DK |
| [03 · Ambiente e build](docs/03-ambiente-build.md) | NCS v3.3.0, scripts, gravação, problemas conhecidos |
| [04 · Arquitetura do legacy](docs/04-arquitetura-legacy.md) | tasks, modos, glossário |
| [05 · Arquitetura do port](docs/05-arquitetura-zephyr.md) | threads, fluxo de dados, pilhas, devicetree |
| [06 · Algoritmos](docs/06-algoritmos.md) | fórmulas e constantes, legacy × port |
| [07 · Rádio](docs/07-radio-ant-ble.md) | ANT+, BLE, stravaAP, Komoot |
| [08 · Interface](docs/08-interface.md) | telas, menus, botões |
| [09 · Armazenamento e USB](docs/09-armazenamento-usb.md) | formatos de arquivo, SD, USB |
| [10 · Status do port](docs/10-status-do-port.md) | matriz, correções, defeitos, roteiro, decisões |
| [11 · Qualidade e MISRA](docs/11-qualidade-misra.md) | regras e análise estática |
| [12 · Ferramentas e testes](docs/12-ferramentas-testes.md) | testes de host, simulador do legacy, licenças |
| [13 · Placa nova](docs/13-placa-nova.md) | proposta de hardware da placa própria |
| [14 · Hardware da placa nova](docs/14-hardware-placa-nova.md) | especificação técnica: alimentação, lista de materiais, pinos, PCB |
| [15 · Avaliação dos componentes](docs/15-avaliacao-componentes.md) | escolha de cada componente da placa nova, com números de datasheet |
| [16 · Arquitetura do firmware](docs/16-arquitetura-firmware.md) | arquitetura-alvo e máquinas de estado para a placa nova |
| [17 · Dispositivos BLE e ANT+](docs/17-dispositivos-ble-ant.md) | catálogo de sensores e acessórios, prioridades e limites do rádio |
| [18 · Interface e telas](docs/18-interface-telas.md) | interface LVGL da placa nova: todas as telas, desenhadas e testadas no PC, em 8 cores e em preto e branco |
| [19 · Lista de compras](docs/19-lista-de-compras.md) | peças validadas em duas passagens, trocas, correções de integração e códigos da DigiKey |
| [CHANGELOG](CHANGELOG.md) | histórico de mudanças |

## Estado e próximos passos

- **Feito em 2026-09-18:** build no NCS v3.3.0 com sysbuild; correção de 13 defeitos críticos (estouro de pilha no Kalman, GPS e modelo dentro de ISR, corrupção de memória no log, botões e pinos do GPS invertidos, conflitos de pinos com o DK); testes de host; documentação e contexto para assistentes de IA.
- **Fase 1 do roteiro:** a base da arquitetura nova (serviços com thread própria, eventos no zbus, máquinas de sistema e de modo no SMF, watchdog por serviço, desligamento automático do legacy), feita em 2026-09-19.
- **Novos alvos e rádio:** o port compila para o nRF54LM20 DK, e a pilha ANT do add-on `sdk-ant` v2.1.1 compila sobre o NCS v3.3.0 nos dois DKs (`ANT=1`); nada disso foi testado em placa.
- **Interface:** as 29 telas em LVGL, testadas no PC, rodam no firmware desde 2026-09-19 com um driver próprio da tela (JDI LPM027M128B em 8 cores ou Sharp em preto e branco, em retrato), teclas com toque longo e a máquina da luz; nunca vistas num painel.
- **Placa nova:** desenho do aparelho, especificação ([14](docs/14-hardware-placa-nova.md)) e avaliação dos componentes ([15](docs/15-avaliacao-componentes.md)). O esquemático é do dono; antes do layout vêm os testes de bancada da carga dupla, do GNSS (C/N0 por banda e isolamento da antena contra o rádio de 2,4 GHz), da coexistência dos rádios e do display.
- **Decidido:** ANT+ e BLE juntos (os equipamentos externos falam ANT+) e placa própria com o nRF54LM20A. Decisões e pendências em [docs/10-status-do-port.md](docs/10-status-do-port.md#decisões-do-dono).
- **Próximo:** bancada e esquemático da placa nova, depois fidelidade dos algoritmos, armazenamento, rádio e interface. Roteiro em [docs/10-status-do-port.md](docs/10-status-do-port.md#roteiro).

## Créditos e licenças

- **stravaV10** e **myStravaB**: Vincent Gollé ([vincent290587/stravaV10](https://github.com/vincent290587/stravaV10)), licenciado sob **Creative Commons Atribuição-NãoComercial 4.0 (CC BY-NC 4.0)**. O port deriva desse código; ver [`legacy/README.md`](legacy/README.md).
- Bibliotecas de terceiros em `libraries/` e `tools/` mantêm suas licenças (BSD, LGPL-2.1, GPL-2.0, Nordic, SEGGER): tabela em [docs/12-ferramentas-testes.md](docs/12-ferramentas-testes.md#bibliotecas-e-licenças).
- O add-on `sdk-ant` e o material ANT+ não fazem parte do repositório: o ANT+ Adopter Agreement proíbe redistribuir ([07](docs/07-radio-ant-ble.md#decisão-ant-e-ble)).
- A licença do port ainda não foi definida.
