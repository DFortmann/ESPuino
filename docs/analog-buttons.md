# Additional buttons (6..11) and analog buttons (resistor ladder)

ESPuino supports up to 12 buttons: the six classic ones (`NEXT_BUTTON`, `PREVIOUS_BUTTON`,
`PAUSEPLAY_BUTTON`, `ROTARYENCODER_BUTTON`, `BUTTON_4`, `BUTTON_5`) plus `BUTTON_6` ... `BUTTON_11`.
Every button can be connected to

| Channel     | Meaning                                                         |
|-------------|-----------------------------------------------------------------|
| 0 ... 39    | GPIO of the ESP32 (internal pullup unless `BUTTON_n_ACTIVE_STATE` is 1) |
| 100 ... 115 | Port-expander PCA9555 (`PORT_EXPANDER_ENABLE`)                 |
| 120 ... 127 | Analog button on a resistor ladder (`ANALOG_BUTTONS_ENABLE`)   |
| 99          | Not connected                                                   |

Short and long press of every connected button can be assigned via the web-UI (as well as
"hold button + turn encoder" gestures). Rows of `BUTTON_6` ... `BUTTON_11` are only shown if the
button is connected. Multi-button combinations are still limited to buttons 0 ... 5.

## Resistor ladder

With `ANALOG_BUTTONS_ENABLE` several buttons share one ADC-pin (`ANALOG_BUTTONS_PIN`, has to be an
ADC1-GPIO so it keeps working while WiFi is active). A pullup pulls the pin to 3.3 V; every button
connects it to GND via its own resistor. The resulting voltage identifies the button:

```
3.3 V (switched) ──[ 10 kΩ ]──┬──────────────── ANALOG_BUTTONS_PIN
                              │
                              ├──[ S9 ]─────────────────── GND   => channel 120 (   0 mV)
                              ├──[ S10 ]──[  4.7 kΩ ]───── GND   => channel 121 (1055 mV)
                              └──[ S11 ]──[ 22 kΩ ]─────── GND   => channel 122 (2270 mV)
```

`U = 3.3 V * R / (R + 10 kΩ)`. The expected levels are configured in `settings.h`
(`analogButtonsLevelsMv`, up to 8 entries) together with the allowed deviation
(`analogButtonsToleranceMv`). Keep the levels at least ~600 mV apart and below ~2.6 V,
as the ESP32's ADC saturates at about 3.1 V (which is where "nothing pressed" ends up).

The serial log (loglevel debug) shows the measured voltage whenever an analog button is
detected, and reports voltages that don't match any level — useful to fine-tune the levels.

Limitations compared to digital buttons:
- Only one button of a ladder can be detected at a time (pressing two reports the one with
  the lower resistor). So they can't be used for multi-button combinations.
- Analog buttons can't wake up ESPuino from deepsleep.

### ESPuino complete

Ext. conn1 provides `IO32` (default `ANALOG_BUTTONS_PIN`), `IO5`, `PE109`, `PE112` and
"3.3 V switched". IO32 is also used by the PN5180's IRQ if LPCD is enabled via JP1 — don't
combine both. Example configuration via `platformio-override.ini`:

```ini
[env:complete-11buttons]
extends = env:complete
build_flags = ${env:complete.build_flags}
              -DANALOG_BUTTONS_ENABLE
              -DBUTTON_6=109
              -DBUTTON_7=112
              -DBUTTON_8=5
              -DBUTTON_9=120
              -DBUTTON_10=121
              -DBUTTON_11=122
```
