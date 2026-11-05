# Arbeitsstand

Uebergabe zum Weiterarbeiten auf einem anderen Rechner.

## Einrichten

```powershell
git clone https://github.com/OliverDoMS/esp-matter-over-thread.git
cd esp-matter-over-thread
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install "esptool>=4.8,<5" bleak pyserial
```

Vor dem Flashen `.venv\Scripts` an den Anfang von `PATH` setzen, denn `flash.ps1` ruft
`python -m esptool` auf. Gebaut wird ueber GitHub Actions, lokal gibt es keinen Build ohne
Docker. Das Artefakt laesst sich nur mit GitHub-Login laden und
wird nach 30 Tagen geloescht; dann den Lauf neu starten.
Artefakte heissen jetzt `esp32h2-<variante>`, siehe [variants/README.md](../variants/README.md).

## Branches

Seit der Konsolidierung (05.11.2026) gibt es nur noch `main`. Regensensor und
Drei-Sensor-Bodenfeuchte sind Varianten in `variants/`, der Diagnose-Build ist das
Overlay `variants/diagnostic.sdkconfig`. Alle frueheren Branches sind in `main`
enthalten oder ueberholt und koennen geloescht werden:

| Branch | Verbleib |
|---|---|
| `feature/soil-moisture-3`, `codex/h2-soil-moisture` | in `main` gemergt, Variante `soil3` |
| `feature/rain-sensor-diag` | ersetzt durch Diagnose-Overlay |
| `feature/rain-sensor`, `feature/rain-sensor-lowpower` | Vorstufen, in `main` enthalten |
| `codex/base-system`, `codex/usb-console-sleep`, `codex/lowpower-restore` | in `main` enthalten |

## Hardware Regensensor

ESP32-H2 Super Mini, Regenplatine: AO an GPIO1, VCC an GPIO10, GND an GND. Fuer Tests nur
per USB versorgen.

## Was belegt ist

- Der Firmware-Build auf `main` ist gruen.
- Flashen klappt, esptool verifiziert das Image.
- `tests/rain_logic.cpp` und `tests/test_pairing_code.py` laufen in der CI. Der Anlerncode
  stimmt mit dem Matter-Testvektor ueberein (PIN 20202021, Discriminator 3840 ergibt 34970112332).

## Offenes Problem

Das Geraet laesst sich noch nicht anlernen:

- `scripts/monitor.py` empfaengt keine einzige Textzeile, auch nicht mit dem Diagnose-Build
  und auch nicht nach einem Neustart bei laufendem Monitor. Das Board meldet sich dabei
  sichtbar neu am USB an.
- `scripts/ble_scan.py` findet keinen Dienst 0xFFF6, auch nicht nach BOOT 3 bis 9 s. Der
  Scanner sieht andere Geraete mit ihren Diensten.

Die Ursache ist unbekannt. Es ist auch nicht belegt, dass die Firmware nach dem Start laeuft
oder dass sie denselben Anlerncode ableitet wie `scripts/pairing_code.py`.

Eingrenzung: Das Drei-Sensor-Bodenfeuchte-Board DA:0B startete am 05.11.2026 mit
derselben Geraeteidentitaet, schrieb ins USB-Log und warb per BLE (siehe Geraeteliste).
Gegenueber `main` vor der Konsolidierung unterscheidet sich der Code im Wesentlichen nur
durch den Regensensor. Belegt ist die Ursache damit nicht; auch das Board kommt in Frage.

Hinweis: Die erste Regensensor-Firmware ohne Geraeteidentitaet (`6373350`) zeigte dieselben
Symptome. Die Geraeteidentitaet ist also vermutlich nicht die Ursache.

## Naechste Schritte

1. Pruefen, was die LEDs am Board beim Neustart tun.
2. Den Stand `299e0fc` (`codex/lowpower-restore`, ohne Regensensor) flashen. Der Commit-Text
   von `da9fa50` nennt dort ein erfolgreiches Anlernen. Zeigt er ein Log, liegt der Fehler
   vermutlich in den Regensensor-Commits.
3. Sonst das Board selbst pruefen: Log ueber einen UART-Adapter an den UART0-Pins mitlesen.

## Geraete

Alle Boards mit Firmware, Kopplungscode und Status: [geraete/README.md](../geraete/README.md).
Die Drei-Sensor-Bodenfeuchte liegt auf `feature/soil-moisture-3`.

## Anlerncode

Die MAC des Boards auslesen und daraus den Code berechnen:

```powershell
python -m esptool --chip esp32h2 --port COMxx read_mac
python scripts/pairing_code.py <BASE MAC>   # die Zeile "BASE MAC" von read_mac, nicht die 8-Byte-MAC
```

Sobald das Geraet sendet, zeigt `scripts/ble_scan.py` den Discriminator an. Er muss mit der
Ausgabe von `pairing_code.py` uebereinstimmen. Das gilt erst ab dem MAC-Fix der
Konsolidierung; aeltere Firmware rechnet mit der falschen MAC (siehe Geraeteliste).
