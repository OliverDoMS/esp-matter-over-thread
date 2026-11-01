# Firmware flashen und testen

## Voraussetzungen

- ESP32-H2 Super Mini mit 4 MB Flash und USB-Datenkabel.
- Erfolgreich gebautes, entpacktes Artefakt `esp32h2-supermini-base`.
- Python und esptool 4.x auf dem Windows-Rechner.
- Fuer Matter-over-Thread ein erreichbarer Thread Border Router und ein
  Matter-Controller, beispielsweise Home Assistant mit Matter-Integration.

Solange noch kein erfolgreicher Build vorliegt, ist der Quellcode nicht als
flashbare Firmware verifiziert. Das Paketierungsskript erzeugt keine Platzhalter-BINs.

## Erstinstallation

1. Batterie und externen Wandler trennen; das Board nur per USB versorgen.
2. Im Geraetemanager den COM-Port feststellen. Ein Monitorprogramm muss geschlossen sein.
3. PowerShell im entpackten Firmwarepaket oeffnen und ausfuehren:

```powershell
python -m pip install "esptool>=4.8,<5"
.\flash.ps1 -Port COM4 -EraseAll
```

`COM4` ist ein Beispiel und muss zum aktuellen Board passen. Das Skript prueft
Dateigroesse und SHA-256 und laesst esptool das geschriebene Image verifizieren.
`-EraseAll` loescht den gesamten Flash einschliesslich vorhandener Firmware und
Kopplungsdaten. Auch das zusammengefuehrte Factory-Image ueberschreibt den NVS-Bereich.
Die Erstinstallation setzt deshalb eine bereits vorhandene Kopplung zurueck.

Falls PowerShell ein heruntergeladenes Skript blockiert:

```powershell
Unblock-File .\flash.ps1
```

Bei Verbindungsproblemen BOOT gedrueckt halten, RST kurz druecken, BOOT loslassen
und erneut flashen. Falls noetig `-Baud 115200` ergaenzen. Nach dem Flashen RST
kurz druecken. Der COM-Port kann sich beim Wechsel zwischen Bootloader und Anwendung aendern.

## Anlernen

Bei Verbindung mit einem USB-Host bleibt die Firmware wach, damit die native
USB-Konsole erreichbar bleibt. Automatischer Light-sleep ist dann gesperrt;
fuer Strommessungen den USB-Host trennen und separat versorgen.
Ein verschwindender COM-Port allein beweist keinen Neustart. Fuer die Diagnose
den aktuellen Port mit `python -m serial.tools.list_ports` ermitteln und die
Konsole mit `python -m serial.tools.miniterm COM4 115200` oeffnen (Port anpassen).
Miniterm mit Strg+] schliessen, bevor erneut geflasht wird.

Das ungekoppelte Geraet oeffnet nach dem Start automatisch ein Anlernfenster.
In Home Assistant oder einem anderen passenden Matter-Controller ein neues
Matter-Geraet hinzufuegen. Der unveraenderte Entwicklungs-Build verwendet die
Testparameter des ESP-Matter-SDK:

- Manueller Anlerncode: **34970112332**.
- Setup-PIN: **20202021**, Discriminator: **3840**.
- Test-Vendor-ID `0xFFF1`, Test-Product-ID `0x8000`.

Die Firmware ist ein privater Entwicklungs-Build mit Testattestierung und
gemeinsamem Testcode, kein zertifiziertes Serienprodukt. Ein Controller kann
eine Bestaetigung fuer ein nicht zertifiziertes Geraet verlangen oder es ablehnen.
Massgeblich sind im Zweifel die beim Start ausgegebenen Onboarding-Daten.

BOOT 3 bis unter 10 Sekunden halten und loslassen, um das Fenster erneut zu oeffnen.
Ein schon gekoppeltes Geraet fuer eine weitere Fabric bevorzugt ueber die
Freigabefunktion seines bestehenden Controllers teilen.

## Funktionstest

1. Der Controller zeigt einen Kontaktkanal an.
2. BOOT kurz druecken: der virtuelle Kontakt wechselt offen/geschlossen.
3. RST druecken: die Kopplung bleibt bestehen, der Testkontakt startet geschlossen.
4. USB trennen und wieder verbinden: das Geraet soll zum Thread-Netz zurueckkehren.
5. BOOT mindestens 10 Sekunden halten und loslassen: Werksreset; erneut anlernen.

Noch keine Batterielaufzeit angeben: Light-sleep und der mittlere Verbrauch
muessen nach erfolgreichem Funktionstest am echten Board gemessen werden.

## Spaetere Anwendung aktualisieren

Nur wenn die Erstinstallation bereits laeuft und dieselbe Partitionstabelle
verwendet wird, kann die Anwendung ohne Factory-Image geschrieben werden:

```powershell
.\flash.ps1 -Port COM4 -ApplicationOnly
```

Diese Version verwendet fest `ota_0` bei `0x20000`; Matter OTA ist deaktiviert.
`ApplicationOnly` ist nicht fuer fremde Firmware, andere Partitionstabellen oder
ein spaeteres System mit aktivem `ota_1` vorgesehen. Einen Werksreset bei
inkompatiblen Datenmodell-Aenderungen separat einplanen.
