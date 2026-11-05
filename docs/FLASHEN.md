# Firmware flashen und testen

## Drei Bodensensoren ab 0.5.0-soil3

Eigene Variante mit Produkt-ID `0x8001`; sie ersetzt keine laufende Ein-Sensor-Firmware
per Update. Erstinstallation mit `.\flash.ps1 -Port COMxx -EraseAll`.

Diese Variante verwendet NICHT den gemeinsamen Testcode 34970112332. Anlerncode und
Seriennummer werden aus der MAC des Chips abgeleitet:

```powershell
python -m esptool --chip esp32h2 --port COMxx read_mac
python scripts/pairing_code.py <MAC>
```

Dieselben Werte stehen beim Start im USB-Log (`identity: Pairing: ...`).
Verdrahtung und Kalibrierung: [BODENFEUCHTE-3.md](BODENFEUCHTE-3.md).

## Matter OTA und Geraetenamen ab 0.4.0

Das Profil meldet MoistureSensor / DoNetIoT / ESP32-H2 und aktiviert Matter OTA.
Einmalig per USB als Anwendungsupdate installieren. Danach verwendet man
MoistureSensor.ota mit einem Matter-OTA-Provider. Ablauf und Grenzen:
[OTA.md](OTA.md). Die vorhandene Partitionstabelle bleibt unveraendert.

## Direkte Sensorversorgung mit 0.3.1-soil-gpio

0.3.1 versorgt den Sensor direkt aus GPIO1 (HIGH=an, LOW=aus), mit hoechster
Treiberstaerke. Erst das Anwendungsupdate installieren, dann stromlos
Sensor-VCC von 3V3 auf GPIO1 umstecken. GPIO1 nicht mit 3V3 verbinden.
Die Polaritaet unterscheidet sich von 0.3.0; Details: ENERGIESPAREN.md.

## Konfiguration und Stromsparprofil ab 0.3.0

Das neue Profil ergaenzt zwei ModeSelect-Endpunkte fuer Home Assistant und
aktiviert automatischen Light-sleep. Standardintervall 60 Minuten; Verdrahtung
des optionalen externen Versorgungsschalters und Abnahme: ENERGIESPAREN.md.
Ein Anwendungsupdate von 0.2.x erhaelt zunaechst NVS und Kopplung. Danach pruefen,
ob der Controller die neuen Konfigurations-Endpunkte erkennt. Ein Factory-Flash
setzt Kopplung und Kalibrierung zurueck und ist kein automatischer Update-Schritt.

## Aktuelles Bodenfeuchteprofil 0.2.1-soil-led

Beim Bedienungsupdate von 0.2.0 auf 0.2.1 nur die Anwendung schreiben
(`-ApplicationOnly`), damit Kopplung und Kalibrierung erhalten bleiben.
Die folgende Factory-Empfehlung betrifft den Wechsel vom Testkontakt und
die Erstinstallation. Anlernen wird in 0.2.1 per BOOT-Doppelklick geoeffnet;
die LED-Auswahl und Kalibrierung sind in BODENFEUCHTE.md beschrieben.

Das aktuelle Buildprofil ersetzt den Testkontakt durch einen analogen
Bodenfeuchtesensor. Verdrahtung, Kalibrierung und die geaenderten Tastenzeiten
stehen in [BODENFEUCHTE.md](BODENFEUCHTE.md). Automatischer Light-sleep bleibt fuer
diesen ersten Sensor-Funktionstest deaktiviert. Die unten beschriebenen
Low-power-Tests gehoeren zu den vorherigen Basisprofilen.

Weil sich der Endpunkttyp von Kontakt zu Feuchte aendert, das bisherige Geraet
in den verbundenen Controllern entfernen, das neue Factory-Paket installieren
und neu anlernen. Ein Factory-Flash setzt Kopplungs- und Kalibrierdaten zurueck.
Alte lokale Pakete `dist-usbdiag`/`dist-lowpower` enthalten keinen Bodenfeuchtecode.

## Voraussetzungen

- ESP32-H2 Super Mini mit 4 MB Flash und USB-Datenkabel.
- Erfolgreich gebautes, entpacktes Artefakt der Variante, z. B. `esp32h2-soil3` oder `esp32h2-rain`.
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

## Anlerncode je Geraet (Regensensor-Variante)

Diese Variante verwendet NICHT den gemeinsamen Testcode. Anlerncode, Discriminator und
Seriennummer werden aus der MAC-Adresse des Chips abgeleitet. Die MAC zeigt esptool beim
Flashen (`BASE MAC`, oder `python -m esptool read_mac`). Den Code berechnet:

```powershell
python scripts/pairing_code.py 74:4d:bd:65:e1:4d
```

Ausgabe: Seriennummer `DNIOT-<MAC>`, Setup-PIN, Discriminator und der 11-stellige manuelle
Anlerncode fuer Home Assistant. Dasselbe steht beim Start im USB-Log (`identity: Pairing: ...`).
Die Werte sind nicht geheim; das Anlernfenster oeffnet nur der BOOT-Taster. Die Matter-
Testzertifikate (Vendor `0xFFF1`) bleiben, es ist weiterhin kein zertifiziertes Produkt.

## Anlernen

Version 0.1.3-gpiowake setzt `CONFIG_BASE_DIAGNOSTIC_NO_SLEEP=n`. Beim Start
erscheinen `Firmware 0.1.3-gpiowake; reset reason=...` und
`LOW POWER: automatic Light-sleep ENABLED; USB host may inhibit sleep`.
Bei erkanntem USB-Host soll der SDK-Schutz automatischen Light-sleep sperren.
In frueheren Tests traten trotzdem USB-Abbrueche auf; ein stabiler COM-Port ist
deshalb kein Abnahmekriterium fuer den Betrieb ohne USB-Host.
Die vorherige Version 0.1.1-usbdiag sperrte automatischen Light-sleep vollstaendig.
Ein verschwindender COM-Port allein beweist keinen Neustart. Fuer die Diagnose
den aktuellen Port mit `python -m serial.tools.list_ports` ermitteln und die
Konsole mit `python -m serial.tools.miniterm COM4 115200` oeffnen (Port anpassen).
Miniterm mit Strg+] schliessen, bevor erneut geflasht wird.

Falls Miniterm bei USB-Abbruechen beendet wird, im Repository stattdessen
`python scripts/monitor.py` starten. Dieser Monitor erkennt genau ein natives
Espressif-USB-Geraet automatisch, verbindet nach Abbruechen auch bei geaendertem
COM-Port erneut und sichert die Ausgabe in `usb-monitor.log`. Bei mehreren
Geraeten den Port mit `--port COM4` festlegen. Mit Strg+C vor dem Flashen beenden.

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

### Wechsel auf 0.1.3-gpiowake

0.1.2-lowpower reagierte am Testboard nach zwei Minuten Ruhe nicht mehr auf BOOT.
Die Hypothese fuer 0.1.3-gpiowake betrifft den Wechsel der Weckquelle im
[button-4.2.3-Treiber](https://github.com/espressif/esp-iot-solution/blob/9aee93cdca202e88f05ac0c4399480cbf8eccaf2/components/button/button_gpio.c):
Mit `CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP=y` setzt die Initialisierung
EXT1, `exit_power_save` entfernt EXT1, und `enter_power_save` aktiviert nur GPIO.
Das neue Profil setzt die Option auf `n` und verwendet durchgehend GPIO-Wakeup.
Light-sleep und der stromsparende Tastenmodus bleiben aktiviert. Der Verbrauch
kann gegenueber ausgeschalteter Peripherie hoeher liegen; Funktion und Strom
sind am Board noch zu pruefen. Die button-Abhaengigkeit ist auf 4.2.3 festgelegt.

1. Monitor schliessen. BOOT halten, RST kurz druecken, BOOT loslassen.
2. Im Repository `./dist-lowpower/flash.ps1 -Port COM21 -ApplicationOnly -Baud 115200`
   ausfuehren; Port anpassen. Nur die Anwendung aktualisieren, ohne `-EraseAll`.
   Partitionstabelle und Datenmodell sind unveraendert; Kopplungsdaten bleiben erhalten.
3. Falls moeglich die oben genannte Version und Low-power-Meldung im Startlog pruefen.
4. USB am PC trennen und das Board ueber seinen USB-Anschluss aus einem stabilen
   USB-Netzteil versorgen. Batterie und externen Wandler fuer diesen Vergleich abtrennen.
   Ein PC-USB-Host kann den Schlafmodus blockieren; eine Powerbank kann sich bei
   geringem Verbrauch abschalten und ist daher kein verlaesslicher Testaufbau.
5. Zwei Minuten warten, BOOT kurz druecken und Kontaktwechsel in Home Assistant
   pruefen. Nach weiteren fuenf Minuten wiederholen. Keine erneute Kopplung starten.
6. RST und danach eine Stromunterbrechung einzeln pruefen, jeweils den Wiederbeitritt
   abwarten und den Testkontakt umschalten. Die Wartezeit bis zum Wiederbeitritt notieren.

Diese Tests bestaetigen die Funktion, aber noch nicht das tatsaechliche Schlafen
oder einen bestimmten Verbrauch. Anschliessend den mittleren Strom mit geeignetem
Messgeraet ohne USB-Host bestimmen; dabei Board-LEDs und Spannungsregler mit erfassen.

### Allgemeine Pruefungen

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

Bis 0.3.1 wurde fest `ota_0` bei `0x20000` verwendet. Das neue Skript prueft bei
OTA-Paketen die vorhandene Partitionstabelle und verlangt eine noch geloeschte
OTA-Bootauswahl, bevor es diesen festen Slot beschreibt. Nach einem OTA-Update
werden USB-Anwendungsupdates abgelehnt; dann den Matter-OTA-Weg verwenden.
`ApplicationOnly` ist nicht fuer fremde Firmware oder andere Partitionstabellen
vorgesehen. Einen Werksreset bei inkompatiblen Datenmodell-Aenderungen separat einplanen.
