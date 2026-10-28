# 02 · Costruzione passo passo

🌐 [English](../02-build-guide.md) · [Français](../fr/02-build-guide.md) · **Italiano**

## 1. Prima il test al banco

Prima di avvicinare qualsiasi cosa a un tubo:

1. Flasha il firmware con il solo XIAO collegato all'USB ([Firmware](03-firmware.md)).
2. Apri il monitor seriale (`pio device monitor`). Dovresti vedere la calibrazione, il totale salvato e `Waiting for the network; water is counted meanwhile`.
3. Collega il segnale del sensore come in [Hardware e collegamenti](01-hardware-and-wiring.md#il-segnale-degli-impulsi-leggi-qui-prima-di-collegare), alimenta il sensore dal pin `5V` dello XIAO (5 V dall'USB, quindi qui il diodo è obbligatorio) e **soffia piano nel sensore**: entro pochi secondi il log seriale stampa un `Flow ... L/min` diverso da zero.
4. Abbinalo a Zigbee2MQTT ([Zigbee2MQTT e Home Assistant](04-zigbee2mqtt-and-home-assistant.md)) mentre è ancora sul banco.

## 2. Alimentazione

1. Collega `OUT+`/`OUT-` del TP4056 a `5V`/`GND` dello XIAO tramite un connettore che puoi staccare (ti servirà per flashare più avanti).
2. Inserisci il 18650, poi collega il caricatore USB al TP4056. Il LED rosso del modulo indica la ricarica, blu/verde indica carica completa.
3. Facoltativo: collega il partitore della batteria a `D0` e abilitalo nel firmware ([misura della batteria](03-firmware.md#misura-della-batteria)).

## 3. Idraulica

- La freccia sul corpo del sensore indica la direzione dell'acqua. Montato al contrario gira lo stesso, ma conta male.
- Installalo **dopo il contatore principale e il riduttore di pressione**, sul tubo che alimenta tutta la casa, oppure su un singolo ramo (giardino, scaldabagno) se è quello che vuoi tenere d'occhio.
- Usa le guarnizioni e il nastro in PTFE sui filetti G3/4. La maggior parte dei sensori regge ~1,75 MPa (17,5 bar): va bene dopo un riduttore di pressione, non su un'alimentazione diretta ad alta pressione.
- Tieni l'elettronica **sopra** il sensore, lontana dalle gocce, in una scatola IP65. Fai entrare il cavo dal basso con un'ansa di gocciolamento.
- Un sensore Hall è una piccola turbina: metti un filtro a monte se la tua acqua porta sabbia, altrimenti il rotore si consuma e conta meno del dovuto.

## 4. Contenitore

- Le scatole in ABS/policarbonato non bloccano lo Zigbee. Quelle in metallo sì: metti l'antenna U.FL fuori.
- Fissa bene il portabatteria del 18650; lascia raggiungibile la porta USB-C del TP4056 o prolungala attraverso un pressacavo.
- BME280 facoltativo: montalo **fuori** dalla scatola (o in un angolo ventilato), altrimenti misura l'elettronica e non la stanza.

## 5. Calibrazione

Il valore predefinito è 490 impulsi per litro, quello della versione ESPHome. Ogni sensore è un po' diverso e il numero dipende dal flusso; prenditi dieci minuti per verificarlo con un secchio ([calibrazione](03-firmware.md#calibrazione)).

## 6. Controlli finali

| Controllo | Risultato atteso |
|---|---|
| Apri un rubinetto | `flow` in Z2M sale sopra 0 entro 5 s |
| Chiudilo | `flow` torna a 0 entro ~5 s; `water_consumed` è aumentato |
| Stacca il caricatore USB | Il contatore continua a inviare dati (a batteria) |
| Riavvialo (scollega tutto) | `water_consumed` torna con lo stesso valore |
| Mappa di Zigbee2MQTT | Il contatore compare come **router** |

Prossima guida: [Firmware](03-firmware.md).
