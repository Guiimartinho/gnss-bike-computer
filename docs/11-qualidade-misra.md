# Qualidade e MISRA

Regras de código do port, o que a análise estática encontra hoje e o que foi herdado da revisão MISRA do legacy feita em novembro de 2025 ([arquivada](historico/2025-11/01_Code_Review_MISRA.md), com as tabelas por arquivo do legacy).

**Nesta página:** [Regras do port](#regras-do-port) · [Estado atual](#estado-atual) · [Análise estática](#análise-estática) · [Herança do legacy](#herança-do-legacy) · [Próximos passos](#próximos-passos)

## Regras do port

| Regra | Referência | Como aplicar |
|---|---|---|
| Variáveis sempre inicializadas | MISRA C:2012 9.1 | inclusive estruturas locais passadas por ponteiro |
| Tipos de largura fixa, sufixo `U` em constantes sem sinal | 10.1, 7.2 | `uint32_t`, `8U`; nada de `int` para contagem |
| Sem conversão que perca valor sem saturar | 10.3 | `(int8_t)(100.0f * slope)` precisa de `CLAMP` antes |
| Sem igualdade em `float` | 13.3 | comparar com tolerância |
| `default` em todo `switch` | 16.4 | — |
| Retorno de função tratado ou descartado com `(void)` | 17.7 | — |
| Sem alocação dinâmica depois do boot | padrão do projeto | buffers estáticos dimensionados; `k_malloc` só se inevitável |
| Nada pesado em ISR | Zephyr | ver [05](05-arquitetura-zephyr.md#regras-de-concorrência) |
| Estado compartilhado com um dono e trava | Zephyr | idem |
| Constantes nomeadas | 7.2 e legibilidade | as do algoritmo com o nome do legacy e a origem em comentário |

## Estado atual

| Item | Situação em 2026-09-22 |
|---|---|
| Flags do compilador | `-Wall -Wextra -Werror=implicit-function-declaration -Werror=return-type -Wno-unused-parameter` (`zephyr_app/CMakeLists.txt`); sem `-Werror` geral |
| Avisos no build | 0 nos dois alvos (`nrf54lm20dk/nrf54lm20a/cpuapp` e `gnssbike/nrf54lm20a/cpuapp`); com `ANT=1`, só o do símbolo obsoleto do `sdk-ant` |
| Testes de host | 53 conjuntos, 714 casos, com `-Werror` ([12](12-ferramentas-testes.md)) |
| cppcheck 2.20 | achados reais abaixo; os `syntaxError` dos `ble_*.c` são falsos positivos das macros do Zephyr |
| MISRA formal | não verificado: não há ferramenta MISRA configurada |
| Formatação | `.editorconfig` na raiz (4 espaços, LF, 100 colunas no C); sem `.clang-format` ainda |

## Análise estática

```sh
cppcheck --enable=warning,style,performance,portability --std=c11 --inline-suppr --quiet \
  --suppress=missingIncludeSystem --suppress=missingInclude --suppress=unusedFunction \
  "-DDT_NODE_HAS_STATUS(n,s)=1" "-DDT_ALIAS(a)=a" \
  -I zephyr_app/include zephyr_app/src/app zephyr_app/src/svc zephyr_app/src/model zephyr_app/src/rf
```

As duas definições `-D` dão ao cppcheck os macros de devicetree dos `#if` dos serviços; sem elas, cada `#if DT_...` vira um `syntaxError`. Elas não cobrem tudo: `DT_NODE_HAS_STATUS_OKAY` e `DT_NODE_HAS_COMPAT` continuam sem definição e ainda rendem um `syntaxError` cada em `svc/gnss/gnss_svc.c`, `svc/power/power_svc.c`, `svc/ui/ui_input.c` e `svc/ui/ui_svc.c`. A interface (`src/ui`) passa com o LVGL no caminho ([12](12-ferramentas-testes.md#renderizador-de-telas)).

Achados da execução de 2026-09-22 (cppcheck 2.20.0), fora os 7 `syntaxError` falsos positivos (os 4 de devicetree acima e os 3 das macros `BT_GATT_*` em `src/rf/ble/ble_lns.c`, `ble_manager.c` e `ble_nus.c`):

| Achado | Onde | Situação |
|---|---|---|
| shift de valor negativo (`grade_ftms >> 8`, `int16_t`) | `src/rf/ble_fec_client.c:449` | aberto |
| condição sempre verdadeira (`i < ALERT_COUNT`, com `UI_ALERTS` já menor) | `src/svc/model/model_ui.c:515` | aberto, inofensivo |
| ponteiros que podiam ser `const`: 8 `constParameterPointer` (`model/user_settings.c:162` e os `..._on_disconnect()` de sete clientes BLE) e 2 `constParameterCallback` (`ble_fec_client.c:163` e `:164`, num ponteiro de função do GATT) | vários | estilo |

A variável possivelmente não inicializada `seg_dists` saiu: ela é declarada com `= {0}` em `src/model/segment.c:710`.

Também disponível nesta máquina e ainda não aplicado ao projeto: o analisador do GCC pelo Zephyr (`-DZEPHYR_SCA_VARIANT=gcc`), o `checkpatch.pl` do Zephyr, o clang-tidy do LLVM (sobre o `compile_commands.json` dos testes de host) e o addon `misra.py` do cppcheck.

## Herança do legacy

A revisão de novembro de 2025 contou no legacy 12 violações críticas, 28 altas e 45+ médias. As mais importantes para o port:

| Problema do legacy | Situação no port |
|---|---|
| `String` do Arduino e alocação dinâmica (`std::vector`, `new`, `std::list`) | eliminados: C puro com buffers estáticos |
| mutex do LCD por espera ativa com condição de corrida | eliminado: a tela é só da thread `ui`, e o driver novo (`modules/gnss_drivers/drivers/display/memlcd_frame.c`, com `test_memlcd`) não tem trava nenhuma. Não testado em painel |
| variáveis `static` de função guardando estado (`Attitude.cpp`) | parcialmente: o port concentra estado em `static` de arquivo, escrito só pela thread `model`; as outras recebem cópias pelo zbus |
| conversões com perda sem saturação | continuam: `attitude.c:238` (a rampa em `int8_t`) e `attitude.c:544` (os segundos em movimento em `uint16_t`) |
| funções longas (`computeFusion`, `run_internal`, `majPerformance`) | continuam longas em partes do modelo (`segment.c`, `attitude.c`) |
| sem testes | 714 casos em 53 conjuntos de host, cobrindo vetores, listas e segmentos, distância, Kalman e inclinação, zonas de potência e de FC, suffer score, log e recuperação de falha, desligamento automático, as máquinas de sistema e de modo, os quadros UBX e a energia do GNSS, o arquivo FIT, percursos e perfil, mapa, subidas, voltas, radar, queda, treino, alertas, potência (estimativa e métricas de Coggan), LNS, Komoot, `$QRY` e o resto dos comandos, a tela e a luz, os medidores de carga e a formatação dos números da interface; mais o renderizador das telas no PC |

## Próximos passos

1. `.clang-format` do `zephyr_app/` baseado no do Zephyr (4 espaços, 100 colunas, chaves estilo Linux, `SortIncludes: Never`) e `DisableFormat: true` em `legacy/`, `libraries/` e `tools/` herdados.
2. Compilar com `CONFIG_COMPILER_WARNINGS_AS_ERRORS=y`, agora que o build não tem aviso.
3. cppcheck com o `compile_commands.json` do build, para eliminar os falsos positivos das macros.
4. Saturação nas conversões apontadas e `CONFIG_ASSERT=y` num `debug.conf`.
