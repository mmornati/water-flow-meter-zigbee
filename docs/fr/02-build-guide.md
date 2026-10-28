# 02 · Montage pas à pas

🌐 [English](../02-build-guide.md) · **Français** · [Italiano](../it/02-build-guide.md)

## 1. Test sur l'établi d'abord

Avant d'approcher quoi que ce soit d'un tuyau :

1. Flashez le firmware avec seulement le XIAO branché en USB ([Firmware](03-firmware.md)).
2. Ouvrez le moniteur série (`pio device monitor`). Vous devez voir l'étalonnage, le total enregistré et `Waiting for the network; water is counted meanwhile`.
3. Câblez le signal du capteur comme dans [Matériel et câblage](01-hardware-and-wiring.md#le-signal-des-impulsions--à-lire-avant-de-câbler), alimentez le capteur depuis la broche `5V` du XIAO (5 V venant de l'USB, la diode est donc indispensable ici) et **soufflez doucement dans le capteur** : le log série affiche un `Flow ... L/min` non nul en quelques secondes.
4. Appairez-le avec Zigbee2MQTT ([Zigbee2MQTT & Home Assistant](04-zigbee2mqtt-and-home-assistant.md)) tant qu'il est encore sur l'établi.

## 2. Alimentation

1. Câblez `OUT+`/`OUT-` du TP4056 sur `5V`/`GND` du XIAO via une prise que vous pouvez débrancher (pour flasher plus tard).
2. Insérez la 18650, puis branchez le chargeur USB sur le TP4056. La LED rouge du module indique la charge, bleue/verte la charge terminée.
3. Optionnel : câblez le pont diviseur de la batterie sur `D0` et activez-le dans le firmware ([mesure de la batterie](03-firmware.md#mesure-de-la-batterie)).

## 3. Plomberie

- La flèche sur le corps du capteur indique le sens de l'eau. Monté à l'envers, il tourne quand même, mais compte mal.
- Installez-le **après le compteur principal et le réducteur de pression**, sur le tuyau qui alimente toute la maison, ou sur une seule branche (jardin, chauffe-eau) si c'est ce que vous voulez suivre.
- Utilisez les joints et du ruban PTFE sur les filetages G3/4. La plupart des capteurs sont donnés pour ~1,75 MPa (17,5 bar) : très bien derrière un réducteur de pression, pas sur une arrivée brute à haute pression.
- Gardez l'électronique **au-dessus** du capteur, à l'abri des gouttes, dans un boîtier IP65. Faites entrer le câble par le bas avec une boucle d'égouttement.
- Un capteur Hall est une petite turbine : mettez un filtre en amont si votre eau charrie du sable, sinon le rotor s'use et sous-compte.

## 4. Boîtier

- Les boîtiers en ABS/polycarbonate ne bloquent pas le Zigbee. Ceux en métal, si : mettez l'antenne U.FL à l'extérieur.
- Fixez solidement le support de la 18650 ; laissez le port USB-C du TP4056 accessible ou prolongez-le à travers un presse-étoupe.
- BME280 optionnel : montez-le à l'**extérieur** du boîtier (ou dans un coin ventilé), sinon il mesure l'électronique, pas la pièce.

## 5. Étalonnage

La valeur par défaut est de 490 impulsions par litre, celle de la version ESPHome. Chaque capteur est un peu différent et le nombre dépend du débit ; prenez dix minutes pour le vérifier avec un seau ([étalonnage](03-firmware.md#étalonnage)).

## 6. Vérifications finales

| Vérification | Résultat attendu |
|---|---|
| Ouvrez un robinet | `flow` dans Z2M passe au-dessus de 0 en moins de 5 s |
| Fermez-le | `flow` revient à 0 en ~5 s ; `water_consumed` a augmenté |
| Débranchez le chargeur USB | Le compteur continue d'envoyer ses valeurs (sur batterie) |
| Redémarrez-le (débranchez tout) | `water_consumed` revient avec la même valeur |
| Carte de Zigbee2MQTT | Le compteur apparaît comme **routeur** |

Suite : [Firmware](03-firmware.md).
