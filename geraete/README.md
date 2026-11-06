# Geraeteliste (Testphase)

Bewusst oeffentlich, solange alle Geraete Testcharakter haben. Wer einen Code kennt,
braucht zum Anlernen trotzdem das geoeffnete Anlernfenster am Geraet. Vor einer
Weitergabe von Geraeten diese Liste aus dem oeffentlichen Repository entfernen.

Stand 05.11.2026. Ein Abschnitt pro Board, neue Boards unten anfuegen.

## Anlerncode: behobener Fehler

Firmware bis einschliesslich 0.5.1-soil3 und 0.1.3-gpiowake las am H2 die 8-Byte-EUI-64
statt der Base-MAC und nutzte davon nur die ersten 6 Bytes (`74:4d:bd:ff:fe:65` statt
`74:4d:bd:65:da:0b`). Ab dem Konsolidierungsstand liest `main/device_identity.cpp` die
Base-MAC; Firmware und `scripts/pairing_code.py` liefern wieder denselben Code.
Nachgerechnet: `pairing_code.py 74:4d:bd:ff:fe:65` ergibt genau den Code, den DA:0B
mit der alten Firmware meldet.

Nach dem Update aendern sich Code und Seriennummer. Eine bestehende Kopplung bleibt
bei einem Anwendungsupdate erhalten; der neue Code gilt nur fuer kuenftiges Anlernen.
Das Geraet schreibt beim Start Base-MAC und Code ins USB-Log
(`identity: Pairing: base MAC=... passcode=... discriminator=...`).

## Boards

### DA:0B - Bodenfeuchte, drei Sensoren

| Feld | Wert |
|---|---|
| Base-MAC | 74:4d:bd:65:da:0b |
| Firmware | 0.5.0-soil3, Branch `feature/soil-moisture-3`, Commit b18519c |
| Geflasht | 05.11.2026, `-EraseAll`, COM22 |
| Kopplungscode, Firmware 0.5.x | **05160754560** (PIN 89393559, Discriminator 937) |
| Seriennummer, Firmware 0.5.x | DNIOT-744DBDFFFE65 |
| Kopplungscode nach MAC-Fix | 33942859100 (PIN 96836100, Discriminator 3827), berechnet, am Geraet noch nicht bestaetigt |
| Seriennummer nach MAC-Fix | DNIOT-744DBD65DA0B |
| Produkt | MoistureSensor3, 0xFFF1/0x8001 |
| Matter-Endpunkte | 1-3 Bodenfeuchte Sensor 1-3, 4 Bedienen, 5 Messintervall |
| Hardware-Hinweis | 06.11.2026: ADC-Eingang GPIO5 defekt (rest=501, raw=478, pulldown=437 bei 2,3 V aussen). Sensor 2 AOUT auf GPIO3 umgeloetet, ab Firmware 0.6.1-soil3 |
| Status | Startet, Anlernfenster und BLE-Werbung im Log bestaetigt. Anlernen offen. Beim Start waren laut Anschlusspruefung keine Sensoren angeschlossen. |
| Kalibrierung | keine |

### E1:4D - Regensensor

| Feld | Wert |
|---|---|
| Base-MAC | 74:4d:bd:65:e1:4d |
| Firmware | Diagnose-Build, Branch `feature/rain-sensor-diag`, Commit e663da2 (Light-sleep aus) |
| Kopplungscode laut Firmware | vermutlich ebenfalls **05160754560** (gleiches 4. MAC-Byte, siehe oben); am Geraet nicht bestaetigt |
| Status | Nie angelernt. Das stumme USB-Log kam vermutlich vom Mitleseskript, das den Chip in den Download-Modus schaltete. Neu pruefen. |

### DA:89 - Bodenfeuchte, ein Sensor

| Feld | Wert |
|---|---|
| Base-MAC | unbekannt, USB-Seriennummer endet auf DA:89 |
| Firmware | 0.3.1-soil-gpio, Branch `codex/h2-soil-moisture` (0.4.0 gebaut, Installation nicht dokumentiert) |
| Kopplungscode | Matter-Testcode **34970112332** (PIN 20202021, Discriminator 3840) |
| Status | In Home Assistant gekoppelt und in Betrieb, siehe `docs/ENERGIESPAREN.md` auf dem Branch |
| Kalibrierung | trocken 2752 (GPIO-Versorgung); nass nachkalibriert, Wert nicht dokumentiert |

### Altes Board

Startete beim Test am 03.11.2026 nicht; MAC nicht dokumentiert. Laut Branch-Doku ist
kein Hardwaredefekt nachgewiesen.
