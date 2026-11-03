# Arbeitsstand Regensensor

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
Docker. Das Artefakt `esp32h2-supermini-base` laesst sich nur mit GitHub-Login laden und
wird nach 30 Tagen geloescht; dann den Lauf neu starten.

## Branches

| Branch | Inhalt |
|---|---|
| `main` | Regensensor, Geraeteidentitaet aus der MAC, Light-sleep an |
| `feature/rain-sensor-diag` | wie `main`, aber Light-sleep aus, nur zum Lesen des USB-Logs. Nicht nach `main` mergen |
| `feature/rain-sensor-lowpower` | Vorstufe von `main`, kann weg |
| `feature/rain-sensor` | erster Stand ohne Light-sleep-Fixes, kann weg |

## Hardware

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

Hinweis: Die erste Regensensor-Firmware ohne Geraeteidentitaet (`6373350`) zeigte dieselben
Symptome. Die Geraeteidentitaet ist also vermutlich nicht die Ursache.

## Naechste Schritte

1. Pruefen, was die LEDs am Board beim Neustart tun.
2. Den Stand `299e0fc` (`codex/lowpower-restore`, ohne Regensensor) flashen. Der Commit-Text
   von `da9fa50` nennt dort ein erfolgreiches Anlernen. Zeigt er ein Log, liegt der Fehler
   vermutlich in den Regensensor-Commits.
3. Sonst das Board selbst pruefen: Log ueber einen UART-Adapter an den UART0-Pins mitlesen.

## Anlerncode

Die MAC des Boards auslesen und daraus den Code berechnen:

```powershell
python -m esptool --chip esp32h2 --port COMxx read_mac
python scripts/pairing_code.py <MAC>
```

Sobald das Geraet sendet, zeigt `scripts/ble_scan.py` den Discriminator an. Er muss mit der
Ausgabe von `pairing_code.py` uebereinstimmen.
