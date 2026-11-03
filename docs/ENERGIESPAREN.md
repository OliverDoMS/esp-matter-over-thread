# Stromsparbetrieb und Home-Assistant-Einstellungen ab 0.3.1

## Bedienung auf der Geraeteseite

Zwei standardisierte Matter-ModeSelect-Endpunkte erscheinen in Home Assistant
unter Konfiguration (am Testboard bestaetigt, keine zusaetzliche Integration):

- **Bodenfeuchte bedienen:** Bereit, Jetzt messen, Trocken kalibrieren,
  Nass kalibrieren. Den Sensor zuerst in die jeweilige Referenz bringen,
  stabilisieren lassen und dann die passende Aktion waehlen. Nach Bearbeitung
  springt die Auswahl auf Bereit zurueck. Fehler werden im Geraeteprotokoll
  gemeldet; eine abgelehnte Kalibrierung ersetzt keine gueltige Referenz.
- **Messintervall:** 1, 5, 10, 15, 30, 60, 120, 360, 720 oder 1440 Minuten.
  Standard ist 60 Minuten. Eine Aenderung wird im NVS gespeichert und startet
  einen neuen Countdown. Eine zusaetzliche Sofortmessung startet ebenfalls
  einen neuen Countdown, aendert aber nicht das konfigurierte Intervall.

Der einzelne kurze BOOT-Klick misst einmal; der Doppelklick oeffnet das
Anlernfenster. Die LED-Auswahl fuer lokale Kalibrierung bleibt erhalten.
Die Konfigurations-Endpunkte wurden beim Test nach dem Anwendungsupdate ohne
Factory-Reset erkannt. Falls der
Controller die neuen Endpunkte nicht erkennt, den Interview-/Erkennungsablauf
des Controllers pruefen. Vor einem eventuell noetigen Werksreset erst die
bisherige Kopplung und Kalibrierung sichern.

## Schlaf und Erreichbarkeit

Das Geraet bleibt Thread Sleepy End Device und Matter SIT ICD. Automatischer
Light-sleep erlaubt Schlaf zwischen Sensor- und Protokollaufgaben. Eine Messung
alle 60 Minuten bedeutet daher nicht, dass das Funkprotokoll nur einmal pro
Stunde aufwacht. Fernbefehle bleiben moeglich; der aktuelle langsame Thread-
Pollintervall betraegt 5 Sekunden, sodass eine Bedienaktion verzoegert ankommen
kann. Es wird kein stundenlanger Deep-sleep erzwungen, der Fernbefehle verhindert.

Waehrend der Sensor-Einschwingzeit und ADC-Abtastung verhindert eine temporaere
PM-Sperre Light-sleep. Anschliessend werden Versorgungsausgang und ADC
abgeschaltet und die PM-Sperre freigegeben. Der Task wartet blockierend bis
zur naechsten Messung oder einem Bedienbefehl. GPIO-Wakeup fuer BOOT und die
bewaehrte Einstellung ohne Peripherie-Power-down bleiben erhalten.

Ein angeschlossener PC-USB-Host kann Light-sleep sperren. Verbrauch daher ohne
PC messen. Light-sleep, Thread-Polling, Regler und Board-LED-Elektronik gemeinsam
erfassen; aus der Softwarekonfiguration keine Batterielaufzeit ableiten.

## Direkte geschaltete GPIO-Versorgung ab 0.3.1

Auf Wunsch wird die direkte GPIO-Versorgung aus dem bisherigen Sensoraufbau
uebernommen. Sensor-VCC kommt an GPIO1, der nur waehrend einer Messung HIGH
ist. Dazwischen ist GPIO1 LOW. Die Firmware setzt Treiberstaerke
`GPIO_DRIVE_CAP_3` und behaelt den Ausgangszustand im Light-sleep bei.

**Zuerst 0.3.1 installieren, dann stromlos umstecken:** Version 0.3.0 benutzt
GPIO1 noch aktiv LOW fuer einen externen P-MOSFET. Sensor-VCC wird von 3V3 auf
GPIO1 umgesteckt; GPIO1 darf nicht mit der 3V3-Schiene verbunden werden.

| Verbindung | Ziel |
|---|---|
| GPIO1 | Sensor VCC |
| Sensor AOUT | GPIO4 |
| Sensor GND | Board GND |

Nach dem Umstecken die VCC-Spannung waehrend der Messung unter Sensorlast
pruefen. Der Ausgang ist keine ideale 3,3-V-Spannungsquelle; die Versorgung
kann gegenueber der 3V3-Schiene abfallen. Die beiden Kalibrierreferenzen mit
der tatsaechlichen GPIO-Versorgung erneut pruefen. Vorherige Erfahrungen mit
dem Aufbau ersetzen noch nicht diese Messung am konkreten H2.

Der Steuerausgang ist vor dem Start des Messtasks auf aus gesetzt und bleibt
auch im Light-sleep stabil. Pro Messung wird eingeschaltet, standardmaessig
500 ms gewartet, abgetastet und wieder ausgeschaltet. `BASE_SOIL_SETTLE_MS`
kann fuer den konkreten Sensor angepasst werden.

Fuer einen Funktionstest kann VCC weiterhin direkt an 3V3 bleiben, solange
GPIO1 dabei frei bleibt. Dann ist der Sensor dauerhaft versorgt und spart
selbst noch keinen Strom. Ein externer Schalter ist im Standardprofil nicht
noetig; dafuer waere `BASE_SOIL_DIRECT_GPIO_POWER` zu deaktivieren und seine
Polaritaet passend einzustellen.

## Abnahme

Pruefstand 03.11.2026, Firmware 0.3.1-soil-gpio, H2 mit USB-Seriennummer-Endung
DA:89: Build und Flash-Verifikation erfolgreich. Sensor-VCC wurde vom Benutzer
auf GPIO1 umgesteckt. Der erste ADC-Rohwert war 2752; Matter-Wiederbeitritt und
Wertuebertragung wurden im Log bestaetigt. Beide Konfigurationsauswahlen sind
laut Benutzer auf der Home-Assistant-Geraeteseite sichtbar.

Die dort ausgeloeste Trocken-Kalibrierung und das gewaehlte Intervall von
1 Minute wurden nach einer Stromunterbrechung im Startlog als `dry=2752` und
`interval=1 minutes` geladen. Der Benutzer bestaetigte anschliessend, dass
nach Einbringen in feuchte Erde der Wert innerhalb von etwa 90 Sekunden
automatisch anstieg, ohne Taste oder HA-Messbefehl. Die USB-Konsole liefert
nach der Startphase im Stromsparprofil nicht durchgehend Daten; eine stille
Konsole allein darf daher nicht als Stillstand des Sensors gewertet werden.
Lokales Protokoll: `output/h2-recovery/soil-gpio-da89-20261103.log` (nicht in Git).

Der Benutzer bestaetigte auch die Nasskalibrierung ueber die HA-Geraeteseite
(anschliessend etwa 100 %) und die Rueckstellung des Intervalls auf 60 Minuten.
Der Test ohne PC-USB-Host konnte mangels eines passenden USB-Netzteils noch
nicht durchgefuehrt werden. Daher sind BOOT-Wakeup nach Ruhe ohne Host und
das tatsaechliche Schlafverhalten noch nicht als Hardwaretest bestaetigt.

Offen bleiben die elektrische Spannungs-/Strommessung und ein separater
Erreichbarkeitstest ohne PC-USB-Host. Softwareseitig aktivierter Light-sleep
und funktionierende Messungen sind noch kein Nachweis einer Batterielaufzeit.

1. Version, erhaltene Kalibrierung und Matter-Wiederbeitritt im Log pruefen.
2. Beide Konfigurationsauswahlen auf der Home-Assistant-Geraeteseite pruefen.
3. Sofortmessung und Trocken-/Nasskalibrierung von dort aus ausloesen.
4. Messintervall testweise auf 1 Minute setzen und automatische Messung abwarten.
5. Neustart: gewaehltes Intervall und Kalibrierung muessen erhalten bleiben.
6. Auf 60 Minuten zurueckstellen. GPIO-Versorgung pruefen: Sensor-VCC zwischen
   Messungen LOW, waehrend der Messung HIGH; Spannung unter Last messen.
7. Ohne PC-USB-Host nach mehreren Minuten Ruhe BOOT und Fernbedienung pruefen;
   danach den realen mittleren Strom messen.

Referenzen: [Home Assistant ModeSelect](https://github.com/home-assistant/core/blob/dev/homeassistant/components/matter/select.py),
[Matter-ModeSelect im verwendeten SDK](https://github.com/espressif/esp-matter/blob/d3aa1b96b9357b208f15cc12d1864dc124414c6b/components/esp_matter/data_model/esp_matter_cluster.h),
[ESP32-H2 Power Management](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32h2/api-reference/system/power_management.html).
