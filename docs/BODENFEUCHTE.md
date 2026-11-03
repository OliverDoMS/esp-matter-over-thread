# Bodenfeuchte mit ESP32-H2 und kapazitivem Sensor v1.2/v2.0

Ab 0.3.0: [Home-Assistant-Konfiguration und Stromsparbetrieb](ENERGIESPAREN.md).
Das Standard-Messintervall ist jetzt 60 Minuten und ueber Matter auswaehlbar.
Ab 0.3.1 versorgt GPIO1 den Sensor direkt (HIGH=an, LOW=aus). Vorher muss noch
die neue Firmware installiert werden; 0.3.0 hat eine andere Polaritaet.
Die folgenden Diagnoseabschnitte beziehen
sich auf 0.2.x, wo automatischer Light-sleep noch deaktiviert war.

Seit Version 0.2.0-soil-usbdiag ersetzt eine echte ADC-Messung den virtuellen
Kontakt. Version 0.2.1-soil-led ergaenzt die visuelle Tastenauswahl. Die
Sensorversorgung bleibt fuer den ersten Funktionstest dauerhaft eingeschaltet.
Automatischer Light-sleep ist wie im bewaehrten USB-Diagnoseprofil deaktiviert.
Diese Version ist noch nicht fuer Batterielaufzeitmessungen vorgesehen.

## Diagnose-Verdrahtung fuer 0.2.x und 0.3.0

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

**Ab 0.3.1:** Sensor-VCC stattdessen direkt an GPIO1 anschliessen, wie in
ENERGIESPAREN.md beschrieben. GPIO1 nicht mit 3V3 verbinden. Erst Firmware
aktualisieren, dann bei stromlosem Board das Sensor-VCC-Kabel umstecken.

## Flashen und Kopplung

### Update von 0.2.0 auf 0.2.1

Nur die Anwendung mit dem neuen Paket schreiben:
`./flash.ps1 -Port COM26 -ApplicationOnly -Baud 115200` (Port anpassen).
Partitionstabelle und Matter-Endpunkttyp sind unveraendert; Kopplung und
Kalibrierung bleiben dabei erhalten. Kein Factory-Image und kein `-EraseAll`
fuer dieses Bedienungsupdate verwenden. Danach RST kurz druecken und
`Firmware 0.2.1-soil-led` im Log pruefen. Die neue LED-Bedienung ist am Board
noch separat zu bestaetigen.

### Erstinstallation oder Wechsel vom Testkontakt

Das neue Datenmodell ist ein Feuchtesensor statt Kontakt. Das Geraet in den
bisherigen Controllern entfernen und mit dem neuen Firmwarepaket neu anlernen.
Ein erneutes Factory-Flash setzt vorhandene Kopplungsdaten zurueck.
Vor einem Factory-Flash pruefen, dass das richtige Board angeschlossen ist.
Flash- und Kopplungsablauf: FLASHEN.md. Der Entwicklungs-Anlerncode bleibt
34970112332; er wird durch die Kalibrierung nicht geaendert.

## Tasten und Kalibrierung

Die blaue Board-LED an GPIO13 zeigt beim Halten die ausgewaehlte Aktion an.
Erst beim Loslassen wird sie ausgefuehrt. BOOT beim Reset nicht gedrueckt halten.
Die LED bleibt sonst aus. `BASE_BUTTON_LED_FEEDBACK` gilt fuer das dokumentierte
Super-Mini-Layout; bei abweichender LED-Beschaltung deaktivieren.

| BOOT bedienen | Funktion |
|---|---|
| Einmal kurz druecken | Sofort messen |
| Doppelklick (zweimal kurz, innerhalb ca. 400 ms) | Anlernfenster oeffnen |
| Halten bis einmal Blinken, dann loslassen (ab ca. 2 s) | TROCKEN / 0 % speichern |
| Weiterhalten bis zweimal Blinken, dann loslassen (ab ca. 5 s) | NASS / 100 % speichern |
| Weiterhalten bis schnelles Blinken, dann loslassen (erst ab 10 s) | Matter-Werksreset |

Fuer nass das erste einzelne Blinken ignorieren und weiter gedrueckt halten,
bis der Doppelblink erscheint. Kein Anlernfenster liegt mehr zwischen den
Kalibrierstufen. Loslassen zwischen einer und zwei Sekunden bewirkt nichts.

1. Sensor in die gewuenschte trockene Referenz bringen (z.B. trockenes Substrat),
   stabilisieren lassen, BOOT halten bis die LED einmal blinkt, dann loslassen.
2. Sensor in die feuchte Referenz bringen (z.B. durchfeuchtetes Substrat),
   stabilisieren lassen, BOOT ueber den Einzelblink hinaus halten bis die LED
   zweimal blinkt, dann loslassen. Nicht bis zum schnellen Reset-Blinken halten.
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

Pruefstand 03.11.2026, H2 mit USB-Seriennummer-Endung DA:89:
Firmware 0.2.0-soil-usbdiag erfolgreich gebaut, geschrieben und per esptool
verifiziert. Das Testexemplar speicherte trocken 2816 und nass 1008 ADC-Schritte;
diese Werte sind keine allgemeingueltige Kalibrierung fuer andere Sensoren.
100 %, 0,88 % und 0 % wurden ueber Matter gemeldet, vom Controller bestaetigt
und die Feuchteanzeige vom Benutzer in Home Assistant bestaetigt.
Nach USB-Stromunterbrechung wurden dieselben Referenzen und beide Fabrics
geladen. Thread-Wiederbeitritt, Abonnementfortsetzung und die Uebertragung des
ersten neuen Werts (0,60 %) waren im Log sichtbar. Ein separater Tastendruck-RST-
Test und der automatische 15-Minuten-Messzyklus stehen noch aus.
Diagnoseprotokoll lokal: `output/h2-recovery/soil-da89-20261103.log`
(von Git ausgeschlossen).

- Nach dem Start muss die erwartete Firmwareversion im Log stehen.
- Bei 0.2.1 Einzelblink, Doppelblink, Doppelklick zum Anlernen und ausgebliebene
  Reset-Aktion unter 10 Sekunden am Board pruefen.
- Trocken-/Nassreferenzen kalibrieren und Zwischenwerte in Home Assistant pruefen.
- RST und Stromunterbrechung pruefen: Kalibrierung und Kopplung bleiben erhalten.
- Automatische Messung nach 15 Minuten pruefen.
- Erst danach Sensorversorgung schaltbar machen und Low-power-Betrieb separat testen.

Grundlagen: [H2 ADC Oneshot](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32h2/api-reference/peripherals/adc_oneshot.html),
[H2 GPIOs](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32h2/api-reference/peripherals/gpio.html),
[SDK-Endpunkte](https://github.com/espressif/esp-matter/blob/d3aa1b96b9357b208f15cc12d1864dc124414c6b/components/esp_matter/data_model/esp_matter_endpoint.h).
