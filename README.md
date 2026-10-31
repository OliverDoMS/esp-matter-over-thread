# ESP32-H2 Matter-over-Thread Sensorbasis

Gemeinsame Grundlage fuer batteriebetriebene DoNetIoT-Sensoren. Erstes Hardwareprofil:
ESP32-H2 Super Mini (4 MB Flash), externe geregelte 3,3-V-Versorgung.

## Aktueller Stand

Version 0.1.0 ist ein Entwicklungsstand. Ein erfolgreicher Firmware-Build und die
Pruefung auf dem echten Board stehen noch aus. Der Workflow erzeugt erst nach
erfolgreicher Kompilierung ein Flash-Paket. Im Repository liegen keine fertigen BIN-Dateien.

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

Der vorgesehene Build laeuft ueber GitHub Actions. Im Branch `codex/base-system`
die lokalen Dateien committen und pushen:

```powershell
git add .
git commit -m "Add ESP32-H2 low-power Matter base firmware"
git push -u origin codex/base-system
```

Unter **Actions → Build ESP32-H2 firmware** den erfolgreichen Lauf oeffnen und
das Artefakt **esp32h2-supermini-base** herunterladen und entpacken.
Der Build verwendet ESP-IDF 5.4.1 und den exakten ESP-Matter-Commit aus `versions.env`
(Releasefamilie Matter 1.4.2). Die aufgeloesten Komponenten-Versionen werden im
Flash-Paket als `dependencies.lock` mitgeliefert. Komponenten sind bis zum ersten
erfolgreichen Build noch nicht vollstaendig versioniert.

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
