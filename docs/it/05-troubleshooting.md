# 05 · Risoluzione dei problemi

🌐 [English](../05-troubleshooting.md) · [Français](../fr/05-troubleshooting.md) · **Italiano**

Parti dal monitor seriale (`pio device monitor`, con [`OUT+` del TP4056 scollegato](01-hardware-and-wiring.md#tabella-dei-collegamenti)): il firmware scrive nel log la calibrazione, ogni invio e ogni scrittura in flash.

## Il LED continua a lampeggiare lentamente

Il contatore non è collegato alla rete.

- Apri *Permit join* in Zigbee2MQTT, vicino al contatore.
- In passato era abbinato a un'altra rete: tieni premuto **BOOT** 3 s per lasciarla, poi abbinalo di nuovo.
- Portata: controlla che l'antenna U.FL sia collegata; un locale tecnico pieno di tubi è un posto difficile per i 2,4 GHz.

## Nessun impulso

`Flow 0.00 L/min` mentre l'acqua scorre.

- Misura il filo giallo con l'acqua che scorre: dovrebbe commutare (un multimetro mostra circa metà della tensione di alimentazione).
- Diodo nel verso sbagliato: la barra deve essere rivolta verso il sensore.
- Nessun pull-up su `D2`: aggiungi la 10 kΩ verso `3V3`.
- Il sensore non funziona a 3,3–4,2 V: alcuni lavorano solo da ~4,5 V in su. Provalo con un'alimentazione a 5 V. Se la causa è questa, aggiungi un piccolo modulo boost a 5 V (MT3608 o simile) tra `OUT` del TP4056 e il solo filo rosso del sensore.
- Freccia sul corpo del sensore contraria alla direzione dell'acqua.

## Il flusso sembra troppo alto o troppo basso

- Calibralo ([come](03-firmware.md#calibrazione)). 490 impulsi/L è solo un punto di partenza.
- I flussi molto bassi (< 1 L/min, un rubinetto che gocciola) sono sotto ciò che una turbina da 1–30 L/min misura con precisione: potrebbe non girare affatto.
- Letture strane senza acqua: disturbi elettrici su un cavo lungo. Usa un cavo a coppia intrecciata, tienilo lontano dai cavi di rete, aggiungi 100 nF da `D2` a `GND`, oppure aumenta `PCNT_GLITCH_NS`.

## `water_consumed` è tornato indietro dopo un riavvio

Il totale viene salvato quando l'acqua si ferma e ogni 10 min mentre scorre, quindi un reset improvviso fa perdere al massimo i litri dell'utilizzo in corso. Una cancellazione completa della flash (`pio run -t erase`, oppure *Erase All Flash* nell'Arduino IDE) lo azzera: [reimpostalo](03-firmware.md#allineare-il-totale-al-tuo-contatore-dellacqua).

## La calibrazione da Home Assistant non fa nulla

- Lo script di calibrazione si rifiuta di applicarla mentre l'acqua scorre: aspetta che `flow` sia a 0.
- `number.water_meter_pulses_per_liter` non c'è: il converter non è l'ultima versione, oppure il dispositivo ha bisogno di un *Reconfigure* in Z2M.

## Batteria, temperatura o pressione restano unknown

Questi endpoint esistono solo se l'hardware è presente all'avvio. Dopo aver aggiunto il BME280 o il partitore della batteria (e `BATTERY_ADC_PIN`), riavvia il contatore, poi fai *Interview* e *Reconfigure* in Zigbee2MQTT.

All'avvio il log seriale dice `BME280 found` oppure `No BME280, climate endpoints disabled`. Se non viene trovato: SDA su `D7`, SCL su `D8`, alimentato da `3V3`, e deve essere un **BME**280 (un BMP280 non ha l'umidità e ha un chip ID diverso).

## Il contatore si scarica in fretta a batteria

È normale: il sensore Hall (5–15 mA) e un router Zigbee che non dorme mai svuotano un 18650 in uno o due giorni. La batteria serve a superare i blackout. Se il TP4056 non indica mai *carica completa*, il caricatore è troppo debole o la cella è esausta.
