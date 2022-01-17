// ****************************************************
// Mesure CO2
// Noël Utter
// Dernière mise à jour 17/01/2022
//
// Composants:
// - TTGO T-Display (ESP32 + écran 1.14")
// - Capteur CO2 Sensirion SCD30
//
// Câblage
//    ESP32         SCD30
//     3V3           VIN
//     GND           GND
//     GPIO-22/SCL   SCL
//     GPIO21/SDA    SDA
//
// Bibliothèques
// - TFT_eSPI (https://github.com/Bodmer/TFT_eSPI)
// - SparkFun_SCD30_Arduino_Library (https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library)
//
// ****************************************************

#include <SPI.h>
#include <TFT_eSPI.h>
#include <SparkFun_SCD30_Arduino_Library.h> 
#include <wire.h>
#include "co2.h"


#define PROGVER                     2.0     // Version du programme

#define DELAI_MESURE                5       // Nombre de secondes entre deux mesures (2..1800)
#define OFFSET_TEMPERATURE          3.7     // Compensation de température (en °C)
#define COMPENSATION_ALTITUDE       180     // Compensation altitude (en m)
#define ETALO_CO2                   410     // Valeur CO2 à l'étalonnage

#define SEUIL_B                     500.0   // Valeur du CO2 avec couleur bleu pur
#define SEUIL_V                     750.0   // Valeur du CO2 avec couleur vert pur
#define SEUIL_R                     1000.0  // Valeur du CO2 avec couleur rouge pur

#define NB_CYCLES_ETALO             120     // Nombre de cyles pour l'étalonnage
#define CDE_PIN                     0       // GPIO du bouton de commande
#define SLEEP_PIN                   35      // GPIO du bouton de veille
#define ADC_PIN                     34      // GPIO pour lecture de la tension
#define TEMPSAPPUILONG              3000    // Nombre de millisecondes pour considérer qu'un appui sur le bouton de commande est long
#define PAUSEINTERROCAPTEUR         500     // Nombre de millisecondes de pause entre deux interrogations de capteur
#define NBENREGMAX                  200     // Nombre max d'enregistrements (< TFTW)
#define NBENREGMINGRAPH             3       // Nombre d'enregistrement minimum pour afficher un graph
#define TFTH                        135     // Résolution Y
#define TFTW                        240     // Résolution X
#define ECHELLEMIN                  50      // Echelle mini du graphique

TFT_eSPI tft = TFT_eSPI();
SCD30 scd30;

bool etalo;                 // Etalonnage en cours
bool pincde;                // Etat de l'entrée du bouton de commande
bool pinsleep;              // Etat de l'entrée du bouton de veille
byte RGBrouge;              // Valeur du rouge
byte RGBvert;               // Valeur du vert
byte RGBbleu;               // Valeur du bleu
byte modeaff;               // Mode d'affichage
byte nbenreg;               // Nombre de mesures déjà enregistrées
uint16_t delai_etalo;       // Nombre de cyles restant dans l'étalonnage
uint16_t taux_co2;          // Taux de CO2 mesuré
uint16_t enreg[NBENREGMAX]; // Enregistrements
uint32_t refappuicde;       // Temps de référence appui sur le bouton de commande
uint32_t refcapteur;        // Temps de référence interrogation capteur
float temperature;          // Température mesurée
float humidite;             // Humidité mesurée
float tension;              // Tension du système

// --------------
// Initialisation
// --------------
void setup()
{
  uint16_t i;
  
  // Init pins
  pinMode(CDE_PIN, INPUT_PULLUP);
  pinMode(SLEEP_PIN, INPUT_PULLUP);
  
  // Init variables
  etalo = false;
  pincde = true;
  pinsleep = true;
  RGBrouge = 0;
  RGBvert = 0;
  RGBbleu = 0;
  modeaff = 1;
  nbenreg = 0;
  for (i=0; i<NBENREGMAX; i++)
    enreg[i] = 0;
 
  // Init TFT
  tft.init();
  tft.setRotation(1);
  tft.setSwapBytes(true);
  tft.pushImage(0,0,TFTW,TFTH,co2);
  delay(1000);

  // Init SCD30
  Wire.begin();
  Wire.setClock(50000); // 50kHz, recommended for SCD30  
  if (scd30.begin() == false)
  {
    // Le capteur CO2 n'est pas trouvé
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setCursor(0, 0);
    tft.println("Capteur CO2 non detecte !");
    while (1);
  }
  // Paramétrage du SCD30
  scd30.setAutoSelfCalibration(false);
  scd30.setMeasurementInterval(DELAI_MESURE);                // Nombre de secondes entre chaque mesure
  scd30.setAltitudeCompensation(COMPENSATION_ALTITUDE);      // Compensation d'altitude
  scd30.setTemperatureOffset(OFFSET_TEMPERATURE);            // Offset de température
  delay(2000);
}

// ----------------
// Boucle programme
// ----------------
void loop()
{
  if (digitalRead(SLEEP_PIN) != pinsleep)
  {
    // Détection du changement d'état du bouton de veille
    pinsleep = not pinsleep;
    if (pinsleep)
    {
      // Sur relâchement
      // Mise en veille
      scd30.StopMeasurement();
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_35,0);
      esp_deep_sleep_start();
    }
  }
  if (digitalRead(CDE_PIN) != pincde)
  {
    // Détection du changement d'état du bouton de commande
    pincde = not pincde;
    if (!pincde)
    {
      // Sur appui
      refappuicde = millis();     // On sauvegarde le temps courant
    }
    else
    {
      // Sur relâchement
      if (etalo)
      {
        // Si un étalonnage est en cours, on l'arrête
        etalo = false;
      }
      else
      {
        // En mode normal
        if (millis() - refappuicde < TEMPSAPPUILONG)
        {
          // Appui court -> On change de mode d'affichage
          if (modeaff < 3)
            modeaff++;
          else
            modeaff = 1;
        }
        else
        {
          // Appui long -> Démarrage de l'étalonnage
          delai_etalo = NB_CYCLES_ETALO;
          etalo = true;
        }
      }
      afficheEcran();
    }
  }
  if (millis() - refcapteur >= PAUSEINTERROCAPTEUR)
  {
    // On n'interroge pas le capteur dans chaque cycle
    mesure();
    refcapteur = millis();  
  }
}

// ------
// Mesure
// ------
void mesure()
{
  uint16_t i;

  if (scd30.dataAvailable())
  {
    // De nouvelles données duc apteur sont disponibles
    // Lecture tension
    tension = ((float)analogRead(ADC_PIN) / 4095.0) * 2.0 * 3.3 * (1100 / 1000.0);
    // Lecture CO2
    taux_co2 = scd30.getCO2();
    // Bornage de la lecture du CO2 dans les valeurs nominales du capteur
    if (taux_co2 < 400)
      taux_co2 = 400;
    else if (taux_co2 > 10000)
      taux_co2 = 10000;
    // Lecture température
    temperature = scd30.getTemperature();
    // Lecture humidité
    humidite = scd30.getHumidity();
    if (nbenreg < NBENREGMAX)
    {
      // On n'a pas encore enregistré toutes les valeurs possibles
      enreg[nbenreg] = taux_co2;
      nbenreg++;
    }
    else
    {
      // On a déjà enregistré toutes les valeurs possibles -> On décale le tableau
      for (i=0; i<NBENREGMAX; i++)
        enreg[i] = enreg[i+1];
      enreg[NBENREGMAX-1] = taux_co2;
    }
    if (etalo)
    {
      // Etalonnage en cours
      if (delai_etalo > 0)
      {
        // Délai étalonnage non terminé
        delai_etalo--;
      }
      else
      {
        // Délai étalonnage terminé
        etalo = false;
        scd30.setForcedRecalibrationFactor(ETALO_CO2);
        // Vidage du tableau des enregistrements
        for (i=0; i<NBENREGMAX; i++)
          enreg[i] = 0;
        nbenreg = 0;
      }
    }
    afficheEcran();
  }
}

// ---------------
// Affichage écran
// ---------------
void afficheEcran()
{
  String co2s;
  uint16_t i;
  uint16_t valmin;
  uint16_t valmax;
  uint16_t echmin;
  uint16_t echmax;

  // Effacement écran
  tft.fillScreen(TFT_BLACK);
  if (etalo)
  {
    // Etalonnage en cours
    tft.setTextColor(TFT_WHITE);
    tft.setTextFont(1);
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    tft.println("ETALONNAGE");
    tft.println("");
    tft.println("Placez l'appareil");
    tft.println("a l'air libre");
    tft.println("");
    tft.println("Encore " + String(delai_etalo * DELAI_MESURE) + " secondes");
    tft.println("...");
  }
  else
  {
    if (modeaff == 1)
    {
      // Mode valeur instantanée
      setCouleur(taux_co2);
      // Température et humidité
      tft.setTextColor(TFT_WHITE);
      tft.setTextFont(1);
      tft.setTextSize(2);
      tft.setCursor(0, 0);
      tft.println("T: " + (String)temperature + "  H: " + (String)humidite + "%");
      // Tension
      tft.setCursor(0, 120);
      tft.println((String)tension + "V");
      // CO2
      tft.setTextColor(tft.color565(RGBrouge, RGBvert, RGBbleu));
      tft.setCursor(150, 120);
      tft.println("ppm CO2");
      tft.setTextFont(8);
      tft.setTextSize(1);
      tft.setCursor(10, 31);
      co2s = (String)taux_co2;
      while(co2s.length() < 4)
        co2s = "0" + co2s;
      tft.println(co2s);
    }
    else if (modeaff == 2)
    {
      // Mode graphique
      if (nbenreg >= NBENREGMINGRAPH)
      {
        // On a assez d'enregistrements pour afficher un graph
        // Recherche des valeurs min et max
        valmin = 15000;
        valmax = 0;
        for (i=0; i<nbenreg; i++)
        {
          if (enreg[i] < valmin)
            valmin = enreg[i];
          if (enreg[i] > valmax)
            valmax = enreg[i];
        }
        // Définition de l'échelle
        if (valmax-valmin >= ECHELLEMIN)
        {
          // L'écart entre la val min et la val max est supérieur à l'échelle min
          echmin = valmin;
          echmax = valmax;
        }
        else
        {
          // L'écart entre la val min et la val max est inférieur à l'échelle min
          echmin = (valmax+valmin) / 2 - ECHELLEMIN / 2 ;
          echmax = (valmax+valmin) / 2 + ECHELLEMIN / 2 ;
        }
        // Affichage de la valeur courante
        tft.setTextFont(1);
        tft.setTextSize(2);
        setCouleur(taux_co2);
        tft.setTextColor(tft.color565(RGBrouge, RGBvert, RGBbleu));
        tft.setCursor(0, 60);
        tft.print((String)taux_co2);
        // Affichage des valeurs min et max
        setCouleur(valmin);
        tft.setTextColor(tft.color565(RGBrouge, RGBvert, RGBbleu));
        tft.setCursor(0, 120);
        tft.print((String)valmin);
        setCouleur(valmax);
        tft.setTextColor(tft.color565(RGBrouge, RGBvert, RGBbleu));
        tft.setCursor(0, 0);
        tft.print((String)valmax);
        // Dessin du graphque
        setCouleur(enreg[0]);
        tft.drawPixel(TFTW-NBENREGMAX-1, (TFTH-1) - round(float(TFTH-1) * float(enreg[0] - echmin) / float(echmax - echmin)), tft.color565(RGBrouge, RGBvert, RGBbleu));
        for (i=1; i<nbenreg; i++)
        {
          setCouleur(enreg[i]);
          tft.drawLine(i+TFTW-NBENREGMAX-2, (TFTH-1) - round(float(TFTH-1) * float(enreg[i-1] - echmin) / float(echmax - echmin)), i+TFTW-NBENREGMAX-1, (TFTH-1) - round(float(TFTH-1) * float(enreg[i] - echmin) / float(echmax - echmin)), tft.color565(RGBrouge, RGBvert, RGBbleu));
        }        
      }
      else
      {
        // On n'a pas assez d'enregistrements
        tft.setTextColor(TFT_WHITE);
        tft.setTextFont(1);
        tft.setTextSize(2);
        tft.setCursor(0, 0);
        tft.println("Pas encore assez");
        tft.println("de donnees pour");
        tft.println("le graphique...");
      }
    }
    else if (modeaff == 3)
    {
      // Affichage paramètres
      tft.setTextColor(TFT_WHITE);
      tft.setTextFont(1);
      tft.setTextSize(1);
      tft.setCursor(0, 0);
      tft.println("--- MESURE CO2 ---");
      tft.println("Noel Utter    2022");
      tft.println("");
      tft.println("Version " + String(PROGVER));
      tft.println("");
      if (scd30.getFirmwareVersion(&i) == true)
      {  
        tft.print("- Firmware SCD30 :       0x");
        tft.println(i, HEX);
      }
      tft.println("- Autocalibration:       " + String(scd30.getAutoSelfCalibration()));
      tft.println("- Intervalle de mesures: " + String(scd30.getMeasurementInterval()) + " s");
      tft.println("- Compensation altitude: " + String(scd30.getAltitudeCompensation()) + " m");
      tft.println("- Offset temperature:    " + String(scd30.getTemperatureOffset()) + " C");
      tft.println("- Seuils de couleurs:");
      tft.println("    BLEU  " + String(SEUIL_B));
      tft.println("    VERT  " + String(SEUIL_V));
      tft.println("    ROUGE " + String(SEUIL_R));
    }
  }
}

// -------------------------------------
// Taduction de la valeur en couleur RVB
// -------------------------------------
void setCouleur(uint16_t co2val)
{
  float valeur = (float)co2val;

  // Définition du rouge
  if (valeur < SEUIL_V)
    RGBrouge = 0;
  else if (valeur >= SEUIL_R)
    RGBrouge = 255;
  else
    RGBrouge = round((valeur - SEUIL_V) / (SEUIL_R - SEUIL_V) * 255);

  // Définition du bleu
  if (valeur < SEUIL_B)
    RGBbleu = 255;
  else if (valeur >= SEUIL_V)
    RGBbleu = 0;
  else
    RGBbleu = 255 - round((valeur - SEUIL_B) / (SEUIL_V - SEUIL_B) * 255);

  // Définition du vert
  if (valeur < SEUIL_B)
    RGBvert = 0;
  else if (valeur >= SEUIL_R)
    RGBvert = 0;
  else if (valeur < SEUIL_V)
    RGBvert = round((valeur - SEUIL_B) / (SEUIL_V - SEUIL_B) * 255);
  else  
    RGBvert = 255 - round((valeur - SEUIL_V) / (SEUIL_R - SEUIL_V) * 255);
}
