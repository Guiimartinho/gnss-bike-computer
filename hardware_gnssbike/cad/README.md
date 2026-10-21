# Projeto KiCad da placa

Este diretório é o projeto de CAD do GNSS Bike Computer: **esquemático
hierárquico em sete folhas** e **placa com as 166 peças posicionadas e
roteadas**, em KiCad 8, gerados a partir dos documentos de
[`hardware_gnssbike/`](../README.md) e conferidos contra eles por programa.

> [!WARNING]
> **Nada aqui foi fabricado, montado ou medido.** Não existe placa, não existe
> protótipo e nenhum número desta página saiu de bancada. O roteamento é feito
> por programa e está **completo desde 2026-10-19** (0 itens desconectados e 0
> erros no DRC completo), com rotina própria para as linhas de RF; nenhuma
> placa de verdade o conferiu.

## O que abrir

| Arquivo | O que é |
|---|---|
| `gnssbike.kicad_pro` | o projeto; **abra este** no KiCad |
| `gnssbike.kicad_sch` | folha raiz: o diagrama de blocos |
| `folha1-energia.kicad_sch` … `folha6-interface.kicad_sch` | as seis folhas |
| `gnssbike.kicad_pcb` | a placa |
| `gnssbike.kicad_dru` | as duas regras locais de folga |
| `zonas.dxf`, `contorno-r3.dxf`, `contorno-r4.dxf` | as zonas e o contorno da placa em DXF, gerados pelo `make_dxf.py` (entradas da placa, não saídas) |

O que se **lê** sai daqui para as pastas do assunto (reorganização de 2026-09-26): o esquemático em PDF para [`../esquematico/`](../esquematico/), a placa em PDF e SVG e o desenho de montagem para [`../placa/`](../placa/), a proposta de caixa (PDF, STL e os dois scripts dela) para [`../caixa/`](../caixa/README.md), e as vistas 3D da placa e da caixa para [`../../docs/img/hardware/`](../../docs/img/hardware/). A tabela de [`../README.md`](../README.md#o-cad-e-o-que-sai-dele) lista cada arquivo. O `gnssbike.glb` que o `kicad-cli` exporta fica aqui e fora do git (45 MB, regenerado quando a placa muda).

## O esquemático

```mermaid
flowchart TB
    RAIZ["folha raiz · A3<br/>diagrama de blocos<br/>as linhas entre as folhas"]
    RAIZ --> F1["1 Energia · A3<br/>79 posicoes em 6 blocos"]
    RAIZ --> F2["2 MCU e depuracao · A4<br/>14 posicoes em 2 blocos"]
    RAIZ --> F3["3 GNSS · A4<br/>13 posicoes em 2 blocos"]
    RAIZ --> F4["4 Display e luz · A4<br/>15 posicoes em 2 blocos"]
    RAIZ --> F5["5 Memoria e sensores · A4<br/>18 posicoes em 5 blocos"]
    RAIZ --> F6["6 Interface · A4<br/>23 posicoes em 3 blocos"]
```

### Desenhado por bloco funcional, desde 2026-09-26

Até esse dia cada folha era a lista de nós desenhada em fios: as peças numa
grade por ordem de ligação e o roteador de labirinto puxando um fio de cada
pino até o outro extremo, fosse ele do outro lado da página. O dono pediu o
padrão industrial — ligações e rótulos, A4 ou A3, peças certas, fácil de
ler, os circuitos subdivididos em caixas tracejadas —, e o `blocos.py` é
isso:

| Regra | Como | De onde vem |
|---|---|---|
| Cada folha é uma fila de **blocos funcionais**, na ordem em que o sinal flui, cada um numa **caixa tracejada com título** | a tabela `BLOCOS` de `blocos.py` diz as âncoras de cada bloco (o CI ou o conector); os passivos seguem `make_pcb.DECOPLA` e `JUNTO` (as tabelas que já dizem de quem é cada capacitor na placa), depois `sheets.SEGUE`, depois a lista de nós sem os trilhos (o vizinho mais próximo por saltos) | SparkFun, "How to read a schematic": "truly expansive schematics should be split into functional blocks", "following the flow of circuit from input to output" |
| Dentro do bloco, o passivo fica **ao lado do pino que serve**, do lado para onde o pino aponta; o desacoplamento vai numa **prateleira** sob o CI | `_colocar_bloco()`: a âncora no meio, cada membro a dois passos da ponta do pino que compartilha com ela, empurrado para fora até caber sem sobrepor nem encostar ponta de outra rede; o que só toca trilho vai para a prateleira, que quebra linha na largura da âncora | prática corrente; a razão é o roteador: um fio curto sempre passa |
| Um passivo de dois terminais é **espelhado** quando o pino que serve ao CI é o de trás, e **posto em pé** (90° ou 270°) na prateleira e quando é pull-up ou pull-down de uma linha rotulada | `Part.espelho` e `Part.rotacao`. A **biblioteca** guarda sempre o desenho base (`pin_base()`); a instância leva o giro e o espelho, e `pin_local()` diz onde as pontas ficam depois deles. **Medido** no KiCad 8 em 2026-09-26, com treze resistores de biblioteca base e um rótulo colado em cada ponta (`kicad-cli sch export netlist`): `(mirror y)` troca esquerda por direita, `(mirror x)` troca cima por baixo; `(at x y 90)` manda o pino da esquerda para **baixo**, `270` para cima; a rotação vem antes do espelho. E, pelo `sch export pdf`: a justificação do campo de texto também é transformada (sob `(mirror y)` "left" imprime como "right"; a 90° trocam os dois eixos; a 270° nada), e `instance()` escreve a justificação que **renderiza** como a pedida | a primeira "medição" saiu invertida porque foi feita através do gerador, que escrevia a biblioteca já espelhada e ainda punha o `(mirror ...)` na instância: transformação duas vezes. Foi assim que todos os capacitores em pé saíram com as duas pontas no ar em 2026-09-26 |
| Alimentação e terra por **símbolo**, apontando **para longe do pino**: deitado ao longo da linha num pino lateral (seta ou barras para fora, o nome na linha, depois delas), em pé num pino de cima ou de baixo; pinos vizinhos do mesmo trilho no mesmo lado, ou os topos de uma fileira de capacitores em pé, ganham **um fio pela ponta deles e um símbolo só** | `PowerPort.sentido` (U, D, L, R); o nome fica a 3,3 mm do ponto, entre duas linhas de pino. Em pé num pino lateral, o símbolo punha a seta na linha de cima e o nome na seguinte. O fio da corrida é um por par de vizinhos: "wires connect with other wires or pins only if their ends coincide exactly" — um pino no meio de um fio comprido **não** está nele; e cada célula do fio é marcada no roteador (só as pontas e o meio deixavam um rótulo de outra rede pousar no meio dele) | manual do Eeschema 8; prática corrente nos esquemáticos densos |
| A **prateleira** é uma fileira de capacitores em pé, agrupados por trilho, com passo de 12,7 mm; o trilho corre pelos topos e o terra pelos pés, um símbolo por fileira; os pontos de teste ficam em pé com o pad pendurado da linha | `_colocar_bloco()` (prateleira), `PASSO_PRATELEIRA`, `ENTRE_LINHAS_PRATELEIRA`, `LARGURA_PRATELEIRA`; a referência e o valor à direita da peça, escritos a 90° (o KiCad soma o giro da instância ao ângulo do campo) | é como toda folha de desacoplamento é desenhada |
| Pull-up ou pull-down de uma linha que **sai do bloco** fica **em pé, depois do rótulo**; quando a linha segue para outra âncora (os contatos do FPC ao painel), fica **fora do corredor** das linhas, acima (trilho) ou abaixo (terra), e o roteador desce um fio até a linha cruzando as outras em ângulo reto | `_colocar_bloco()`: `em_pe`, distância pelo comprimento do maior rótulo vizinho (`GAP_ROTULO` no mínimo). Deitado na linha, ficava debaixo do texto do rótulo; em pé sobre a linha, era um muro para as linhas ao lado | é como uma fileira de pull-ups é desenhada |
| Os **rótulos têm o lugar reservado antes** das peças: cada pino de âncora cuja rede sai do bloco ganha um retângulo (toco de dois passos, texto, pentágono) que nenhum corpo nem texto de membro toma; e o que cada peça ocupa é testado como retângulos **separados** (corpo com pinos, cada texto), nunca uma caixa envolvente só | `retangulos` em `_colocar_bloco()`; `Part.caixas_de_texto()`. Uma caixa só em volta do conector FPC e do seu valor de 26 mm murava todas as linhas à direita dele | — |
| Sinal que sai do bloco vira **rótulo local**; sinal que sai da folha vira **rótulo hierárquico**, os dois num toco curto saído do pino; o texto do rótulo não pousa sobre peça, valor nem nome de símbolo | `_rotulo()`: toco reto de 2 a 12 passos se o caminho está livre **e** o texto cabe, senão o labirinto leva o toco à célula livre mais próxima onde o texto caiba (e, em último caso, a qualquer célula livre, para o pino nunca ficar aberto). As duas células em frente de todo pino são da rede dele (`dono_da_celula`): nenhuma outra rede passa ali. Num conector **de passagem** (`nets.PASSA_DIRETO`: o pino está em duas redes que são um nó só) o fio do painel continua do ponto do rótulo, e as duas redes podem partilhar células no roteador (`Router.irmas`) | manual do Eeschema 8: "local labels connect items located in the same sheet"; SparkFun: "give a net a name and label it, rather than routing a wire all over the schematic" |
| Um conector de biblioteca do KiCad com os pinos todos de um lado é **espelhado** para encarar a âncora com quem fala, e **alinhado** com ela pela mediana das linhas dos pares de pinos ligados, para as linhas correrem retas | `_colocar_bloco()` (âncoras); `Part.espelho_token()` devolve `y` para símbolo do KiCad. Com os pinos virados para fora, as dez linhas do FPC davam a volta por cima | — |
| Um pino de trilho sem lugar para o símbolo ganha o símbolo no fim de um toco roteado — **nunca um rótulo local com o nome do trilho** | **medido** na lista de nós do KiCad: um rótulo local `3V0` é a rede `/folha/3V0`, outra rede, e não se junta ao `3V0` dos símbolos de alimentação | medida de 2026-09-26 |
| Tudo na **grade de 50 mil** (1,27 mm) | `snap()` em toda coordenada | manual do Eeschema 8: "always use a 50 mil grid" |
| O tamanho da folha continua **medido**: a menor de A4, A3, A2 em que os blocos cabem | `escolher_papel()` coloca os blocos na candidata e aceita quando a última prateleira termina acima da margem | ver abaixo |

O `check_sch.py` é quem diz se deu certo: a lista de nós que o KiCad extrai
tem de bater com a de `nets.py` pino a pino, o ERC não pode acusar erro novo,
nenhum fio pode cruzar componente e nada pode sair da folha. Em 2026-09-26,
depois da passada de legibilidade que o dono pediu (símbolos de alimentação
deitados nos pinos laterais, nome e número do pino escondidos neles, texto
horizontal nas peças em pé, prateleira por trilho, pull-ups em pé depois
dos rótulos, lugar dos rótulos reservado, conector do FPC espelhado e
alinhado com o painel), ele passa com a folha 1 em **A3** e as outras em
A4; oito redes fecham por rótulo em vez de fio porque o labirinto não achou
caminho dentro do bloco (sete na folha 1, uma na 6: o gerador lista quais),
e dois símbolos ficaram onde couberam, com o nome encostado em alguma coisa
(`VBAT_SYS` no `SYS` do medidor, `3V0_GNSS` no ferrite). O que ainda não está
bom: o bloco da colheita solar (os seis módulos em série e paralelo, com
três laços de 0 R, e o lado esquerdo do ADP5091, com os divisores) e a
esquerda do medidor de bateria, que continuam apertados. `SCH_DEBUG=1
python make_sch.py` imprime por que cada símbolo ou rótulo não achou lugar
reto e onde um fio não achou caminho.

### O tamanho da folha é medido, não escolhido

**Cinco das sete folhas são A4 e as outras duas A3.** Antes eram A0, A2 e
cinco A3, e a causa era a mesma do contorno da placa: o desenho era espalhado
para encher o papel em vez de o papel ser escolhido para caber o desenho.

| O que fazia | O que faz agora |
|---|---|
| Orçava **54 × 48 mm por peça**, fosse um 0402 ou um módulo de 80 pads | **desenha** na folha candidata e pergunta se a última linha terminou acima da margem — é o mesmo código que desenha, então a resposta não pode divergir do desenho |
| Nunca considerava A4: a busca começava em A3 | começa em **A4** |
| Corredores de 24 e 30 mm entre peças, mais largos que os próprios símbolos | **16 e 22 mm**, e quem prova que ainda cabe fio é o `check_sch.py`, que falha se um fio cruzar componente |
| Raiz em **A0 fixo**, blocos de 190 × 240 mm num passo de 370 × 350 | blocos dimensionados pelo que contêm (o do MCU tem 33 sinais atravessando), **A3** medido |

Um pino de folha ganhava um espaço de 5,08 mm para um rótulo de 1,27. A dois
passos de grade — o padrão do KiCad — o bloco do MCU caiu de 116 para 73 mm de
altura, e foi isso que tirou o diagrama de A2.

### Bloco é só para circuito integrado

Um retângulo com nomes de pino nas laterais é como se desenha um **CI**, e só
um CI. Este gerador desenhava **tudo** assim — 127 peças, todas retângulos:
resistores, capacitores cerâmicos, diodos, o TVS. Uma folha de blocos ligados
por linhas não mostra circuito nenhum.

Cada classe passou a ter o símbolo dela, no padrão, com o eixo dos terminais
em x e o corpo entre −1,27 e +1,27 mm:

| Classe | Símbolo | Quantas |
|---|---|---|
| `R`, `JP` | ziguezague | 42 |
| `C` | duas placas paralelas (cerâmico); placa curva e `+` quando for eletrolítico | 31 |
| `L` | quatro arcos | 4 |
| `FB` | quatro arcos dentro do retângulo, que é o que distingue a ferrite do indutor | 1 |
| `RT` | ziguezague com a seta na diagonal | 1 |
| `D` | triângulo e barra; com setas para fora se for LED | 2 |
| `SW` | dois contatos, a barra e o êmbolo | 3 |
| `LS` | círculo com a diagonal | 1 |
| `TP` | círculo na ponta do terminal | 14 |
| `U`, `J`, `DS` | **retângulo**, que é o certo para eles | 36 |

Peça de dois terminais não mostra **número nem nome de pino**: "1" e "2" num
resistor é ruído, e o símbolo já diz onde é o quê onde importa — a barra do
catodo, a placa curva.

Três tipos de nó, cada um desenhado do jeito que se faz:

| Tipo | Como aparece | Quantos |
|---|---|---|
| **Trilho** (alimentação e terra) | símbolo de alimentação junto do pino que ele alimenta; o KiCad junta todos os de mesmo nome | 18 |
| **Entre folhas** | rótulo hierárquico na margem, pino no bloco da raiz, e a linha entre os blocos | 36 |
| **Dentro da folha** | só fio | 57 |

Dentro de cada folha as peças são colocadas na ordem em que se ligam, não por
tamanho: o capacitor de desacoplamento cai ao lado do CI que ele desacopla.

## A placa

| Item | Valor |
|---|---|
| Contorno | **34 × 95 mm**, canto de 3 mm, 0,8 mm de espessura — **derivado das regras**, não escrito à mão; os 5 mm além dos 90 de 2026-09-24 são das três teclas no passo de 13,4 (a área da antena do módulo desce com ele; `GNSSBIKE_H` troca a altura) |
| Camadas | **4**: `F.Cu`, `In1.Cu` (terra), `In2.Cu` (alimentação), `B.Cu` |
| Por que esse tamanho | a 7.2 do ME54BS13 pede **50 mm entre dois módulos de rádio**, e esta placa tem dois. Varrendo cada milímetro que cumpre isso e ainda cabe a fila de teclas, 34 × 90 é o menor contorno com folga — **3.060 mm² contra os 5.335 do 55 × 97, 43 % menos** |
| Relação com a caixa | **nenhuma.** A placa sai do circuito; a caixa sai do display, da bateria e da mão. Encolher uma não encolhe a outra |
| Peças na placa | **156**, com rotação (2026-09-26); as de trás (`U502`, `RT101`, `LS601`, `J103`) estão em `make_pcb.ATRAS`. Desde 2026-10-18 são **167** com footprint, com as onze peças das duas telas que vão soldadas, e o `D105` e o `J403` também vão atrás; desde 2026-10-19, **166**, sem o `R110` |
| Fora da placa | **8** desde 2026-10-18 (`footprints.FORA_DA_PLACA`): as duas telas, `DS401` e `DS402`, que vão no `J401` uma de cada vez, e os seis módulos solares, que moram na caixa. A célula não é peça do CAD, e a antena GNSS passou a morar na placa |
| Redes | **118** (2026-10-19; eram 114 em 2026-09-26) |
| Furos de fixação | **2**, M2, em (3,2; 7,0) e (14,4; 91,7) — decisão do dono em 2026-09-25 (dois furos), depois da conta de [02](../02-calculos.md#quantos-parafusos-a-placa-precisa); as posições são do dry run da caixa de 2026-09-26: nenhum sobre a célula, o segundo entre o USB-C (que andou 0,4 para a esquerda por ele) e o módulo, fora do lugar do sensor de luz e da faixa do verso que segura o `J103` |
| Planos de terra | **3**, em `In1.Cu` e nas duas faces, preenchidos pelo `fill_zones.py` (com o Python do KiCad) |
| Roteamento | **completo** em 2026-10-19: 2.683 segmentos e 769 vias, 372 delas de terra; 1.305 mm de trilha na frente, 1.770 na `In2.Cu` e 1.565 no verso, medidos depois do casamento do par USB. O `route.py` faz a placa base e o `acabar.py` fecha o resto ([abaixo](#o-acabamento-uma-ligação-de-cada-vez)); o par USB vai à mão, num corredor reservado de `D102` a `J101`, e `RF_IN`, `RF_ANT`, `RF_CHIP` e `RF_UFL` têm rotina própria |
| Ligações sem trilha | **0**: o DRC completo (`--severity-all`), com as malhas preenchidas, não acha item desconectado, nem trilha ou via solta — a regra `RT1` do `dry_run_pcb.py`. Eram 58 em 2026-09-26 e 48 na manhã de 2026-10-19 |
| Erros de DRC | **0**, com a ponte mínima de máscara da JLCPCB (0,1 mm) no setup da placa desde 2026-10-19; 25 avisos, os mesmos do arquivo de antes do roteamento: 23 de biblioteca de footprint e 2 de uma linha de serigrafia a 1,2 mm da borda de baixo. Os 3 do par USB contra o ponto de teste `TP101` saíram com o corredor do par em 2026-09-26 |

### O que decide a posição de cada peça

1. **A borda**, para quem tem de alcançá-la: o USB-C com a boca para fora, os
   fitas das telas chegando pela esquerda, o `J103` do chicote solar e
   o módulo de rádio deitado no canto de baixo à direita, com a antena sobre o
   recorte — que é o arranjo que a figura 1 da seção 7.5 da ficha dele chama
   de "Best". Nada disso é a caixa mandando no tamanho da placa: é o circuito
   dizendo de que lado cada coisa sai.
2. **A zona** de [`04-pcb-e-caixa.md`](../04-pcb-e-caixa.md), para quem tem
   uma. As zonas são **derivadas de `W` e `H`**, não retângulos fixos.
3. **A ligação**, para todo o resto: cada peça vai ao centro de gravidade das
   peças a que se liga.
4. **A rotação**: peça de dois terminais deita ao longo da linha entre as duas
   peças que ela junta, para a trilha sair reta.

### O que a regra dos 50 mm impôs ao arranjo

Com 34 mm de largura, três decisões deixaram de ser gosto:

| O que | Por quê |
|---|---|
| A fila de teclas **não** fica na borda de baixo | o módulo ocupa 17,0 mm dela e a fila pede 30,6; e os 5 mm que a 7.4 pede em volta da antena comem mais. A fila subiu para acima do módulo |
| O buzzer vai na **face de trás** | 10,5 × 9,5 mm não cabem na faixa de 9,25 mm que sobra na frente entre as teclas e o módulo. Empurrado, ele parava a 0,84 mm do TPS7A02 e tomava o anel que o desacoplamento dele precisa |
| O receptor GNSS fica a **180°** | a 0 o pino `RF_IN` saía do lado da borda, a 1,2 mm dela, e a rede π não tinha para onde ir — ficava a 10,5 mm de um pino que a 4.4 quer "as short as possible". Virado, o pino olha para dentro e a rede cabe em linha |

## Verificação

```sh
python hardware_gnssbike/cad/check_sch.py                 # regera e confere o esquematico
"D:/KiCAD/bin/kicad-cli.exe" sch export pdf --output hardware_gnssbike/esquematico/gnssbike-esquematico.pdf hardware_gnssbike/cad/gnssbike.kicad_sch
python hardware_gnssbike/cad/make_pcb.py                  # 1. coloca as pecas
python hardware_gnssbike/cad/route.py                     # 2. roteia o que consegue
python hardware_gnssbike/cad/acabar.py                    # 3. fecha o resto, uma ligacao de cada vez
"D:/KiCAD/bin/python.exe" hardware_gnssbike/cad/fill_zones.py   # 4. preenche as malhas de terra
python hardware_gnssbike/cad/check_pcb.py --como-esta      # 5. confere o arquivo como esta
python hardware_gnssbike/cad/dry_run_pcb.py               # 6. as regras das fichas
python hardware_gnssbike/cad/aplicar_modelos.py           # um modelo 3D novo na placa roteada, sem mexer no cobre
python hardware_gnssbike/cad/impedancia.py                # a impedancia das linhas na pilha do fabricante
python hardware_gnssbike/cad/make_fab.py                  # Gerber, furacao, BOM e CPL da JLCPCB em ../fabricacao/
python hardware_gnssbike/cad/mouser.py jdi 2              # o pedido na Mouser e as planilhas da LCSC e da DigiKey (chave em .claude/local/)
```

O `route.py` é um labirinto A\* numa grade de 0,15 mm: faz bem o que pede
regra própria (as linhas de 50 Ω do GNSS, o par USB pelo diodo, a costura da
borda), e até 2026-10-18 deixava dezenas de ligações sem cobre. O `autoroute.py`, de
2026-10-18, tenta fechar o resto com o Freerouting (fora do repositório, em
`~/tools/freerouting`, como no projeto irmão do medidor de potência): trava o
cobre de RF e USB e as vias de terra, exporta o DSN pelo `pcbnew` e importa o
SES. **Medido em 2026-10-18, ele não fechou**: em 20 minutos, 55 itens
desconectados contra os 64 do `route.py` sozinho, e erros novos de DRC. Ele
fica fora da cadeia.

Em 2026-10-19 o `route.py` mesmo foi atrás das causas, e várias não eram
falta de espaço:

| Causa | O que fazia | Correção |
|---|---|---|
| UUIDs repetidos no arquivo | o DRC dava cada ligação faltando na peça errada | `make_pcb.py` dá UUIDs próprios a cada footprint; o `check_pcb.py` acusa repetição |
| keep-out da antena de chip pintado sobre os pads dela | nem a ligação de 1,7 mm do `E301` ao `C305` saía | o keep-out aceita, na frente, só as redes da própria antena |
| `JP301` a 0° | as linhas do U.FL e da antena de chip tinham de se cruzar na frente | girado 180° |
| vias de terra na primeira direção livre, começando por "para baixo" | as vias dos capacitores do nPM1300 cercavam o chip, e metade dos pinos não saía | a via vai para o lado de fora do CI denso mais perto |
| pad de terra sem lugar para via reta | ficava solto | saída dobrada até onde uma via cabe |
| grade de 0,15 mm num passo de 0,4 | duas saídas de terra a 0,11 mm de uma esfera do TPS610997 | toda saída de terra passa por uma conferência geométrica contra os pads reais |
| trilhos roteados por último | os trilhos largos achavam a placa cheia | uma segunda passagem roteia os trilhos primeiro, e fica a melhor |
| um passe em ordem não volta atrás | quem vinha tarde não achava caminho | *rip-up and reroute*: a rede que falhou é procurada de novo por cima do cobre que pode ser desfeito; se até 6 redes estão no caminho, elas saem, a que falhou passa primeiro, elas voltam depois, e a troca só fica se as falhas de todas juntas caírem |
| a frente "sob o receptor" fechada pelo retângulo da zona de colocação (22 × 11,5 mm) | desde que o receptor andou 2,67 mm para a direita, o retângulo cobria os pads do `J402`, os `R406` a `R409` e os pinos do lado esquerdo do próprio receptor: as cinco redes do GNSS e três catodos da luz nunca saíam | fecha só o corpo do receptor (9,7 × 10,1 mm), que é o que a 4.4 do manual do MAX-F10S pede |
| todo trilho de alimentação com 0,4 mm | o backup de 28 µA e o 3V0 dos sensores ocupavam a largura dos trilhos de carga, e eram eles que os sinais achavam no caminho | a largura sai da corrente de cada trilho ([02](../02-calculos.md#corrente-por-trilho-e-largura-de-trilha)): 0,25 mm para os de centenas de mA e para o `3V0` e os ramos dele, largura de sinal para os de microampères a dezenas de mA |
| correntes do `VSYS` e da `VBUSOUT` acima do que passa pelo pino | o `VSYS` levava somada a carga da célula, que fica dentro do nPM1300, e a `VBUSOUT` a corrente do limitador, que a chave de 7,5 Ω dela não passa | 0,5 A e 0,1 A no dry run, pela ficha do nPM1300 (PS v1.1); a `VBUSOUT` virou sinal |
| trilhos de carga com 0,4 mm e fora da camada interna | as duas margens somadas: 0,4 mm é o que a `In2.Cu` pediria (a `AL2` do dry run pedia 0,57 para os 800 mA da bateria), e por fora 0,22 bastam | `VBUS`, `VSYS`, `VBAT`, `VBAT_SYS` e `VBAT_CELULA` só nas faces de fora, com 0,25 mm, que levam 0,87 A |
| janelas de pasta tratadas como cobre | as 9 aberturas do estêncil no pad central do nPM1300 entravam na grade como cobre sem rede, em cima do próprio pad: o terra dele não saía do centro, e nenhuma trilha ou via chegava perto | ilha só de pasta não entra na grade |
| via proibida sob o pad central do nPM1300 | a `AL6` copiava para ele a proibição das fichas do TPS7A02 e do OPT3001; o pad só achava o terra se o cobre da frente passasse entre os pinos, e numa das rodadas ficou ilhado | as 9 vias em matriz de 3 × 3 do layout de referência da Nordic (PS v1.1, figuras 61 a 64); a `AL6` agora cobra as 9 |
| o NTC do corte solar sob o nPM1300 | o `RT101` seguia o `U101` e caía no verso exatamente sob o pad central: leria o calor do carregador, e os pads dele tomavam o lugar da linha do meio das vias térmicas | segue o `U103`, o colhedor cuja carga ele corta, ainda no verso e sob a célula |
| via a 0,35 mm de toda ilha | a regra guardava 0,25 mm de máscara entre a via e a ilha, mas as vias da placa saem cobertas de máscara (*tented*) e não abrem janela; o PVDD do nPM1300, entre os dois pinos de chaveamento, não tinha lugar para via | só o isolamento elétrico (0,14 mm na grade) |
| `ALRT` do MAX17262 com pull-up | a bola do meio de um WLP de 3 × 3 a 0,4 mm não tem saída pela face, e o esquemático a mandava a um pull-up que nada lê: falhava em toda rodada | ao GND, como a ficha pede quando o alerta não é usado, por uma ponte de 0,1 mm até a bola de GND vizinha (`route.PONTES`, exceção de largura no `gnssbike.kicad_dru` só dentro do `U102`); o `R110` saiu |
| nós de chaveamento depois das vias de terra | com a via a 0,2 mm da ilha, a via de terra do pino vizinho tomava o milímetro entre o pino e o indutor, e o `BUCK2_SW` falhava | os nós de chaveamento passam antes das vias de terra |
| os capacitores de sintonia da antena de chip em pé, no meio do recorte | o terra do `C305` não tinha terra ao alcance | deitados na borda, nas pontas do recorte, como nas figuras 5-1 e 8-1 da ficha da Unictron: uma ponta na faixa de 0,35 mm que vem do pino da antena, a outra no terra de fora do recorte |
| o par USB pelos canais do `D102` na ordem dos nomes | o par sai do diodo subindo com o D− à esquerda e entra no módulo descendo com o D+ a oeste; seguindo os nomes (`D2P`, `D2N`), só chegava cruzando a si mesmo, numa face em que não pode ter via, e o `USB_DP` nunca roteava | D+ pelo canal dos pinos 5 e 6, D− pelo dos 4 e 7: os quatro canais do TPD4E05U06 são o mesmo diodo ao terra |
| todo sinal cruzando a zona de energia pelo meio | a zona ocupa a largura da placa, e os sinais do módulo para o receptor, a tela, a memória e os sensores tomavam o lugar das ligações dos conversores | um corredor de sinais na borda esquerda, livre de peças nas duas faces (`make_pcb.CORREDOR_SINAIS`, decisão do dono em 2026-10-19); no roteador, as redes que cruzam a zona passam logo depois dos nós de chaveamento, e cada passo dentro da zona e fora do corredor custa 4 a mais |
| o ADP5091 a 2,2 mm do lado direito do nPM1300 | com os capacitores de `VSYS`, `VBUSOUT` e `VBUS` entre os dois, oito pinos tinham um canal de 0,65 mm para sair | a busca do lugar dele começa 9 mm à direita e 3 mm acima (`make_pcb.AFASTADO`): cai 1,5 mm mais longe, e nada mais anda |
| indutores dos conversores postos como peças soltas | o `L102` ficou com o pad de chaveamento do lado de longe, e o `BUCK2_SW` dava uma volta de 6,3 mm e não roteava | os três indutores vão como o desacoplamento, com o pad do nó de chaveamento virado para o pino `SW` (`DECOPLA`, `PINO_ALVO`); e cada chip leva o seu desacoplamento antes de o próximo ser posto (o `C113` e o `C114` caíam a 3,1 e 4,9 mm do `TPS7A02`) |
| as redes longas do módulo lado a lado na `In2.Cu` | faziam uma parede que nada cruzava | direção preferida: na `In2.Cu`, ao longo da placa, e o passo contra custa 0,5 a mais (`route.DIRECAO`); 25 para 21 soltas na mesma placa |
| o *rip-up and reroute* só aceitava troca que baixasse a conta | parava no primeiro patamar: 45 tentativas, 8 aceitas, e a varredura seguinte não pegava nada | aceita também a troca que deixa a conta igual (até 120 por passagem, nunca duas vezes para as mesmas redes): a que falhou passa, e a que abriu espaço tenta de novo na varredura seguinte |

Com as causas acima corrigidas, a melhor rodada medida do `route.py` (as
redes curtas primeiro, depois o *rip-up and reroute*: 50 tentativas, 23
aceitas, 12 delas de lado, de 26 a 11 falhas em uns 19 minutos) deixou 19
ligações sem cobre em 13 redes. Passagens extras com as redes que falharam
na frente pioraram o resultado (`PASSAGENS_EXTRA = 0`). Terra, o par USB e
as linhas de RF nunca são desfeitos. O resto fecha o `acabar.py`.

### O acabamento, uma ligação de cada vez

Pedido do dono em 2026-10-19: começar da melhor placa roteada e fechar uma
trilha de cada vez, mudando a regra até chegar a 100 %. O `acabar.py` lê a
placa como está, acha toda rede com o cobre em mais de um pedaço e tenta
cada ligação que falta, nesta ordem:

| Passo | O que faz |
|---|---|
| 1 | a busca normal da rede: a largura da classe, as camadas que ela pode usar, a placa toda |
| 2 | a regra afrouxada: a menor largura que a IPC-2221 aceita para a corrente da rede (`dry_run_pcb.CORRENTE`) numa face de fora, e depois a camada interna também, na largura que a IPC pede lá |
| 3 | por cima de quem está no caminho: a busca anda sobre o cobre que pode sair, a rede que bloqueia sai (uma de cada vez, depois todas juntas), a ligação entra e a que saiu volta pelos passos 1 e 2 |
| 4 | um corredor: todo cobre perto do caminho achado sai, a ligação passa, e o resto volta |
| 5 | as duas ou três redes que disputam um canal, rasgadas inteiras e refeitas em cada ordem |

Uma troca só fica se nada que estava ligado se soltar. Onde a grade de
0,15 mm não tem lugar, o cobre vai à mão, em coordenadas exatas, no próprio
`acabar.py`: três ligações com guia (`GUIAS`: o `PVDD` do nPM1300, o `3V0`
do `JP102` e o `DIS_SW` do ADP5091) e quatro trechos desenhados (`MAO`,
`TIRAR` e `VIAS_MAO`): os dois I²C nas bolas de 0,4 mm do MAX17262, com
0,1365 mm até o cobre mais perto, e o `VSYS` e o `VBUS` do lado direito do
nPM1300, cada um com a sua via. O par USB vai inteiro à mão, do diodo de
proteção ao módulo (`REFAZER` tira o que o `route.py` desenhou fora da área
do conector): o `route.py` roteava as duas linhas separadas, e o D− dava a
volta pela borda esquerda, com 67,6 mm contra 32,4. O cobre de outra rede
no caminho do cobre à mão sai e é religado; e as linhas de RF ficam na
largura que o `route.py` dá hoje (0,15 mm), no mesmo eixo. No fim, ele
corta o que o DRC chamaria de solto: a ponta de trilha que nada pega volta
até o último ponto que outra parte da rede toca, e a via ligada a uma
camada só sai.

Ligado e solto são medidos como o KiCad mede, e é por isso que a conta do
`acabar.py` bate com a do DRC: cobre que se sobrepõe está ligado (a ponta de
uma trilha a menos da soma das meias larguras do eixo da outra, dentro do
anel de uma via ou dentro de uma ilha, e a ilha redonda é redonda); na
regra de ponta solta, cada item conta para uma ponta só, a mais perto das
pontas *dele* e não da linha, e a comparação é estrita, em nanômetros
inteiros.

**Medido em 2026-10-19**, na placa do repositório: o DRC completo com as
malhas preenchidas dá **0 itens desconectados, 0 erros e nenhuma trilha ou
via solta**. O dry run mede as 32 regras e não viola nenhuma; duas têm
desvio aceito pelo dono, com conferência de bancada
([09](../09-dry-run-da-pcb.md#resultado-de-2026-10-19-com-o-roteamento-fechado)).
Os trilhos de carga correm com 0,25 mm nas faces de fora e estreitam a 0,15
só junto de pino: 30 estreitamentos curtos, que a `AL2` aceita.

### A pilha e o pedido de fabricação

A placa é para a JLCPCB, na pilha **JLC04081H-3313** (0,8 mm, 4 camadas),
pedida com **impedância controlada** (±10 %) e com as vias em ilha de SMD
**cheias e tampadas** (POFV, *Epoxy Filled & Capped*). As três coisas estão
em `dry_run_pcb.FABRICACAO`, que a regra `FA1` cobra, e numa nota da própria
placa em `Cmts.User`.

| O quê | Valor | De onde |
|---|---|---|
| Cobre | 35 µm nas faces de fora; 15,2 µm (0,5 oz) nas de dentro | página de impedância da JLCPCB |
| Da `F.Cu` ao plano da `In1.Cu` | prepreg 3313 de 0,0994 mm, Dk 4,05; núcleo de Dk 4,6 fecha os 0,8 mm | idem |
| Máscara | 0,5 mil sobre a trilha, 0,8 mil sobre o laminado, Dk 3,8 | idem |
| Linhas de RF | **0,15 mm: 51,1 Ω** (eram 0,196 mm, 44 Ω) | `impedancia.py`, campo 2D da seção real, conferido contra Wheeler e Hammerstad-Jensen |
| Par USB | **0,15 mm a 0,2 mm de vão: 93,6 Ω** diferenciais | idem |
| Na outra pilha de 0,8 mm (JLC7628, 0,21 mm de prepreg) | a linha de 0,15 mm daria 66 Ω | idem: o pedido tem de nomear a 3313 |
| Trilha e vão mínimos | 0,1 mm (a ponte de terra do `ALRT`) e 0,127 mm; a JLCPCB faz 0,09/0,09 | capacidades da JLCPCB |
| Via | 0,45 mm com furo de 0,25 (anel de 0,20, o recomendado); a JLCPCB faz 0,25/0,15 | idem |
| Cobre até a borda fresada | 0,31 mm no mínimo (as ilhas da antena de chip); a JLCPCB pede 0,2 | medido na placa |
| Ponte de máscara | 0,1 mm, no setup da placa; dentro de um footprint as aberturas podem se juntar (o u.FL abre uma janela sobre os próprios pads) | idem |

A janela de máscara que o footprint do u.FL (`J302`) traz sob o corpo
deixava exposto o terra despejado a 0,2 mm do pino de RF, sem máscara entre
os dois, além de uma trilha e duas vias de terra. Desde 2026-10-19 toda
janela de máscara de footprint ganha uma área proibida de cobre na face dela
(`make_pcb.janelas_de_mascara`, `SEM_COBRE_JANELA_J302`), e o plano da
`In1.Cu` continua por baixo como referência.

O `make_fab.py` gera, da placa roteada e preenchida, o que se manda para a
JLCPCB, em `hardware_gnssbike/fabricacao/` (fora do git, porque é gerado):

| Arquivo | O que é |
|---|---|
| `gnssbike-gerber-jlcpcb.zip` | Gerber das 4 camadas de cobre, máscara, pasta, serigrafia e contorno, e a furação Excellon (com e sem metalização separadas), nas opções do guia da JLCPCB para o KiCad: extensões Protel, sem X2, máscara subtraída da serigrafia, milímetros com zeros decimais |
| `bom-jdi.csv`, `cpl-jdi.csv` | BOM e posições para a tela JDI: sem o boost de 5 V nem a luz do Azumo, o `JP401` entre `COMUM` e `3V0` |
| `bom-sharp.csv`, `cpl-sharp.csv` | idem para a Sharp com a luz do Azumo: sem o `J402`, o `JP401` entre `COMUM` e `5V0` |
| `LEIA-ME.txt` | as opções do pedido (pilha, impedância, POFV, ENIG) e o que conferir na prévia |

O `JP301` e o `JP401` são três ilhas que levam um 0402 entre duas delas: o
CPL põe a peça no meio do par que a variante pede, não no centro do
footprint. O `U201` (o módulo) não existe na LCSC e vai na BOM sem código,
para consignar; o `C302`, o `JP404`, o `JP405` e o `J202` (conector SWD
ainda não escolhido) ficam sem peça.

A placa do repositório é essa rodada do `route.py` acabada pelo
`acabar.py` em vários passes, enquanto as regras e o cobre à mão entravam;
rodado de novo sobre ela, o `acabar.py` não muda nada. O `route.py` mudou
depois da rodada (o halo da via passou a ser medido do centro verdadeiro),
e o *rip-up* tem limite de tempo: rodar a cadeia do zero **não reproduz
este cobre**, e o cobre à mão, preso a coordenadas, pode não caber no novo.
Não rode o `make_pcb.py` nem o `route.py` sobre a placa roteada sem querer
refazer o roteamento: os dois reescrevem o arquivo.

A ordem importa por dois motivos. Sem `--como-esta`, o passo 5 **regera a
placa** e apaga as trilhas. E **sem o passo 4 o DRC mente**: os geradores
escrevem a zona de terra como contorno e mais nada, que é arquivo legal — o
KiCad preenche ao abrir —, mas o `kicad-cli pcb drc` **não preenche**. Numa
placa sem preenchimento todo pad que só chega ao plano aparece como
desligado e toda via de costura como solta, e nenhuma folga é medida contra
o cobre despejado, que é justamente a mais importante numa placa coberta de
terra. O `fill_zones.py` chama o preenchedor do próprio KiCad, pelo `pcbnew`,
e por isso roda com o Python do KiCad. O que isso mudou nos números, medido:
**296 ligações sem trilha caíram para 100**, e apareceram **11 erros reais**
de haste térmica insuficiente que a placa vazia escondia.

Depois, as vistas:

```sh
python hardware_gnssbike/cad/make_dxf.py     # contorno e zonas em DXF
"D:/KiCAD/bin/kicad-cli.exe" pcb export glb --output hardware_gnssbike/cad/gnssbike.glb     --include-tracks --include-zones --subst-models hardware_gnssbike/cad/gnssbike.kicad_pcb
python hardware_gnssbike/cad/make_3d.py      # as quatro vistas 3D em PNG, em docs/img/hardware/
python hardware_gnssbike/caixa/dry_run_caixa.py   # a caixa medida contra a placa: 13 regras (falha = codigo 1)
python hardware_gnssbike/caixa/make_caixa.py   # a caixa: PDF e STL em caixa/, tres vistas em docs/img/hardware/
"D:/KiCAD/bin/kicad-cli.exe" pcb export svg --output hardware_gnssbike/placa/gnssbike-2d.svg     --layers "F.Cu,In1.Cu,In2.Cu,B.Cu,F.SilkS,Edge.Cuts,F.Fab"     --page-size-mode 2 --exclude-drawing-sheet hardware_gnssbike/cad/gnssbike.kicad_pcb
python hardware_gnssbike/cad/make_2d.py      # o PDF em placa/, uma pagina por camada
```

O `make_2d.py` existe por causa do preenchimento. As quatro camadas numa
folha só eram legíveis enquanto as malhas estavam vazias; cheias, o despejo é
área sólida e **cobre as trilhas de baixo**, e a folha passa a mostrar o
contorno do cobre e quase nada mais. Quem lê uma placa lê **uma camada de
cada vez**, e é o que ele gera: quatro páginas com uma camada cada sobre o
contorno e a serigrafia, mais a quinta de conjunto.

### Por que o 3D não sai do KiCad sozinho

O `kicad-cli pcb export glb` só lê modelos **STEP**. Os corpos desenhados
aqui são VRML, então eles não entram no GLB — e o `make_3d.py` desenha esses
por conta própria, lendo o `.wrl` de cada um: a forma e a **cor** que o
arquivo traz, não uma caixa. A diferença não é estética. Enquanto ele
desenhava um paralelepípedo preto de contorno de footprint para os 23 corpos
próprios, o módulo de rádio, o receptor GNSS, as três teclas, o buzzer e o
receptáculo USB-C eram **tijolos pretos iguais** numa vista que existe
justamente para conferir se a peça é a que se pensa.

Desde que a placa foi roteada, um modelo novo não pode entrar regerando a
placa: o `make_pcb.py` coloca tudo de novo e desfaz o roteamento. O
`aplicar_modelos.py` calcula, peça por peça, o bloco `(model ...)` que o
`make_pcb.py` escreveria agora e troca só esse bloco no arquivo, no formato
do KiCad. Quando rodou pela primeira vez, em 2026-10-21, as únicas
diferenças eram os indutores `L101`, `L102` e `L401` e o boost `U402`, que
estavam sem corpo porque o KiCad não tem modelo para eles e a tabela de
alturas não os tinha. A lista das 24 caixas e de onde baixar o STEP de cada
peça está em [`3d/LEIAME.md`](3d/LEIAME.md). As imagens mostram a montagem da
JDI: sem as peças que ela não leva e com o 0 Ω do `JP301` e do `JP401` no
par de ilhas que ela usa.

**Esquemático** — os 7 arquivos abrem; o **próprio KiCad** percorre a
hierarquia e exporta o netlist; os **100 nós elétricos saem iguais** aos da
lista de nós, pino a pino; nenhum nó a mais; os 37 pinos sem ligação são
exatamente os que a lista deixa soltos; **nenhum fio passa por cima de
componente** em folha nenhuma; nada fora da folha; nenhuma peça sobre outra;
e todo rótulo hierárquico tem o pino de folha que responde por ele.

**Placa** — o KiCad abre e roda o **DRC completo, que fecha em zero erro e
zero item desconectado**, com as malhas preenchidas (2026-10-19); as **166
peças** estão lá, uma vez cada; **todo pad leva a rede da lista de nós** e
nenhuma ligação ficou sem pad; nenhum contorno sobre outro; nada passa da
borda; nada dentro das áreas de antena; a placa cabe na caixa; os três planos
de terra existem.

**Regras das fichas** ([09](../09-dry-run-da-pcb.md#resultado-de-2026-10-19-com-o-roteamento-fechado))
— na placa roteada de 2026-10-19, **32 cumpridas, 2 com desvio aceito pelo
dono** (o `RF9` e o `C210` da `AL1`), **nenhuma violada e nenhuma sem
medida**. Antes, as duas que falhavam na placa de 55 × 97 caíram quando a
placa passou a ser dimensionada pelo circuito; o `RF9` voltou em 2026-10-18,
quando o contorno da tela passou a servir às três (JDI, Sharp e Azumo) e a
sombra dela chegou a 13,4 mm do módulo:

| Regra | Na placa de 55 × 97 | Na de 34 × 90, em 2026-09-24 |
|---|---|---|
| `RF9`, os 25 mm do display | falhava: a sombra do display chegava a **11,3 mm** do módulo | **passa**: o display é mais largo que a placa e não fica mais sobre ela; o pior agora é o cabo plano `J402`, a 31,3 mm |
| `AL1`, desacoplamento | falhava: `C114` a 3,3 mm do `U104` contra 2 | **passa**: o buzzer saiu de cima do regulador e o anel dele ficou livre |

### As duas regras locais de folga

`gnssbike.kicad_dru` abre exceção em dois footprints, e só neles:

| Regra | Por quê |
|---|---|
| `J101`, furo a cobre ≥ **0,175 mm** | os furos da blindagem do receptáculo USB-C ficam a **0,1801 mm** dos próprios pads |
| `U104`, folga ≥ **0,115 mm** | o pad térmico do TPS7A02 em X2SON de 1 × 1 mm fica a **0,1196 mm** dos próprios pinos |

Os dois são land patterns de fabricante, não desenho nosso. O resto da placa
segue **0,127 mm** de folga e **0,2 mm** de furo a cobre, que é o que uma
fábrica de quatro camadas faz sem custo extra. **Nenhuma fábrica foi
consultada** sobre essas folgas; a pilha de camadas e as larguras de 50 Ω e
de 90 Ω diferencial saem da JLC04081H-3313 da JLCPCB desde 2026-10-19
([a pilha](#a-pilha-e-o-pedido-de-fabricação)).

## Serigrafia e pontos de teste

**Os dezesseis pontos de teste de [06](../06-conectores-e-pontos-de-teste.md)
estão na placa**, menos dois: o `TP111` media o `ST_STO` do AEM10900, que
saiu em 2026-09-24 (o ADP5091 não tem esse sinal), e o `TP401` mediria o
`5V0`, que até 2026-10-18 não existia com a JDI e desde então sai do boost
da Sharp (`U402`), montado em toda placa: o ponto ainda não foi criado. A
lista mora em `parts.TESTE` e o `nets.py` **recusa** um ponto que aponte
para um nó inexistente.

**As referências estão em serigrafia nas duas faces**, cada peça na sua, no
mínimo que a fábrica imprime — **0,8 mm de altura com traço de 0,15** — e
**nenhuma cai sobre outra**. Não é automático: com todas no mesmo deslocamento
de 1,8 mm acima da peça, **37 pares ficavam sobrepostos**. O `rotulos()` de
[`make_pcb.py`](make_pcb.py) procura, para cada rótulo, o lugar livre mais
perto do contorno da própria peça, com duas regras em ordem — nunca sobre
outro rótulo, de preferência fora do contorno de outra peça.

| Medida | Antes | Agora |
|---|---|---|
| Altura do texto | 0,7 mm, traço 0,1 | **0,8 mm, traço 0,15** (mínimo de fábrica) |
| Pares sobrepostos | **37** | **0**, medidos no export do KiCad |
| Referências visíveis | 128, uma delas `REF**` | **127** (o `REF**` do furo foi escondido) |

### O desenho de montagem não sai de camada nenhuma do KiCad

Nem a serigrafia nem o `F.Fab` dão um desenho de montagem legível desta placa,
e as duas razões são diferentes.

A **serigrafia** é serigrafia: obedece à fabricação, 0,6 mm de texto, e 127
designadores numa placa de 34 × 90 mm a 48 % de ocupação não cabem em volta
das peças sem cair um sobre o outro ou se afastar tanto da peça que deixam de
nomear alguma coisa.

O **`F.Fab`** carrega o que cada footprint da biblioteca resolveu pôr lá, no
tamanho que o autor daquele footprint escolheu, mais o valor — então imprime
`TP110` a 1,0 mm ao lado de `100 nF` a 0,5 e os dois por cima do contorno.

Desenho de montagem não é camada de cobre. É desenho, e pode fazer a única
coisa que a serigrafia não pode: pôr o rótulo onde se lê e traçar uma **linha
de chamada** até a peça. É o que o [`montagem.py`](montagem.py) faz — rótulo
dentro do contorno quando cabe, e fora com ponto e linha quando não cabe.
Das 127 peças, **92 usam chamada**.

### Por que a conferência é feita no export, e não aqui

O `make_pcb.py` tem um modelo de quanto espaço um rótulo ocupa, e ele estava
**30 % estreito**: 0,75 do tamanho do texto por caractere, contra os **1,06**
que o `textLength` do próprio KiCad mede. Com isso ele reportava **zero**
sobreposições enquanto a serigrafia exportada tinha **35 pares** um sobre o
outro. Havia um segundo erro por trás: o deslocamento era escolhido no
referencial da **placa**, e o KiCad gira uma propriedade de footprint pelo
ângulo do próprio footprint — um rótulo posto "acima" de uma peça girada 90°
saía ao lado dela.

Por isso o `check_pcb.py` **exporta as duas camadas de serigrafia em SVG** e
conta as sobreposições nos retângulos que o KiCad escreve, com o `textLength`
e o tamanho de fonte de cada texto. Modelo interno não confere modelo interno.

## Três footprints eram de outra peça

A conferência `ME3` compara o **corpo que a ficha de cada peça cota** com o
contorno do footprint desenhado para ela. Ao ganhar as cotas dos conectores,
ela pegou três casos em que o land pattern na placa é de uma peça **diferente
da que está na lista de compras** — e um land pattern errado não é detalhe:
a peça não solda.

| Peça da lista | Footprint que estava em uso | O que a ficha diz | Diferença |
|---|---|---|---|
| **Same Sky CPT-1117-83-SMT** (buzzer) | `Buzzer_CUI_CPT-9019S-SMT`, **redondo de 9 mm** | corpo **retangular de 11,0 × 9,0 × 1,7**, preso por duas abas de 2,0 × 0,2 mm, uma em cada ponta e em lados opostos, com furo de 0,8; padrão de solda de **duas ilhas de 2,5 × 2,5 a 10,5 mm** | **corrigido**: footprint desenhado aqui |
| **Molex 2036150003** (USB-C IPX8) | `USB_C_Receptacle_Palconn_UTC16-G` | corpo **9,99 × 8,58 × 4,21** | o footprint desenha **8,94 × 7,32**: falta **1,05 × 1,26 mm**. **Superado em 2026-09-24**: a peça virou o HRO TYPE-C-31-M-12, que tem footprint com o nome dela na biblioteca do KiCad (**EXATO**) |
| **Hirose FH28-10S-0.5SH(05)** (FPC do display) | `Hirose_FH12-10S-0.5SH`, outra série | corpo **9,90 × 5,70 × 2,55** | o footprint desenha **8,10** de largura: falta **1,80 mm**. **Superado**: a peça virou o FH12 em 2026-09-24 e o **Molex 503480-1000** em 2026-10-18, com footprint gerado aqui do desenho SD-503480-001 |

> [!CAUTION]
> **Os dois em aberto não foram consertados de propósito.** Reconstruir um
> land pattern de USB-C ou de conector FPC a partir de um desenho
> **renderizado** é exatamente o tipo de palpite que essa conferência existe
> para pegar, e errar o padrão de solda de um USB-C mata a placa. Nem a
> Molex 2036150003 nem a Hirose FH28 existem na biblioteca do KiCad 8. O que
> resolve é o arquivo de land pattern do fabricante, ou ler o desenho com
> confiança maior do que uma imagem dá. Enquanto isso, o `ME3` **falha**, e é
> para falhar.

A altura do USB-C também estava errada e foi corrigida: **4,21 mm** acima da
superfície de montagem, não os 3,26 que estavam escritos como "conferir".

> [!NOTE]
> **As três peças da tabela saíram do desenho depois**, e com elas os casos.
> O buzzer passou ao Murata PKLCS1212E4001-R1 em 2026-09-24 e ao Same Sky
> CPT-12123-81-SMT-TR em 2026-10-18, no land de 12 × 12 da ficha
> (`Buzzer_CPT-12123-81-SMT_12x12mm`). O USB-C passou ao HRO. E o conector
> do display passou ao Molex 503480-1000, de contato dos dois lados, junto
> com o `J402` (Molex 503480-0540) e o `J403` (Molex 503480-0440), os três
> gerados em `footprints.py` com a frente do lado dos pregos de fixação e o
> land na posição do contorno do desenho da Molex
> ([11](../11-revisao-do-esquematico.md#correções-aplicadas)).

## Footprints

| Origem | Quantos | O que significa |
|---|---|---|
| **EXATO** | 24 | o footprint da KiCad é desta peça |
| **ENCAPSULAMENTO** | 125 | mesmo encapsulamento, desenhado para outra peça. Cabe, e **ainda tem de ser conferido contra a ficha** |
| **GERADO** | 18 | não existe na KiCad; gerado aqui a partir das dimensões, com o land pattern **aproximado** |

São 167 peças com footprint em 2026-10-19 (`python footprints.py`); a tabela
dava 10, 92 e 10 antes das trocas de peça de 2026-09-24 e das duas telas.

O **módulo de rádio** é o `MinewSemi ME54BS13` (nRF54LM20A), 16,5 × 12,0 ×
2,4 mm, **80 pads**: 20 castelados nas laterais e uma matriz LGA de 60 no
miolo, 64 GPIO. O footprint foi gerado das cotas do desenho mecânico da ficha
V1.0.0, porque **a Minew não publica land pattern** — a ficha diz para pedir o
dela.

> [!CAUTION]
> **Os dois desenhos da Minew discordam do espelhamento.** O da ficha V1.0.0
> põe os pinos 1–10 (RF, SWD, USB) na lateral **esquerda**; o da V0.5.0 e o da
> página do produto põem na **direita**. Este projeto seguiu a V1.0.0, que é a
> mais nova e a única com números de pino. **Confira num módulo real antes de
> fabricar**: pelo desenho adotado, os três GND da primeira linha da matriz
> (D0, E0, F0) ficam do **mesmo lado do VCC (pino 19)**. Se estiverem do lado
> do SWD, o footprint está espelhado.

Oito peças **não estão na placa**: as duas telas, `DS401` e `DS402`, que
vão no `J401` uma de cada vez, e os seis módulos solares `PV101` a `PV106`,
que moram na caixa e chegam pelo `J103`, um JST ZH de 4 vias. Até 2026-09-24
a antena `E301` também morava fora, e os módulos chegavam por três pares de
pads de mola (`J103` a `J105`); desde então a antena é de chip, soldada na
borda, o `J302` é um u.FL para uma antena externa, e não há mais mola a
escolher.

## De onde vem cada coisa

```mermaid
flowchart LR
    DOCS["hardware_gnssbike/<br/>01 esquematico · 03 netlist<br/>04 placa e caixa · 05 materiais"]
    DOCS --> PARTS["parts.py<br/>pecas e pinagem"]
    DOCS --> NETS["nets.py<br/>ligacoes"]
    DOCS --> DXF["make_dxf.py<br/>contorno e zonas"]
    PARTS --> SCH["make_sch.py + blocos.py + sheets.py"]
    NETS --> SCH
    PARTS --> PCB["make_pcb.py + make_pro.py"]
    NETS --> PCB
    DXF --> PCB
    FP["footprints.py + fp_load.py"] --> PCB
    SCH --> CS["check_sch.py"]
    PCB --> CP["check_pcb.py"]
    PCB --> D3["make_3d.py<br/>GLB do KiCad renderizado em numpy"]
```

Nenhum arquivo do KiCad é editado à mão: tudo sai dos geradores, e os
geradores leem os documentos. Editar no KiCad e regerar apaga a edição — o
lugar de mudar é o gerador.

## O que falta

- [x] **Terminar o roteamento.** Fechado em 2026-10-19: 0 itens
      desconectados e 0 erros no DRC completo ([acima](#o-acabamento-uma-ligação-de-cada-vez)).
      As linhas de RF têm rotina própria, porque impedância e caminho delas
      não são negociáveis.
- [x] **Os 25 mm entre o módulo de rádio e o display** (`RF9`): aceito
      pelo dono em 2026-10-19 como desvio, a conferir na bancada (RSSI e taxa
      do BLE com a tela atualizando).
- [x] **O desenho cotado do conector do display.** O Hirose FH28 saiu; os
      três conectores do display são Molex 503480 desde 2026-10-18, com o
      land do desenho SD-503480-001. O que fica em aberto é a dobra das fitas
      até a boca deles, que só a tela na mão confirma.
- [ ] Conferir cada footprint **ENCAPSULAMENTO** contra a ficha da peça; onze
      já estão conferidos, com a cota do desenho mecânico em
      `footprints.PACOTE` e a conferência automática em `ME3`.
- [ ] Pedir à Minew o land pattern oficial do ME54BS13. Os footprints do
      Molex 2036150003, do Hirose FH28 e do Molex 503480-0500 deixaram de
      ser necessários: as peças saíram do desenho.
- [x] ~~Escolher a mola dos contatos do painel solar e da antena~~: o painel
      chega pelo `J103`, um JST ZH, e a antena é a de chip ou o u.FL `J302`.
- [x] A pilha de camadas do fabricante, e com ela as larguras de 50 Ω e 90 Ω: JLC04081H-3313, 2026-10-19.
- [ ] Encurtar as amarrações entre os contatos duplicados do USB-C (5,4 mm no D+, 7,7 mm no D−), que fazem a
      diferença de comprimento do par depender do lado do plugue; e medir o USB High-Speed na bancada.
- [ ] As pendências que os documentos já carregam: o lado do sensor do
      MAX17262, a polaridade do LED do Azumo (jumpers `JP402` a `JP405`) e a
      dobra das fitas das telas. A ordem das cinco vias da luz da JDI saiu da
      spec 1.4.2, e as pendências do AEM10900 e da tecla central caíram com
      as decisões de 2026-09-24 e de 2026-09-26.
