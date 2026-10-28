# ESPuino – persönlicher Fork mit 11 Tastern

Übergabe-Notizen für neue Chats. Stand: 24.10.2026. Kommunikation mit David auf Deutsch.

## Ziel und Hardware

- Nachbau von [ESPuino](https://github.com/biologist79/ESPuino) (RFID-Musikbox auf ESP32) mit der Platine
  **ESPuino complete** (ESP32-WROVER, Port-Expander PCA9555 an I²C 0x20, HAL 6, `src/settings-complete.h`).
  Doku der Platine: https://forum.espuino.de/t/espuino-complete/3817
- Vorhandenes Gehäuse mit Lautsprecher, **Drehencoder mit eingebautem Drucktaster** und **11 weiteren Drucktastern**.
- Die Complete hat ab Werk nur 5 Button-Buchsen und den Encoder-Taster. Freie Pins am **Ext. Conn1**:
  IO0, IO5, IO32, PE109, PE112, „3,3 V switched“, GND. IO0 wird bewusst nicht benutzt (Strapping-Pin:
  gedrückt beim Booten → Flash-Modus).
- Entscheidung: **Widerstandsleiter an IO32** für 3 Taster (Variante A). Ein zweiter PCA9555 am externen
  I²C-Anschluss (Variante C) wurde verworfen. Hinweis an David: I²C ist ein Bus, ein zweiter Expander hätte den
  Anschluss nicht blockiert; er hat sich trotzdem bewusst für A entschieden.

### Akku und Lötbrücken (Stand 28.10.2026)

- Akku: **Eremit IFR32700 LiFePO4, 3,2 V, 6 Ah, 19,2 Wh (ERE-G03)**, mit eingebauter Schutzschaltung.
- Lötbrücken für LiFePO4 (von David geprüft bzw. korrigiert):
  - **JP5 offen** (Ladespannung 3,6 V). War ab Werk auf „LiPo“ gesetzt → Ladung auf 4,2 V, Zelle bis ca. 3,75 V
    überladen, dann hat die Schutzschaltung im Pack getrennt (Lade-LED blinkte schnell). Akku danach unauffällig
    (Ruhespannung 3,44 V, nicht warm).
  - **JP6 auf 1+2** (LFP-Spannungswächter, Abschaltung ca. 2,75 V). War auf 2+3 (LiPo, ca. 3,15 V) → Box ging
    anfangs ohne USB gar nicht an.
- Erste Ladung nach der Korrektur: LED aus bei 3,65 V, also richtig.
- Lade-LED (TP5000): dauerhaft an = lädt, aus = voll oder kein USB, schnell blinkend = kein Akku erkannt bzw.
  Schutzschaltung im Pack hat getrennt.
- Noch offen: Ruhespannung nach einigen Tagen ohne Benutzung nachmessen (Selbstentladung, ca. 3,35–3,45 V erwartet).
- Einbau: Akku festlegen und gegen den Lautsprecherkorb isolieren; Abstand zur WLAN-Antenne des ESP32 (ca. 1,5 cm)
  und zur RFID-Antenne halten.

## Git

- Fork: https://github.com/DFortmann/ESPuino, lokal in `/Users/david/Documents/EspUino`
- Remotes: `origin` = Fork, `upstream` = biologist79/ESPuino
- Arbeitsbranch: **`feature/more-buttons`** (abgezweigt von `upstream/dev`, dort wird entwickelt; `master` hinkt hinterher)
- Commits:
  - `a74b577` – Funktion: bis zu 12 Taster + Widerstandsleiter (allgemein gehalten, wäre PR-fähig)
  - `5d33059` – persönliche `platformio-override.ini` (per `git add -f`, steht eigentlich in `.gitignore`)
- **Kein Pull Request ans Original gewünscht.** Committen und Pushen in den eigenen Branch ist erlaubt.
- Updates vom Original übernehmen: `git fetch upstream` und dann `git merge upstream/dev`.
  Konflikte am ehesten in `src/Button.cpp`, `src/Web.cpp`, `html/management.html`.

## Tasterbelegung (Umgebung `complete-11buttons`)

| Index | Makro | Anschluss | Kanal |
|---|---|---|---|
| 0 | `NEXT_BUTTON` | Button-Buchse | PE102 |
| 1 | `PREVIOUS_BUTTON` | Button-Buchse | PE100 |
| 2 | `PAUSEPLAY_BUTTON` | Button-Buchse | PE101 |
| 3 | `ROTARYENCODER_BUTTON` | Encoder-Taster | PE105 |
| 4 | `BUTTON_4` | Button-Buchse | PE103 |
| 5 | `BUTTON_5` | Button-Buchse | PE104 |
| 6 | `BUTTON_6` | Ext. Conn1 | 109 |
| 7 | `BUTTON_7` | Ext. Conn1 | 112 |
| 8 | `BUTTON_8` | Ext. Conn1 IO5 | 5 |
| 9 | `BUTTON_9` | Leiter an IO32, 0 Ω | 120 |
| 10 | `BUTTON_10` | Leiter an IO32, 4,7 kΩ | 121 |
| 11 | `BUTTON_11` | Leiter an IO32, 22 kΩ | 122 |

Encoder: CLK = GPIO34, DT = GPIO39. Die Pins 6–11 und `ANALOG_BUTTONS_ENABLE` werden per `build_flags` in
`platformio-override.ini` gesetzt (`settings-complete.h` hat `#ifndef`-Defaults 99).

### Schaltung der Widerstandsleiter

```
3,3 V switched ──[ 10 kΩ ]──┬───────────────────── IO32
                            ├──[ Btn9 ]──────────────── GND   (0 V)
                            ├──[ Btn10 ]──[ 4,7 kΩ ]─── GND   (ca. 1055 mV)
                            └──[ Btn11 ]──[ 22 kΩ ]──── GND   (ca. 2270 mV)
```

- Lötbrücke **JP1 darf nicht gesetzt sein** (sonst liegt der PN5180-IRQ für LPCD auf IO32).
- LPCD (Aufwecken per Karte) ist deshalb gesperrt: `Rfid_LpcdAvailable` in `src/Rfid.h` ist false, wenn
  `ANALOG_BUTTONS_PIN == RFID_IRQ`; die Firmware ignoriert dann die NVS-Einstellung, die Web-UI blendet den Schalter aus.
  Grund: Im Tiefschlaf ist „3,3 V switched“ aus, IO32 läge auf 0 V und würde die Box sofort wieder wecken.
- Spannungsstufen/Toleranz: `analogButtonsLevelsMv` / `analogButtonsToleranceMv` (300 mV) in `src/settings.h`.
- Serielles Log (Debug) meldet „Analog buttons: channel 12x pressed (… mV)“ bzw. unpassende Spannungen → zum Kalibrieren.
- Einschränkungen: Leiter-Taster können nicht aus dem Deepsleep wecken und nicht gleichzeitig gedrückt werden.
  Auch IO5 (Btn8) kann nicht wecken. Wecken können nur Taster am Port-Expander (Interrupt auf GPIO36 = `WAKEUP_BUTTON`).

## Was an der Firmware geändert wurde

- `src/Button.h`: `BUTTON_COUNT = 12`, `gButtons[BUTTON_COUNT + 1]` (+ Dummy), neue Funktionen
  `Button_IsEnabled()`, `Button_GetActionDefault()`.
- `src/Button.cpp`: tabellengesteuert (`buttonHwConfig`, `buttonCmdConfig`, `rotaryCmdConfig` je 12 Einträge)
  statt sechs ausgeschriebener `#if`-Blöcke. `#ifndef`-Fallbacks für `BUTTON_6..11`, `_ACTIVE_STATE`, `_SHORT`,
  `_LONG`, `_ROTARY_CW/CCW`, damit alte `settings-override.h` weiter bauen. Multi-Button-Kombinationen bleiben bei 0–5.
  `Port_Cyclic()` wird auch bei `ANALOG_BUTTONS_ENABLE` aufgerufen.
- `src/Port.h` / `src/Port.cpp`: virtuelle Kanäle **120–127** (`ANALOG_BUTTONS_FIRST_CHANNEL`, max. 8) in `Port_Read()`;
  `Port_AnalogButtonsHandler()` liest `analogReadMilliVolts()` (100 Hz über den Button-Timer), übernimmt einen Wert
  erst nach zwei gleichen Messungen.
- `src/Web.cpp`: Laden/Speichern/Defaults der Tasteraktionen als Schleifen über `BUTTON_COUNT`; sendet zusätzlich
  `buttons.enabled[]`. Speichern ist pro Schlüssel abgesichert (ältere Web-UI schickt weniger Felder).
- `html/management.html`: Zeilen für Btn6–11 in „Taster-Zuordnungen“ und „Drehgesten“ (`<tr data-button="n">`),
  ausgeblendet, wenn `enabled[n]` false. JS-Konstante `BUTTON_COUNT = 12` muss zu `Button.h` passen.
  Übersetzungen `button6`–`button11` in `html/locales/{de,en,fr}.json`.
- `src/Led.cpp`: eigene `extern`-Deklaration von `gButtons[7]` entfernt (kommt jetzt aus `Button.h`).
- `src/settings.h`, `src/settings-complete.h`, `docs/analog-buttons.md`, `changelog.md`.

## Bauen

- Projektlokale Umgebung: `venv/` (in `.gitignore`) mit **PlatformIO 6.1.19** und **clang-format 16**.
  Neu anlegen falls nötig: `/opt/homebrew/bin/python3.12 -m venv venv` und dann
  `venv/bin/pip install "platformio<6.2" "clang-format==16.0.6"`.
- **PlatformIO 6.2.0 funktioniert nicht** (bringt SCons 4.11 mit → `No module named 'SCons.Tool.FortranCommon'`).
  Python muss ≥ 3.10 sein; die System-Python 3.9 reicht nicht (Python 3.12 per Homebrew installiert).
- Bauen: `venv/bin/pio run -e complete-11buttons` (ist auch `default_envs`)
- Flashen: `venv/bin/pio run -e complete-11buttons -t upload`, Log: `venv/bin/pio device monitor`
- Gegenprobe, dass Standard-Hardware weiter baut: `venv/bin/pio run -e complete -e lolin_d32_pro`
- Formatierung (CI des Originals prüft clang-format 16): `venv/bin/clang-format -i src/<Datei>.cpp`
- Alternativ für David: PlatformIO-Erweiterung in VS Code.

## Stand und nächste Schritte

Erledigt und geprüft:
- `complete-11buttons`, `complete` und `lolin_d32_pro` bauen ohne Fehler (zwei IRAM-Warnungen in `Button.cpp`/`Port.cpp`
  gab es schon vorher).
- Webinterface in einer lokalen Kopie im Browser getestet: Laden, Ausblenden nicht angeschlossener Taster,
  Speichern (63 Werte: 12 × short/long/rotCw/rotCcw + 15 Kombinationen).

Noch offen:
1. Widerstandsleiter und Taster verdrahten (Belegung oben).
2. Firmware flashen, im seriellen Log prüfen, ob die drei Leiter-Taster richtig erkannt werden; ggf.
   `analogButtonsLevelsMv` anpassen.
3. Aktionen im Webinterface zuordnen (Einstellungen → Taster-Zuordnungen). Bestimmte Ordner/Hörbücher per
   „Virtuelle RFID-Karte 1–12“ (IDs `900000000001`…`900000000012`; 11 und 12 sind eigene Ergänzung) auf Taster legen. Den Schlafmodus-Taster auf einen der
   5 Haupt-Buttons legen (die können wecken).
4. Nichts davon ist bisher auf echter Hardware getestet.
