# Documentação do GNSS Bike Computer

Índice da documentação técnica. Comece pela visão geral; para trabalhar no firmware, leia o ambiente de build e o status do port.

| # | Documento | Assunto |
|---|---|---|
| 01 | [Visão geral](01-visao-geral.md) | o produto, as funcionalidades, os modos, o legacy e o port |
| 02 | [Hardware](02-hardware.md) | placa myStravaB V3, componentes, pinagem das três revisões, overlay, alimentação, alvo híbrido no DK |
| 03 | [Ambiente e build](03-ambiente-build.md) | NCS v3.3.0, scripts, build, gravação, console, problemas conhecidos |
| 04 | [Arquitetura do legacy](04-arquitetura-legacy.md) | firmware original stravaV10: tasks, modos, módulos, glossário dos nomes em francês |
| 05 | [Arquitetura do port Zephyr](05-arquitetura-zephyr.md) | threads, módulos, fluxo de dados, HAL, devicetree, Kconfig |
| 06 | [Algoritmos](06-algoritmos.md) | Kalman de altitude, drift do barômetro, potência estimada, segmentos, posição relativa, zonas, suffer score, RMSSD |
| 07 | [Rádio: ANT+ e BLE](07-radio-ant-ble.md) | perfis ANT+ do legacy, serviços BLE, stravaAP, Komoot, LNS, estratégia do port |
| 08 | [Interface](08-interface.md) | display, telas, menus, botões, notificações |
| 09 | [Armazenamento e USB](09-armazenamento-usb.md) | arquivos no SD e na flash NOR, formatos, USB CDC e MSC |
| 10 | [Status do port](10-status-do-port.md) | matriz legacy × port, defeitos conhecidos, roteiro |
| 11 | [Qualidade e MISRA](11-qualidade-misra.md) | revisão MISRA do legacy, regras do port, cppcheck |
| 12 | [Ferramentas e testes](12-ferramentas-testes.md) | simulador TDD do legacy, zpm, testes de host do port, bibliotecas e licenças |

Imagens da interface e fotos do aparelho ficam em [`img/`](img/). Os documentos de novembro de 2025, substituídos por estes, estão em [`historico/`](historico/).
