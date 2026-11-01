# Erstes Hardwareprofil

## Festgelegt

- Abgebildetes ESP32-H2 Super Mini mit BOOT an GPIO9.
- Geregelte 3,3 V als geplante externe Versorgung.
- TPS63802-Modul als Wandlerkandidat; genaue Bestueckung und MODE-Verdrahtung
  des gekauften Moduls sind noch zu pruefen.
- S09 ist ohne identifizierten Wandler-IC noch keine freigegebene Alternative.

## Vorlaeufige externe Verdrahtung

| Anschluss | Ziel |
|---|---|
| 2 x AA Plus | Wandler IN+ |
| 2 x AA Minus | Wandler IN- |
| Wandler OUT+, gemessene 3,3 V | Board 3V3 |
| Wandler OUT- | Board GND |

Vor dem Aufbau den Ausgang ohne ESP messen und die externe Einspeisung anhand
des konkreten Board-Reglers auf Rueckspeisung pruefen. USB und externe Versorgung
bis zur geklaerten Entkopplung nicht gleichzeitig verwenden. B+/B- bleiben bei
dieser Variante frei. Deren Beschriftung allein belegt weder Ladeparameter noch
einen vorhandenen Ueber-/Unterspannungsschutz.

Der TPS63802 benoetigt laut TI mehr als 1,8 V zum Starten. Eine einzelne AA oder
NiMH-Zelle ist fuer einen zuverlaessigen Kaltstart mit diesem Wandler ungeeignet.
Bei Batteriewechseln nach Reset muss der Wandler erneut starten koennen.

## Stromverbrauch

Die Firmware setzt die dokumentierte blaue LED an GPIO13 auf LOW und sendet
einmal Schwarz an die WS2812 an GPIO8. Anschliessend wird der RMT-Treiber
freigegeben. Ladeanzeige, Regler und WS2812 koennen trotzdem Ruhestrom ziehen.
Bei abweichender Boardrevision `BASE_DISABLE_SUPERMINI_LEDS` deaktivieren und
die Beschaltung klaeren. GPIO8, GPIO9 und GPIO25 sind ausserdem Strapping-Pins.

Zuerst den Wandler allein messen, danach das Board ohne USB, anschliessend das
gekoppelte Gesamtsystem einschliesslich Funkimpulsen ueber mehrere Minuten.
Die Messung muss Stromspitzen aufloesen koennen, ohne die Versorgung einbrechen
zu lassen. Sleep-Strom und zeitlicher Mittelwert sind verschiedene Groessen.

Die Sensorschicht darf spaeter eine abschaltbare Versorgung erhalten. Ein
Hall-Schalter als Weckquelle braucht dagegen eine dauerhaft aktive, sparsame
Versorgung. GPIO- und I2C-Leitungen duerfen abgeschaltete Sensoren nicht rueckspeisen.

Quellen:

- [TPS63802-Datenblatt](https://www.ti.com/lit/ds/symlink/tps63802.pdf)
- [ESP32-H2-Datenblatt](https://www.espressif.com/sites/default/files/documentation/esp32-h2_datasheet_en.pdf)
- [ESP-Matter ICD-Referenz](https://github.com/espressif/esp-matter/tree/d3aa1b96b9357b208f15cc12d1864dc124414c6b/examples/icd_app)
