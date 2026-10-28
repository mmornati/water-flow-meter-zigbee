# 03 · Firmware

🌐 [English](../03-firmware.md) · **Français** · [Italiano](../it/03-firmware.md)

## Compiler et flasher

### PlatformIO (recommandé)

```bash
cd firmware
pio run -t erase && pio run -t upload
pio device monitor
```

N'effacez qu'au premier flashage (cela efface aussi le total enregistré et l'étalonnage). Pour les mises à jour suivantes : `pio run -t upload`.

### Arduino IDE

1. Installez le core **esp32** d'Espressif, 3.3.0 ou plus récent (Boards Manager).
2. Installez **Adafruit BME280 Library** (Library Manager), même sans BME280 : le sketch l'inclut.
3. Ouvrez `firmware/flow_meter/flow_meter.ino` et réglez le menu **Tools** (Outils) :

| Paramètre | Valeur |
|---|---|
| Board | XIAO_ESP32C6 |
| Zigbee mode | Zigbee ZCZR (coordinator/router) |
| Partition Scheme | Zigbee ZCZR 4MB with spiffs |
| Erase All Flash Before Sketch Upload | Enabled (premier flashage uniquement) |

Pensez à [débrancher le `OUT+` du TP4056](01-hardware-and-wiring.md#tableau-des-connexions) avant de brancher l'USB sur le XIAO.

## Comment ça marche

| Quoi | Comment |
|---|---|
| Comptage | Le compteur matériel **PCNT** de l'ESP32-C6 compte les fronts montants sur `D2`, avec un filtre anti-parasites de 10 µs. Pas d'interruption par impulsion, rien n'est manqué quand la radio est occupée |
| Débit | Échantillonné chaque seconde, moyenné sur les 5 dernières secondes, en L/min |
| Total | Conservé en litres dans un double ; exact au millilitre près pendant des décennies |
| Rapports | Le début ou l'arrêt de l'écoulement est envoyé tout de suite ; pendant l'écoulement, toutes les 5 s si le débit a bougé d'au moins 0,1 L/min ou le total d'au moins 0,5 L ; tout est renvoyé toutes les 5 min |
| Écritures en flash | Le total est enregistré une fois à chaque arrêt de l'eau, et toutes les 10 min pendant un long écoulement. En général quelques dizaines d'écritures par jour |
| Réseau indisponible | L'eau est toujours comptée et enregistrée ; les valeurs sont envoyées dès qu'il rejoint le réseau |

### Endpoints Zigbee

| EP | Cluster | Propriété Z2M | Unité |
|---|---|---|---|
| 1 | Analog Input | `flow` | L/min |
| 2 | Analog Input | `water_consumed` | L |
| 2 | Analog Output (écriture) | réglage de `water_consumed` | L |
| 3 | Analog Output | `pulses_per_liter` | impulsions/L |
| 4 | Analog Input | `battery_voltage`, `battery` | V, % |
| 10 | Temperature + Humidity | `temperature`, `humidity` | °C, % |
| 11 | Pressure | `pressure` | hPa |

L'endpoint 4 n'existe que si la [mesure de la batterie](#mesure-de-la-batterie) est activée ; les 10 et 11 seulement si un BME280 a répondu au démarrage. Si vous ajoutez l'un ou l'autre plus tard, appairez de nouveau l'appareil (ou *Reconfigure* + *Interview* dans Z2M) pour que Zigbee2MQTT découvre les nouveaux endpoints.

## Étalonnage

Les capteurs Hall sont vendus avec une formule du type « F = 5,5 × Q », mais le nombre réel d'impulsions par litre varie d'un exemplaire à l'autre et selon le débit. Le firmware le stocke en flash ; modifiez-le depuis Home Assistant ou Z2M (`pulses_per_liter`), sans reflasher.

Avec le package Home Assistant ([guide](04-zigbee2mqtt-and-home-assistant.md#étalonnage)) :

1. Lancez **Water calibration - start**.
2. Remplissez un récipient de volume connu (seau de 10 L, arrosoir...) à votre **débit habituel**, depuis un robinet situé après le capteur. Assurez-vous que rien d'autre ne consomme d'eau pendant ce temps.
3. Fermez le robinet et attendez que `flow` affiche 0.
4. Renseignez **Water calibration - real volume** et lancez **Water calibration - finish**. Il calcule `new = current × counted / real` et l'applique.

À la main : `new pulses_per_liter = current × litres counted by the meter / litres really used` (nouvelle valeur = valeur actuelle × litres comptés par le compteur / litres réellement utilisés).

Changer l'étalonnage ne réécrit jamais les litres déjà comptés.

## Aligner le total sur votre compteur d'eau

Relevez votre compteur d'eau (disons 254,318 m³ = 254 318 L) et publiez-le une fois :

```bash
mosquitto_pub -t zigbee2mqtt/water_meter/set -m '{"water_consumed": 254318}'
```

Ou depuis la *Dev console* de Zigbee2MQTT : endpoint 2, cluster `genAnalogOutput`, écrivez `presentValue`. Utilisez 0 pour remettre à zéro.

## Mesure de la batterie

1. Câblez le pont diviseur entre le `+` de la 18650 et `D0` ([câblage](01-hardware-and-wiring.md#tableau-des-connexions)).
2. Dans `firmware/platformio.ini`, décommentez `-DBATTERY_ADC_PIN=0` (Arduino IDE : remplacez `#define BATTERY_ADC_PIN -1` par `0` dans le sketch).
3. Flashez et appairez de nouveau. `battery_voltage` est envoyé chaque minute, `battery` est estimé linéairement entre 3,3 V (0 %) et 4,2 V (100 %).

## Paramètres

Tous dans le bloc `Behaviour` en haut de `flow_meter.ino` :

| Define | Défaut | Signification |
|---|---|---|
| `DEFAULT_PULSES_PER_LITER` | 490 | Utilisé tant qu'aucun étalonnage n'est enregistré |
| `REPORT_MIN_INTERVAL_MS` | 5000 | Cadence de rapport la plus rapide pendant l'écoulement |
| `HEARTBEAT_MS` | 5 min | Renvoi de toutes les valeurs |
| `FLOW_REPORT_DELTA` / `TOTAL_REPORT_DELTA` | 0,1 L/min / 0,5 L | Variation nécessaire pour un rapport |
| `SAVE_MAX_INTERVAL_MS` | 10 min | Écriture en flash pendant un long écoulement |
| `CLIMATE_REPORT_MS` | 5 min | Cadence de rapport du BME280 |
| `PCNT_GLITCH_NS` | 10000 | Les impulsions plus courtes sont considérées comme du bruit |

## LED et bouton

| LED | Signification |
|---|---|
| Clignotement lent | Réseau non rejoint : activez *Permit join* |
| Éteinte | Réseau rejoint |
| Clignotement rapide | Bouton BOOT maintenu |

Maintenez **BOOT** pendant 3 s pour quitter le réseau (réinitialisation d'usine Zigbee). Le total et l'étalonnage sont conservés.

## Tests

La logique de comptage, de rapport et d'enregistrement se trouve dans `flow_logic.h`, sans dépendance à Arduino, et elle est testée unitairement sur votre ordinateur :

```bash
cd firmware
pio test -e native
```

Suite : [Zigbee2MQTT & Home Assistant](04-zigbee2mqtt-and-home-assistant.md).
