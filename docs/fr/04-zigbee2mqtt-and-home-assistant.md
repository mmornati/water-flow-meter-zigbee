# 04 · Zigbee2MQTT & Home Assistant

🌐 [English](../04-zigbee2mqtt-and-home-assistant.md) · **Français** · [Italiano](../it/04-zigbee2mqtt-and-home-assistant.md)

## Installer le convertisseur

1. Copiez [`zigbee2mqtt/water_flow_meter.mjs`](../../zigbee2mqtt/water_flow_meter.mjs) dans `<z2m data dir>/external_converters/water_flow_meter.mjs`.
   Avec l'add-on Home Assistant, c'est `/config/zigbee2mqtt/external_converters/` (ou `/homeassistant/zigbee2mqtt/...` selon la version de l'add-on).
2. Redémarrez Zigbee2MQTT. Le log affiche `Loaded external converter 'water_flow_meter.mjs'`.

## Appairer

1. Cliquez sur **Permit join** (autoriser l'appairage) dans Zigbee2MQTT.
2. Alimentez le compteur. Sa LED clignote lentement pendant la recherche et s'éteint une fois le réseau rejoint.
3. Renommez-le **`water_meter`** (le package Home Assistant suppose ce nom), en cochant *Update Home Assistant entity ID*.

Si l'appareil apparaît comme *Unsupported*, le convertisseur n'a pas été chargé : vérifiez le log, puis faites *Reconfigure* sur l'appareil.

## Ce que vous obtenez

| Propriété Z2M | Entité Home Assistant | Remarques |
|---|---|---|
| `flow` | `sensor.water_meter_flow` | L/min, classe d'appareil *volume flow rate* |
| `water_consumed` | `sensor.water_meter_water_consumed` | L, classe d'appareil *water*, *total increasing* |
| `pulses_per_liter` | `number.water_meter_pulses_per_liter` | Étalonnage, dans *Configuration* |
| `battery_voltage`, `battery` | `sensor.water_meter_battery_voltage`, `sensor.water_meter_battery` | Uniquement avec la mesure de la batterie |
| `temperature`, `humidity`, `pressure` | `sensor.water_meter_temperature`, ... | Uniquement avec un BME280 |

Les entités des capteurs optionnels non montés restent simplement *unknown* (inconnu) ; masquez-les si vous voulez.

## Tableau de bord Énergie

*Settings → Dashboards → Energy → Water consumption → Add water source* (Paramètres → Tableaux de bord → Énergie → Consommation d'eau → Ajouter une source d'eau) et choisissez `sensor.water_meter_water_consumed` (ou le `sensor.water_total_m3` du package). Home Assistant gère les litres et les m³.

## Le package Home Assistant

[`homeassistant/water_meter.yaml`](../../homeassistant/water_meter.yaml) ajoute :

| Élément | Ce qu'il fait |
|---|---|
| `sensor.water_today`, `_this_month`, `_this_year` | Compteurs de consommation (utility meters) sur le total |
| `sensor.water_total_m3` | Le total en m³, comme sur votre facture d'eau |
| Alerte **débit continu** | L'eau coule sans interruption pendant N minutes (60 par défaut, `input_number.water_leak_alert_minutes`) : une fuite, un tuyau d'arrosage ou un robinet resté ouvert |
| Alerte **consommation nocturne** | Plus de 2 L consommés entre 01:00 et 06:00 : un robinet qui goutte ou une chasse d'eau qui fuit |
| Alerte **compteur hors ligne** | Aucune donnée depuis 15 min |
| Alerte **sur batterie** | Batterie sous 4,0 V pendant 5 min, ce qui signifie que l'alimentation USB est coupée (uniquement avec la mesure de la batterie) |
| Scripts d'**étalonnage** | Voir ci-dessous |

Installation :

1. Copiez-le dans `/config/packages/water_meter.yaml`.
2. Vérifiez que `configuration.yaml` contient :

   ```yaml
   homeassistant:
     packages: !include_dir_named packages
   ```

3. Remplacez `notify.notify` par votre propre service de notification (par exemple `notify.mobile_app_<phone>`).
4. *Developer tools → Check configuration* (Outils de développement), puis redémarrez.

### Étalonnage

1. Lancez **Water calibration - start** (il enregistre le total actuel).
2. Remplissez un récipient de volume connu à votre débit habituel ; rien d'autre ne doit consommer d'eau pendant ce temps.
3. Fermez le robinet et attendez que `sensor.water_meter_flow` soit à 0.
4. Réglez **Water calibration - real volume** sur les litres réellement versés et lancez **Water calibration - finish**.

Le script écrit la valeur corrigée de `pulses_per_liter` dans le compteur et vous notifie l'ancienne et la nouvelle valeur. Recommencez une fois pour confirmer : le second passage ne doit la modifier que de quelques impulsions.

Suite : [Dépannage](05-troubleshooting.md).
