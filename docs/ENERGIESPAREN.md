# Stromsparbetrieb und Home-Assistant-Einstellungen ab 0.3.0

## Bedienung auf der Geraeteseite

Zwei standardisierte Matter-ModeSelect-Endpunkte sollen in Home Assistant unter
Konfiguration erscheinen (keine zusaetzliche Integration erforderlich):

- **Bodenfeuchte bedienen:** Bereit, Jetzt messen, Trocken kalibrieren,
  Nass kalibrieren. Den Sensor zuerst in die jeweilige Referenz bringen,
  stabilisieren lassen und dann die passende Aktion waehlen. Nach Bearbeitung
  springt die Auswahl auf Bereit zurueck. Fehler werden im Geraeteprotokoll
  gemeldet; eine abgelehnte Kalibrierung ersetzt keine gueltige Referenz.
- **Messintervall:** 1, 5, 10, 15, 30, 60, 120, 360, 720 oder 1440 Minuten.
  Standard ist 60 Minuten. Eine Aenderung wird im NVS gespeichert und startet
  einen neuen Countdown. Eine zusaetzliche Sofortmessung startet ebenfalls
  einen neuen Countdown, aendert aber nicht das konfigurierte Intervall.

Der einzelne kurze BOOT-Klick misst einmal; der Doppelklick oeffnet das
Anlernfenster. Die LED-Auswahl fuer lokale Kalibrierung bleibt erhalten.
Die neuen Konfigurations-Endpunkte muessen nach dem Update in Home Assistant
neu erkannt werden. Zunaechst ohne Factory-Reset aktualisieren; falls der
Controller die neuen Endpunkte nicht erkennt, den Interview-/Erkennungsablauf
des Controllers pruefen. Vor einem eventuell noetigen Werksreset erst die
bisherige Kopplung und Kalibrierung sichern.

## Schlaf und Erreichbarkeit

Das Geraet bleibt Thread Sleepy End Device und Matter SIT ICD. Automatischer
Light-sleep erlaubt Schlaf zwischen Sensor- und Protokollaufgaben. Eine Messung
alle 60 Minuten bedeutet daher nicht, dass das Funkprotokoll nur einmal pro
Stunde aufwacht. Fernbefehle bleiben moeglich; der aktuelle langsame Thread-
Pollintervall betraegt 5 Sekunden, sodass eine Bedienaktion verzoegert ankommen
kann. Es wird kein stundenlanger Deep-sleep erzwungen, der Fernbefehle verhindert.

Waehrend der Sensor-Einschwingzeit und ADC-Abtastung verhindert eine temporaere
PM-Sperre Light-sleep. Anschliessend werden Versorgungsausgang und ADC
abgeschaltet und die PM-Sperre freigegeben. Der Task wartet blockierend bis
zur naechsten Messung oder einem Bedienbefehl. GPIO-Wakeup fuer BOOT und die
bewaehrte Einstellung ohne Peripherie-Power-down bleiben erhalten.

Ein angeschlossener PC-USB-Host kann Light-sleep sperren. Verbrauch daher ohne
PC messen. Light-sleep, Thread-Polling, Regler und Board-LED-Elektronik gemeinsam
erfassen; aus der Softwarekonfiguration keine Batterielaufzeit ableiten.

## Schaltbare Sensorversorgung vorbereiten

GPIO1 ist nur ein Steuersignal. Den Sensor nicht ungeprueft direkt daraus
versorgen. Das Standardprofil erwartet einen **externen P-Kanal-MOSFET** als
High-Side-Schalter, der bei GPIO1=LOW einschaltet:

| Verbindung | Ziel |
|---|---|
| Geregelte 3V3 | P-MOSFET Source |
| P-MOSFET Drain | Sensor VCC |
| GPIO1 ueber etwa 1 kOhm | P-MOSFET Gate |
| 100 kOhm zwischen Gate und Source | Haltet Sensor beim Start ausgeschaltet |
| Sensor AOUT | GPIO4 |
| Sensor GND | Board GND |

Vor dem Aufbau die konkrete Source-/Drain-/Gate-Pinbelegung und Eignung des
MOSFETs fuer VGS=-3,3 V pruefen. Kein ungeprueftes Modul und keine 5-V-Schiene
verwenden. Fuer ein anderes Lastschaltermodul GPIO-Polaritaet und Beschaltung
anhand seines Datenblatts anpassen (`BASE_SOIL_POWER_ACTIVE_LOW`).

Der Steuerausgang ist vor dem Start des Messtasks auf aus gesetzt und bleibt
auch im Light-sleep stabil. Pro Messung wird eingeschaltet, standardmaessig
500 ms gewartet, abgetastet und wieder ausgeschaltet. `BASE_SOIL_SETTLE_MS`
kann fuer den konkreten Sensor angepasst werden.

Solange der externe Schalter fehlt, kann VCC fuer den Funktionstest weiter
direkt an 3V3 bleiben. Dann schaltet die Firmware zwar GPIO1, aber der Sensor
selbst bleibt dauerhaft versorgt. Das ist noch kein Nachweis des angestrebten
stromsparenden Gesamtsystems.

## Abnahme

1. Version, erhaltene Kalibrierung und Matter-Wiederbeitritt im Log pruefen.
2. Beide Konfigurationsauswahlen auf der Home-Assistant-Geraeteseite pruefen.
3. Sofortmessung und Trocken-/Nasskalibrierung von dort aus ausloesen.
4. Messintervall testweise auf 1 Minute setzen und automatische Messung abwarten.
5. Neustart: gewaehltes Intervall und Kalibrierung muessen erhalten bleiben.
6. Auf 60 Minuten zurueckstellen. Versorgungsschalter am konkreten Bauteil
   pruefen: Sensor-VCC zwischen Messungen aus, waehrend der Messung etwa 3,3 V.
7. Ohne PC-USB-Host nach mehreren Minuten Ruhe BOOT und Fernbedienung pruefen;
   danach den realen mittleren Strom messen.

Referenzen: [Home Assistant ModeSelect](https://github.com/home-assistant/core/blob/dev/homeassistant/components/matter/select.py),
[Matter-ModeSelect im verwendeten SDK](https://github.com/espressif/esp-matter/blob/d3aa1b96b9357b208f15cc12d1864dc124414c6b/components/esp_matter/data_model/esp_matter_cluster.h),
[ESP32-H2 Power Management](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32h2/api-reference/system/power_management.html).
