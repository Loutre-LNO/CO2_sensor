# Mesure-CO2

## Mesure du CO2 avec ESP32 et capteur SCD30

### Fonctionnalités
- Mesure toutes les 5 secondes du taux de CO2 dans l'air, de la température et du taux d'humidité
- Deux modes d'affichage, navigation avec les boutons
  - Affichage des valeurs instantanées avec un code couleur RGB proportionnel au taux de CO2
  - Affichage d'un graphique avec les 200 dernières mesures
- Etalonnage (appui long sur bouton 2)
- Mise en veille (Deep Sleep) (appui long sur bouton 1)
- Surveillance du niveau de la batterie
- Extinction automatique si batterie faible

![Photo1](https://github.com/Loutre-LNO/Mesure-CO2/blob/main/Photos/photo1.jpg)

![Photo2](https://github.com/Loutre-LNO/Mesure-CO2/blob/main/Photos/photo2.jpg)


### Liste de Composants
- TTGO T-Display (ESP32 + écran 1.14")
- Capteur CO2 Sensirion SCD30
- (Optionnel) Batterie Lithium 3.7V

### Câblage
|ESP32 T-Display|SCD30|
|-----|-----|
|3V3|VIN|
|GND|GND|
|GPIO-22/SCL|SCL|
|GPIO21/SDA|SDA|



### Inspiré par
- https://github.com/fvanderbiest/arduino-sketches/tree/main/sketch_scd30_ssd1306_neopixel
- http://nousaerons.fr/makersco2/

### Licence
Distribué sous [licence MIT](license.txt)
