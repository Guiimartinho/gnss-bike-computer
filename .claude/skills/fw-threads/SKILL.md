---
name: fw-threads
description: Trabalhar com threads, interrupções, pilhas, prioridades e concorrência no port Zephyr do GNSS Bike Computer - criar ou mudar threads, mover trabalho para fora de ISR, dimensionar e medir pilhas com CONFIG_STACK_USAGE, proteger estado compartilhado, callbacks do BT. Use em qualquer mudança em zephyr_app/src/main.c, em callbacks de ISR ou do Bluetooth, ou quando algo roda em mais de uma thread.
---

# Threads, ISR e pilhas

## Mapa de execução

| Contexto | Prioridade | Pilha | Período | Pode |
|---|---|---|---|---|
| ISR da UARTE1 (GPS) | IRQ | pilha de ISR 2048 B | por byte | só `ring_buf_put` |
| outras ISRs (GPIO, SPI, I2C do Zephyr) | IRQ | idem | — | só sinalizar |
| thread RX do BT (callbacks de conexão, GATT, notificações) | cooperativa | do Zephyr | — | copiar o dado e sair |
| `main_loop` (`main_thread`) | 5 | 4096 B | 100 ms | sob `model_lock()`: botões, GPS, sensores, modelo, Kalman, log; fora dela: bateria no BLE quando muda |
| `display` | 7 | 2048 B | 50 ms (quadro a cada 250 ms) | compor o quadro sob `model_lock()`, enviar o LCD fora dela |

O legacy era cooperativo (nada preemptava uma task no meio de uma estrutura); aqui tudo é preemptivo. Estado compartilhado sem trava é defeito.

## Regras

1. **ISR não processa.** Copie para `ring_buf` (produtor único e consumidor único dispensam trava) ou `k_msgq` e processe numa thread. Proibido em ISR: `k_mutex_lock`, `fs_*`, `snprintf` com float, trigonometria, log com float, chamadas ao modelo. Modelo: `src/hal/hal_uart.c` (ISR só enfileira; `hal_uart_process()` na `main_loop`).
2. **Um dono por estrutura.** O modelo (`attitude`, `boucle`, `segment`, `gps_data`, os caches dos drivers de sensor e o estado de página e menu da `vue`) é escrito só pela `main_loop`, dentro de `model_lock()`/`model_unlock()` (`include/model/model_lock.h`). A `display` só lê o modelo com a trava, na composição do quadro, e a solta antes do SPI. Não crie outra thread que escreva o modelo: mande os dados para a `main_loop` (fila ou `ring_buf`). Segure a trava só pelo passo do modelo: nada de `k_msleep`, espera de rádio ou notificação GATT dentro dela.
3. **Callbacks do BT** copiam para uma estrutura com trava ou para uma fila e retornam; não chamam o modelo, não seguram mutex que a interface segura, não dormem.
4. **Sem `k_msleep` longo** em thread que precisa responder (a `main_loop` precisa drenar a UART a cada 100 ms: o ring de 512 B aguenta ~530 ms a 9600 baud).
5. **Prioridade:** dados que chegam de fora (GPS) acima da interface. Não crie duas threads com a mesma prioridade sem motivo: o `CONFIG_TIMESLICING` só age entre iguais.

## Nova thread ou cadeia pesada

1. Defina pilha e prioridade como constantes em `src/main.c` com comentário do porquê.
2. Meça a pilha: build com `CONFIG_STACK_USAGE=y` numa pasta separada e some a cadeia mais funda.

   ```sh
   source tools/fw/ncs_env.sh
   cd zephyr_app
   python -m west build -p always -b nrf52840dk/nrf52840 -d build_su --no-sysbuild . -- -DCONFIG_STACK_USAGE=y
   find build_su/CMakeFiles/app.dir -name "*.su" -exec cat {} + | sort -t$'\t' -k2 -rn | head -20
   ```

   Some os quadros da cadeia (o `.su` dá o quadro de cada função), acrescente ~200 B para o empilhamento de exceção com FPU e o `snprintf` com float (~400 a 500 B, fora dos `.su`). Deixe pelo menos 1 KB de folga.
3. Referência medida em 2026-09-18: Kalman na `main_loop` ~2.200 B (`measurement_update` sozinho tem 1.672 B), GPS e log ~1,2 KB, tela < 1 KB.
4. Apague a pasta `build_su` no fim.
5. Na placa, confirme com `CONFIG_THREAD_ANALYZER=y` (imprime o uso real das pilhas).

## Falhas

- `CONFIG_MPU_STACK_GUARD=y`: estouro de pilha vira falha fatal na hora.
- `CONFIG_RESET_ON_FATAL_ERROR=y` (lib `fatal_error` do NCS): a falha é logada e o aparelho reinicia, em vez de travar.
- Sem watchdog ainda: quando entrar, alimente só na `main_loop` depois de um ciclo completo, com timeout acima do pior caso (Kalman + GPS + flush do log).

## Antes de terminar

- Build sem aviso novo e testes de host verdes (skills `fw-build` e `fw-testes`).
- Se mudou quem chama o quê, atualize a tabela de threads e o fluxo de dados em `docs/05-arquitetura-zephyr.md`.
