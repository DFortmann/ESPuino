#pragma once

#ifdef CONFIG_IDF_TARGET_ESP32S3
	#define MAX_GPIO 47
#else
	#define MAX_GPIO 39
#endif

// Analog buttons (resistor ladder on one ADC-pin) are addressed like GPIOs via channels 120 -> 127:
// 120 => first entry of analogButtonsLevelsMv, 121 => second entry, ...
constexpr uint8_t ANALOG_BUTTONS_FIRST_CHANNEL = 120;
constexpr uint8_t ANALOG_BUTTONS_MAX = 8;
#ifdef ANALOG_BUTTONS_ENABLE
constexpr uint8_t Port_NumAnalogButtons = sizeof(analogButtonsLevelsMv) / sizeof(analogButtonsLevelsMv[0]);
static_assert(Port_NumAnalogButtons <= ANALOG_BUTTONS_MAX, "Too many analog buttons configured in analogButtonsLevelsMv");
#endif

void Port_Init(void);
void Port_Cyclic(void);
bool Port_Read(const uint8_t _channel);
void Port_Write(const uint8_t _channel, const bool _newState, const bool _initGpio);
void Port_Exit(void);
