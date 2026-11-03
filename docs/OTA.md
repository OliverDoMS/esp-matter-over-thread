# Matter-OTA fuer MoistureSensor

Ab 0.4.0-soil-ota ist der Standard-Matter-OTA-Requestor aktiviert.
Produkt: MoistureSensor, Hersteller: DoNetIoT, Hardware: ESP32-H2.
Die IDs bleiben 0xFFF1/0x8000. 0.4.0 hat SoftwareVersion 10; jeder spaetere
OTA-Build muss PROJECT_VER_NUMBER erhoehen, auch wenn nur Namen geaendert werden.

## Einmalige Installation

0.3.1 hat noch keinen OTA-Empfaenger. Der erste Wechsel auf 0.4.0 erfolgt deshalb
per USB mit dem neuen Paket:

```powershell
.\flash.ps1 -Port COM27 -ApplicationOnly -Baud 115200
```

Der Port ist ein Beispiel. Vorher den seriellen Monitor schliessen.
Das Skript vergleicht zuerst die Partitionstabelle und prueft, dass die
OTA-Bootauswahl noch geloescht ist. Nur dann schreibt es die Anwendung nach
ota_0; NVS, Kalibrierung und Kopplung bleiben dabei erhalten. Nach einem bereits
erfolgten OTA-Update verweigert dieser USB-Modus vorsichtshalber das Schreiben.
Kein Factory-Image verwenden, um diese Sperre zu umgehen: Es ueberschreibt NVS.

Die bestehende 4-MB-Tabelle bleibt unveraendert: ota_0 bei 0x20000 und ota_1 bei
0x200000, je 0x1E0000 Bytes. Das SDK schreibt OTA-Updates in den inaktiven Slot
und schaltet die Bootauswahl erst nach erfolgreicher Image-Pruefung um.
Im Startlog stehen der aktive Slot und seine Adresse.

Nach Installation in Home Assistant bei Bedarf "Geraet neu abfragen" ausfuehren.

## Updatepaket

Der SDK-Build erzeugt esp_matter_base-ota.bin mit Matter-TLV-Header, VID/PID,
numerischer Softwareversion und SHA-256-Digest. Das Flashpaket enthaelt es als
**MoistureSensor.ota** mit Dateigroesse und SHA-256 im manifest.json.
Nur diese Datei an einen Matter-OTA-Provider geben; factory.bin und die reine
esp_matter_base.bin sind keine Matter-OTA-Pakete.

Die Paketierung prueft die beiden Slots, die maximale Anwendungsgroesse und
die Uebereinstimmung von OTA-Payload und gebauter Anwendung.
Der Matter-Header ersetzt keine Firmware-Signatur; Secure Boot und signierte
Updates sind in diesem Entwicklungsprofil nicht eingerichtet.

## Home Assistant

Private Firmware erscheint nicht automatisch im oeffentlichen Matter-Updatekatalog.
Der Matter Server muss das lokale .ota-Paket als Updatequelle importieren.
Beim aktuellen Matter.js-Server erfordert dies **enable_test_net_dcl** und
einen konfigurierten OTA-Provider-Ordner. Die Serverversion und der passende
Importpfad muessen vor dem ersten Funkupdate an der konkreten Installation
geprueft werden; nicht mit dem allgemeinen www-Ordner gleichsetzen.

Fuer einen selbst betriebenen Matter.js-Server:

```sh
matter-server --enable-test-net-dcl --ota-provider-dir /pfad/zu/ota-dateien
```

Eine neuere MoistureSensor.ota vor dem Serverstart in diesen Ordner legen.
Der Server importiert die Datei und entfernt sie danach aus dem Importordner.
Anschliessend nach Updates suchen und die Aktualisierung in Home Assistant
starten. Fuer einen Funk-Nachweis muss der Zielbuild eine hoehere numerische
Version haben als die bereits per USB installierte Firmware.

Quellen:

- [Matter Server: eigene OTA-Dateien](https://github.com/matter-js/matterjs-server#importing-custom-ota-firmware-files)
- [Matter-Server-App: Konfiguration](https://github.com/home-assistant/apps/blob/master/matter_server/DOCS.md)
- [SDK: OTA-Requestor](https://github.com/espressif/esp-matter/blob/d3aa1b96b9357b208f15cc12d1864dc124414c6b/components/esp_matter/esp_matter_ota.cpp)

## Stromsparen und Abnahme

Waehrend Download und Anwenden sperrt die Firmware automatischen Light-sleep.
Nach Downloadabschluss, Abbruch oder Fehler wird die Sperre freigegeben.
Der SDK-Empfaenger startet nach erfolgreichem Anwenden automatisch neu.
Messintervall, Sensorversorgung und Kalibrierdaten werden durch OTA nicht geaendert.

Noch am Board zu pruefen: Empfang ueber Thread, Wechsel ota_0 -> ota_1,
erneute Erreichbarkeit, unveraenderte Kalibrierung/Kopplung und ein zweiter
OTA-Wechsel zurueck auf ota_0. Build und Hosttests ersetzen diese Abnahme nicht.
Das Profil aktiviert keinen automatischen Boot-Rollback nach einem spaeteren
Anwendungsabsturz; das setzt einen passenden Bootloader und eine gesonderte
Gesundheitspruefung voraus.
