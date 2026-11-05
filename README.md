# ESP32-H2 Matter-over-Thread Sensorbasis

Gemeinsame Grundlage fuer batteriebetriebene DoNetIoT-Sensoren. Erstes Hardwareprofil:
ESP32-H2 Super Mini (4 MB Flash), externe geregelte 3,3-V-Versorgung.

## Variante drei Bodensensoren (dieser Branch)

Version 0.5.0-soil3 liest drei kapazitive Bodenfeuchtesensoren nacheinander und meldet
drei Feuchte-Endpunkte. Jeder Sensor hat eigene Trocken-/Nassreferenzen, kalibriert
ueber Home Assistant. Produkt `MoistureSensor3`, Produkt-ID `0x8001`, Anlerncode je
Geraet aus der MAC. Verdrahtung und Bedienung: [BODENFEUCHTE-3.md](docs/BODENFEUCHTE-3.md).
Gebaut und am Board getestet ist diese Variante noch nicht.

Die folgenden Abschnitte beschreiben die Ein-Sensor-Variante, auf der sie aufbaut.

## Aktueller Stand

Version 0.4.0-soil-ota meldet das Produkt `MoistureSensor`, den Hersteller
`DoNetIoT` und die Hardware `ESP32-H2` an Matter. Die numerischen Produkt- und
Hersteller-IDs bleiben gleich. Nach einem Update gegebenenfalls in Home Assistant
`Geraet neu abfragen` ausfuehren, damit die Angaben aktualisiert werden.

Die Firmware liest einen kapazitiven Bodenfeuchtesensor v1.2/v2.0
an GPIO4. Sensor-VCC kommt direkt an GPIO1: HIGH waehrend der Messung,
LOW dazwischen. GND bleibt an GND. VCC erst nach Installation von 0.3.1 umstecken;
0.3.0 verwendet noch die umgekehrte Polaritaet fuer einen externen Schalter.
Trocken-/Nasskalibrierung per BOOT wird in NVS gespeichert. Ohne Kalibrierung
wird kein Prozentwert erfunden. Messung erfolgt beim Start, standardmaessig alle 60 Minuten
und auf kurzen BOOT-Druck. Anleitung: [BODENFEUCHTE.md](docs/BODENFEUCHTE.md).

Die Home-Assistant-Geraeteseite erhaelt zwei Matter-Modusauswahlen:
**Bodenfeuchte bedienen** (Jetzt messen, Trocken/Nass kalibrieren) und
**Messintervall** (1, 5, 10, 15, 30, 60, 120, 360, 720, 1440 Minuten).
Einstellungen bleiben bei Neustart erhalten. Automatischer Light-sleep ist
aktiviert; fuer echte Verbrauchsmessungen USB vom PC trennen.
Die beiden Einstellungs-Endpunkte und die direkte GPIO-Versorgung wurden am
03.11.2026 am Board bestaetigt. Die ueber Home Assistant gesetzte Trockenreferenz
und das Testintervall von 1 Minute blieben nach einer Stromunterbrechung erhalten.
Der Benutzer bestaetigte die automatische Feuchteaenderung in Home Assistant
ohne Tastendruck, die Nasskalibrierung mit etwa 100 % und die Rueckstellung
auf 60 Minuten. Der Aufwecktest ohne PC-USB-Host steht noch aus.
Die reale Versorgungsspannung unter Last, der Stromverbrauch
und das tatsaechliche Schlafverhalten ohne PC-USB-Host sind noch zu messen.

Seit 0.2.1: Auswahl waehrend des Haltens an der blauen GPIO13-LED erkennen,
anstatt Sekunden abzuzaehlen. Beim ersten Blink loslassen fuer trocken,
fuer nass weiterhalten bis zum Doppelblink. Ab 10 Sekunden zeigt schnelles
Blinken den Matter-Werksreset an. Anlernen per Doppelklick. Diese neue
Einzelblink und Trocken-Speicherung wurden am Board bestaetigt; der Doppelblink
und die Doppelklick-Bedienung stehen noch zur visuellen Abnahme aus.
Die folgenden Hardwaretests wurden mit 0.2.0-soil-usbdiag durchgefuehrt.

Am 03.11.2026 wurden Firmware-Build, Flashen und die reale Bodenfeuchtemessung am
neuen H2 (USB-Seriennummer endet DA:89) bestaetigt. Trocken-/Nasskalibrierung,
Home-Assistant-Kopplung und Wertuebertragung funktionieren. Nach einer
USB-Stromunterbrechung wurden Kalibrierung und beide Matter-Fabrics geladen,
Thread-Wiederbeitritt und Abonnementfortsetzung bestaetigt. Auch eine automatische
Messung nach 15 Minuten wurde mit 0.2.0 bestaetigt. Batteriebetrieb und Verbrauch
sind noch nicht geprueft.
Das alte Board startete im Vergleich nicht; ein konkreter Hardwaredefekt ist
nicht bewiesen. Der gepruefte Build:
[0.3.1-soil-gpio auf GitHub Actions](https://github.com/OliverDoMS/esp-matter-over-thread/actions/runs/41654752377).
Das lokal heruntergeladene GPIO-Paket 0.3.1 liegt in `dist/soil-gpio`.
0.4.0-soil-ota wurde ebenfalls erfolgreich gebaut und heruntergeladen:
[OTA-Build auf GitHub Actions](https://github.com/OliverDoMS/esp-matter-over-thread/actions/runs/41726895662).
Das neue Paket liegt in `dist/soil-ota`. SHA-256, Matter-Header und Payload,
Geraetenamen sowie die unveraenderte Partitionstabelle wurden lokal geprueft.
Die Anwendung belegt 1544224 Bytes; pro Slot bleiben etwa 21 % frei.
Installation und Funkuebertragung von 0.4.0 sind am Board noch nicht abgenommen.
Die alten lokalen Pakete `dist-usbdiag` und `dist-lowpower` enthalten weiterhin
die bisherigen Versionen. Neue Builds ersetzen diese Diagnosepakete nicht.

Enthalten sind Thread Sleepy End Device, Matter Short Idle Time ICD und eine reale
ADC-Messung. Ab 0.3.0 ist automatischer Light-sleep aktiviert.
Der Sensortask wartet zwischen Messungen; ADC-Abtastung blockiert den Matter-Loop
nicht. Es gibt keine WLAN-Verbindung und keine simulierten Messwerte.

| BOOT-Taste nach dem Start | Funktion beim Loslassen |
|---|---|
| Einmal kurz druecken | Bodenfeuchte jetzt messen |
| Zweimal kurz hintereinander | Anlernfenster fuer 5 Minuten oeffnen |
| Halten bis die LED einmal blinkt, dann loslassen (ab ca. 2 s) | TROCKEN / 0 % kalibrieren |
| Weiterhalten bis die LED zweimal blinkt, dann loslassen (ab ca. 5 s) | NASS / 100 % kalibrieren |
| Halten bis die LED schnell blinkt, dann loslassen (ab 10 s) | Werksreset, Kopplung und Thread-Konfiguration loeschen |

Die Bodenfeuchte wird als relativer Zweipunktindex ueber einen Matter-Humidity-
Endpunkt uebertragen; Home Assistant kann ihn zunaechst als Luftfeuchte anzeigen.
Die Firmware misst keine Luftfeuchte und keinen volumetrischen Wassergehalt.
Die BOOT-Taste beim Einschalten/Reset fuehrt stattdessen in den ROM-Downloadmodus.

## Firmware erzeugen

Der vorgesehene Build laeuft ueber GitHub Actions. Aenderungen auf dem aktuellen
Arbeitsbranch committen und pushen:

```powershell
git add .
git commit -m "Update ESP32-H2 Matter firmware"
git push
```

Unter **Actions → Build ESP32-H2 firmware** den erfolgreichen Lauf oeffnen und
das Artefakt **esp32h2-supermini-base** herunterladen und entpacken.
Der Build verwendet ESP-IDF 5.4.1 und den exakten ESP-Matter-Commit aus `versions.env`
(Releasefamilie Matter 1.4.2). Die aufgeloesten Komponenten-Versionen werden im
Flash-Paket als `dependencies.lock` mitgeliefert.

Alternativ mit Docker Desktop im Linux-Container-Modus aus dem Repositoryordner:

```powershell
docker run --rm -v "${PWD}:/project" -w /project espressif/esp-matter:release-v1.4.2_idf_v5.4.1 bash scripts/build.sh
```

Docker benoetigt beim ersten Start mehrere GB Speicher und Internetzugang.
Eine native Windows-Installation von ESPHome/ESP-IDF allein ersetzt die
Linux-Buildumgebung fuer ESP-Matter nicht.

## Flashen und anlernen

Siehe [docs/FLASHEN.md](docs/FLASHEN.md). Fuer den ersten Funktionstest ausschliesslich
USB verwenden und Batterie/Wandler abtrennen. Die Versorgung mit dem TPS63802 wird
anschliessend getrennt aufgebaut; siehe [docs/HARDWARE.md](docs/HARDWARE.md).

## Erweiterungen

| Datei | Aufgabe |
|---|---|
| `main/app_main.cpp` | NVS, Matter, Thread, Energieverwaltung, Anlernen und Reset |
| `main/board.cpp` | BOOT-Taste und Abschalten der steuerbaren Board-LEDs |
| `main/sensors.cpp` | Sensor-Endpunkte |
| `main/soil_moisture.cpp` | ADC, Kalibrierung, Messintervall und Matter-Publizieren |
| `main/soil_math.h` | Gepruefte Zweipunktumrechnung |
| `main/Kconfig.projbuild` | Boardoptionen und Testkontakt |
| `sdkconfig.defaults` | Netzwerk-, ICD- und Schlafkonfiguration |

Neue Varianten ergaenzen passende Endpunkte und Treiber in der Sensorschicht.
I2C-Pins und Sensorversorgung werden erst mit der konkreten Erweiterung festgelegt.
Batteriespannung, Batterie-Prozentanzeige, reale Hall-Eingaenge, Temperatur,
Luftfeuchte und Helligkeit sind noch nicht implementiert. Ein Spannungsteiler darf nicht einfach
an die konstant geregelte 3,3-V-Schiene angeschlossen werden, wenn die Batterie
gemessen werden soll. Ab 0.4.0 ist Matter OTA aktiviert; der Build erzeugt
`MoistureSensor.ota`. Die Erstaktivierung erfolgt einmalig per USB, danach
benoetigt Home Assistant eine lokale OTA-Updatequelle: [OTA.md](docs/OTA.md).

## Pruefungen

```powershell
python -m unittest discover -s tests -v
.\tests\test_flash.ps1
```

Die lokalen Tests pruefen das Flash-Layout und die Paketierung einschliesslich
falschem Chip, fehlenden Dateien, Ueberlappungen und Pruefsummen. Sie ersetzen weder
den echten Firmware-Build noch Anlernen, Sleep-/Wake-Pruefung und Strommessung am Board.
Das PowerShell-Testskript faengt alle esptool-Aufrufe ab; es schreibt keine Firmware
auf ein Geraet. `tests/button_actions.cpp` prueft ausserdem die Tasten-Zeitgrenzen
per C++-Compile-time-Assertions, insbesondere die Grenze fuer den Werksreset.

Grundlage: Espressifs [ICD-Beispiel](https://github.com/espressif/esp-matter/tree/d3aa1b96b9357b208f15cc12d1864dc124414c6b/examples/icd_app).
