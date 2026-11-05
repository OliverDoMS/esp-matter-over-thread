# Firmware-Varianten

Eine Codebasis, ein Firmware-Image pro Sensorvariante. Jede Variante liegt in einem
eigenen Ordner:

| Datei | Inhalt |
|---|---|
| `<variante>/variant.env` | Produktname, Version, Versionsnummer, mitgelieferte Doku |
| `<variante>/sdkconfig.defaults` | Sensorwahl, GPIOs, Endpunktanzahl, Produkt-ID |
| `diagnostic.sdkconfig` | Overlay ohne Light-sleep, nur zum Lesen des USB-Logs |

Gemeinsame Einstellungen stehen in `sdkconfig.defaults` im Projektordner.
`scripts/package_firmware.py` prueft beim Paketieren, dass jede Zeile des
Varianten-Overlays im fertigen Build wirklich gilt.

## Vorhandene Varianten

| Variante | Produkt | Produkt-ID | Doku |
|---|---|---|---|
| `rain` | RainSensor | 0xFFF1/0x8000 | README, Abschnitt Regensensor |
| `soil3` | MoistureSensor3 | 0xFFF1/0x8001 | [BODENFEUCHTE-3.md](../docs/BODENFEUCHTE-3.md) |

Achtung: Die fruehen Ein-Sensor-Bodenfeuchte-Builds (`MoistureSensor`, Branch
`codex/h2-soil-moisture`) nutzten ebenfalls 0x8000. Ein OTA-Image der Variante
`rain` darf nie einem solchen Geraet angeboten werden.

## Bauen

```bash
bash scripts/build.sh              # alle Varianten
bash scripts/build.sh soil3        # nur eine
DNIOT_DIAGNOSTIC=1 bash scripts/build.sh rain   # Diagnose-Image
```

Ergebnis: `dist/<variante>[-diag]/`. In GitHub Actions baut jede Variante als eigener
Job, Artefakt `esp32h2-<variante>`. Diagnose-Images ueber "Run workflow" mit dem
Haken "diagnostic".

## Neue Version einer Variante

`PROJECT_VER` und `PROJECT_VER_NUMBER` in `variant.env` erhoehen. Die Nummer muss bei
jeder Veroeffentlichung steigen, sonst ignoriert Matter-OTA das Image.

## Neue Variante

1. Ordner mit `variant.env` und `sdkconfig.defaults` anlegen, eigene Produkt-ID waehlen.
2. Sensor in `main/Kconfig.projbuild` in die Auswahl `BASE_SENSOR_VARIANT` aufnehmen.
3. Den Variantennamen in `.github/workflows/firmware.yml` unter `matrix.variant`
   eintragen. Ein Test schlaegt fehl, wenn das vergessen wird.
