# 03 · Firmware

🌐 [English](../03-firmware.md) · [Français](../fr/03-firmware.md) · **Italiano**

## Compilazione e flash

### PlatformIO (consigliato)

```bash
cd firmware
pio run -t erase && pio run -t upload
pio device monitor
```

Cancella la memoria solo al primo flash (cancella anche il totale salvato e la calibrazione). Per gli aggiornamenti successivi: `pio run -t upload`.

### Arduino IDE

1. Installa il core **esp32** di Espressif, versione 3.3.0 o successiva (Boards Manager).
2. Installa **Adafruit BME280 Library** (Library Manager), anche senza un BME280: lo sketch la include.
3. Apri `firmware/flow_meter/flow_meter.ino` e imposta il menu **Tools**:

| Impostazione | Valore |
|---|---|
| Board | XIAO_ESP32C6 |
| Zigbee mode | Zigbee ZCZR (coordinator/router) |
| Partition Scheme | Zigbee ZCZR 4MB with spiffs |
| Erase All Flash Before Sketch Upload | Enabled (solo al primo flash) |

Ricordati di [scollegare `OUT+` del TP4056](01-hardware-and-wiring.md#tabella-dei-collegamenti) prima di collegare l'USB allo XIAO.

## Come funziona

| Cosa | Come |
|---|---|
| Conteggio | Il contatore hardware **PCNT** dell'ESP32-C6 conta i fronti di salita su `D2`, con un filtro anti-glitch da 10 µs. Nessun interrupt per impulso, niente si perde mentre la radio è occupata |
| Flusso | Campionato ogni secondo, mediato sugli ultimi 5 s, in L/min |
| Totale | Tenuto in litri come double; preciso al millilitro per decenni |
| Invii | L'inizio o la fine del passaggio d'acqua vengono inviati subito; mentre l'acqua scorre, ogni 5 s se il flusso è cambiato di ≥ 0,1 L/min o il totale di ≥ 0,5 L; tutto di nuovo ogni 5 min |
| Scritture in flash | Totale salvato una volta ogni volta che l'acqua si ferma, e ogni 10 min durante un flusso lungo. Di solito qualche decina di scritture al giorno |
| Rete assente | L'acqua viene comunque contata e salvata; i valori vengono inviati appena si ricollega |

### Endpoint Zigbee

| EP | Cluster | Proprietà Z2M | Unità |
|---|---|---|---|
| 1 | Analog Input | `flow` | L/min |
| 2 | Analog Input | `water_consumed` | L |
| 2 | Analog Output (scrittura) | imposta `water_consumed` | L |
| 3 | Analog Output | `pulses_per_liter` | impulsi/L |
| 4 | Analog Input | `battery_voltage`, `battery` | V, % |
| 10 | Temperature + Humidity | `temperature`, `humidity` | °C, % |
| 11 | Pressure | `pressure` | hPa |

L'endpoint 4 esiste solo con la [misura della batteria](#misura-della-batteria) abilitata; il 10 e l'11 solo se un BME280 ha risposto all'avvio. Se ne aggiungi uno più tardi, riabbina il dispositivo (oppure *Reconfigure* + *Interview* in Z2M) così Zigbee2MQTT impara i nuovi endpoint.

## Calibrazione

I sensori Hall vengono venduti come "F = 5,5 × Q" o simili, ma il numero reale di impulsi per litro cambia da esemplare a esemplare e con il flusso. Il firmware lo salva in flash; lo cambi da Home Assistant o da Z2M (`pulses_per_liter`), senza riflashare.

Con il package di Home Assistant ([guida](04-zigbee2mqtt-and-home-assistant.md#calibrazione)):

1. Esegui **Water calibration - start**.
2. Riempi un contenitore di volume noto (secchio da 10 L, annaffiatoio...) al tuo **flusso abituale**, da un rubinetto a valle del sensore. Assicurati che nient'altro usi acqua nel frattempo.
3. Chiudi il rubinetto e aspetta che `flow` mostri 0.
4. Imposta **Water calibration - real volume** ed esegui **Water calibration - finish**. Calcola `new = current × counted / real` e lo applica.

A mano: `nuovo pulses_per_liter = attuale × litri contati dal contatore / litri realmente usati`.

Cambiare la calibrazione non riscrive mai i litri già contati.

## Allineare il totale al tuo contatore dell'acqua

Leggi il contatore dell'acqua (per esempio 254,318 m³ = 254318 L) e pubblicalo una volta:

```bash
mosquitto_pub -t zigbee2mqtt/water_meter/set -m '{"water_consumed": 254318}'
```

Oppure dalla *Dev console* di Zigbee2MQTT: endpoint 2, cluster `genAnalogOutput`, scrivi `presentValue`. Usa 0 per azzerarlo.

## Misura della batteria

1. Collega il partitore dal `+` del 18650 a `D0` ([collegamenti](01-hardware-and-wiring.md#tabella-dei-collegamenti)).
2. In `firmware/platformio.ini`, togli il commento a `-DBATTERY_ADC_PIN=0` (Arduino IDE: cambia `#define BATTERY_ADC_PIN -1` in `0` nello sketch).
3. Flasha e riabbina. `battery_voltage` viene inviato ogni minuto, `battery` è stimato in modo lineare tra 3,3 V (0 %) e 4,2 V (100 %).

## Impostazioni

Tutte nel blocco `Behaviour` in cima a `flow_meter.ino`:

| Define | Predefinito | Significato |
|---|---|---|
| `DEFAULT_PULSES_PER_LITER` | 490 | Usato finché non viene salvata una calibrazione |
| `REPORT_MIN_INTERVAL_MS` | 5000 | Frequenza massima di invio mentre l'acqua scorre |
| `HEARTBEAT_MS` | 5 min | Reinvio di tutti i valori |
| `FLOW_REPORT_DELTA` / `TOTAL_REPORT_DELTA` | 0,1 L/min / 0,5 L | Variazione necessaria per un invio |
| `SAVE_MAX_INTERVAL_MS` | 10 min | Scrittura in flash durante un flusso lungo |
| `CLIMATE_REPORT_MS` | 5 min | Frequenza di invio del BME280 |
| `PCNT_GLITCH_NS` | 10000 | Gli impulsi più brevi di questo valore sono disturbi |

## LED e pulsante

| LED | Significato |
|---|---|
| Lampeggio lento | Non collegato: apri *Permit join* |
| Spento | Collegato |
| Lampeggio veloce | Pulsante BOOT premuto |

Tieni premuto **BOOT** per 3 s per lasciare la rete (reset di fabbrica Zigbee). Il totale e la calibrazione restano.

## Test

La logica di conteggio, invio e salvataggio sta in `flow_logic.h`, senza dipendenze da Arduino, ed è coperta da unit test che girano sul tuo computer:

```bash
cd firmware
pio test -e native
```

Prossima guida: [Zigbee2MQTT e Home Assistant](04-zigbee2mqtt-and-home-assistant.md).
