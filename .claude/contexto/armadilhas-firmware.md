# Armadilhas do firmware, do build e das ferramentas

Tabela que saiu do [`CLAUDE.md`](../../CLAUDE.md) em 2026-10-16 (era a seção 6). As da placa nova, do KiCad e da caixa estão em [armadilhas-hardware.md](armadilhas-hardware.md). Armadilha nova entra na tabela da área dela, com a medição que a provou.

| Armadilha | Como evitar |
|---|---|
| `ValueError: path is on mount 'F:', start on mount 'C:'` no `west` | rode o `west` de dentro do `zephyr_app` (os scripts fazem isso); o NCS está em `C:` e o projeto em `F:` |
| `TOOLCHAIN_ROOT` definido quebra o CMake do Zephyr (`.../cmake/toolchain/zephyr/generic.cmake` não encontrado) | nunca exporte esse nome; os scripts usam `NCS_TOOLCHAIN_DIR` |
| No Windows, `Scripts/` (o que um `pip install` sem venv cria na raiz) e `scripts/` são a mesma pasta | ferramentas do projeto ficam em `tools/fw/` e `tools/docs/`; nunca rode `pip install` na raiz sem venv |
| O `sdk-ant` v2.1.1 é feito para o sdk-nrf v3.2.4 e testa `SOC_SERIES_NRF52X` e `SOC_SERIES_NRF54LX`, que o NCS v3.3.0 não liga mais | compile com `ANT=1` pelos scripts: `zephyr_app/modules/ant_ncs33_compat` religa os símbolos; o aviso "Deprecated symbol ... is enabled" é esperado |
| O `sdk-ant` já define `ant_stack_init()` e outras funções `ant_*` | código do port usa nomes fora desse prefixo (`rf_ant_init()`) |
| A camada de comandos do Git Bash transforma `\\n` em quebra de linha real | para caminhos com `\` (arquivos `.bat`), use a ferramenta de edição, não `sed` com `\\` |
| As ferramentas de escrita gravam LF | depois de editar um `.bat`, volte para CRLF: `sed -i 's/\r$//; s/$/\r/' arquivo.bat` |
| `modem_ubx` compara todo quadro recebido com o filtro do script em curso sem conferir se existe um | o driver do F10 e do M10 aponta `ubx.inst.script` para o seu script, com o filtro zerado, logo depois do `modem_ubx_init()`; sem isso, um receptor já configurado na BBR responde antes do primeiro script e o ponteiro é nulo |
| Configuração do receptor só na camada RAM se perde | o standby por software apaga a RAM do receptor, inclusive a configuração (manual de integração do M10, 3.7.4.2): o driver grava em RAM **e** BBR |
| O MAX-F10S não tem o grupo `CFG-PM`: não existe LEAP nem economia de rastreio | escrever `CFG-PM-OPERATEMODE` nele dá NAK; o driver devolve `-ENOTSUP` e a máquina de `gnss_power.c` recebe `has_leap = false` ([15](../../docs/15-avaliacao-componentes.md#firmware)) |
| Toda escrita em `CFG-SIGNAL` reinicia o subsistema GNSS | mande as chaves num `UBX-CFG-VALSET` só (`ubx_m10_valset_many()`) e espere 0,5 s depois do reconhecimento (UBX-23002975 R02, 4.9.20) |
| Esperar resposta do receptor dentro do workqueue do modem trava | a resposta chega num item de trabalho do mesmo workqueue: quem manda comando e espera é a thread `gnss` |
| O overlay entra pelo nome da placa | `boards/<placa>.overlay`, com `/` trocado por `_` (`nrf52840dk_nrf52840.overlay`); com outro nome ele é ignorado sem aviso |
| No nRF54LM20A, o SCL do TWIM e o SCK do SPIM precisam de pino de clock (tabela 79), e P1.01 e P1.02 saem do reset como antena NFC, sem GPIO | pinos de clock e pads do NFC na skill `fw-hardware` e em [14](../../docs/14-hardware-placa-nova.md#alocação-de-pinos); no DK, o I2C dos sensores usa SDA P1.29 e SCL P1.03, e o de energia, SDA P1.11 e SCL P1.14 |
| Desligar um nó do DK não desliga os filhos | o `mx25r64` precisa de `status = "disabled"` próprio, senão o driver `qspi-nor` volta |
| Build incremental guarda símbolos Kconfig que saíram (`NRFX_QSPI=y` continuou depois de desligar o QSPI) | afirmações sobre `.config`, devicetree ou tamanho só com `bash tools/fw/fw.sh build pristine` |
| Caminho de build longo (pasta temporária do usuário) passa do limite de 250 caracteres dos objetos | compile dentro do repositório: `zephyr_app/build` ou uma pasta `build/` da raiz |
| `--no-sysbuild` e o Partition Manager estão depreciados no NCS 3.3 | sysbuild com `zephyr_app/sysbuild.conf` (`SB_CONFIG_PARTITION_MANAGER=n`) |
| `JLink.exe` do PATH é a ferramenta do Java | use o da SEGGER em `C:\Program Files\SEGGER\JLink_V924a\` |
| ST-LINK e outras seriais conectadas nesta máquina | os scripts usam `--traits jlink`; com vários J-Link, `NRF_SERIAL` |
| Console no `uart0` (P0.06/P0.08) | só existe no DK; na placa real esses pinos não têm ligação |
| Breakpoint longo com o `task_wdt` ligado | a placa reinicia ao continuar (o timer do kernel não pausa); para depurar passo a passo, compile com `-DCONFIG_TASK_WDT=n` |
| O WDT do nRF52 continua contando depois de um reset por software (`sys_reboot`, erro fatal) | o `main()` o alimenta até os serviços criarem os canais (`app_wdt_feed_if_running()`); inicialização nova e demorada precisa alimentá-lo também |
| cppcheck acusa `syntaxError` nos `ble_*.c` ou nos `#if DT_...` dos serviços | macros do Zephyr sem os headers: falso positivo nos `ble_*.c`; nos serviços, passe `"-DDT_NODE_HAS_STATUS(n,s)=1" "-DDT_ALIAS(a)=a"` |
| Script (Python, `cmd`) chama `bash` | no Windows isso abre o `bash.exe` do `System32`, o lançador do **WSL**, que o projeto proíbe; chame as ferramentas direto (`cmake`, `ctest`) ou o Git Bash pelo caminho completo, e confira o código de saída |
| clangd do editor reclama dos testes de host ou da interface | ele usa o `compile_commands.json` do firmware; `tests/host/.clangd` aponta para o dos testes depois do primeiro `host_tests.sh`, e `tests/ui/.clangd` e `src/ui/.clangd` para `build/ui` depois do primeiro `render_screens.py` |
| O LVGL 9.5 não desliga a suavização das primitivas | `lv_display_set_antialiasing()` só vale para camadas e imagens; círculos, linhas inclinadas e cantos saem suavizados, e o driver quantiza para as cores do painel ([18](../../docs/18-interface-telas.md#implementação)) |
| Num `choice` do Kconfig o **primeiro** `default` que se aplica ganha | para trocar o padrão de um `choice` do Zephyr, escreva o seu `default` **antes** do `source` (foi assim que o MCUboot entrou no `zephyr_app/Kconfig.sysbuild`) |
| `CONFIG_LV_Z_BITS_PER_PIXEL` fica em 32 qualquer que seja a cor (o primeiro `default` do Kconfig do Zephyr ganha) | `CONFIG_LV_Z_BITS_PER_PIXEL=16` no `prj.conf`; sem ele o buffer de desenho do LVGL dobra (38.400 B) |
| Pilha da thread `ui` | o desenho do LVGL é recursivo, 624 B por nível da árvore de objetos; objeto aninhado a mais ou o log do LVGL ligado pedem nova medição com `CONFIG_STACK_USAGE` |
| O LPM027M128B quer os sinais no nível do VDD dele (3,0 V; alto acima de VDD − 0,1 V) | confira a tensão de I/O do DK antes de ligar o painel; se não for 3,0 V, tradutor de nível |
| `Path.write_text()` do Python grava CRLF no Windows | passe `newline="\n"`; o `.gitattributes` normaliza no commit, mas a cópia de trabalho fica misturada |
| Testes de host no mesmo shell do `ncs_env.sh` | o ambiente do NCS troca o `cmake`; use um shell limpo |
| Mermaid: `;` numa mensagem de `sequenceDiagram` | é separador de comandos; escreva "e" |
| Shunt do STC3100 | esquema: 20 mΩ; código: 100 mΩ; confirme na placa antes de confiar em corrente e carga |
| `git push` já foi bloqueado pelo classificador do auto mode; em 2026-09-23 passou a funcionar | tente o push quando o dono pedir, com o ramo explícito (`git push origin develop`); se voltar a ser recusado, peça a ele que rode no prompt, no modo bash, um comando por vez (`! git push origin develop`). Nunca crie regra de permissão para você, nunca use `--force` |
| A `main` do GitHub pode ter merge de pull request feito pela interface, que a `main` local não tem, e o push é recusado como non-fast-forward | `git fetch origin` e confira com `git log --oneline --no-merges origin/main --not main`: se não sair nada, o commit remoto não traz conteúdo novo e um `git merge origin/main` junta as histórias sem mexer em arquivo nenhum (compare `main^{tree}` antes e depois). Nunca resolva com `--force` |
| O CMake dá quatro avisos em todo build, e eles não são regressão | três são `No SOURCES given to Zephyr library: drivers__charger`, `drivers__display` e `drivers__fuel_gauge`: os drivers dessas classes são do projeto e moram em `zephyr_app/modules/gnss_drivers`, então a biblioteca da árvore do Zephyr fica vazia e é excluída. O quarto é a chave de desenvolvimento do MCUboot. **Aviso de compilador é que tem de ser zero** |
| `legacy/` não compila aqui | faltam o nRF5 SDK 16, o S340 e os submódulos `libraries/ant_profiles` e `ble_services` |
