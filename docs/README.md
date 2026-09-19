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
| 13 | [Placa nova](13-placa-nova.md) | proposta de hardware da placa própria: nRF54LM20A, display colorido, GNSS e antena interna, energia com painel solar, sensores, orçamentos de pinos e de energia |
| 14 | [Hardware da placa nova](14-hardware-placa-nova.md) | especificação técnica: decisões, arquitetura, árvore de alimentação, lista de materiais, barramentos, pinos, PCB, empilhamento, regras de layout, bring-up |
| 15 | [Avaliação dos componentes](15-avaliacao-componentes.md) | escolha de cada componente da placa nova: carga por USB-C e painel solar, GNSS, MCU, display, antena, sensores, armazenamento, USB, bateria; testes de bancada antes do layout |
| 16 | [Arquitetura do firmware](16-arquitetura-firmware.md) | arquitetura-alvo para a placa nova: serviços, threads, eventos no zbus, máquinas de estado no SMF (sistema e energia, modo, gravação, GNSS, sensores, carga, interface, luz), partida e desligamento, energia por estado, atualização de firmware, migração do port |
| 17 | [Dispositivos BLE e ANT+](17-dispositivos-ble-ant.md) | catálogo dos dispositivos externos (FC, velocidade e cadência, potência, rolo, radar, luzes, câmbio, e-bike, celular), perfis ANT+ e serviços BLE, o que o SDK já tem, prioridades, limites de canais e conexões, pareamento |

Imagens da interface e fotos do aparelho ficam em [`img/`](img/). Os documentos de novembro de 2025, substituídos por estes, estão em [`historico/`](historico/).
