# Drei Bodenfeuchtesensoren an einem ESP32-H2

Ab 0.5.0-soil3. Gleiche Sensoren wie in [BODENFEUCHTE.md](BODENFEUCHTE.md)
(kapazitiv v1.2/v2.0), gleiches Stromsparprofil wie in [ENERGIESPAREN.md](ENERGIESPAREN.md).
Noch nicht gebaut und nicht am Board getestet.

## Verdrahtung

Board stromlos verdrahten. GND aller Sensoren an GND.

| Sensor | AOUT | VCC |
|---|---|---|
| 1 | GPIO4 | GPIO10 |
| 2 | GPIO5 | GPIO11 |
| 3 | GPIO1 | GPIO12 |

Achtung, anders als die Ein-Sensor-Variante: Dort liegt VCC an GPIO1. Hier ist GPIO1
ein Messeingang, VCC von Sensor 1 kommt an GPIO10.

Der H2 kann nur an GPIO1 bis GPIO5 messen. GPIO2 und GPIO3 sind Strapping-Pins und
bleiben frei. Die VCC-Pins sind nur waehrend der Messung ihres Sensors HIGH; es ist
immer hoechstens ein Sensor versorgt. Die Pins sind per `idf.py menuconfig` aenderbar,
der Build prueft Doppelbelegungen und Konflikte mit BOOT und LEDs.

Nicht alle drei Sensoren muessen angeschlossen sein. Ein fehlender Sensor wird als
unbekannt gemeldet.

## Matter und Home Assistant

Das Geraet meldet drei Feuchte-Endpunkte in fester Reihenfolge (Sensor 1, 2, 3) und
zwei Auswahlen:

- **Bodenfeuchte bedienen:** Bereit, Jetzt messen, Sensor 1 trocken, Sensor 1 nass,
  Sensor 2 trocken, Sensor 2 nass, Sensor 3 trocken, Sensor 3 nass.
- **Messintervall:** wie bei der Ein-Sensor-Variante, Standard 60 Minuten.

Home Assistant zeigt die drei Werte voraussichtlich als Luftfeuchte. Sie nach dem
Anlernen umbenennen; welcher Wert zu welchem Sensor gehoert, zeigt das USB-Log beim
Start (`Sensor 1: AOUT GPIO4, VCC GPIO10, endpoint ...`) oder eine Kalibrierung.

## Kalibrierung

Jeder Sensor wird einzeln kalibriert, die anderen bleiben unveraendert.

1. Sensor n in die trockene Referenz bringen und stabilisieren lassen.
2. In **Bodenfeuchte bedienen** `Sensor n trocken` waehlen. Gemessen wird nur dieser
   Sensor; danach springt die Auswahl auf Bereit zurueck.
3. Sensor n in die nasse Referenz bringen, dann `Sensor n nass` waehlen.

Der Trockenwert muss mindestens 100 ADC-Schritte ueber dem Nasswert liegen, sonst
wird die Kalibrierung abgelehnt.

Ab 0.5.1: Bis beide Werte eines Sensors gesetzt sind, meldet er den Platzhalter
**1,23 %**, auch wenn er nicht angeschlossen ist. Home Assistant legt einen Matter-Sensor
nur an, wenn er beim Einlesen einen Wert hat; mit „unbekannt“ wuerde er fehlen. Ein
kalibrierter Sensor meldet bei einem Messfehler weiterhin unbekannt. Wird Home Assistant
genau dann neu gestartet, fehlt dieser Sensor bis zum naechsten Neuladen der
Matter-Integration (Einstellungen → Geraete & Dienste → Matter → Neu laden). Die Werte liegen im NVS-Namespace `soil_cal` (`dry1`/`wet1` bis
`dry3`/`wet3`) und ueberstehen Neustart und Matter-Werksreset, nicht aber `-EraseAll`.

## BOOT-Taste

| BOOT | Funktion |
|---|---|
| Einmal kurz | Alle Sensoren jetzt messen |
| Doppelklick | Anlernfenster fuer 5 Minuten |
| Halten bis schnelles Blinken (ab 10 s), dann loslassen | Matter-Werksreset |

Kalibrieren per BOOT gibt es in dieser Variante nicht, weil die Taste keinen
Sensor auswaehlen kann.

## Anschlusspruefung

Ein offener Analogeingang liefert sonst zufaellige, plausible Werte. Deshalb misst
die Firmware jeden Eingang dreimal:

1. Sensor aus, interner Pulldown an: Ruhewert, bei angeschlossenem Sensor nahe 0.
2. Sensor an: Messwert, muss mindestens etwa 300 mV (370 ADC-Schritte) ueber dem
   Ruhewert liegen.
3. Sensor noch an, Pulldown an: der Sensor muss mindestens 25 % des Messwerts halten.

Schlaegt die Pruefung fehl, meldet der Sensor unbekannt und das Log zeigt
`Sensor n not connected? rest=... raw=... with pulldown=...`. Eine Kalibrierung wird
dann nicht gespeichert. Die Schwellen stammen aus der ESPHome-Multi-Variante am C3 und
sind am H2 noch zu bestaetigen. Weist die Pruefung einen angeschlossenen Sensor ab,
`BASE_SOIL_CONNECTION_CHECK` abschalten.

## Energie

Pro Sensor etwa 60 ms Ruhemessung, 500 ms Einschwingzeit, 320 ms Abtastung und 30 ms
Lastprobe; bei drei Sensoren also etwa 2,7 s pro Messung ohne Light-sleep. Bei 60 Minuten
Intervall faellt das gegenueber dem Thread-Betrieb kaum ins Gewicht; gemessen ist es nicht.

## Abnahme am Board

1. Startlog: `Firmware 0.5.0-soil3`, die drei Sensorzeilen und `identity: Pairing`.
2. Mit dem Code aus `scripts/pairing_code.py` anlernen.
3. Ohne Kalibrierung: alle drei Werte unbekannt.
4. Jeden Sensor einzeln trocken und nass kalibrieren, Werte in HA pruefen.
5. Einen Sensor abziehen: nur dieser meldet unbekannt, die anderen messen weiter.
6. Neustart und Stromunterbrechung: Kalibrierung und Kopplung bleiben.
