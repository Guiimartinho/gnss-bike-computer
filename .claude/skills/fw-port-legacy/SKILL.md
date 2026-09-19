---
name: fw-port-legacy
description: Portar ou corrigir um módulo do stravaV10 original (legacy/, C/C++ sobre nRF5 SDK) para o port Zephyr do GNSS Bike Computer com fidelidade - localizar o original, mapear classes C++ para módulos C, manter constantes e fórmulas, testar contra o legacy como oráculo e registrar diferenças. Use ao implementar qualquer item do roteiro de docs/10-status-do-port.md ou ao investigar por que o port se comporta diferente do original.
---

# Portar do legacy

## Mapa legacy → port

| Legacy | Port |
|---|---|
| `main.cpp`, `source/Model.cpp` (boot, tasks, globais) | `src/main.c` (threads) e estado `static` nos módulos |
| `source/model/Boucle*.cpp` | `src/model/boucle.c` (e `zwift.c` para o `BoucleZwift`) |
| `source/model/Attitude.cpp` | `src/model/attitude.c`, `kalman_altitude.c`, `udmatrix.c`, `crash_recovery.c`, `sd_logger.c` |
| `source/model/Locator.cpp` | `src/model/locator.c`, `loc_source.c` |
| `PowerZone`, `SufferScore`, `RRZone`, `UserSettings` | `power_zone.c`, `suffer_score.c`, `rr_zone.c`, `user_settings.c` |
| `source/routes/Vecteur`, `Points` | `src/model/vecteur.c` |
| `source/routes/ListePoints`, `Segment`, `Parcours` | `liste_points.c`, `segment.c`, `parcours.c` |
| `source/sensors/GPSMGMT.cpp`, TinyGPS++ | `src/drivers/gps/gps_mgmt.c`, `nmea_parser.c`, `gps_epo.c` |
| `source/sensors/fxos.cpp`, `bme280.c`, `AltiBaro`, `STC3100.cpp` | `src/drivers/sensors/fxos.c`, `baro.c`, `stc3100.c` |
| `source/vue/*`, `source/display/*`, Adafruit GFX | `src/vue/*`, `src/drivers/lcd/ls027.c` |
| `rf/*` (ANT+ e BLE) | `src/rf/*` (só BLE) |
| `source/sd/*`, `source/usb/*` | `sd_logger.c`, `utils/fs_stubs.c`, `src/usb/*` (fora do build) |
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
| `gps_mgmt.c` | o modelo rodava por sentença NMEA, não por época (corrigido em 2026-09-18) |
| `attitude.c` | fórmula de potência trocada por outra (88 W contra 147 W a 30 km/h) |
| `segment.c` | `DIST_ALLOC` 3000 m em vez de 300 m, `MARGE_DESACT` aplicada ao ponto errado |
| constantes do gap analysis antigo | valores errados (`PSCAL_LIM` 0,85, `DIST_ALLOC` 250, `HISTO_POINT_SIZE` 40): use sempre o código do legacy, nunca o documento arquivado |
