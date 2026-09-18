---
name: fw-radio
description: Trabalhar com o rádio do GNSS Bike Computer no port Zephyr - BLE central (clientes HRS, CSC, FTMS, CPS, LNS, Komoot) e periférico (NUS, LNS, BAS, DIS), scan, conexões, descoberta e inscrição GATT, pareamento, levar dados de sensores ao modelo, stravaAP e comandos por NUS, e a decisão sobre ANT+. Use ao mexer em zephyr_app/src/rf/ ou em prj.conf de Bluetooth.
---

# Rádio: BLE e ANT+

Referência: `docs/07-radio-ant-ble.md`. Topologia do legacy: BLE **só central** (NUS para o stravaAP, LNS, Cycling Power, Komoot) + ANT+ (HRM, BSC, FE-C). O port é periférico + central e **não tem ANT+**.

## Estado no port (2026-09-18)

| Problema | Onde | Correção esperada |
|---|---|---|
| scan nunca iniciado | `ble_manager.c` (só chamado nos próprios callbacks) | iniciar depois do advertising, parar quando os sensores configurados conectarem |
| `bt_gatt_subscribe` com `ccc_handle=0` + `CONFIG_BT_GATT_AUTO_DISCOVER_CCC=y` e sem `disc_params` | `ble_hrs_client.c:160`, `ble_bsc_client.c:268`, `ble_fec_client.c:270` | preencher `subscribe_params.end_handle` e `disc_params` (estático), ou descobrir o CCC antes; **corrija antes de ligar o scan**, senão `memset(NULL)` |
| `bt_conn_le_create` sem `bt_conn_unref` | `ble_manager.c:218` | soltar a referência do create; o callback `connected` recebe a sua |
| classificação por endereço pendente | `ble_manager.c:299-342` | usar `bt_conn_get_info()` e `info.role` |
| velocidade CSC 3600× menor, flags do FTMS, 1 RR só | clientes | parsers pela especificação (FTMS Indoor Bike Data: cadência bit 2, tempo bit 11, energia 5 B) |
| dados só na tela | clientes → `vue.c` | callbacks → `boucle_update_hrm/bsc`, zonas, log |
| Komoot com UUID de característica, layout e papel errados | `ble_komoot_client.c` | central, característica `503DD605-9BCB-4F6E-B235-270A57483026`, pacote id(4) direção(1) distância(4 LE) rua(UTF-8), leitura depois da notificação |
| sem SMP | `prj.conf` | LESC com passkey no display, lista de permitidos |

## Regras

1. **Callbacks do BT rodam na thread RX do host (cooperativa)**: copie o dado para uma estrutura com trava ou fila e saia (skill `fw-threads`).
2. **Conexões**: `CONFIG_BT_MAX_CONN=4` = 1 periférico + 3 centrais no SoftDevice Controller. Um sensor a mais exige subir o limite e a RAM do controlador; confira com `bash tools/fw/fw.sh size`.
3. **Uma conexão pode ter vários serviços** (HRS + CSC num mesmo sensor): o desconectar precisa limpar todos os clientes daquela conexão.
4. **Consumo**: intervalo de conexão de 100 a 500 ms com latência para sensores de 1 a 4 Hz; scan com janela pequena e com fim; advertising sob demanda.
5. **Aparência** 1153 (Cycling Computer) e nome no scan response.
6. **Teste sem sensor real**: os samples `peripheral_hr` e `peripheral_csc` do Zephyr num segundo nRF52840-DK servem de sensores; o app nRF Connect no celular serve de central para o NUS e o LNS.

## ANT+

- O legacy usa o S340; não existe caminho para ele no NCS. O caminho real é o add-on **ANT for nRF Connect SDK** (Garmin/ANT com a Nordic): suporta nRF52840 com ANT e BLE juntos, licença ANT+ (chave de avaliação ou comercial), versão listada compatível com o sdk-nrf 3.2.4 (confirmar com o 3.3.0), exemplos de HRM, BSC e potência, sem FE-C.
- A alternativa é ficar só em BLE (HRS, CSC, CPS, FTMS).
- **Decisão do dono** (`docs/10-status-do-port.md#decisões-do-dono`): não comece nenhum dos dois sem ela.

## stravaAP e comandos

O legacy aceita `$LOC`, `$DWN` e `$QRY` pelo NUS (vindo do dongle stravaAP) e pela USB CDC; as respostas de `$QRY` saem pelo NUS. No port o NUS é servidor e descarta o RX. Para trazer os comandos de volta: parser de linhas estilo VParser numa thread, comandos destrutivos (`$DWN,13`, `$QRY,3`) só com conexão autenticada, e streaming com controle de fluxo e MTU 247 (`CONFIG_BT_L2CAP_TX_MTU=247`, buffers ACL de 251).

## Antes de terminar

Build sem aviso novo, `bash tools/fw/fw.sh size` para a RAM do BLE e, se possível, teste com o app nRF Connect ou um segundo DK. Diga o que não foi testado com sensor real.
