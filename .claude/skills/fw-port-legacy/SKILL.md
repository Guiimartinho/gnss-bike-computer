---
name: fw-port-legacy
description: Portar ou corrigir um módulo do stravaV10 original (legacy/, C/C++ sobre nRF5 SDK) para o port Zephyr do GNSS Bike Computer com fidelidade - localizar o original, mapear classes C++ para módulos C, manter constantes e fórmulas, testar contra o legacy como oráculo e registrar diferenças. Use ao implementar qualquer item do roteiro de docs/10-status-do-port.md ou ao investigar por que o port se comporta diferente do original.
---

# Portar do legacy

## Mapa legacy → port

| Legacy | Port |
|---|---|
| `main.cpp`, `source/Model.cpp` (boot, tasks, globais) | `src/app/main.c` (boot) e os serviços de `src/svc/`, cada um com a sua thread |
| `source/model/Boucle*.cpp` | `src/svc/model/model_svc.c` (máquina de modos e o ciclo por evento) |
| `source/model/Attitude.cpp` | `src/model/attitude.c`, `kalman_altitude.c`, `udmatrix.c`, `crash_recovery.c`, `sd_logger.c` |
| `source/model/Locator.cpp` | `src/model/locator.c`, `loc_source.c` |
| `PowerZone`, `SufferScore`, `RRZone`, `UserSettings` | `power_zone.c`, `suffer_score.c`, `rr_zone.c`, `user_settings.c` |
| `source/routes/Vecteur`, `Points` | `src/model/vecteur.c` |
| `source/routes/ListePoints`, `Segment`, `Parcours` | `liste_points.c`, `segment.c`, `parcours.c` |
| `source/sensors/GPSMGMT.cpp`, TinyGPS++ | `src/svc/gnss/gnss_svc.c` sobre a API de GNSS do Zephyr |
| `source/sensors/fxos.cpp`, `bme280.c`, `AltiBaro`, `STC3100.cpp` | `src/svc/sensors/sensors_svc.c` (API de sensores) e `tilt.c`; a bateria no serviço de energia |
| `source/scheduling/power_scheduler.cpp` | `src/model/power_scheduler.c` e a máquina de sistema (`src/svc/power/sys_fsm.c`) |
| `source/vue/*`, `source/display/*`, Adafruit GFX | `src/ui/*` (LVGL) e o driver de tela do passo da interface |
| `rf/*` (ANT+ e BLE) | `src/rf/*` (só BLE) |
| `source/sd/*`, `source/usb/*` | `sd_logger.c` e `src/svc/storage/storage_svc.c` (FatFs do Zephyr); USB no passo da USB |
| `custom_board_v3.h` | `boards/nrf52840dk_nrf52840.overlay` |

Glossário dos nomes em francês: `docs/04-arquitetura-legacy.md#glossário`.

## Passo a passo

1. **Leia o original inteiro**, inclusive quem chama (grep em `legacy/`), e anote: entradas, saídas, estado, constantes (com `arquivo:linha`), taxa de chamada e contexto (task, ISR, `app_scheduler`).
2. **Confira `docs/06-algoritmos.md` e `docs/10-status-do-port.md`**: o item pode já ter diferenças catalogadas.
3. **Escreva o teste antes** (skill `fw-testes`): as regras do original viram casos; fórmulas curtas vão para `tests/host/support/legacy_ref.h` como oráculo.
4. **Porte em C** no estilo do `zephyr_app`: 4 espaços, `snake_case`, prefixo do módulo nas funções públicas, `app_err_t` nos retornos, Doxygen curto nos headers, constantes `#define` com sufixo `U`/`f`, sem alocação dinâmica. Classe C++ vira `struct` + funções `modulo_*`; `std::list` vira array estático com contagem; `String` vira `char[]` com `snprintf`.
5. **Taxa e contexto iguais ao original**: o legacy roda o modelo uma vez por localização; filtros com constantes de tempo (drift τ = 800/801, contadores de pontos) dependem disso.
6. **Ligue ao fluxo**: um módulo sem chamador não conta como portado. Confira no `zephyr.map` que ele não caiu em "Discarded input sections".
7. **Não porte os defeitos do legacy** listados nos documentos (por exemplo `Vecteur::project`, o `ind_P1` velho, o rtime com divisão inteira); se corrigir algo que o original fazia errado, registre a decisão.
8. **Verifique**: testes de host (e mutação no ponto novo), build sem aviso novo, cppcheck nos arquivos tocados.
9. **Atualize** a matriz e os defeitos em `docs/10-status-do-port.md`, as fórmulas em `docs/06-algoritmos.md`, a seção "Estado" do `CLAUDE.md` e o `CHANGELOG.md`.

## Armadilhas de fidelidade já vistas

| Onde | O que aconteceu |
|---|---|
| `liste_points.c` | índice 0 virou o ponto mais antigo (no legacy é o mais recente) e a ordem dos segmentos inverteu |
| `udmatrix.c` | `ones()` virou identidade e o `bound()` perdeu o valor absoluto: o Kalman deixou de estimar α0 |
| `gps_mgmt.c` (removido) | o modelo rodava por sentença NMEA, não por época (corrigido em 2026-09-18; hoje a API de GNSS dá uma posição por época) |
| `boucle.c` (removido) | as zonas recebiam só amostras acima de zero e a potência estimada no CRS; o legacy alimenta a potência só no FEC e o score em todo ciclo (corrigido em 2026-09-19) |
| `attitude.c` | fórmula de potência trocada por outra (88 W contra 147 W a 30 km/h) |
| `segment.c` | `DIST_ALLOC` 3000 m em vez de 300 m, `MARGE_DESACT` aplicada ao ponto errado |
| constantes do gap analysis antigo | valores errados (`PSCAL_LIM` 0,85, `DIST_ALLOC` 250, `HISTO_POINT_SIZE` 40): use sempre o código do legacy, nunca o documento arquivado |
