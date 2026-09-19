---
name: fw-vue
description: Trabalhar com a interface do GNSS Bike Computer no port Zephyr - LCD Sharp LS027 (framebuffer, VCOM, orientação retrato), primitivas e fontes, telas (cadrans CRS, PRC com mapa, FEC, GPS, debug), menu, botões curtos e longos, notificações e LED. Use ao mexer em zephyr_app/src/vue/, src/drivers/lcd/ls027.c ou no tratamento de botões.
---

# Interface

Referência: `docs/08-interface.md` (telas do legacy, imagens em `docs/img/`, estado do port e defeitos).

## Display

- Placa nova: Sharp LS027B7DH01A (1 bit) ou JDI LPM027M128C (8 cores) no mesmo conector, em retrato, pelo driver próprio do passo da interface (`docs/18-interface-telas.md#framework`): quadro na orientação do painel, só as linhas que mudaram, EXTCOMIN por PWM, a quantização da interface.
- V3 e legacy: LS027B7DH01, 400 × 240, 1 bit, sem luz, CS ativo alto, SPI a 2 MHz LSB primeiro; buffer de 12.482 B `[comando][endereço][50 B][dummy] × 240 + [dummy]`; VCOM alternado pelo bit M1 a cada quadro (`legacy/drivers/lcd/ls027.c`).
- **O aparelho é retrato** (240 × 400; decisão do dono em 2026-09-18): o legacy usa `setRotation(3)` e `drawPixel(x, y)` → físico `(y, 239 − x)`.

## Regras

1. **Só a thread `ui` desenha.** Os outros serviços publicam (`model_state`, `notif`); ninguém chama o LVGL de fora dela.
2. **Nada de desenho fora da tela**: recorte antes de converter float para inteiro.
3. **Números pelo `ui_fmt.c`**, que trunca como o `_fmkstr` do legacy; `snprintf` com float arredonda.
4. **Teclas** pelo subsistema de entrada (`gpio-keys` e `zephyr,input-longpress`; na placa nova o centro chega pelo nPM1300), publicadas no `chan_input`.
5. **Energia**: a tela redesenha a cada época do GNSS ou botão, não num período fixo.

## Interface da placa nova (LVGL)

A interface da placa nova está em `zephyr_app/src/ui` e `zephyr_app/include/ui` (`docs/18-interface-telas.md`): C puro sobre o LVGL 9.5 do NCS, sem Zephyr, testada no PC antes de ir para o firmware. Substitui a `src/vue` quando o driver da tela e a thread existirem.

1. **Legacy primeiro:** antes de mexer numa tela, leia a função original (`VueCRS.cpp`, `VuePRC.cpp`, `VueFEC.cpp`, `VueGPS.cpp`, `VueDebug.cpp`, `Menuable.cpp`, `MenuObjects.cpp`) e cite a linha no comentário. Campo, ordem, formato e limite seguem o legacy; diferença nova entra na tabela "Diferenças para o legacy" do `docs/18`.
2. **Dados só pelo retrato:** a tela lê `ui_ctx.m` (`ui_model_t`), nunca o modelo nem drivers; pedidos saem por `ui_action()`. Mapas chegam projetados em milésimos da janela (`UI_PM`).
3. **Nada só por cor:** o tema preto e branco precisa dizer o mesmo, pelo sinal, pela palavra ou pela forma.
4. **Textos** na tabela de `ui_text.c` (pt e en); **números** por `ui_fmt.c`, com os formatos do legacy.
5. **Fontes** só por `tools/ui/font_gen.py`: caractere novo entra na lista `FULL` ou `NUM` e as fontes são geradas de novo.
6. **Uma thread só** chama `ui_*` (o LVGL não é reentrante).
7. O LVGL 9.5 suaviza círculos, linhas inclinadas e cantos sem opção de desligar: vale o quadro depois da quantização (regra no `docs/18`, seção Implementação).

## Verificar

- Build sem aviso.
- Interface nova: `python tools/ui/render_screens.py` precisa terminar com `0 problems`; olhe as folhas de `docs/img/telas-lvgl/` nos dois temas. Tela nova ganha um `snap()` e, se tiver navegação, `expect_screen` e `expect_action` em `zephyr_app/tests/ui/ui_render.c`, e entra numa folha de `tools/ui/render_screens.py`.
- A interface em paisagem do port (`src/vue`) saiu em 2026-09-19. Sem placa, a tela só se vê no PC (`render_screens.py`); o simulador do legacy (`tools/TDD` + `LS027simulator.jar`) não compila aqui.
- Diga o que não foi visto na tela de verdade.
