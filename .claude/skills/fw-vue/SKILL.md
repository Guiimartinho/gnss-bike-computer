---
name: fw-vue
description: Trabalhar com a interface do GNSS Bike Computer no port Zephyr - LCD Sharp LS027 (framebuffer, VCOM, orientação retrato), primitivas e fontes, telas (cadrans CRS, PRC com mapa, FEC, GPS, debug), menu, botões curtos e longos, notificações e LED. Use ao mexer em zephyr_app/src/vue/, src/drivers/lcd/ls027.c ou no tratamento de botões.
---

# Interface

Referência: `docs/08-interface.md` (telas do legacy, imagens em `docs/img/`, estado do port e defeitos).

## Display

- LS027B7DH01, 400 × 240, 1 bit, sem backlight, CS ativo alto, SPI a 2 MHz LSB primeiro.
- Buffer de 12.482 B: `[comando][endereço][50 B][dummy] × 240 + [dummy]`; bit 0 = pixel da esquerda; mesmo layout no legacy e no port.
- VCOM precisa alternar: o port manda o comando a cada 1 s (`ls027_toggle_vcom()`); o legacy alterna o bit M1 a cada quadro.
- **O aparelho é retrato** (240 de largura × 400 de altura; decisão do dono em 2026-09-18, também para o display colorido da placa nova): o legacy usa `setRotation(3)` e `drawPixel(x, y)` → físico `(y, 239 − x)`. O port desenha em paisagem, e `transform_coords` do `ls027.c` tem as contas de retrato erradas.

## Regras

1. **Só a thread `display` desenha.** Outras threads pedem (flag, `k_msgq`), nunca chamam primitivas: o framebuffer e o SPI não têm trava nas primitivas.
2. **Nada de desenho fora da tela**: recorte antes de converter float para inteiro (o mapa hoje converte sem saturar).
3. **`snprintf` com float** está habilitado (`CONFIG_CBPRINTF_FP_SUPPORT`), mas custa pilha (a `display` tem 2 KB) e arredonda, enquanto o legacy trunca (`_fmkstr`).
4. **Item selecionado do menu** em XOR ou texto branco sobre barra preta (hoje é preto sobre preto).
5. **Botões** chegam por `hal_gpio_btn_process()` (polling de 100 ms na `main_loop`): curto na liberação, longo com 1 s. `gpio_pin_get_dt()` já devolve nível lógico; não inverta de novo (corrigido em 2026-09-18).
6. **Energia**: um quadro inteiro custa ~50 ms de SPI; o legacy atualiza por evento (~1 Hz). Evite subir a taxa.

## Fidelidade com o legacy

| Legacy | Port | Para portar |
|---|---|---|
| fonte `Org_01` (`libraries/AdafruitGFX/Fonts/Org_01.h`) escalada de 1 a 4 | 5×7 de 256 glifos | renderizador GFXfont sobre `include/vue/gfxfont.h` |
| grade de 2 × 7 cadrans com rótulo, valor e unidade | listas de texto | `cadran`, `cadranH`, alinhamento à direita (`printRev`) |
| menu com Back, modos CRS/PRC/FEC/Zwift/DBG, Settings (pareamento, FTP, peso, calibração, formatar), Shutdown | `menu.c` não ligado, com controle de atividade | ligar no botão central, rotear os eventos antes de `vue_handle_button` |
| fila de 10 notificações numa faixa no topo | um slot, sem produtores | fila e produtores (boot, GPS, pareamento, FDIR) |
| mapa e mini-mapa com projeção linear e zoom `nível² × 250 / 100` m | mapa sem zoom | ver `legacy/source/display/Zoom.cpp` e `VuePRC.cpp` |
| ícones Komoot 110 × 110 | 6 ícones de 32 × 32 sem uso | `libraries/komoot/komoot_icons.h` (~45 KB de flash) |

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

- Build sem aviso novo; hoje há 7 avisos de funções de `vue.c` portadas e não ligadas: ao ligar uma, o número cai.
- Interface nova: `python tools/ui/render_screens.py` precisa terminar com `0 problems`; olhe as folhas de `docs/img/telas-lvgl/` nos dois temas. Tela nova ganha um `snap()` e, se tiver navegação, `expect_screen` e `expect_action` em `zephyr_app/tests/ui/ui_render.c`, e entra numa folha de `tools/ui/render_screens.py`.
- Interface do firmware atual (`src/vue`): sem placa, a única forma de ver a tela é gravar no DK com um LS027 ligado aos pinos do overlay (o simulador do legacy, `tools/TDD` + `LS027simulator.jar`, não compila aqui).
- Diga o que não foi visto na tela de verdade.
