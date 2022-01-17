# Mesure-CO2

## Mesure du CO2 avec ESP32 et capteur SCD30

### Fonctionnalités
- Mesure toutes les 5 secondes du taux de CO2 dans l'air, de la température et du taux d'humidité
- Affichage des valeurs instantanées
- Affichage d'un graphique avec les 200 dernières mesures
- Etalonnage
- Mise en veille (Deep Sleep)

### Liste de Composants
- TTGO T-Display (ESP32 + écran 1.14")
- Capteur CO2 Sensirion SCD30

### Câblage
ESP32			SCD30
  3V3             VIN
  GND             GND
  GPIO-22/SCL     SCL
  GPIO21/SDA      SDA

### Inspiré par
- https://github.com/fvanderbiest/arduino-sketches/tree/main/sketch_scd30_ssd1306_neopixel
- http://nousaerons.fr/makersco2/

