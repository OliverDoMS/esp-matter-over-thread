# Bodenfeuchte mit ESP32-H2 und kapazitivem Sensor v1.2/v2.0

Version 0.2.0-soil-usbdiag ersetzt den virtuellen Kontakt durch eine echte
ADC-Messung. Der Sensor bleibt fuer den ersten Funktionstest dauerhaft versorgt.
Automatischer Light-sleep ist wie im bewaehrten USB-Diagnoseprofil deaktiviert.
Diese Version ist noch nicht fuer Batterielaufzeitmessungen vorgesehen.

## Verdrahtung

Board vor dem Verdrahten stromlos machen. Fuer den ersten Test nur USB nutzen,
Batterie und Wandler abtrennen.

| Sensor | H2 Super Mini |
|---|---|
| VCC | 3V3 |
| GND | GND |
| AOUT / Analog | GPIO4 |

Nicht an 5 V betreiben. GPIO4 ist beim H2 ADC1 Kanal 3. Anders als beim alten
ESPHome-C3-Aufbau wird VCC hier nicht ueber GPIO1 geschaltet. Die Elektronik
des Sensors und die markierte Eintauchgrenze muessen trocken bleiben.
Optional 100 nF von GPIO4 nach GND, nahe am Board, zur ADC-Filterung.

## Flashen und Kopplung

Das neue Datenmodell ist ein Feuchtesensor statt Kontakt. Das Geraet in den
bisherigen Controllern entfernen und mit dem neuen Firmwarepaket neu anlernen.
Ein erneutes Factory-Flash setzt vorhandene Kopplungsdaten zurueck.
Vor einem Factory-Flash pruefen, dass das richtige Board angeschlossen ist.
Flash- und Kopplungsablauf: FLASHEN.md. Der Entwicklungs-Anlerncode bleibt
34970112332; er wird durch die Kalibrierung nicht geaendert.

## Tasten und Kalibrierung

Die Aktion erfolgt beim Loslassen; BOOT beim Reset nicht gedrueckt halten.

| BOOT halten | Funktion |
|---|---|
| Unter 1 Sekunde | Sofort messen |
| 1 bis unter 3 Sekunden | Aktuellen Messwert als TROCKEN / 0 % speichern |
| 3 bis unter 6 Sekunden | Anlernfenster oeffnen |
| 6 bis unter 10 Sekunden | Aktuellen Messwert als NASS / 100 % speichern |
| Ab 10 Sekunden | Matter-Werksreset |

1. Sensor in die gewuenschte trockene Referenz bringen (z.B. trockenes Substrat),
   stabilisieren lassen, BOOT etwa 2 Sekunden halten und loslassen.
2. Sensor in die feuchte Referenz bringen (z.B. durchfeuchtetes Substrat),
   stabilisieren lassen, BOOT etwa 7 Sekunden halten und loslassen.
3. Im USB-Log beide Meldungen `Saved DRY calibration` und `Saved WET calibration`
   sowie plausible Rohwerte pruefen. Der Trockenwert muss mindestens 100
   ADC-Schritte ueber dem Nasswert liegen; andernfalls wird die zweite
   Kalibrierung abgelehnt. Bei einem Wechsel des Sensortyps zuerst denselben
   Referenzzustand wie bei der bisherigen Kalibrierung herstellen.
4. Kurzer BOOT-Druck misst erneut. Automatisch wird alle 15 Minuten gemessen.

Die Kalibrierung bleibt nach RST und Stromunterbrechung erhalten. Sie liegt im
NVS-Namespace `soil_cal`. Ein Matter-Werksreset entfernt die Matter-/Thread-Daten;
die Kalibrierung muss bei Sensorwechsel erneut gesetzt werden. Ein Factory-Flash
setzt auch die Kalibrierung zurueck.

Ohne vollstaendige gueltige Kalibrierung wird der Wert als unbekannt gemeldet.
Messfehler und Rohwerte nahe den ADC-Grenzen fuehren ebenfalls zu unbekannt.
Ein offener Analogeingang kann zufaellige plausible Werte liefern: Die Software
kann einen abgezogenen Sensor nicht verlaesslich erkennen.

## Darstellung in Home Assistant

Das festgelegte ESP-Matter-SDK 1.4.2 stellt hier den `humidity_sensor` mit
`RelativeHumidityMeasurement` bereit. Die Firmware nutzt diesen kompatiblen
Prozent-Endpunkt. Home Assistant kann ihn als Luftfeuchte anzeigen; den Namen
auf Bodenfeuchte setzen. Es ist ein relativer Zweipunktindex fuer den konkreten
Sensor und das Substrat, weder eine Luftfeuchtemessung noch volumetrischer
Wassergehalt. Werte ausserhalb der Referenzen werden auf 0 bis 100 % begrenzt.

## Abnahme am Board

- Nach dem Start muss `Firmware 0.2.0-soil-usbdiag` im Log stehen.
- Trocken-/Nassreferenzen kalibrieren und Zwischenwerte in Home Assistant pruefen.
- RST und Stromunterbrechung pruefen: Kalibrierung und Kopplung bleiben erhalten.
- Automatische Messung nach 15 Minuten pruefen.
- Erst danach Sensorversorgung schaltbar machen und Low-power-Betrieb separat testen.

Grundlagen: [H2 ADC Oneshot](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32h2/api-reference/peripherals/adc_oneshot.html),
[H2 GPIOs](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32h2/api-reference/peripherals/gpio.html),
[SDK-Endpunkte](https://github.com/espressif/esp-matter/blob/d3aa1b96b9357b208f15cc12d1864dc124414c6b/components/esp_matter/data_model/esp_matter_endpoint.h).
