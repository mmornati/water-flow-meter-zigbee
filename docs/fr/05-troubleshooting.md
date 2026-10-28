# 05 · Dépannage

🌐 [English](../05-troubleshooting.md) · **Français** · [Italiano](../it/05-troubleshooting.md)

Commencez par le moniteur série (`pio device monitor`, [`OUT+` du TP4056 débranché](01-hardware-and-wiring.md#tableau-des-connexions)) : le firmware logue l'étalonnage, chaque rapport et chaque écriture en flash.

## La LED clignote lentement sans s'arrêter

Le compteur n'a pas rejoint le réseau.

- Activez *Permit join* dans Zigbee2MQTT, à proximité du compteur.
- Il était déjà appairé à un autre réseau : maintenez **BOOT** 3 s pour le quitter, puis appairez de nouveau.
- Portée : vérifiez que l'antenne U.FL est bien branchée ; un local technique plein de tuyaux est un endroit difficile pour le 2,4 GHz.

## Aucune impulsion

`Flow 0.00 L/min` alors que l'eau coule.

- Mesurez le fil jaune pendant que l'eau coule : il doit basculer (un multimètre affiche environ la moitié de la tension d'alimentation).
- Diode dans le mauvais sens : la barre doit être côté capteur.
- Pas de pull-up sur `D2` : ajoutez la 10 kΩ vers `3V3`.
- Le capteur ne fonctionne pas entre 3,3 et 4,2 V : certains ne marchent qu'à partir de ~4,5 V. Testez-le avec une alimentation 5 V. Si c'est la cause, ajoutez un petit module élévateur 5 V (MT3608 ou équivalent) entre le `OUT` du TP4056 et le fil rouge du capteur uniquement.
- Flèche sur le corps du capteur à contresens de l'eau.

## Le débit semble trop élevé ou trop faible

- Étalonnez ([comment](03-firmware.md#étalonnage)). 490 impulsions/L n'est qu'un point de départ.
- Les très faibles débits (< 1 L/min, un robinet qui goutte) sont en dessous de ce qu'une turbine 1–30 L/min mesure avec précision : elle peut même ne pas tourner du tout.
- Des valeurs erratiques sans eau : du bruit électrique sur un long câble. Utilisez une paire torsadée, éloignez-la du secteur, ajoutez 100 nF entre `D2` et `GND`, ou augmentez `PCNT_GLITCH_NS`.

## `water_consumed` a reculé après un redémarrage

Le total est enregistré quand l'eau s'arrête et toutes les 10 min pendant l'écoulement : une coupure brutale fait donc perdre au plus les litres de l'utilisation en cours. Un effacement complet de la flash (`pio run -t erase`, ou *Erase All Flash* dans l'Arduino IDE) l'efface : [réglez-le de nouveau](03-firmware.md#aligner-le-total-sur-votre-compteur-deau).

## L'étalonnage depuis Home Assistant ne fait rien

- Le script d'étalonnage refuse de s'appliquer pendant que l'eau coule : attendez que `flow` = 0.
- `number.water_meter_pulses_per_liter` absent : le convertisseur n'est pas à jour, ou l'appareil a besoin d'un *Reconfigure* dans Z2M.

## Batterie, température ou pression restent inconnues

Ces endpoints n'existent que si le matériel est présent au démarrage. Après avoir ajouté le BME280 ou le pont diviseur de la batterie (et `BATTERY_ADC_PIN`), redémarrez le compteur, puis faites *Interview* et *Reconfigure* dans Zigbee2MQTT.

Au démarrage, le log série indique `BME280 found` ou `No BME280, climate endpoints disabled`. S'il n'est pas trouvé : SDA sur `D7`, SCL sur `D8`, alimenté en `3V3`, et c'est bien un **BME**280 (un BMP280 n'a pas d'humidité et un identifiant de puce différent).

## Le compteur s'éteint vite sur batterie

C'est normal : le capteur Hall (5–15 mA) et un routeur Zigbee qui ne dort jamais vident une 18650 en un à deux jours. La batterie est là pour passer les coupures de courant. Si le TP4056 n'indique jamais *charge terminée*, le chargeur est trop faible ou la cellule est usée.
