# 04 · Zigbee2MQTT e Home Assistant

🌐 [English](../04-zigbee2mqtt-and-home-assistant.md) · [Français](../fr/04-zigbee2mqtt-and-home-assistant.md) · **Italiano**

## Installa il converter

1. Copia [`zigbee2mqtt/water_flow_meter.mjs`](../../zigbee2mqtt/water_flow_meter.mjs) in `<z2m data dir>/external_converters/water_flow_meter.mjs`.
   Con l'add-on di Home Assistant è `/config/zigbee2mqtt/external_converters/` (oppure `/homeassistant/zigbee2mqtt/...`, a seconda della versione dell'add-on).
2. Riavvia Zigbee2MQTT. Il log mostra `Loaded external converter 'water_flow_meter.mjs'`.

## Abbinamento

1. Clicca **Permit join** (consenti l'accesso) in Zigbee2MQTT.
2. Alimenta il contatore. Il LED lampeggia lentamente mentre cerca la rete e si spegne una volta collegato.
3. Rinominalo in **`water_meter`** (il package di Home Assistant si aspetta questo nome), con *Update Home Assistant entity ID* spuntato.

Se il dispositivo compare come *Unsupported*, il converter non è stato caricato: controlla il log, poi fai *Reconfigure* del dispositivo.

## Cosa ottieni

| Proprietà Z2M | Entità di Home Assistant | Note |
|---|---|---|
| `flow` | `sensor.water_meter_flow` | L/min, device class *volume flow rate* |
| `water_consumed` | `sensor.water_meter_water_consumed` | L, device class *water*, *total increasing* |
| `pulses_per_liter` | `number.water_meter_pulses_per_liter` | Calibrazione, sotto *Configuration* |
| `battery_voltage`, `battery` | `sensor.water_meter_battery_voltage`, `sensor.water_meter_battery` | Solo con la misura della batteria |
| `temperature`, `humidity`, `pressure` | `sensor.water_meter_temperature`, ... | Solo con un BME280 |

Le entità dei sensori facoltativi che non hai montato restano semplicemente *unknown*; se vuoi, nascondile.

## Dashboard Energia

*Settings → Dashboards → Energy → Water consumption → Add water source* e scegli `sensor.water_meter_water_consumed` (oppure `sensor.water_total_m3` del package). Home Assistant gestisce sia i litri sia i m³.

## Il package di Home Assistant

[`homeassistant/water_meter.yaml`](../../homeassistant/water_meter.yaml) aggiunge:

| Elemento | Cosa fa |
|---|---|
| `sensor.water_today`, `_this_month`, `_this_year` | Utility meter sul totale |
| `sensor.water_total_m3` | Il totale in m³, come sulla bolletta |
| Avviso **flusso continuo** | Acqua che scorre senza interruzioni per N minuti (predefinito 60, `input_number.water_leak_alert_minutes`): una perdita, una canna o un rubinetto lasciati aperti |
| Avviso **consumi notturni** | Più di 2 L usati tra l'01:00 e le 06:00: un rubinetto che gocciola o uno sciacquone che perde |
| Avviso **contatore offline** | Nessun dato per 15 min |
| Avviso **a batteria** | Batteria sotto 4,0 V per 5 min, il che significa che l'alimentazione USB è saltata (solo con la misura della batteria) |
| Script di **calibrazione** | Vedi sotto |

Installazione:

1. Copialo in `/config/packages/water_meter.yaml`.
2. Assicurati che `configuration.yaml` contenga:

   ```yaml
   homeassistant:
     packages: !include_dir_named packages
   ```

3. Sostituisci `notify.notify` con il tuo servizio di notifica (per esempio `notify.mobile_app_<phone>`).
4. *Developer tools → Check configuration*, poi riavvia.

### Calibrazione

1. Esegui **Water calibration - start** (salva il totale attuale).
2. Riempi un contenitore di volume noto al tuo flusso abituale; nient'altro deve usare acqua nel frattempo.
3. Chiudi il rubinetto e aspetta che `sensor.water_meter_flow` sia a 0.
4. Imposta **Water calibration - real volume** ai litri che hai davvero versato ed esegui **Water calibration - finish**.

Lo script scrive il `pulses_per_liter` corretto sul contatore e ti notifica il vecchio e il nuovo valore. Ripeti una volta per conferma: il secondo giro dovrebbe cambiarlo solo di pochi impulsi.

Prossima guida: [Risoluzione dei problemi](05-troubleshooting.md).
