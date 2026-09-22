# Ferramentas e testes

As ferramentas herdadas do stravaV10 em `tools/` (simulador de host, scripts Node, debug em monitor mode, emulador), as ferramentas novas do projeto (`tools/fw/`, `tools/docs/`, `tools/ui/`), os testes de host do port, o renderizador das telas e as bibliotecas de terceiros com suas licenças.

**Nesta página:** [Mapa de tools/](#mapa-de-tools) · [Testes de host do port](#testes-de-host-do-port) · [Renderizador de telas](#renderizador-de-telas) · [Simulador TDD do legacy](#simulador-tdd-do-legacy) · [Outras ferramentas herdadas](#outras-ferramentas-herdadas) · [Bibliotecas e licenças](#bibliotecas-e-licenças)

## Mapa de tools/

| Pasta | Origem | Uso |
|---|---|---|
| `tools/fw/` | projeto | `ncs_env.sh`/`.bat` (ambiente do NCS), `fw.sh` (build, flash, recover, devices, size), `host_tests.sh` |
| `tools/docs/` | projeto | `mermaid_check.py` e `links_check.py` (skill `docs-gnss`); `case_drawing.py` gera o desenho do aparelho da [placa nova](13-placa-nova.md#como-fica-o-aparelho) e `screens_drawing.py`, as maquetes das [telas](18-interface-telas.md#telas) |
| `tools/ui/` | projeto | `font_gen.py` (fontes de 1 bit da interface) e `render_screens.py` (compila e roda o [renderizador de telas](#renderizador-de-telas) e gera `docs/img/telas-lvgl/`) |
| `tools/TDD/`, `tools/TDDW/` | stravaV10 | simulador de host do firmware original (Linux e Windows) |
| `tools/zpm/` | stravaV10 | scripts Node: posição do Zwift (`$LOC`), download de logs, conversão para GPX |
| `tools/MMD/` | stravaV10 | monitor mode debugging do J-Link |
| `tools/jumper/` | stravaV10 | descrição de placa para o Jumper Virtual Lab |

## Testes de host do port

Os módulos de lógica do `zephyr_app/src` compilam com o GCC do PC contra shims mínimos do Zephyr e rodam com Unity e CTest.

```sh
bash tools/fw/host_tests.sh             # tudo
bash tools/fw/host_tests.sh -R tilt     # um conjunto
```

| Conjunto | Código testado | Casos | O que garante |
|---|---|---|---|
| `test_segment_file` | `model/segment_file.c` | 6 | nome de segmento do legacy (12 caracteres, `#` e `.` nas posições certas), posição em base 36 no nome, leitura da linha `lat ; lon ; rtime ; alt`, ida e volta nome↔posição e a conferência dos **138 segmentos reais** de `tools/TDD/DB`: o nome tem de bater com o primeiro ponto de dentro |
| `test_liste_points` | `model/liste_points.c` | 15 | ordem das listas (segmento em ordem de arquivo, histórico com o mais novo no índice 0), corte do histórico, distância à lista, caixa e centro em graus, e a posição relativa com cinco pontos, projetada e fora do triângulo |
| `test_vecteur` | `model/vecteur.c` | 25 | distância dentro de 0,5 % da fórmula do legacy, sinais dos eixos, produto escalar, normalização |
| `test_crash_recovery` | `model/crash_recovery.c` | 6 | a partida limpa não restaura nada, o bloco volta inteiro (com o recorde), não depende de falha registrada, um byte trocado quebra o CRC, o bloco limpo não é aceito e a segunda gravação vale |
| `test_kalman_altitude` | `model/kalman_altitude.c`, `udmatrix.c` | 6 | a elevação acompanha a subida pelo pitch, a rampa e a velocidade vertical saem do filtro, o offset de montagem é estimado, parado não atualiza e a descida dá velocidade vertical negativa |
| `test_udmatrix` | `model/udmatrix.c` | 8 | `ones` preenche tudo (P0 = 900), `bound` compara valor absoluto e mantém covariância negativa, e soma e subtração com o destino igual a uma das entradas |
| `test_distance` | `model/distance.c` | 9 | descarte dos primeiros 25 m, instantâneo a cada 15 m (e um só quando a época traz 100 m), parado não soma, total igual ao do legacy em 300 m e a volta do total pela recuperação de falha |
| `test_power_estimate` | `model/power_estimate.c` | 7 | a fórmula do legacy em 64 pontos de velocidade e rampa contra a transcrição em `legacy_ref.h`, os 147 W a 30 km/h no plano, os 151 W a 12 km/h em 5 %, potência negativa na descida e a saturação do `int16_t` |
| `test_power_zone` | `model/power_zone.c` | 6 | limites das 7 zonas pelo FTP, janela de 50 a 1950 W, acumulação |
| `test_suffer_score` | `model/suffer_score.c` | 5 | zonas de FC e pontos por hora do legacy |
| `test_sd_logger` | `model/sd_logger.c` | 7 | intervalo de 15 m, lote de 5 com cabeçalho CSV, **nenhuma escrita fora do buffer sem cartão** |
| `test_ui_fmt` | `ui/ui_fmt.c` | 10 | números como o `_fmkstr` do legacy numa varredura, truncamento em `float` (0,21 vira `0.20`), negativos, limite de 100000, NaN, buffer pequeno, horas e hora desconhecida, larguras do `cadran` e do `cadranH`, valores com sinal |
| `test_tilt` | `svc/sensors/tilt.c` | 8 | inclinação, rolagem e rumo contra leituras construídas por rotação (aerospacial, norte-leste-baixo), janela de 50 amostras, rugosidade contra o laço de `fxos.cpp:761-766` |
| `test_backlight` | `svc/ui/backlight.c` | 11 | luz por 10 s depois de uma tecla, nova tecla reinicia, automática com pouca luz até a luz voltar, histerese entre 20 e 50 lux, desligada pelo menu, volta do contador de 32 bits |
| `test_max17262` | `modules/gnss_drivers`: `max17262_regs.h` | 10 | passos da tabela 2 da ficha (78,125 µV, 156,25 µA com sinal, 1/256 %, 0,5 mAh, 5,625 s, 1/256 °C), tempo desconhecido, arredondamento da carga, `DesignCap`, `IChgTerm`, `VEmpty` (o 0xA561 de fábrica) e `ModelCfg` |
| `test_aem10900` | `modules/gnss_drivers`: `aem10900_regs.h` | 7 | limiares de carga e de descarga da ficha (0x32 é 4,05 V, 0x2D é 3,04 V, passos de 56,25 mV, mínimos de 2,70 V e 2,51 V), tensão da bateria (4,8 V em 256 passos), energia do APM (`POWER << OFFSET`, além de 32 bits) e potência em µW sem calibração |
| `test_ubx_m10` | `modules/gnss_drivers`: `ubx_m10.c` | 24 | quadros UBX com o checksum de Fletcher, `UBX-CFG-VALSET` de uma chave e de um lote (`ubx_m10_valset_many()`, usado nos sinais de L5 do F10) e `VALGET` (tamanho do valor pelos bits 30 a 28 da chave, camadas RAM e BBR), `UBX-RXM-PMREQ` com backup e force, `UBX-CFG-RST` de partida fria, e a leitura do `UBX-NAV-PVT` (92 B, Nancy, bits de `valid`, psmState, rumo negativo) e do `UBX-NAV-SAT` (truncado e com elevação negativa); os vetores vêm de uma implementação independente das seções das fichas do F10 SPG 6.00 e do M10 SPG 5.30 |
| `test_activity` | `model/activity.c` | 19 | o cronômetro que conta só em movimento, o semáforo que o para depois de 3 s e o retomar acima de 3 km/h, a bicicleta empurrada a 2 km/h que não faz o cronômetro oscilar, a volta automática por distância e a volta pela tecla, a volta aberta que fecha no fim, a descida com a banda morta de 2 m contra o ruído do barômetro, as médias só do tempo em movimento e a cinta que cai sem derrubar a média |
| `test_fit_encode` | `model/fit_encode.c` | 15 | o cabeçalho de 14 bytes e o `.FIT`, as mensagens de definição e de dados, os tipos base, as escalas de altitude, distância e velocidade, os valores "inválidos" do formato, os semicírculos, a data do legacy virando `date_time`, as voltas numeradas, o CRC-16 conferido contra o cálculo direto para 33 tamanhos de arquivo, o resíduo zero do cabeçalho (que é o que deixa o arquivo fechar sem reler o que foi gravado) e um passeio inteiro lido de volta mensagem a mensagem |
| `test_climb` | `model/climb.c` | 16 | o que é e o que não é subida (a ponte curta demais, o falso plano de 1 %), os números de uma subida de verdade, o falso plano que não parte um colo em dois e o vale que parte, o percurso que acaba no topo, as categorias pelo ganho, o que falta ao ciclista em cada ponto da subida, a próxima subida anunciada no vale, a inclinação dos próximos 200 m e o percurso com mais subidas do que cabem |
| `test_csc_calc` | `model/csc_calc.c` | 13 | a volta de roda por segundo que são 7,58 km/h (o número que o port devolvia como zero), a velocidade de pedalada, a roda de 29", a bicicleta parada com o sensor ainda mandando, a volta do contador de tempo a cada 64 s e a do contador de voltas, a leitura impossível descartada sem travar o sensor, a cadência de 90 rpm, a roda-livre e a volta do contador de pedivela |
| `test_ftms_parse` | `model/ftms_parse.c` | 10 | o pacote que a maioria dos rolos manda (velocidade, cadência e potência instantâneas), o bit *More Data* invertido, os campos de média que têm de ser pulados e não lidos, a notificação com todos os campos, os quatro bytes da energia, a potência negativa, a notificação que mente sobre o próprio tamanho, e a checagem de que as constantes dos flags são uma sequência sem lacuna nem repetição |
| `test_radar` | `model/radar.c` e `model/radar_wire.c` | 20 | o carro que aparece com a distância, o mais perto vindo primeiro, o mesmo veículo mantendo o lugar enquanto se aproxima, o quadro perdido que não faz a marca piscar, o que sai de alcance, o nível de ameaça por distância e velocidade, o nível que o rádio manda ganhando do palpite, mais veículos do que o modelo guarda, o radar que se desconecta limpando a pista, e os quadros do Varia pelo BLE (dois carros, pista limpa, casa vazia, pacote que não é nosso) |
| `test_incident` | `model/incident.c` | 20 | sobretudo o lado do **não**: pedalar não é incidente, meio-fio não é queda, semáforo não é queda, bicicleta que volta a rolar cancela, vento não toca o alarme; e o lado do sim: a queda que abre contagem, a contagem que zera, a tecla que cancela e desarma, o carro que bate na bicicleta parada, e a amostra sem número que não pode parecer queda |
| `test_e2e_activity` | `model/activity.c` e `model/fit_encode.c` juntos | 6 | um passeio inteiro saindo de Nancy — plano, subida de 400 m, semáforo de 3 min, descida e arrancada — que vira arquivo FIT: as 2.100 mensagens de ponto, as marcas de cronômetro da pausa, a sessão com os mesmos números do modelo, as voltas automáticas a cada 5 km, o CRC do arquivo e o tamanho por ponto |
| `test_gnss_power` | `svc/gnss/gnss_power.c` | 21 | backup fora de CRS e PRC, aquisição até o primeiro fix, configuração de novo com 10 s de silêncio e reset com 30 s. Com LEAP (o MAX-M10N): LEAP no rastreio, potência plena depois de 10 épocas sem fix e volta ao LEAP depois de 60 s com fix. **Sem LEAP** (o MAX-F10S, que não tem o grupo `CFG-PM`): os mesmos cinco cenários, conferindo que a máquina nunca pede um modo de energia e mostra o rastreio como potência plena |
| `test_battery` | `svc/power/battery.c` | 10 | bateria fraca uma vez a 10 %, de novo só depois de 15 %, crítica a 0 % descarregando e nunca carregando (VBUS ou corrente do painel) |
| `test_charge` | `svc/power/charge.c` | 11 | cada estado da máquina de carga pelos bits do `BCHGCHARGESTATUS`, do `BCHGERRREASON` e do `NTCSTATUS`; VBUS acima do sol; frio e quente pausam, fresco e morno não; erro vira falha só com VBUS |
| `test_memlcd` | `modules/gnss_drivers`: `memlcd_frame.c` e `memlcd_pixel.h` | 19 | quantização (cores puras, bordas cinza pela média, luminância na Sharp), retrato do legacy e as outras rotações, bits de cada painel, quadro da Sharp com os 12.482 B do legacy e endereços de 10 bits do JDI, linhas marcadas, `pitch`, área fora da tela, trechos de linhas |
| `test_power_scheduler` | `model/power_scheduler.c` | 7 | 15 min exatos mantêm ligado, pings de posição e do rolo, ping desconhecido não conta, nova tentativa 15 min depois, volta do contador de 32 bits |
| `test_sys_fsm` | `svc/power/sys_fsm.c` + `lib/smf/smf.c` do Zephyr | 16 | Partida e Ligado, desligamento que espera cada serviço por até 5 s, System OFF com USB, auto-off por modo (CRS, PRC e DBG pela posição, FEC pelo rolo), bateria no fim, MSC, desligamento no boot, comandos durante o desligamento, energia cortada uma vez só |

```mermaid
flowchart LR
    SRC["zephyr_app/src/*.c<br/>modules/gnss_drivers"] --> EXE["test_x.exe"]
    SHIM["tests/host/shim/zephyr/<br/>kernel.h · logging/log.h · fs/fs.h · sys/util.h"] --> EXE
    SUP["tests/host/support/<br/>host_kernel · host_fs · legacy_ref.h"] --> EXE
    ZEP["lib/smf/smf.c do Zephyr<br/>(ZEPHYR_BASE)"] --> EXE
    T["tests/host/test_x.c"] --> EXE
    UNITY["Unity 2.6.1<br/>FetchContent com SHA-256"] --> EXE
    EXE --> CT["ctest"]
```

- **Shims**: log vira nada, `k_mutex` nunca bloqueia, relógio controlável (`host_uptime_set/advance`), `k_msleep` avança o relógio.
- **Falsos**: `host_fs` (sistema de arquivos em memória que pode "sumir").
- **Zephyr de verdade**: `test_sys_fsm` compila o `lib/smf/smf.c` do NCS (`ZEPHYR_BASE`, padrão `C:/ncs/v3.3.0/zephyr`), com os headers do Zephyr procurados depois dos shims (`-idirafter`) e o shim `sys/util.h` com os macros `IF_ENABLED` e `COND_CODE_1`; sem o NCS, o conjunto é pulado com um aviso.
- **Oráculo**: `support/legacy_ref.h` transcreve fórmulas do legacy com a origem.
- **Mutação**: as correções de 2026-09-18 do log foram revertidas e o teste falhou, como devia. Em 2026-09-19, com um executor que chama o `cmake` e o `ctest` direto: 6 de 6 mutações do `ui_fmt.c`, 6 de 6 da máquina de sistema, 3 de 3 do `tilt.c` e a do `power_scheduler` mortas, com os conjuntos verdes antes e depois. No driver da tela e na luz, 22 de 22 (14 do quadro e da quantização, 8 da luz), depois de reforçar o teste do `pitch`, que deixava passar a troca do `pitch` pela largura. No medidor e na bateria, 15 de 15 (9 das unidades do MAX17262, 6 da bateria), depois de acrescentar o caso de 1 %. Na máquina de carga, 7 de 7. Nas unidades do AEM10900, 10 de 10. Na distância, na potência e na fórmula do legacy, 14 de 15: a sobrevivente troca `>` por `>=` no limiar dos 15 m, que só mudaria de resultado se a distância caísse exatamente em 15,000 m de `float`, o que a fórmula não produz. No UBX e na máquina de energia do GNSS, 22 de 22 (13 dos quadros e das mensagens, 9 da máquina), depois de acrescentar o caso que separa os bits de `valid` do `UBX-NAV-PVT` e de tirar uma condição que nenhum caminho alcançava. Em 2026-09-20, com a troca para o MAX-F10S, a condição `has_leap` do `gnss_power.c` foi apagada e os casos novos falharam, como deviam.
- **Cuidado ao automatizar**: no Windows, um `bash` chamado de Python ou do `cmd` é o `bash.exe` do `System32`, o lançador do WSL, que este projeto não usa. Chame o `cmake` e o `ctest` direto, ou o Git Bash pelo caminho completo, e confira o código de saída e se o filtro achou o conjunto.
- Compilador: MinGW-w64 GCC 15.2 no Windows; o mesmo CMake funciona com o GCC do Linux.
- Os testes de ztest do Zephyr (`native_sim`, `unit_testing`) só rodam em Linux e não são usados.

## Renderizador de telas

A interface da placa nova ([18](18-interface-telas.md)) compila no PC com o LVGL do NCS e o GCC do PC, e o `ui_render` desenha cada tela nos dois temas, como o painel mostra.

```sh
python tools/ui/render_screens.py              # compila, desenha, confere e gera docs/img/telas-lvgl
python tools/ui/render_screens.py --no-build   # pula o CMake e usa o ui_render já compilado
```

| Peça | Papel |
|---|---|
| `zephyr_app/tests/ui/CMakeLists.txt` | o LVGL do NCS (`C:/ncs/v3.3.0/modules/lib/gui/lvgl`, ou a variável `NCS_LVGL`) com o `lv_conf.h` da pasta, e a interface de `src/ui` com `-Werror` |
| `zephyr_app/tests/ui/ui_samples.c` | dados de exemplo: pedal com 0, 1 e 2 segmentos, GNSS procurando, rolo, sensores e percursos |
| `zephyr_app/tests/ui/ui_render.c` | monta cada tela, desenha num quadro RGB565 de 240 × 400, quantiza pelas regras do driver da tela (`memlcd_pixel.h`) e grava PPM; confere cores, textos e navegação; mede o heap do LVGL e a pilha das telas |
| `tools/ui/render_screens.py` | CMake, `ui_render`, PPM para PNG, as folhas por grupo e tema e uma imagem por tela e tema em `docs/telas/` |
| `tools/ui/font_gen.py` | as fontes de 1 bit de `zephyr_app/src/ui/fonts` (DejaVu Sans, sem suavização) |

- O `ui_render` sai com erro quando aparece cor no tema preto e branco, quando um texto sai da caixa, quando a navegação não chega à tela esperada ou quando uma ação não sai.
- Resultado em 2026-09-19: 29 telas em 2 temas, 58 quadros, 0 problemas; 23,9 KB de heap do LVGL no pico e 7.359 B de pilha, com ponteiros de 64 bits (x86-64).
- A pilha é medida pintando a pilha antes de desenhar e procurando o ponto mais fundo que mudou; o `snap()` mede antes das próprias conferências e da gravação do arquivo e pinta de novo. No PC, com ponteiros de 8 B e os 32 B de sombra por chamada do Windows, é uma cota superior da pilha do Cortex-M33 ([05](05-arquitetura-zephyr.md#pilhas)).
- O aviso do LVGL sobre as conferências de objeto e de estilo (`LV_USE_ASSERT_OBJ`, `LV_USE_ASSERT_STYLE`) é esperado: estão ligadas de propósito, para pegar uso errado da API.
- O clangd usa a base de compilação de `build/ui` pelos `.clangd` de `zephyr_app/tests/ui` e `zephyr_app/src/ui`, depois da primeira execução.
- O teste confere o desenho, não o painel: tempo de SPI, COM, luz e leitura ao sol ficam para a bancada.

## Simulador TDD do legacy

`tools/TDD` compila o firmware original inteiro (C/C++ com `-DTDD`) como executável de PC e simula o aparelho:

- **Entradas:** GPS gerado a partir de `GPX_simu.csv` (sem fix nos primeiros 10 s, depois RMC/GGA/VTG, depois posição por LNS), barômetro e acelerômetro sintéticos com ruído, HRM e FE-C falsos, cartão SD mapeado para a pasta `tools/TDD/DB/`, botões por um roteiro de tempo ou pelo teclado.
- **Tela:** framebuffer enviado por TCP (porta 8080, 12.004 bytes por quadro) para o `LS027simulator.jar` (Java), que mostra o LCD e salva capturas. As imagens de `docs/img/` vieram dele.
- **Testes do legacy** (`unit_testing.cpp`, rodam no início de cada execução):

| Teste | Critério | Equivalente no port |
|---|---|---|
| `test_power_zone` | FTP 256: máximo 1831 ± 2 s | `test_power_zone` (regras, não o mesmo cenário) |
| `test_score` | suffer score ≈ 66,46 ± 4 | `test_suffer_score` (regras) |
| `test_liste` | avanço num segmento de 14 ± 0,1 s | a portar junto com a correção do `liste_points` |
| `test_rollover` | tempo do FE-C com virada de 8 bits | a portar com o FTMS |
| `test_projection`, `test_fusion`, `test_fram`, `test_lsq` | projeção, fusão (só loga), configurações, regressão linear | parcialmente cobertos |

- **Não compila neste repositório:** a `CMakeLists.txt` de host do upstream não veio, `legacy/` e `libraries/` foram separados, e os submódulos `ant_profiles` e `ble_services` estão vazios. O `tools/TDDW` (MinGW) tem shims para Windows, mas herda os mesmos problemas.
- Útil como referência para um simulador do port: replay de GPX em NMEA (corrigindo a data fixa e o hemisfério do gerador) e a fonte de posição simulada.

## Outras ferramentas herdadas

| Ferramenta | Situação |
|---|---|
| `tools/zpm` | `LNS.js` (Zwift → `$LOC`, obsoleto desde a criptografia do Zwift em 2022), `LNS_test.js`, `getGPX.js` (`$QRY`), `gpx_convert.js` (CSV → GPX); `serialport` 8 e `cap` precisam de rebuild para o Node 22; incompatíveis com o port enquanto não houver comandos |
| `tools/MMD` | monitor mode do J-Link para depurar sem derrubar o BLE; no Zephyr o equivalente é `CONFIG_CORTEX_M_DEBUG_MONITOR_HOOK` + `CONFIG_SEGGER_DEBUGMON` |
| `tools/jumper` | pinos da placa v1 para o Jumper Virtual Lab; ferramenta não instalada |

## Bibliotecas e licenças

Nenhuma pasta de `libraries/` é compilada pelo port; ele reimplementa o que precisa.

| Pasta | Origem | Licença | No port |
|---|---|---|---|
| `AdafruitGFX` | Adafruit GFX + Print (Arduino) + fontes | BSD; `Print.cpp` LGPL-2.1; fonte `Tiny3x3a` **CC BY-NC-SA 3.0** (sem uso) | substituída pelo LVGL do NCS (a interface em paisagem que a reimplementava saiu em 2026-09-19) |
| `TinyGPSPlus` | TinyGPS++ (Mikal Hart) | LGPL-2.1+ | substituída pela API de GNSS do Zephyr (o parser próprio do port saiu em 2026-09-19) |
| `rtt`, `sysview` | SEGGER | estilo BSD da SEGGER | sem uso |
| `task_manager`, `SST/app_sdcard.c` | nRF5 SDK | Nordic 5 cláusulas | substituídas pelo Zephyr |
| `utils/WString` | Arduino | LGPL-2.1 | sem uso |
| `kalman`, `filters`, `AltiBaro`, `GlobalTop`, `hardfault`, `jscope`, `komoot`, `utils`, `VParser`, `SST` (resto) | autor do stravaV10 | a do repositório original: CC BY-NC 4.0 | reimplementadas em parte (`kalman_altitude`, `udmatrix`, `gps_epo`, `crash_recovery`) |
| `ant_profiles`, `ble_services` | submódulos do autor | — | **vazias** nesta cópia |
| `tools/TDD/timer` | Teunis van Beelen | **GPL-2.0** | sem uso |
| `tools/TDD/sd/fatfs` | FatFs R0.12b (ChaN) | licença do FatFs | sem uso |

O código do stravaV10 é CC BY-NC 4.0 (atribuição e uso não comercial) e o port deriva dele: a licença do projeto é uma [decisão pendente do dono](10-status-do-port.md#decisões-do-dono).
