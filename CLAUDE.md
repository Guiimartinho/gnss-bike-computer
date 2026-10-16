# CLAUDE.md · GNSS Bike Computer

Contexto e regras para assistentes de IA que trabalham neste repositório. Este arquivo traz o que vale em toda sessão: leia inteiro antes de mexer em qualquer coisa. O detalhe (estado, mapa, comandos, armadilhas) fica em [`.claude/contexto/`](.claude/contexto/) e é lido quando a tarefa pede; os procedimentos passo a passo estão nas skills de [`.claude/skills/`](.claude/skills/), cuja lista já vem carregada; a documentação técnica está em [`docs/`](docs/README.md).

## 1. Missão

Portar para **Zephyr / nRF Connect SDK** o **stravaV10**, computador de bordo GPS para ciclismo de Vincent Gollé, que roda na placa **myStravaB V3** (nRF52840 no módulo BMD-340, GNSS M10578-A3, LCD Sharp LS027 em retrato, BME280, FXOS8700, STC3100, microSD, ANT+ e BLE).

| Parte | Pasta | Papel |
|---|---|---|
| Port Zephyr | `zephyr_app/` | o firmware ativo, em C puro sobre NCS v3.3.0 |
| Original | `legacy/` | nRF5 SDK 16 + S340, C/C++: **especificação de comportamento**, só leitura |
| Bibliotecas do original | `libraries/` | Adafruit GFX, TinyGPS++, Kalman, SEGGER, etc.: só leitura |
| Ferramentas | `tools/` | `fw/`, `docs/` e `ui/` são do projeto; `TDD/`, `TDDW/`, `zpm/`, `MMD/`, `jumper/` vêm do legacy |
| Placa | `hardware/` | Eagle da V3 (os Gerbers da pasta são da **V2**), só leitura |
| Placa nova | `hardware_gnssbike/` | docs 01 a 10 (folhas, cálculos, lista de nós, placa e caixa, materiais, conectores, layout, dry runs); `cad/` com o KiCad e os geradores, `esquematico/` e `placa/` com os PDF, `caixa/` com o gerador, o dry run e os STL; **nada montado nem impresso** |

O dono do projeto é um desenvolvedor brasileiro de eletrônica embarcada que quer investigação completa, código nativo e verificação de verdade, não atalhos.

## 2. Regras que não se discutem

### Comunicação

- **Responda sempre em português do Brasil.** Código, identificadores e comentários de código ficam em inglês.
- Não afirme nada sem verificar. O que não foi testado na placa é dito como "não testado na placa".
- Em tarefas longas, mande um status curto de vez em quando.

### Firmware

- **O legacy é a referência.** Antes de portar ou mudar um comportamento, leia o original e cite `legacy/<arquivo>:<linha>`. Toda diferença de constante ou fórmula é corrigida ou registrada em `docs/06-algoritmos.md` e `docs/10-status-do-port.md`.
- **Nunca altere `legacy/`, `libraries/` nem `hardware/`** (a exceção é o `legacy/README.md`).
- **Zephyr nativo:** devicetree, Kconfig, drivers e subsistemas do Zephyr/NCS. Nada de Arduino nem de bibliotecas do legacy compiladas no port.
- **ISR não processa:** só copia dados para um buffer e acorda uma thread. Nada de mutex, arquivo, `snprintf` com float ou trigonometria em ISR.
- **Sem alocação dinâmica depois do boot**; buffers estáticos dimensionados e documentados.
- **Pilha medida:** thread nova ou cadeia pesada nova passa por `CONFIG_STACK_USAGE` (skill `fw-threads`) com pelo menos 1 KB de folga.
- **Verificação antes de dizer que terminou:** build sem aviso (com `ANT=1`, só o do símbolo obsoleto), testes de host, cppcheck nos arquivos tocados e, se mexeu em docs, `tools/docs/*.py`.

### Commits

- **Commit de cada item assim que ele estiver pronto e verificado** (regra do dono para este projeto, confirmada em 2026-09-18). Push só com pedido do dono, para o `origin` ([`Guiimartinho/gnss-bike-computer`](https://github.com/Guiimartinho/gnss-bike-computer), **público**). Mensagens em **inglês**, Conventional Commits com escopo (`fix(hal): ...`, `feat(model): ...`, `docs(docs): ...`).
- **Branches:** o trabalho vai na `develop`; a `main` guarda as versões estáveis e só recebe merge da `develop` quando o dono pedir. Não existe `master`. O histórico antigo do GitHub (stravaV11 de 2025-11, 817 commits, sem ligação com o atual) fica no ramo `archive/stravav11-2025-11`: não apague nem reescreva.
- **Nunca atribua commit a IA:** sem `Co-Authored-By` de assistente, sem "Generated with", sem menção a Claude. O autor é a identidade git configurada (Luiz Guilherme Ito). Procedimento na skill `commit-gnss`.
- Nunca faça commit de credenciais nem de arquivos gerados (`build*/`, `Lib/`, `Scripts/`).
- Nunca faça commit de material do ANT+ (perfis de dispositivo, ferramentas, código sob a ANT+ Shared Source License, chave de rede): o ANT+ Adopter Agreement proíbe distribuir, e o repositório é público ([07](docs/07-radio-ant-ble.md#decisão-ant-e-ble)).

### Documentação

- Português do Brasil, direto, voz ativa; **diagramas sempre em Mermaid**, nunca em texto puro; README no padrão industrial. Skill `docs-gnss`.
- Quando o comportamento muda, a documentação e o `CHANGELOG.md` mudam junto.

### Ambiente

- **Nunca use WSL**, máquinas virtuais nem Docker local.
- Não instale pacotes na máquina do dono sem perguntar (downloads de dependência de build, como o Unity via FetchContent, são aceitos).
- **CI desligado:** `.github/workflows/ci.yml` só roda à mão (`workflow_dispatch`), para não gastar minutos do GitHub Actions. Não acrescente gatilhos (push, pull request, agendamento) nem outros workflows sem o dono pedir. A verificação é local.

### Licença

- O projeto é **CC BY-NC 4.0** (`LICENSE` na raiz, decisão do dono em 2026-09-26): todo arquivo novo nasce sob ela. O que é de terceiros entra com a licença de origem registrada em [`docs/12`](docs/12-ferramentas-testes.md#bibliotecas-e-licenças); nunca misture ao projeto código cuja licença proíba derivados não comerciais ou exija relicenciar (GPL) sem o dono decidir.

### Armadilhas que valem em qualquer tarefa

- Script (Python, `cmd`) que chama `bash` abre o `bash.exe` do `System32`, o lançador do **WSL**: chame as ferramentas direto (`cmake`, `ctest`) ou o Git Bash pelo caminho completo, e confira o código de saída.
- O Git Bash transforma `\\n` em quebra de linha: arquivo com `\` (os `.bat`) se edita com a ferramenta de edição, não com `sed`. As ferramentas de escrita gravam LF: depois de editar um `.bat`, volte para CRLF. No Python, `Path.write_text()` grava CRLF no Windows: passe `newline="\n"`.
- Afirmação sobre `.config`, devicetree ou tamanho só depois de `bash tools/fw/fw.sh build pristine`: o build incremental guarda símbolos Kconfig que saíram.
- Os quatro avisos do CMake em todo build são esperados (três bibliotecas de driver vazias e a chave de desenvolvimento do MCUboot); **aviso de compilador é que tem de ser zero**.
- Push só quando o dono pedir, com o ramo explícito (`git push origin develop`), nunca com `--force` e nunca criando regra de permissão para você; se for recusado, siga a skill `commit-gnss`.

## 3. Estado em resumo

Estado registrado até 2026-09-26. O registro completo, as decisões com data e a ordem do que falta estão em [`.claude/contexto/estado.md`](.claude/contexto/estado.md): atualize lá quando o estado mudar, e aqui quando o resumo mudar.

- **Port:** compila no NCS v3.3.0 e passa nos testes de host; **nunca rodou em placa nem no DK**. Matriz em [`docs/10-status-do-port.md`](docs/10-status-do-port.md).
- **Placa nova:** nRF54LM20A, gerada no KiCad pelos scripts de `hardware_gnssbike/cad/`, com dry run da placa e da caixa; **nada fabricado, montado nem impresso**.
- **Decisões do dono que pesam em todo trabalho:** ANT+ **e** BLE, pelo `sdk-ant` v2.1.1 sobre o NCS v3.3.0 (build com `ANT=1`); GNSS u-blox **MAX-F10S** a 3,0 V no trilho do MCU, sem modo econômico (o F10 não tem `CFG-PM`); tela **JDI LPM027M128C**, peça única com luz integrada (2026-09-23), num driver que também atende a LPM027M128B e a Sharp LS027B7DH01; memória soldada no lugar do cartão.
- **Em aberto:** aprovação dos componentes da placa nova ([13](docs/13-placa-nova.md), [14](docs/14-hardware-placa-nova.md), [15](docs/15-avaliacao-componentes.md), [19](docs/19-lista-de-compras.md)), hardware de teste, formatos no SD e o teste com cabo da fase 6.

## 4. Comandos do dia a dia

| Tarefa | Comando (Git Bash, na raiz) |
|---|---|
| Build incremental / do zero (nRF54LM20 DK, o alvo) | `bash tools/fw/fw.sh build` / `bash tools/fw/fw.sh build pristine` |
| Build com ANT (add-on em `C:\ncs\sdk-ant`) | `ANT=1 bash tools/fw/fw.sh build pristine` |
| Testes de host | `bash tools/fw/host_tests.sh` |
| Diagramas e links da documentação | `python tools/docs/mermaid_check.py` e `python tools/docs/links_check.py` |

Placa própria, gravação, recuperação, telas no PC, percursos, a cadeia da placa nova e da caixa, os `.bat` do `cmd` e o mapa do repositório: [`.claude/contexto/mapa-e-comandos.md`](.claude/contexto/mapa-e-comandos.md). O procedimento completo está nas skills `fw-build` e `fw-testes`.

## 5. Armadilhas

As armadilhas medidas do projeto estão em duas tabelas; leia a da área antes de mexer nela:

- [firmware, build, NCS, ferramentas e git](.claude/contexto/armadilhas-firmware.md): `west` entre discos, `TOOLCHAIN_ROOT`, `sdk-ant`, `modem_ubx` e o receptor, overlay, pinos do nRF54LM20A, Kconfig, J-Link, watchdog, cppcheck, clangd, LVGL, painel, shunt do STC3100, Mermaid, push recusado;
- [placa nova, KiCad, 3D e caixa](.claude/contexto/armadilhas-hardware.md): a ordem e o intérprete da cadeia da placa, modelos STEP girados, render visto de cima, VRML, GLB velho, dry run que passa vazio, fatos do esquemático do KiCad 8, Gerbers da V2.

## 6. Onde está cada coisa

| Assunto | Documento |
|---|---|
| Visão geral, início rápido e índice da documentação | [README.md](README.md), [01](docs/01-visao-geral.md), [docs/README.md](docs/README.md) |
| Build e ambiente | [03](docs/03-ambiente-build.md) |
| Placa V3 e pinagem | [02](docs/02-hardware.md) |
| Placa nova: proposta, especificação, componentes, compras | [13](docs/13-placa-nova.md), [14](docs/14-hardware-placa-nova.md), [15](docs/15-avaliacao-componentes.md), [19](docs/19-lista-de-compras.md) |
| Esquemático, CAD e caixa da placa nova | [hardware_gnssbike](hardware_gnssbike/README.md), [cad](hardware_gnssbike/cad/README.md), [caixa](hardware_gnssbike/caixa/README.md) |
| Legacy | [04](docs/04-arquitetura-legacy.md), [legacy/README.md](legacy/README.md) |
| Port e arquitetura do firmware | [05](docs/05-arquitetura-zephyr.md), [16](docs/16-arquitetura-firmware.md) |
| Algoritmos | [06](docs/06-algoritmos.md) |
| Rádio e dispositivos BLE e ANT+ | [07](docs/07-radio-ant-ble.md), [17](docs/17-dispositivos-ble-ant.md) |
| Interface e telas | [08](docs/08-interface.md), [18](docs/18-interface-telas.md), [os 44 quadros](docs/telas/README.md) |
| Armazenamento e USB | [09](docs/09-armazenamento-usb.md) |
| Status, defeitos e roteiro | [10](docs/10-status-do-port.md) |
| Qualidade e testes | [11](docs/11-qualidade-misra.md), [12](docs/12-ferramentas-testes.md) |
| Histórico de mudanças | [CHANGELOG.md](CHANGELOG.md) |

## 7. Economia de contexto (medido em out/2026)

Cada pedido ao modelo relê a conversa inteira: 75% do consumo da conta era essa releitura, com 300 a 530 mil tokens por pedido nas sessões longas, milhares de chamadas de ferramenta e centenas de saídas acima de 40 kB. Regras:

- **Saída de ferramenta curta.** Limitar todo comando (`| tail -n 40`, `| head -n 40`, `cut -c1-200`, `grep -c`, `--quiet`). Ler arquivo por trecho (offset/limit), nunca inteiro sem precisar. Diff, log, JSON ou listagem grande vai para arquivo, e só o trecho que importa entra na conversa.
- **Menos chamadas.** Juntar comandos relacionados numa chamada só; não repetir leitura de arquivo já lido.
- **Agentes.** Agente novo com briefing curto, não fork (o fork copia o contexto inteiro). Workflow só quando o usuário pedir. No máximo 3 agentes ao mesmo tempo.
- **Web.** WebFetch/WebSearch só quando a resposta depende da web, uma vez na fonte primária.
- **Imagem.** Só quando for preciso olhar, em resolução baixa.
- **Sessão por tarefa.** Ao trocar de assunto, ou quando a sessão passar de ~200 mil tokens, salvar na memória o que importa e sugerir ao usuário `/compact` ou `/clear`.
