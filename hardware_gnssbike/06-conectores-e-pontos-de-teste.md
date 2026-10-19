# Conectores e pontos de teste

Todo conector e todo ponto de teste da placa, **contato a contato**. As
[folhas do esquemático](01-esquematico.md) dizem quais sinais existem e a
[lista de nós](03-netlist.md) diz de onde cada um sai e aonde chega;
nenhuma das duas diz **em que contato** de cada conector o sinal entra. É
essa lacuna que este documento fecha, porque ela é a forma clássica de
inverter a polaridade da bateria na primeira montagem — e de descobrir
isso pelo cheiro.

**Nesta página:** [Como ler](#como-ler) · [De onde vem cada pinagem](#de-onde-vem-cada-pinagem) · [J101 · USB-C](#j101--usb-c) · [J102 · Bateria](#j102--bateria) · [J201 · Depuração SWD](#j201--depuração-swd) · [J401 · Display](#j401--display) · [J402 · Luz da JDI](#j402--luz-da-jdi) · [J403 · Luz do Azumo](#j403--luz-do-azumo-no-verso) · [JP401 · Tensão do display](#jp401--tensão-do-display) · [JP101 · Jumper de medição de corrente](#jp101--jumper-de-medição-de-corrente) · [Pontos de teste](#pontos-de-teste) · [O que falta conferir](#o-que-falta-conferir)

> [!WARNING]
> **Nada disto foi montado, medido ou fabricado.** Não existe placa, não
> existe layout, nenhum conector foi encaixado e nenhum ponto de teste foi
> tocado por ponta de prova. Cada pinagem abaixo vem de uma norma, de uma
> ficha de fabricante ou de uma escolha feita aqui, e **está marcada como
> tal**. O que não foi encontrado em fonte nenhuma está dito como
> faltando, não preenchido por simetria.

## Como ler

- **Contato**: o número do contato como o desenho do fabricante o numera.
  Onde o fabricante usa nome em vez de número (o USB-C usa `A1`, `B5`), o
  nome é o do fabricante.
- **Direção**: do ponto de vista **da placa**. `entrada` é o que chega,
  `saída` é o que a placa aciona, `bidir` é linha compartilhada, `alim` é
  alimentação ou retorno.
- **Origem da pinagem**: cada seção diz, logo no começo, se a ordem dos
  contatos é **padrão da indústria**, veio da **ficha do fabricante** ou é
  **escolha deste projeto**. As três coisas têm graus de confiança
  diferentes e quem for fabricar a placa precisa saber qual é qual.
- Os designadores (`J101`, `JP401`, `TP203`) seguem o esquema por folha
  já usado em [05](05-materiais.md#como-ler): a centena é o número da
  folha do esquemático.

```mermaid
flowchart LR
    subgraph EXT["Fora da placa"]
        CABO["cabo USB-C"]
        PACK["pack LiPo 1S<br/>PCM e NTC"]
        PAINEL["painel JDI LPM027M128C<br/>2,7&quot;, com luz integrada"]
        SONDA["J-Link<br/>cabo TC2030-CTX-NL"]
    end
    CABO --> J101["J101 · USB-C<br/>Molex 2036150003<br/>16 contatos"]
    PACK --> J102["J102 · bateria<br/>JST SM06B-SRSS-TB<br/>6 vias"]
    PAINEL --> J401["J401 · display<br/>Molex 503480-1000<br/>10 vias, JDI ou Sharp"]
    PAINEL -.->|"por onde?"| LUZ["conector da luz<br/>não definido"]
    SONDA --> J201["J201 · SWD<br/>Tag-Connect TC2030-NL<br/>6 pads, sem peça"]
    SONDA --> J202["J202 · SWD<br/>Cortex Debug 2 × 5, 1,27 mm<br/>adaptador de 9 vias do J-Link"]
    J101 --> F1["folha 1 · energia"]
    J102 --> F1
    J201 --> F2["folha 2 · MCU"]
    J202 --> F2
    J401 --> F4["folha 4 · display"]
    LUZ -.-> F4
```

## De onde vem cada pinagem

| Conector | Origem da pinagem | Quem fecha o assunto |
|---|---|---|
| `J101` USB-C | **padrão da indústria**: USB Type-C, receptáculo de 16 contatos (USB 2.0) | a norma define os sinais; o desenho da Molex dá o número do pad no footprint |
| `J102` bateria | **escolha deste projeto** — não existe padrão para isto | o fabricante do pack, antes de fechar o pedido |
| `J201` depuração | **conferido em 2026-09-25** na ficha `TC2030-CTX_1.pdf` da Tag-Connect: 1 `VCC`, 2 `SWDIO`, 3 `nRESET`, 4 `SWCLK`, 5 `GND`, 6 `SWO`. O projeto tinha o reset no 6 e foi corrigido | ficha oficial da Tag-Connect |
| `J202` depuração, conector | **padrão da ARM** (Cortex Debug Connector, 10 vias): 1 `VTref`, 2 `SWDIO`, 3 `GND`, 4 `SWDCLK`, 5 `GND`, 6 `SWO`, 7 chave, 8 TDI, 9 `GNDDetect`, 10 `nRESET`; é a pinagem do adaptador Cortex-M de 9 vias do J-Link | a norma; a peça (Samtec FTSH-105-01-L-DV-K ou equivalente) ainda não foi escolhida |
| `J401` display | **ficha do fabricante**, e a mesma ordem nas duas telas | fichas JDI LPM027M128B Ver.01 e Sharp LS027B7DH01A (LD-28305A) |
| Conector da luz do JDI | **falta tudo**: não se sabe sequer se ele existe como conector separado ou se o C traz um FPC maior | ficha do **LPM027M128C**, ou uma amostra |
| `JP401` tensão do display | **escolha deste projeto** | este documento |
| `JP101` medição | **escolha deste projeto**, e peça que ainda não existe na lista | o dono |

## J101 · USB-C

**Molex 2036150003**, receptáculo IPX8, SMT, USB 2.0, **16 contatos**
([14](../docs/14-hardware-placa-nova.md#componentes-principais),
[15](../docs/15-avaliacao-componentes.md#usb-c-e-proteção)).

**A pinagem é padrão da indústria.** Ela não é escolha deste projeto nem
da Molex: é a norma do USB Type-C, e todo receptáculo a segue. Um
receptáculo USB 2.0 de 16 contatos é o de 24 contatos **sem os oito da
faixa de alta velocidade** (`A2`, `A3`, `A10`, `A11`, `B2`, `B3`, `B10` e
`B11`), que só existem para USB 3.x. Sobram estes:

| Contato | Sinal | Direção | Nota |
|---|---|---|---|
| `A1` | `GND` | alim | retorno |
| `A4` | `VBUS` | entrada | 4,0 a 5,5 V; ao `VBUS` do nPM1300, com o TVS **ESD761** junto do conector |
| `A5` | `CC1` | bidir | **direto** ao `CC1` do nPM1300, com o **TPD4E05U06**; o Rd de 5,1 kΩ é interno ao PMIC |
| `A6` | `D+` | bidir | unido ao `B6`; par de 90 Ω até o pad 8 do ME54BS13 |
| `A7` | `D−` | bidir | unido ao `B7`; par de 90 Ω até o pad 7 do ME54BS13 |
| `A8` | `SBU1` | — | **não usado**, deixado aberto |
| `A9` | `VBUS` | entrada | mesmo nó do `A4` |
| `A12` | `GND` | alim | retorno |
| `B1` | `GND` | alim | retorno |
| `B4` | `VBUS` | entrada | mesmo nó do `A4` |
| `B5` | `CC2` | bidir | **direto** ao `CC2` do nPM1300, com o TPD4E05U06 |
| `B6` | `D+` | bidir | unido ao `A6` |
| `B7` | `D−` | bidir | unido ao `A7` |
| `B8` | `SBU2` | — | **não usado**, deixado aberto |
| `B9` | `VBUS` | entrada | mesmo nó do `A4` |
| `B12` | `GND` | alim | retorno |
| carcaça | `SHIELD` | alim | ao `GND`, direto, junto com as abas de fixação |

**Os quatro `VBUS` são um nó só e os quatro `GND` também.** Isso não é
economia: é o que faz o cabo funcionar nas duas orientações, e é também o
que divide os até 1,5 A do `VBUS` ([02](02-calculos.md#corrente-por-trilho-e-largura-de-trilha))
por quatro contatos.

**Os dois `D+` são unidos na placa, e os dois `D−` também.** A norma manda
exatamente isso num receptáculo USB 2.0: como o cabo pode entrar de
qualquer lado, o par ativo cai ora na faixa A, ora na B, e o aparelho não
tem como saber qual é. Unir os dois resolve sem multiplexador.

**O `CC1` e o `CC2` vão ao nPM1300 sem nada no meio além do ESD.** O
resistor Rd de 5,1 kΩ que anuncia "sou um consumidor" é **interno** ao
PMIC ([14](../docs/14-hardware-placa-nova.md#ligações-fixas-dos-cis)).

> [!CAUTION]
> **Não acrescente Rd externo no `CC1` e no `CC2`.** É o erro quase
> automático de quem já projetou USB-C com outro CI: os 5,1 kΩ de fora em
> paralelo com os 5,1 kΩ de dentro dão cerca de 2,6 kΩ, que **não é Rd
> nem Ra** — cai no meio da faixa indefinida da norma. O resultado não é
> "carrega mais devagar", é a fonte podendo nunca ligar o `VBUS`, e um
> aparelho que não carrega sem que nada esteja queimado.

**O `SBU1` e o `SBU2` ficam abertos, e não no `GND`.** São as linhas de
banda lateral, que um acessório de áudio ou de vídeo aciona; aterradas,
seriam saída contra terra.

## J102 · Bateria

**JST SM06B-SRSS-TB** na placa e **SHR-06V-S-B** no cabo, com terminais
SSH-003T-P0.2 (a conferir no catálogo eSH ao fechar o pedido). Série SH,
passo de 1,0 mm, 6 vias, **1 A por contato**, entrada lateral, **2,9 mm de
altura**. Até 2026-09-26 era o GH SM06B-GHS-TB, escolhido pela trava
positiva ([19](../docs/19-lista-de-compras.md#trocas)); a caixa desenhada
nesse dia deixa **4,2 mm** entre a face da placa e a tampa fora do display,
e o GH tem 4,25. O SH cabe e perde a trava: o cabo da célula, curto, é preso
à caixa com fita (a alternativa é a caixa 0,5 mm mais alta, com o GH:
decisão do dono). Fica **na frente**, em pé na borda esquerda do bolso entre
o SWD `J202` e o USB-C, a 270°: o footprint do KiCad tem as ilhas de sinal em
y −2 e a boca do lado oposto, e a rotação manda a boca para a esquerda, para
a parede da caixa, por cima do diodo `D102`; a célula fica debaixo da placa.

> [!IMPORTANT]
> **A pinagem abaixo é escolha deste projeto.** Não existe padrão de
> indústria para conector de bateria: cada fabricante de pack monta o cabo
> como o cliente pedir, e é o cliente que erra. A proposta abaixo é
> argumentada contato a contato justamente para poder ser discutida com o
> fabricante do pack antes de a placa ir para fabricação.

### O que o conector precisa levar

Quatro sinais, em seis vias:

- **`VBAT+`** e **`GND`**, o par de potência;
- **`NTC_BAT`**, o NTC do pack, que vai ao pino `NTC` do nPM1300 e serve
  ao perfil JEITA de 0, 10, 45 e 60 °C;
- **`TH_MON`**, um **segundo NTC**, que corta a carga solar fora de 0 a
  45 °C. Desde 2026-09-26 quem o lê é o comparador `U105` (TLV7031), porque
  o ADP5091 que substituiu o AEM10900 não tem entrada de temperatura: o
  divisor `RT101`/`R106` é alimentado pelo próprio painel e a saída do
  comparador leva o `DIS_SW` para cima pelo `D106`
  ([01](01-esquematico.md#folha-1--energia), [03](03-netlist.md#nós-de-alimentação)).

### Proposta

| Contato | Sinal | Direção | Nota |
|---|---|---|---|
| 1 | `VBAT+` | alim, nos dois sentidos | com o contato 6; positivo da célula, **depois** do PCM do pack |
| 2 | `GND` | alim | com o contato 5; retorno do par 1–2 |
| 3 | `NTC_BAT` | entrada analógica | ao pino `NTC` do nPM1300; 10 kΩ, B3380, referenciado ao negativo da célula |
| 4 | `TH_MON` | entrada analógica | **via reservada e não populada** na montagem escolhida: o NTC do `TH_MON` é o `RT101`, na face de trás da placa ([05](05-materiais.md#folha-1--energia)). Montar os dois mata a carga solar |
| 5 | `GND` | alim | com o contato 2; retorno do par 5–6 |
| 6 | `VBAT+` | alim, nos dois sentidos | com o contato 1 |

### Por que assim

**`VBAT+` e `GND` em dois contatos cada, por causa dos 600 mA de carga.**
O pior caso do nó `VBAT` é a carga pelo USB, 600 mA
([02](02-calculos.md#corrente-por-trilho-e-largura-de-trilha)), e o
contato da série GH vale 1 A. Um contato só daria 1,67 vezes de folga, que
é pouco para uma peça que o usuário vai desconectar e reconectar. Dois dão
3,3 vezes. E há um segundo motivo, menos óbvio: a resistência de contato
fica **em série com a célula**, antes do pino `BATT` do MAX17262, de modo
que a queda sobre ela entra direto na tensão que o medidor usa para
estimar a carga. Dobrar os contatos corta essa queda pela metade.

**A ordem é um palíndromo, de propósito.** Lida ao contrário — que é
exatamente o que acontece quando o fabricante do pack numera o cabo pela
face oposta —, `VBAT+, GND, NTC, NTC, GND, VBAT+` continua sendo
`VBAT+, GND, NTC, NTC, GND, VBAT+`. Um cabo espelhado troca **só os dois
NTC entre si**, e os dois são a mesma peça, 10 kΩ com B3380, na mesma
célula: o nPM1300 passa a ler o NTC do AEM e vice-versa, e os dois
continuam funcionando. Com a ordem "natural"
(`VBAT+, VBAT+, GND, GND, NTC, NTC`), o mesmo cabo espelhado poria **4,2 V
no pino `NTC` do nPM1300 e no `TH_MON` do AEM10900**, e os dois CI
morreriam antes de alguém medir coisa alguma.

**Cada par de potência fica junto.** Os contatos 1–2 e 5–6 são dois pares
`VBAT+`/`GND` adjacentes: duas espiras de corrente estreitas em vez de uma
larga, o que importa porque essa é a única espira de 600 mA da placa.

**Os dois NTC têm de compartilhar o negativo da célula.** É isso que faz
os quatro sinais caberem em seis vias: um fio para cada termistor, com a
outra ponta no negativo do pack. Se o fabricante entregar um pack com os
termistores isolados, cada um passa a precisar de duas vias e **seis não
bastam** — aí o conector muda para oito vias e este documento junto.

> [!CAUTION]
> **O NTC do pack e o NTC de placa não podem conviver.** A [lista de
> compras](../docs/19-lista-de-compras.md#energia) aprovou um **TDK
> NTCG103JF103FT1 na face de trás da placa, sob a célula**, e a
> [avaliação](../docs/15-avaliacao-componentes.md#carga-usb-c-e-painel-solar)
> trata o segundo NTC do pack e o NTC de placa como alternativas — "vem do
> pack **ou** é um TDK". Montar os dois põe dois termistores de 10 kΩ em
> paralelo no `TH_MON`, isto é, 5 kΩ. **Conta feita aqui:** com B3380 e
> 10 kΩ a 25 °C, 5 kΩ correspondem a **44,4 °C**, e o limite superior do
> AEM10900 é 45 °C. O aparelho sairia da bancada com a carga solar
> desligada a qualquer temperatura ambiente acima de uns 25 °C, sem
> mensagem nenhuma, porque **nada disso passa por firmware**. Ou se monta
> o NTC de placa e as vias 4 do conector e do cabo ficam vazias, ou se usa
> o do pack e o footprint do TDK fica sem peça. **Escolher é decisão do
> dono.**

> [!NOTE]
> **O `VBAT+` que chega ao contato 1 já saiu do PCM do pack.** Quando a
> proteção abre por subtensão ou por curto, a placa vê a célula
> desaparecer, não vê a célula descarregada. É o comportamento correto — o
> nPM1300 não tem UVLO de bateria, e é justamente por isso que o PCM é
> obrigatório ([15](../docs/15-avaliacao-componentes.md#bateria)) —, mas
> muda o que se espera na bancada.

## J103 · Painel solar

**JST S4B-ZR-SM4A-TF** na placa e **ZHR-4** no cabo, com terminais
SZH-002T-P0.5. Série ZH, passo de 1,5 mm, 4 vias, SMD lateral, com trava.

> [!IMPORTANT]
> **Os seis módulos não são soldados na placa, e não há um conector por
> módulo.** Eles ficam nas paredes da caixa, virados para o sol — dois na
> face inclinada e dois em cada chanfro —, enquanto a placa fica dentro,
> atrás do display. Entre os dois há **um chicote**, e ele entra na placa
> por **um único conector de 4 vias**. Quem junta os módulos é o chicote,
> não a placa.

### Pinagem

| Contato | Sinal | O que chega nele |
|---|---|---|
| 1 | `PV_A` | o **positivo** dos dois módulos da **face inclinada** (`PV101`, `PV102`), já unidos no chicote |
| 2 | `PV_B` | o **positivo** dos dois módulos do **chanfro esquerdo** (`PV103`, `PV104`), já unidos |
| 3 | `PV_C` | o **positivo** dos dois módulos do **chanfro direito** (`PV105`, `PV106`), já unidos |
| 4 | `GND` | o **negativo dos seis**, todos unidos num fio só |

### Por que quatro vias e não doze

Os seis módulos ficam **em paralelo**, e isso não é escolha de arranjo: é
obrigação. Cada KXOB25-05X3F já tem 3 células em série e abre em **2,07 V**,
e o MPPT do colhedor rastreia de 0,12 a **2,73 V**. Dois módulos em série
dariam 4,14 V, 1,4 V acima do teto
([04](04-pcb-e-caixa.md#como-os-painéis-chegam-à-placa)).

Em paralelo, os seis negativos são **o mesmo nó elétrico**. Levar seis fios
de terra até a placa seria repetir o mesmo nó seis vezes: mais crimpagem,
mais conector, e um corpo de 6 vias que tem 13,5 mm e faria sombra no sensor
de luz ambiente, que pede o dobro da própria altura livre em volta.

Os positivos poderiam ser um só pela mesma lógica, mas são **três**, um por
face, de propósito: na placa cada um passa por um resistor de 0 Ω
(`R113`, `R114`, `R115`) antes de se juntarem. Abrindo um deles, mede-se
**uma face sozinha no sol**. Com um fio só, os seis dariam um número só e
nunca se saberia qual face está rendendo.

### Como montar o chicote

São **12 pontos de solda nos módulos** e **4 terminais crimpados** no
conector.

![Como ligar os seis módulos solares na placa: onde fica cada módulo na caixa, o chicote desenrolado com os 12 pontos de solda e as 4 emendas, e o conector J103 na placa com os três resistores de 0 Ω](../docs/img/chicote-solar.svg)

O desenho acima está **em escala** e sai de
[`tools/docs/solar_harness_drawing.py`](../tools/docs/solar_harness_drawing.py):
os módulos têm os 23 × 8 mm de verdade, o conector tem o passo de 1,5 mm de
verdade, e o `J103` está na posição que ocupa na placa. Ele é a folha para
levar à bancada.

Passo a passo:

1. **Solde um par de fios em cada módulo**, no `+` e no `−` da serigrafia do
   verso. A ficha do KXOB25-05X3F **não numera os terminais**: ela só marca
   `+` e `−` no desenho do verso, e é essa marca que vale.
2. **Una os dois positivos de cada face** numa emenda, e leve um fio dessa
   emenda ao conector. São três emendas, uma por face.
3. **Una os seis negativos** numa emenda só, e leve um fio dela ao contato 4.
4. **Crimpe os quatro fios** nos terminais SZH-002T-P0.5 e encaixe na
   carcaça ZHR-4, na ordem da tabela acima.

**Fio:** a corrente máxima do arranjo inteiro é **110 mA**, e a de uma face
é 37 mA. O terminal SZH-002T-P0.5 aceita de **AWG 32 a 28**; AWG 28 é o que
usar, com folga de sobra em corrente e a flexibilidade que um fio dentro de
caixa precisa.

> [!WARNING]
> **Confira a polaridade com um multímetro antes de plugar, com os módulos
> no sol.** O conector certo já impede plugar a bateria aqui — a célula usa
> um **JST GH de 1,25 mm** e os dois não entram um no outro, de propósito
> ([J102](#j102--bateria)) —, mas **fio invertido no crimp é erro de
> bancada, não de projeto**. O `D105` na entrada é o grampo que existe para
> esse caso, e **a peça dele ainda não foi escolhida**: até escolher, a
> proteção é o multímetro.

> [!NOTE]
> **A ordem das quatro vias no chicote ainda não foi conferida contra um
> conector real.** A tabela acima é a ordem dos pinos do footprint; confirme
> o contato 1 na marca da carcaça antes de crimpar.

## J201 · Depuração SWD

**Footprint Tag-Connect TC2030-NL**: só furos e pads, **nenhuma peça
montada** ([05](05-materiais.md#folha-2--mcu)). O cabo é o
**TC2030-CTX-NL**, preso pelo **TC2030-CLIP**, e termina no conector
Cortex de 10 vias do J-Link ([14](../docs/14-hardware-placa-nova.md#teste-e-bring-up),
[19](../docs/19-lista-de-compras.md#placas-de-avaliação-e-ferramentas)).

A pinagem abaixo é a da ficha oficial **`TC2030-CTX_1.pdf`** da
Tag-Connect, tabela *Connections*, conferida em 2026-09-25.

| Contato | Sinal | Direção | Nota |
|---|---|---|---|
| 1 | `VCC` / `VTref` | saída | ao **`3V0`**; é o alvo informando à sonda em que tensão falar |
| 2 | `SWDIO` / `TMS` | bidir | pad **5** do ME54BS13 |
| 3 | **`nRESET`** | entrada | pad **4** do ME54BS13; a sonda puxa para baixo |
| 4 | `SWCLK` / `TCK` | entrada | pad **6** do ME54BS13 |
| 5 | `GND` | alim | também é o `GNDDetect` do lado da sonda |
| 6 | `SWO` / `TDO` | saída | **sem ligação hoje.** É trace do alvo para a sonda; levá-lo a um pad do módulo daria `printf` por ITM no bring-up, e falta descobrir qual pad do ME54BS13 expõe o `SWO` |

> [!WARNING]
> **Esta tabela mudou em 2026-09-25, e a versão anterior era um erro de
> placa.** Ela trazia `GND` no contato 3 e `nRESET` no 6 — o arranjo
> `1 VCC, 2 SWDIO, 3 GND, 4 SWCLK, 5 GND, 6 SWO` é a numeração do
> **cabeçalho Cortex de 10 vias, do lado da sonda**, e não a do footprint
> de 6 pinos da placa. Quem mistura as duas chega exatamente ao que estava
> escrito aqui.
>
> Fabricada assim, a placa amarraria o `nRESET` dreno aberto da sonda ao
> **terra em cobre** e o alvo ficaria **travado em reset para sempre**. A
> versão do `parts.py`, que punha o reset no contato 6, era menos grave e
> ainda assim fatal para a depuração: o `nRESET` da sonda não acionaria
> nada e o reset do micro ficaria pendurado numa **entrada** da sonda —
> nada queima, e o depurador nunca reseta o alvo. Num nRF54LM20A que
> reinicia pelo `task_wdt` ou reconfigura os pinos de SWD, isso tira a
> saída de emergência e sobra só o apagamento total por CTRL-AP.
>
> O aviso que estava aqui previa exatamente esta variante. Era ela.

**Sem o `VTref` a maioria das sondas recusa conectar.** Elas o usam para
descobrir a tensão de I/O e, em muitos modelos, como prova de que existe
um alvo alimentado do outro lado. Ele custa um contato e evita uma tarde
de depuração.

> [!IMPORTANT]
> **O `VTref` está no `3V0`, que sai do BUCK2 — e o BUCK2 está desligado
> em ship mode.** Consequência prática para o bring-up: com a placa
> desligada, a sonda não enxerga alvo nenhum. Antes de encostar o clipe é
> preciso acordar o aparelho (tecla central no `SHPHLD`, ou cabo USB). Não
> é defeito; é o que acontece quando se referencia o `VTref` a um trilho
> comutado em vez do `VSYS`, e a alternativa teria o custo de expor a
> sonda a 5,5 V com cabo ligado.

**O footprint pede espaço.** O TC2030-NL não tem pernas de retenção, o que
é o motivo de existir o clipe: a placa precisa da área livre em volta dos
pads e dos três furos de alinhamento, nos dois lados, para o clipe
encaixar ([04](04-pcb-e-caixa.md#posicionamento)).

## J202 · Depuração SWD, o conector

Em 2026-09-26 o dono pediu o conector pequeno de SWD do J-Link, e ele entrou
**ao lado** do Tag-Connect, nas mesmas redes: o **Cortex Debug Connector**
de 10 vias, 2 × 5 a 1,27 mm, SMD, **sem carcaça** (2,5 mm de altura; o
blindado, Samtec FTSH-105-01-L-DV-K com 4,83 mm, não cabe sob a tampa da
caixa proposta, que deixa 3,6 mm da face da placa). **A peça ainda não foi
escolhida**, e o footprint é o genérico do KiCad, a conferir contra a ficha
dela. É nele que o adaptador Cortex-M de 9 vias do J-Link encaixa, sem
clipe (sem carcaça não há chave: o pino 7 fica sem pino, como manda a
pinagem). Só um dos dois é usado de cada vez.

| Pino | Sinal | Rede | Nota |
|---|---|---|---|
| 1 | `VTref` | `3V0` | o mesmo aviso do `J201`: o `3V0` sai do BUCK2, desligado em ship mode |
| 2 | `SWDIO` | `SWDIO` | pad 5 do ME54BS13 |
| 3 | `GND` | `GND` | — |
| 4 | `SWDCLK` | `SWDCLK` | pad 6; `passive` no ERC, porque o `J201` já declara a saída |
| 5 | `GND` | `GND` | — |
| 6 | `SWO` | aberto | a mesma pendência do `J201`: qual pad do módulo expõe o `SWO` |
| 7 | chave | não existe | o conector blindado tem o pino 7 removido |
| 8 | TDI | aberto | só JTAG; o nRF54LM20A é SWD |
| 9 | `GNDDetect` | aberto | a sonda o lê para saber se há alvo; aberto, como nas placas de referência da Nordic |
| 10 | `nRESET` | `MOD_RESET` | pad 4; `passive` no ERC pelo mesmo motivo do 4 |

**Custa 8 × 6 mm de placa** (pads mais courtyard) e, montado, 2,5 mm de
altura, mais o cabo com o aparelho aberto — é o que
[01](01-esquematico.md#folha-2--mcu) chamava de volume gasto por
uma coisa de protótipo, e é o que o dono quis. Onde ficou na placa está em
[04](04-pcb-e-caixa.md#posicionamento).

## J401 · Display

**Molex 503480-1000** (LCSC C127355), 10 vias, passo de 0,5 mm, ZIF com
atuador traseiro, 1,0 mm de altura e **contato dos dois lados**. Desde
2026-10-18 serve **as duas telas**: a JDI LPM027M128C e a Sharp
LS027B7DH01A, nua ou como o módulo Azumo 11103-06, que é a Sharp com luz
frontal ([11](11-revisao-do-esquematico.md)).

**Um conector só, porque as duas fitas caem no mesmo lugar.** A fita de
sinal das duas telas sai do meio da borda longa, com o centro no centro da
área ativa, e a área ativa das duas é a mesma (58,8 × 35,28 mm). Dois
conectores ali se sobreporiam. **A ordem dos dez pinos também é a mesma**
nas fichas da JDI (spec 1.4.1) e da Sharp (LCP-2110015A e LD-28305A):
`SCLK`, `SI`, `SCS`, `EXTCOMIN`, `DISP`, `VDDA`, `VDD`, `EXTMODE`, `VSS`,
`VSSA`. O que muda entre as duas é a tensão, e quem a escolhe é o
[`JP401`](#jp401--tensão-do-display).

**Contato dos dois lados, porque a fita chega dobrada.** O vidro fica sobre
a placa, colado sob a tampa, e passa da borda esquerda; a fita sai do vidro
para fora, dobra para baixo e volta por baixo dele até o conector, e chega
com os contatos para cima. O Hirose FH12 de antes tinha contato por baixo,
e nenhum contato encostava (`display-a1` da revisão). O 503480 aceita a fita
dos dois jeitos, com 2 mm de condutor exposto e 3 mm de reforço no mínimo:
a JDI tem 3,8 mm e a Sharp 3,5.

| Onde | Valor |
|---|---|
| Posição | boca virada para a borda esquerda, em x = 1,27 mm; centro em y = 31,5 mm |
| Alcance da fita | a boca pode ficar de 0 a 1,87 mm da borda com a JDI e de 0,57 a 2,17 mm com a Sharp |
| Pino 1 | o pino 1 das duas fitas cai na ponta do lado das teclas (y maior), que é o **circuito 10** do conector: a fita entra pelo lado dos pregos de fixação, e o circuito 1 da Molex fica na outra ponta |

| Pino da tela | Circuito do J401 | Sinal | Direção | Nota |
|---|---|---|---|---|
| 1 | 10 | `SCLK` | saída | `DISP_SCK`, P3.03, `spi22`; 1 MHz típico, 2 MHz máximo |
| 2 | 9 | `SI` | saída | `DISP_MOSI`, P3.00 |
| 3 | 8 | `SCS` | saída | `DISP_CS`, P3.02, **ativo alto**; pull-down de 100 kΩ ao `GND` |
| 4 | 7 | `EXTCOMIN` | saída | `DISP_EXTCOMIN`, P3.06; a Sharp pede de 1 a 20 Hz, a JDI aceita até 140 Hz |
| 5 | 6 | `DISP` | saída | `DISP_ON`, P3.05; liga a matriz; pull-down de 100 kΩ |
| 6 | 5 | `VDDA` | alim | do `JP401`: 3,0 V para a JDI, 5,0 V para a Sharp |
| 7 | 4 | `VDD` | alim | do `JP401`, o mesmo trilho do pino 6 |
| 8 | 3 | `EXTMODE` | alim | ao `VDD` em cobre: alto, o VCOM inverte nas bordas do `EXTCOMIN` nas duas telas |
| 9 | 2 | `VSS` | alim | `GND` |
| 10 | 1 | `VSSA` | alim | `GND` |

A entrada lógica da Sharp sobe a partir de 2,7 V com VDD em 5 V, e o MCU dá
pelo menos 2,85 V a 3,0 V: 0,15 V de margem, sem tradutor de nível.

> [!CAUTION]
> **5 V na JDI a queima** (máximo absoluto de 3,6 V). A tensão do painel é
> uma posição de solda, não firmware: ver o `JP401`.

## J402 · Luz da JDI

**Molex 503480-0540** (LCSC C5527934), 5 vias, passo de 0,5 mm, 1,0 mm de
altura, contato dos dois lados: a série que a JDI nomeia no desenho de
contorno (o 503480-0500 dela está fora de linha na LCSC). A fita da luz tem
2,2 a 2,8 mm de condutor exposto; o HCTL HC-FPC-05-10-5RLTAG de antes pedia
3,0 (`display-a4`), e este pede 2,0.

A ficha da tela (spec 1.4.2) dá a pinagem: **1 a 4 são os catodos**, um por
LED, e **5 é o anodo comum**, no `3V3BL` (os 3,3 V da `LDSW2`). O pino 1
fica do lado da fita de sinal, e a fita chega como a de sinal, dobrada sob o
vidro: a boca também fica em x = 1,27 mm, com o centro em y = 16,8 mm. Como
no `J401`, os circuitos do conector correm ao contrário dos pinos da fita: o
catodo 1 cai no circuito 5 e o anodo no circuito 1.

| Especificação | Valor |
|---|---|
| Tensão direta da luz | 2,67 V típica, a 4 mA por LED |
| Corrente | um resistor de 150 Ω por catodo (`R406` a `R409`): cerca de 4 mA por LED, 16 mA no total |
| Acionamento | `BL_PWM` (P3.08) na porta do `Q401`, que fecha os quatro catodos ao `GND` |

## J403 · Luz do Azumo, no verso

**Molex 503480-0440** (LCSC C3170007), 4 vias, passo de 0,5 mm, contato dos
dois lados, **na face de trás**. O Azumo 11103-06 é a Sharp com uma luz
frontal, e a fita da luz sai da mesma borda longa, na ponta do pino 1 da
Sharp, com 27,8 mm além do vidro: contorna a borda esquerda da placa e chega
ao conector por baixo, em (17,07; 58,18), com a boca para a borda.

**A fita tem dois pads, não quatro**: 0,80 mm de largura a 1,00 mm de passo
(desenho T7 de 2022-01-10). Cada pad cobre duas vias de 0,5 mm: 1-2 e 3-4.

| Item | Valor |
|---|---|
| Polaridade pelo desenho T7 | anodo no par do lado das teclas, que no verso são os circuitos 1-2 do conector: `JP402` e `JP403` montados |
| Polaridade pela folha de 2020 | o contrário: `JP404` e `JP405` no lugar deles |
| Corrente | `R410` de 39,2 Ω do `3V3BL`: 13 mA com V_F de 2,8 V, 8 mA com 3,0 V; máximo de 25 mA |
| Acionamento | o mesmo `Q401` da luz da JDI |

> [!IMPORTANT]
> **As duas fichas do Azumo se contradizem sobre a polaridade.** Antes de
> ligar, meça o LED da fita com o multímetro em teste de diodo e monte o par
> de jumpers certo. Montar os quatro põe o `3V3BL` em curto com o `Q401`
> pelo `R410`.

O Azumo tem uma barra de 6,9 × 38,1 mm atrás do vidro, perto da ponta da
fita da luz, que desce até 0,59 mm da placa: **entre y = 53,6 e 60,5 mm não
há peça na face da frente**, só cobre e pads. O posicionador garante, e o
CX3 do [dry run da caixa](caixa/README.md) mede.

## JP401 · Tensão do display

**Escolha deste projeto.** Um jumper de 0 Ω em **três pads em linha**: o do
meio vai aos contatos 6, 7 e 8 do `J401`, um lado ao `3V0` e o outro ao
`5V0` do boost da Sharp. **Com a JDI, monta-se o lado do `3V0`**; com a
Sharp ou o Azumo, o do `5V0`. A serigrafia diz `3V0 · JDI` de um lado e
`5V0 · SHARP` do outro.

```mermaid
flowchart LR
    A["pad 1<br/>3V0, do BUCK2"] -->|"0 Ω montado · JDI"| B["pad 2<br/>VDD, VDDA e EXTMODE<br/>J401 contatos 6, 7 e 8"]
    C["pad 3<br/>5V0, do TPS610997"] -.->|"posição vazia · Sharp"| B
    B --> J401["J401"]
```

**O 5 V vem de um TPS610997** (`U402`, LCSC C2072359), boost de saída fixa
em 5,0 V (4,90 a 5,10), dentro dos 4,8 a 5,5 V da Sharp; o REG710NA-5 do
plano antigo dá 4,7 V no mínimo. Ele gasta cerca de 1 µA ligado e 0,5 µA
desligado, e **desligado desconecta a saída**. O `EN` é o `DISP_PWR_EN`
(P3.07), com o pull-down `R403`, e **só o firmware da Sharp o liga**: com o
firmware da JDI o boost fica desligado, e um `JP401` esquecido na posição
do `5V0` dá 0 V à JDI em vez de 5 V. Em volta dele: `L401` de 2,2 µH (o
mesmo indutor dos bucks), 10 µF na entrada e 2 × 10 µF na saída (a ficha
pede 20 µF nominais).

> [!CAUTION]
> **Por que não duas posições independentes de 0 Ω.** Dois footprints
> separados, um do `3V0` ao display e outro do `5V0`, deixam quatro estados
> em vez de dois, e o quarto, com os dois montados, põe 5 V na JDI e o
> boost em curto com o BUCK2. O pad do meio único apaga esse estado. O que
> ele não resolve é montar o 0 Ω na posição do `5V0` com a JDI no `J401`
> **e** gravar o firmware da Sharp: contra isso, a serigrafia e o
> procedimento de montagem.

## JP101 · Jumper de medição de corrente

**Escolha deste projeto**: o `JP101` nasceu aqui e entrou na
[lista de nós](03-netlist.md#nós-sem-ligação-ao-mcu) e na
[lista de materiais](05-materiais.md#folha-1--energia) na mesma rodada de
2026-09-23. O valor de 0 Ω da lista de compras (`ERJ-2GE0R00X`, 0402)
**não serve**: ver os requisitos abaixo.

**O que ele é:** um resistor de 0 Ω em série no caminho da célula, entre o
`VBAT+` do `J102` e o pino `BATT` do MAX17262. Tirado o resistor, os dois
pads viram os terminais do amperímetro.

```mermaid
flowchart LR
    CELL["célula LiPo 1S<br/>com PCM"] --> J102["J102<br/>contatos 1 e 6"]
    J102 --> JP101["JP101 · 0 Ω<br/>aberto, entra o amperímetro"]
    JP101 --> BATT["MAX17262 BATT"]
    BATT -->|"sensor interno de 7 mΩ"| SYS["MAX17262 SYS<br/>nó VBAT"]
    SYS --> NPM["nPM1300 VBAT<br/>carga pelo USB"]
    SYS --> AEM["AEM10900 STO<br/>carga pelo sol"]
    SYS --> TPS["TPS7A02 IN<br/>VBCKP do receptor"]
```

As setas mostram o caminho, não o sentido da corrente: ela vai nos dois
sentidos, e é esse o ponto.

**Por que ele vale a peça.** Numa placa que nunca foi montada, subir sem
poder medir a corrente é caro. Tudo o que a célula entrega ou recebe passa
por esse ponto — a carga pelo USB, a carga pelo sol, o consumo do sistema
e o ship mode —, e é o mesmo caminho que o MAX17262 mede, de modo que o
jumper também é como se confere o medidor contra um instrumento, que a
[avaliação](../docs/15-avaliacao-componentes.md#bancada-antes-do-layout)
já lista como trabalho de bancada. Sem ele, medir os 370 nA de ship mode
exige dessoldar o fio da bateria.

**Especificação proposta:** footprint **1206**, 0 Ω, corrente de pelo
menos **2 A**, com a **menor resistência de jumper disponível**. O motivo
do tamanho é a queda: o `JP101` fica antes do pino `BATT`, ou seja,
**dentro** da tensão que o medidor usa. Um jumper de 50 mΩ com os 600 mA
da carga produz 30 mV de erro; um de 20 mΩ produz 12 mV. O `ERJ-2GE0R00X`
0402 que o `JP401` usa vale 1 A e **não serve aqui**.

> [!NOTE]
> **Duas armadilhas de bancada, que fazem a medição falhar sem que a placa
> tenha nada.** A primeira: um multímetro na faixa de corrente tem
> resistência de carga própria, que na faixa de microampères chega a
> volts — o aparelho reinicia ao primeiro pico de rádio ou de tela. O
> remédio é um capacitor grande, de 100 µF ou mais, em paralelo com as
> pontas do instrumento. A segunda: quase todo multímetro **abre o
> circuito ao trocar de faixa**, o que desliga o aparelho no meio da
> medida. Medir consumo em repouso pede instrumento feito para isso, ou
> muita paciência.

> [!CAUTION]
> **Com o `JP101` aberto e o cabo USB ligado, o aparelho funciona e o
> medidor mente.** O nPM1300 continua alimentando tudo pelo `VBUS`, mas o
> pino `BATT` do MAX17262 fica no ar e o `STO` do AEM10900 tenta carregar
> uma célula que não está lá. Qualquer leitura de carga nesse estado é
> lixo. Medir sempre com a célula ligada **através** do amperímetro, nunca
> com o caminho aberto.

## Pontos de teste

**O esquema de numeração é `TP<folha><nn>`**, a mesma centena dos
designadores da folha: `TP1xx` na folha 1 (energia), `TP2xx` na folha 2
(MCU e console), `TP4xx` na folha 4 (display). Bate com o que
[05](05-materiais.md#folha-2--mcu) já usa (`TP201` e `TP202`).

### Folha 1 · Energia

| Ponto | Sinal | O que se vê ali | Nota |
|---|---|---|---|
| `TP101` | `VBUS` | o que a fonte USB entrega | depois do TVS, junto do conector |
| `TP102` | `VBUSOUT` | saída do nPM1300 | alimenta o `VBUS` do módulo e o divisor do `DIS_STO_CH` |
| `TP103` | `VBAT` | lado `SYS` do medidor | 3,0 a 4,2 V |
| `TP104` | `VSYS` | saída do power path | **chega a 5,5 V com cabo**; é a referência do LED RGB |
| `TP105` | `3V0` | BUCK2, o trilho do MCU | sem ele o aparelho não liga |
| `TP106` | `1V8` | BUCK1 | **máximo absoluto de 1,98 V** no `V_IO` do receptor |
| `TP107` | `SD3V0` | `LDSW1` | sem carga desde 2026-10-18, quando a flash foi para o `3V0`; o firmware não liga o `LDSW1`, e o ponto deve estar em 0 V |
| `TP108` | `3V3BL` | `LDSW2` | a luz; sai de regulação com o `VSYS` perto de 3,4 V |
| `TP109` | `VBCKP` | TPS7A02 | **deve continuar de pé com o aparelho desligado** |
| `TP110` | `SRC` | entrada do painel | em regime fica em 80,7 % da tensão em aberto, onde o MPPT do ADP5091 trabalha. Era o `VINT` do AEM10900, que saiu; o ADP5091 não tem trilho interno em pino nenhum |
| `TP112` | `GND` | referência do bloco de energia | via própria ao plano |

### Folha 2 · MCU e console

| Ponto | Sinal | O que se vê ali | Nota |
|---|---|---|---|
| `TP201` | `CON_TX` | console, P1.00, `uart20` | 115200 baud ([03](03-netlist.md#console--uart20)) |
| `TP202` | `CON_RX` | console, P1.31 | [03](03-netlist.md#console--uart20) |
| `TP203` | `GND` | **terra do console** | ao lado dos dois |

**O `TP203` não é enfeite.** Um conversor USB-serial precisa de três
fios, e sem terra comum entre o conversor e a placa o console não
funciona — ou, pior, funciona mal o bastante para parecer defeito de
firmware. Ele fica **junto** do `TP201` e do `TP202`, não do outro lado da
placa, porque o que importa é a distância do laço.

### Folha 4 · Display

A folha 4 não tem ponto de teste. O `TP401` do REG710 saiu com ele; o `5V0`
do boost da Sharp se mede no pad do `5V0` do `JP401`, que é o trilho que
jamais pode chegar a um painel JDI.

### Como os pads são

Escolha deste projeto, para o layout:

- Pad redondo de cerca de 1,0 mm, sem máscara, na face de cima, onde um
  clipe de mola encosta.
- Cada ponto de trilho fica **na ponta do trilho, junto da maior carga**,
  e não ao lado do regulador: um pad colado no conversor esconde
  exatamente a queda que interessa medir.
- Os dois `GND` (`TP112` e `TP203`) com via própria ao plano, curta.

## O que falta conferir

Honesto, item a item:

| Em aberto | Por quê | Quem resolve |
|---|---|---|
| **`J101`: nome do contato contra o número do pad** | a pinagem acima é a norma do USB-C, que não muda; o que muda de fabricante para fabricante é a numeração do footprint. O desenho da Molex **não foi lido nesta sessão** | desenho do 2036150003 rev. A |
| **`J101`: quais oito contatos a Molex omite** | [15](../docs/15-avaliacao-componentes.md#usb-c-e-proteção) diz "16 pinos"; que os ausentes sejam `A2`, `A3`, `A10`, `A11`, `B2`, `B3`, `B10` e `B11` é **dedução da norma**, não leitura do desenho | o mesmo desenho |
| **`J101`: carcaça direto ao `GND` ou por RC** | aqui vai direto, que é o comum num aparelho a bateria e sem terra; há projetos que põem um RC | decisão de layout |
| **`J102`: a pinagem inteira** | é proposta deste documento, e o fabricante do pack é quem monta o cabo. **Conferir antes de fechar o pedido** ([19](../docs/19-lista-de-compras.md#antes-de-fechar-o-pedido)) | fabricante do pack |
| **`J102`: os dois NTC referenciados ao negativo da célula** | é o que faz quatro sinais caberem em seis vias; com termistores isolados o conector muda para oito | especificação do pack |
| **`J102`: NTC do pack ou NTC de placa** | os dois juntos dão 5 kΩ, que o AEM10900 lê como 44,4 °C contra um limite de 45 °C, e a carga solar morre | decisão do dono |
| **`J102`: para que lado sai o cabo** | o prefixo `SM` da JST é entrada lateral; no footprint do KiCad as ilhas de sinal ficam em y −2 e as de fixação em y +1,875, e a boca é do lado das de fixação (+y). Com 270° na placa a boca aponta para a esquerda. O desenho da JST **não foi lido aqui**: conferir a boca no catálogo eSH antes de fabricar | catálogo SH da JST |
| **`TH_MON`: ordem do divisor** | [14](../docs/14-hardware-placa-nova.md#ligações-fixas-dos-cis) registra "`RDIV` de 22 kΩ" e o NTC, mas não diz qual perna fica no `TH_REF` e qual no `GND`. Trocar inverte o sentido da leitura de temperatura | ficha do AEM10900 |
| **`J201`: a que pad do ME54BS13 levar o `SWO`** | o contato 6 do TC2030 é o `SWO` e hoje está sem ligação. Levá-lo a um pad de trace daria `printf` por ITM no bring-up, e qual pad do módulo expõe o `SWO` não está na ficha dele | ficha do ME54BS13 |
| **Por onde a luz do LPM027M128C se liga** | **resolvido em 2026-10-18**: conector próprio de 5 vias, o [`J402`](#j402--luz-da-jdi), com a pinagem da spec 1.4.2 | ficha da tela, [11](11-revisao-do-esquematico.md) |
| **A dobra das fitas sob o vidro** | o alcance de cada fita até a boca do conector está calculado com folga de cerca de 0,6 mm; a dobra real só uma maquete de papel ou a tela na mão confirma | amostra das telas ([11](11-revisao-do-esquematico.md#decisões-do-dono)) |
| **`J403`: a polaridade do LED do Azumo** | o desenho T7 e a folha de 2020 da Azumo se contradizem; os jumpers `JP402` a `JP405` escolhem | teste de diodo na fita, antes de ligar |
| **`R401` com o filme** | só no plano B: depende da tensão direta da amostra; 25 mA é o máximo do LED. Com o JDI são 39 Ω, fechados | medida na amostra |
| **`JP101`: existir ou não** | é uma junta a mais no caminho da célula, que é o caminho mais crítico da placa; ganha-se medição, perde-se um ponto de falha | decisão do dono |
| **`JP101` e os pontos de teste novos** | o `JP101` e os pontos de teste entraram em [03](03-netlist.md) e em [05](05-materiais.md) na mesma rodada, que estão sendo editados em paralelo | próxima passagem nesses dois |
