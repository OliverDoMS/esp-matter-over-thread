# Geraeteliste (Testphase)

Bewusst oeffentlich, solange alle Geraete Testcharakter haben. Wer einen Code kennt,
braucht zum Anlernen trotzdem das geoeffnete Anlernfenster am Geraet. Vor einer
Weitergabe von Geraeten diese Liste aus dem oeffentlichen Repository entfernen.

Stand 05.11.2026. Ein Abschnitt pro Board, neue Boards unten anfuegen.

## Bekannter Fehler im Anlerncode

Die Firmware (`main/device_identity.cpp`) liest die MAC am H2 in der 8-Byte-Form und
verwendet davon nur die ersten 6 Bytes, z. B. `74:4d:bd:ff:fe:65` statt `74:4d:bd:65:da:0b`.
Dadurch erhalten Boards mit demselben 4. MAC-Byte denselben Code und dieselbe
Seriennummer. `scripts/pairing_code.py` rechnet mit der echten MAC und liefert deshalb
fuer die aktuelle Firmware einen ANDEREN Code als das Geraet.

Bis zur Korrektur gilt der Code, den das Geraet beim Start ins USB-Log schreibt
(`identity: Pairing: passcode=... discriminator=...`). Daraus den manuellen Code berechnen:

```powershell
python -c "import sys; sys.path.insert(0,'scripts'); import pairing_code as p; print(p.manual_code(PIN, DISCRIMINATOR))"
```

Nach der Korrektur aendern sich Code und Seriennummer. Eine bestehende Kopplung bleibt
bei einem Anwendungsupdate erhalten; der neue Code gilt nur fuer kuenftiges Anlernen.

## Boards

### DA:0B - Bodenfeuchte, drei Sensoren

| Feld | Wert |
|---|---|
| Base-MAC | 74:4d:bd:65:da:0b |
| Firmware | 0.5.0-soil3, Branch `feature/soil-moisture-3`, Commit b18519c |
| Geflasht | 05.11.2026, `-EraseAll`, COM22 |
| Kopplungscode laut Firmware | **05160754560** (PIN 89393559, Discriminator 937) |
| Seriennummer laut Firmware | DNIOT-744DBDFFFE65 |
| Produkt | MoistureSensor3, 0xFFF1/0x8001 |
| Matter-Endpunkte | 1-3 Bodenfeuchte Sensor 1-3, 4 Bedienen, 5 Messintervall |
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
