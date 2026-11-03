# ESP32-H2 Matter-over-Thread Sensorbasis

Gemeinsame Grundlage fuer batteriebetriebene DoNetIoT-Sensoren. Erstes Hardwareprofil:
ESP32-H2 Super Mini (4 MB Flash), externe geregelte 3,3-V-Versorgung.

## Aktueller Stand

Version 0.2.0-soil-usbdiag liest einen kapazitiven Bodenfeuchtesensor v1.2/v2.0
an GPIO4. VCC wird fuer den ersten Test mit 3V3 verbunden, GND mit GND.
Trocken-/Nasskalibrierung per BOOT wird in NVS gespeichert. Ohne Kalibrierung
wird kein Prozentwert erfunden. Messung erfolgt beim Start, alle 15 Minuten
und auf kurzen BOOT-Druck. Anleitung: [BODENFEUCHTE.md](docs/BODENFEUCHTE.md).

Am 03.11.2026 wurden Firmware-Build, Flashen und die reale Bodenfeuchtemessung am
neuen H2 (USB-Seriennummer endet DA:89) bestaetigt. Trocken-/Nasskalibrierung,
Home-Assistant-Kopplung und Wertuebertragung funktionieren. Nach einer
USB-Stromunterbrechung wurden Kalibrierung und beide Matter-Fabrics geladen,
Thread-Wiederbeitritt und Abonnementfortsetzung bestaetigt. Automatische Messung
nach 15 Minuten und Batteriebetrieb sind noch nicht separat am Board geprueft.
Das alte Board startete im Vergleich nicht; ein konkreter Hardwaredefekt ist
nicht bewiesen. Der gepruefte Build:
[GitHub Actions](https://github.com/OliverDoMS/esp-matter-over-thread/actions/runs/41641382706).
Das lokal heruntergeladene Bodenfeuchtepaket liegt in `dist/soil-usbdiag`.
Die alten lokalen Pakete `dist-usbdiag` und `dist-lowpower` enthalten weiterhin
die bisherigen Versionen. Neue Builds ersetzen diese Diagnosepakete nicht.

Enthalten sind Thread Sleepy End Device, Matter Short Idle Time ICD und eine reale
ADC-Messung. Fuer die Sensorinbetriebnahme ist automatischer Light-sleep deaktiviert.
Der Sensortask wartet zwischen Messungen; ADC-Abtastung blockiert den Matter-Loop
nicht. Es gibt keine WLAN-Verbindung und keine simulierten Messwerte.

| BOOT-Taste nach dem Start | Funktion beim Loslassen |
|---|---|
| Unter 1 Sekunde | Bodenfeuchte jetzt messen |
| 1 bis unter 3 Sekunden | TROCKEN / 0 % kalibrieren |
| 3 bis unter 6 Sekunden | Anlernfenster fuer 5 Minuten oeffnen |
| 6 bis unter 10 Sekunden | NASS / 100 % kalibrieren |
| Ab 10 Sekunden | Werksreset, Kopplung und Thread-Konfiguration loeschen |

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
gemessen werden soll. Updates per Matter OTA sind in dieser Version deaktiviert;
die Partitionstabelle reserviert bereits einen zweiten Anwendungsslot.

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
