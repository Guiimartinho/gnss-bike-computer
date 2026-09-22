---
name: fw-radio
description: Trabalhar com o rádio do GNSS Bike Computer no port Zephyr - BLE central (clientes HRS, CSC, FTMS, CPS, LNS, Komoot, radar e ANCS) e periférico (NUS, LNS, BAS, DIS), scan, conexões, descoberta e inscrição GATT, pareamento, levar dados de sensores ao modelo, stravaAP e comandos por NUS, e os canais ANT+. Use ao mexer em zephyr_app/src/rf/ ou em prj.conf de Bluetooth.
---

# Rádio: BLE e ANT+

Referências: `docs/07-radio-ant-ble.md` e o catálogo de dispositivos com prioridades e limites do rádio em `docs/17-dispositivos-ble-ant.md`. Topologia do legacy: BLE **só central** (NUS para o stravaAP, LNS, Cycling Power, Komoot) + ANT+ (HRM, BSC, FE-C). O port é periférico + central; com `ANT=1` a pilha ANT sobe no boot e há código de canal escravo em `src/rf/ant/ant_channel.c` e `ant_sensors.c`, mas ainda sem perfil que abra canal (ver a tabela abaixo).

Clientes BLE no repositório: `ble_hrs_client.c` (cinta), `ble_bsc_client.c` (velocidade e cadência), `ble_fec_client.c` (rolo, FTMS), `ble_cps_client.c` (medidor de potência), `ble_lns_client.c` (posição do celular), `ble_komoot_client.c` (navegação), `ble_radar_client.c` (radar traseiro) e `ble_ancs_client.c` (notificações do iPhone). O lado periférico e o gerente estão em `src/rf/ble/` (`ble_manager.c`, `ble_nus.c`, `ble_lns.c`).

## Estado no port (2026-09-22)

| Problema | Onde | Correção esperada |
|---|---|---|
| velocidade CSC 3600× menor, flags do FTMS, 1 RR só | clientes | a velocidade CSC saiu para `model/csc_calc.c` e o FTMS para `model/ftms_parse.c`; **falta o RR**: `ble_hrs_client.c:118-124` guarda só o primeiro intervalo do pacote |
| Komoot com UUID de característica, layout e papel errados | `ble_komoot_client.c` | central, característica `503DD605-9BCB-4F6E-B235-270A57483026`, pacote id(4) direção(1) distância(4 LE) rua(UTF-8), leitura depois da notificação |
| perfis ANT+ sem decodificador de páginas | `src/rf/ant/ant_sensors.c`, `radar_ant.c`, `power_ant.c` | as funções de página são `__weak` e devolvem falso; os `CONFIG_GNSS_ANT_*_DEV_TYPE` valem 0 (`zephyr_app/Kconfig:80-172`), então `ant_ch_open()` devolve `-ENOTSUP` e **nenhum canal abre de fato**. Os arquivos fortes (`radar_pages.c`, `power_pages.c`, `sensor_pages.c`) não estão no repositório nem nesta máquina: o dono os escreve na máquina dele, porque os perfis estão sob a ANT+ Shared Source License e o repositório é público |
| pareamento sem endurecimento | `prj.conf`, `src/rf/ble/` | o SMP está ligado (o ANCS pede vínculo e link cifrado), mas não há `bt_conn_auth_cb_register()`, nem LESC obrigatório, nem lista de permitidos: LESC com passkey no display e lista de permitidos continuam por fazer |

Corrigido desde então, confirmado no código: o scan começa em `bt_le_scan_start()` numa função pública (`ble_manager.c:662`); a inscrição GATT tem `end_handle` e `disc_params` estáticos (`ble_hrs_client.c:169-171`, `ble_bsc_client.c:176-178`, `ble_fec_client.c:208-210`); a referência do `bt_conn_le_create` é solta (`ble_manager.c:291`); a classificação usa `bt_conn_get_info()` (`ble_manager.c:361`); e o SMP está ligado (`prj.conf:191-192`, `CONFIG_BT_SMP=y` e `CONFIG_BT_BONDABLE=y`). O dado do sensor vai dos clientes aos callbacks do `radio_svc.c` e de lá ao modelo e às zonas, por `ext_sensor` e `link_status`. Nada disso foi testado com rádio de verdade.

## Regras

1. **Callbacks do BT rodam na thread RX do host (cooperativa)**: copie o dado para uma estrutura com trava ou fila e saia (skill `fw-threads`).
2. **Conexões**: `CONFIG_BT_MAX_CONN=4` = 1 periférico + 3 centrais no SoftDevice Controller. Um sensor a mais exige subir o limite e a RAM do controlador; confira com `bash tools/fw/fw.sh size`.
3. **Uma conexão pode ter vários serviços** (HRS + CSC num mesmo sensor): o desconectar precisa limpar todos os clientes daquela conexão.
4. **Consumo**: intervalo de conexão de 100 a 500 ms com latência para sensores de 1 a 4 Hz; scan com janela pequena e com fim; advertising sob demanda.
5. **Aparência** 1153 (Cycling Computer) e nome no scan response.
6. **Teste sem sensor real**: os samples `peripheral_hr` e `peripheral_csc` do Zephyr num segundo nRF52840-DK servem de sensores; o app nRF Connect no celular serve de central para o NUS e o LNS.

## ANT+

**Decidido em 2026-09-18: ANT+ e BLE juntos** (os equipamentos externos falam ANT+). Detalhes em `docs/07-radio-ant-ble.md#decisão-ant-e-ble`.

- Caminho: add-on **ANT for nRF Connect SDK** (`sdk-ant` v2.1.1) **sobre o NCS v3.3.0**, por decisão do dono: clone da tag em `C:\ncs\sdk-ant` e build com `ANT=1` (`fw.sh` ou `build.bat`), que acrescenta o add-on, `zephyr_app/modules/ant_ncs33_compat` e `zephyr_app/ant.conf`. Detalhes e mapa de integração em `docs/07-radio-ant-ble.md#ant-no-ncs-v330`.
- Nomes: o add-on usa o prefixo `ant_` (`ant_init`, `ant_stack_init`, `ant_channel_*`); funções nossas ficam fora dele (`rf_ant_init()` em `src/rf/ant/ant.c`). As `sd_ant_*` do legacy viram `ant_*`, uma para uma.
- Nada do material do ANT+ entra no repositório (acordo do dono): nem código do add-on, nem chave de rede, nem documentos de perfil.
- Kconfig: `CONFIG_ANT` + `CONFIG_BT`; `CONFIG_ANT_EVALUATION_KEY=y` no desenvolvimento; `CONFIG_ANT_LICENSE_KEY` só com licença comercial; nRF5340 usa `CONFIG_ANT_LIBRARY_CORE` e imagem de rede.
- O add-on tem exemplos de HRM, BSC e potência, **sem FE-C**: o perfil do rolo sai do legacy (`legacy/rf/fec.c`, páginas 16 e 25; o controle das páginas 49/51 nunca foi enviado no legacy).
- Pareamento: porte o `ant_device_manager` (canal de busca em background, lista de até 7 sensores com RSSI, número salvo nas configurações).
- ANT e BLE dividem o rádio: reveja `CONFIG_BT_MAX_CONN` e a RAM do controlador.

## stravaAP e comandos

O legacy aceita `$LOC`, `$DWN` e `$QRY` pelo NUS (vindo do dongle stravaAP) e pela USB CDC; as respostas de `$QRY` saem pelo NUS. No port o NUS é servidor e **entrega o RX ao `cmd_parser`**: o `radio_svc.c:244-251,317-318` registra o callback do NUS, alimenta um `struct cmd_parser` byte a byte e chama `app_cmd_handle()` (`src/app/app_cmd.c`); as respostas de `$QRY` são montadas por `src/model/qry.c`. Os comandos destrutivos são recusados pelo rádio. Ao mexer nisso: parser de linhas estilo VParser fora da thread RX do host, e streaming com controle de fluxo e MTU 247 (`CONFIG_BT_L2CAP_TX_MTU=247`, buffers ACL de 251).

## Antes de terminar

Build sem aviso novo, `bash tools/fw/fw.sh size` para a RAM do BLE e, se possível, teste com o app nRF Connect ou um segundo DK. Diga o que não foi testado com sensor real.
