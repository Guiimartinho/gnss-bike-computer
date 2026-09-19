---
name: fw-vue
description: Trabalhar com a interface do GNSS Bike Computer no port Zephyr - driver da tela (JDI LPM027M128B e Sharp LS027 em retrato, quantização, linhas que mudaram, COM), LVGL e as telas de src/ui, thread ui, teclas curtas e longas, luz, notificações. Use ao mexer em zephyr_app/src/ui/, src/svc/ui/, modules/gnss_drivers/ ou no tratamento de teclas.
---

# Interface

Referência: `docs/08-interface.md` (telas do legacy, imagens em `docs/img/`, estado do port e defeitos).

## Display

- Placa nova: **JDI LPM027M128B** (8 cores, refletivo, sem luz; decisão do dono em 2026-09-19), com a Sharp LS027B7DH01A de reserva no mesmo conector. Driver próprio em `zephyr_app/modules/gnss_drivers` (`docs/05-arquitetura-zephyr.md#tela`): compatíveis `jdi,lpm027m128b`, `jdi,lpm027m128c` e `sharp,ls027b7dh01`; quadro no formato do fio, só as linhas que mudaram, COM pelo EXTCOMIN (timer) ou pelo SPI, a quantização de `memlcd_pixel.h` (a mesma do renderizador de host).
- O JDI quer os sinais no nível do VDD dele (3,0 V); DISP baixo mostra preto. Protocolo na ficha do LPM027M128B (6.2, 6.8, 8).
- V3 e legacy: LS027B7DH01, 400 × 240, 1 bit, sem luz, CS ativo alto, SPI a 2 MHz LSB primeiro; buffer de 12.482 B `[comando][endereço][50 B][dummy] × 240 + [dummy]`; VCOM alternado pelo bit M1 (`legacy/drivers/lcd/ls027.c`). Na V3, EXTMODE e EXTCOMIN vão ao GND e o DISP ao VCC (R13, R16, R17, C43).
- **O aparelho é retrato** (240 × 400; decisão do dono em 2026-09-18): o legacy usa `setRotation(3)` e `drawPixel(x, y)` → físico `(y, 239 − x)`.

## Regras

1. **Só a thread `ui` desenha** (`src/svc/ui/ui_svc.c`). Os outros serviços publicam (`model_state`, `notif`, `mode`, `system_state`); ninguém chama o LVGL de fora dela. Ações das telas viram `system_cmd`, menos luz e tema, que a `ui` guarda em `ui/prefs`.
2. **Nada de desenho fora da tela**: recorte antes de converter float para inteiro.
3. **Números pelo `ui_fmt.c`**, que trunca como o `_fmkstr` do legacy; `snprintf` com float arredonda.
4. **Teclas** pelo subsistema de entrada: `gpio-keys` e o nó `longpress` (`zephyr,input-longpress`, 1 s; curto LEFT/ENTER/RIGHT, longo HOME/MENU/END), publicadas no `chan_input` por `src/svc/ui/ui_input.c`; na placa nova o centro chega também pelo nPM1300.
5. **Energia**: a tela redesenha a cada época do GNSS ou botão, não num período fixo; a thread dorme até o próximo timer do LVGL, uma mensagem ou 1 s.
6. **Pilha**: a `ui` tem 6 KB para ~4,7 KB medidos; o desenho é recursivo (624 B por nível da árvore de objetos). Objeto aninhado a mais, log do LVGL ou widget novo pedem nova medição (skill `fw-threads`).
7. **LVGL igual nos dois lados**: opção nova no `prj.conf` entra também em `zephyr_app/tests/ui/lv_conf.h`, e o renderizador precisa continuar dando as mesmas imagens.

## Interface da placa nova (LVGL)

A interface da placa nova está em `zephyr_app/src/ui` e `zephyr_app/include/ui` (`docs/18-interface-telas.md`): C puro sobre o LVGL 9.5 do NCS, sem Zephyr, testada no PC e compilada no firmware desde 2026-09-19.

1. **Legacy primeiro:** antes de mexer numa tela, leia a função original (`VueCRS.cpp`, `VuePRC.cpp`, `VueFEC.cpp`, `VueGPS.cpp`, `VueDebug.cpp`, `Menuable.cpp`, `MenuObjects.cpp`) e cite a linha no comentário. Campo, ordem, formato e limite seguem o legacy; diferença nova entra na tabela "Diferenças para o legacy" do `docs/18`.
2. **Dados só pelo retrato:** a tela lê `ui_ctx.m` (`ui_model_t`), nunca o modelo nem drivers; pedidos saem por `ui_action()`. Mapas chegam projetados em milésimos da janela (`UI_PM`).
3. **Nada só por cor:** o tema preto e branco precisa dizer o mesmo, pelo sinal, pela palavra ou pela forma.
4. **Textos** na tabela de `ui_text.c` (pt e en); **números** por `ui_fmt.c`, com os formatos do legacy.
5. **Fontes** só por `tools/ui/font_gen.py`: caractere novo entra na lista `FULL` ou `NUM` e as fontes são geradas de novo.
6. **Uma thread só** chama `ui_*` (o LVGL não é reentrante).
7. O LVGL 9.5 suaviza círculos, linhas inclinadas e cantos sem opção de desligar: vale o quadro depois da quantização (regra no `docs/18`, seção Implementação).

## Verificar

- Build sem aviso nos dois DKs; `test_memlcd` e `test_backlight` verdes se mexeu no driver ou na luz.
- Interface nova: `python tools/ui/render_screens.py` precisa terminar com `0 problems`; olhe as folhas de `docs/img/telas-lvgl/` nos dois temas. Tela nova ganha um `snap()` e, se tiver navegação, `expect_screen` e `expect_action` em `zephyr_app/tests/ui/ui_render.c`, e entra numa folha de `tools/ui/render_screens.py`.
- A interface em paisagem do port (`src/vue`) saiu em 2026-09-19. Sem painel ligado, a tela só se vê no PC (`render_screens.py`); o simulador do legacy (`tools/TDD` + `LS027simulator.jar`) não compila aqui.
- Diga o que não foi visto na tela de verdade.
