# ESP32-H2 Matter-over-Thread Sensorbasis

Gemeinsame Grundlage fuer batteriebetriebene DoNetIoT-Sensoren. Erstes Hardwareprofil:
ESP32-H2 Super Mini (4 MB Flash), externe geregelte 3,3-V-Versorgung.

## Aktueller Stand

Version 0.1.3-gpiowake aktiviert automatischen Light-sleep mit GPIO-Wakeup und
laesst die Peripherie dabei versorgt. Die vorherige Version 0.1.2-lowpower reagierte
am Testboard nach zwei Minuten Ruhe nicht mehr auf den Testkontakt. Mit der
Diagnoseversion 0.1.1-usbdiag wurden am Testboard Matter-Kopplung mit Home Assistant,
Kontaktwechsel per BOOT sowie Wiederanlauf nach RST und Stromunterbrechung bestaetigt.
Sleep-/Wake-Verhalten und Stromverbrauch der Low-power-Version sind noch am Board
zu pruefen. Der Workflow erzeugt nach erfolgreichem Build ein Flash-Paket.
Im Repository liegen keine fertigen BIN-Dateien.

Enthalten sind Thread Sleepy End Device, Matter Short Idle Time ICD, automatischer
Light-sleep und ein virtueller Kontakt zum Testen der Verbindung. Keine WLAN-Verbindung,
keine Messwertsimulation fuer Temperatur oder Batterie, kein periodischer Anwendungstask.
Matter und Thread duerfen fuer ihre Protokollaufgaben selbststaendig aufwachen.

| BOOT-Taste nach dem Start | Funktion beim Loslassen |
|---|---|
| Unter 1 Sekunde | Aufwecken und virtuellen Testkontakt umschalten |
| 1 bis unter 3 Sekunden | Nur Aufwecken |
| 3 bis unter 10 Sekunden | Anlernfenster fuer 5 Minuten oeffnen |
| Ab 10 Sekunden | Werksreset, Kopplung und Thread-Konfiguration loeschen |

Der Testkontakt startet nach einem Neustart als geschlossen. Er ist kein echter
Hall-/Fenstersensor und wird nicht bei jedem Tastendruck in Flash gespeichert.
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
| `main/sensors.cpp` | Sensor-Endpunkte und Publizieren der Werte im Matter-Kontext |
| `main/Kconfig.projbuild` | Boardoptionen und Testkontakt |
| `sdkconfig.defaults` | Netzwerk-, ICD- und Schlafkonfiguration |

Neue Varianten ergaenzen passende Endpunkte und Treiber in der Sensorschicht.
I2C-Pins und Sensorversorgung werden erst mit der konkreten Erweiterung festgelegt.
Batteriespannung, Prozentanzeige, reale Hall-Eingaenge, Temperatur, Feuchte und
Helligkeit sind noch nicht implementiert. Ein Spannungsteiler darf nicht einfach
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

## Variante Regensensor

Auf dem Branch `rain-sensor`: analoge Regenplatine als Matter-Geraetetyp *Rain Sensor*
(Boolean State, `true` = Regen). Weiterhin nur Thread, Short Idle Time ICD und Light Sleep.

| Regenplatine | ESP32-H2 | Hinweis |
|---|---|---|
| AO | GPIO1 (ADC1) | `BASE_RAIN_ADC_GPIO` |
| VCC | GPIO10 | `BASE_RAIN_POWER_GPIO`, nur waehrend der Messung an |
| GND | GND | |

- Messung alle 60 s (`BASE_RAIN_INTERVAL_S`): Sensor an, 100 ms einschwingen, 8 Werte mit
  Ausreisserfilter, Sensor aus. Kurzer BOOT-Druck loest sofort eine Messung aus.
- Die Platine liefert nass eine NIEDRIGERE Spannung. Regen beginnt unter
  `BASE_RAIN_WET_BELOW_MV` (1500) und endet ueber `BASE_RAIN_DRY_ABOVE_MV` (2000).
  Startwerte: trocken und nass im USB-Log (`Rain sensor ... mV`) ablesen und per
  `idf.py menuconfig` anpassen.
- Der virtuelle Testkontakt ist in dieser Variante aus (`BASE_TEST_CONTACT=n`).
- Die Anlernung laeuft ueber Matter (Code/QR), es steht kein Thread-Schluessel in der Firmware.
- Nicht gebaut und nicht auf Hardware getestet; `tests/rain_logic.cpp` prueft die Auswertung.
