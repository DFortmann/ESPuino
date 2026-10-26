#include <Arduino.h>
#include "settings.h"

#include "Button.h"

#include "Cmd.h"
#include "Log.h"
#include "Port.h"
#include "System.h"

#include <atomic>

bool gButtonInitComplete = false;

// settings-override.h, when present, replaces settings.h wholesale -- so an override written before buttons 6..11
// existed defines none of their macros. Default them to "not connected" (99) / CMD_NOTHING so those configs keep
// building and simply don't use the additional buttons.
#ifndef BUTTON_6
	#define BUTTON_6 99
#endif
#ifndef BUTTON_7
	#define BUTTON_7 99
#endif
#ifndef BUTTON_8
	#define BUTTON_8 99
#endif
#ifndef BUTTON_9
	#define BUTTON_9 99
#endif
#ifndef BUTTON_10
	#define BUTTON_10 99
#endif
#ifndef BUTTON_11
	#define BUTTON_11 99
#endif
#ifndef BUTTON_6_ACTIVE_STATE
	#define BUTTON_6_ACTIVE_STATE 0
#endif
#ifndef BUTTON_7_ACTIVE_STATE
	#define BUTTON_7_ACTIVE_STATE 0
#endif
#ifndef BUTTON_8_ACTIVE_STATE
	#define BUTTON_8_ACTIVE_STATE 0
#endif
#ifndef BUTTON_9_ACTIVE_STATE
	#define BUTTON_9_ACTIVE_STATE 0
#endif
#ifndef BUTTON_10_ACTIVE_STATE
	#define BUTTON_10_ACTIVE_STATE 0
#endif
#ifndef BUTTON_11_ACTIVE_STATE
	#define BUTTON_11_ACTIVE_STATE 0
#endif
#ifndef BUTTON_6_SHORT
	#define BUTTON_6_SHORT CMD_NOTHING
#endif
#ifndef BUTTON_6_LONG
	#define BUTTON_6_LONG CMD_NOTHING
#endif
#ifndef BUTTON_7_SHORT
	#define BUTTON_7_SHORT CMD_NOTHING
#endif
#ifndef BUTTON_7_LONG
	#define BUTTON_7_LONG CMD_NOTHING
#endif
#ifndef BUTTON_8_SHORT
	#define BUTTON_8_SHORT CMD_NOTHING
#endif
#ifndef BUTTON_8_LONG
	#define BUTTON_8_LONG CMD_NOTHING
#endif
#ifndef BUTTON_9_SHORT
	#define BUTTON_9_SHORT CMD_NOTHING
#endif
#ifndef BUTTON_9_LONG
	#define BUTTON_9_LONG CMD_NOTHING
#endif
#ifndef BUTTON_10_SHORT
	#define BUTTON_10_SHORT CMD_NOTHING
#endif
#ifndef BUTTON_10_LONG
	#define BUTTON_10_LONG CMD_NOTHING
#endif
#ifndef BUTTON_11_SHORT
	#define BUTTON_11_SHORT CMD_NOTHING
#endif
#ifndef BUTTON_11_LONG
	#define BUTTON_11_LONG CMD_NOTHING
#endif

// GPIO / port-channel and active state of every button, indexed like gButtons.
// 0 -> MAX_GPIO: GPIOs
// 100 -> 115: Port-expander
// 120 -> 127: Analog buttons (resistor ladder)
// Everything else (e.g. 99) disables the button.
static constexpr struct {
	uint8_t channel;
	bool activeState;
} buttonHwConfig[BUTTON_COUNT] = {
	{		 NEXT_BUTTON,  BUTTON_0_ACTIVE_STATE},
	{	 PREVIOUS_BUTTON,  BUTTON_1_ACTIVE_STATE},
	{	 PAUSEPLAY_BUTTON,  BUTTON_2_ACTIVE_STATE},
	{ROTARYENCODER_BUTTON,  BUTTON_3_ACTIVE_STATE},
	{			BUTTON_4,  BUTTON_4_ACTIVE_STATE},
	{			BUTTON_5,  BUTTON_5_ACTIVE_STATE},
	{			BUTTON_6,  BUTTON_6_ACTIVE_STATE},
	{			BUTTON_7,  BUTTON_7_ACTIVE_STATE},
	{			BUTTON_8,  BUTTON_8_ACTIVE_STATE},
	{			BUTTON_9,  BUTTON_9_ACTIVE_STATE},
	{		   BUTTON_10, BUTTON_10_ACTIVE_STATE},
	{		   BUTTON_11, BUTTON_11_ACTIVE_STATE},
};

// Button command configuration: {prefsKeyShort, prefsKeyLong, defaultShort, defaultLong}
static const struct {
	const char *prefsKeyShort;
	const char *prefsKeyLong;
	uint8_t defaultShort;
	uint8_t defaultLong;
} buttonCmdConfig[BUTTON_COUNT] = {
	{ "btnShort0",  "btnLong0",  BUTTON_0_SHORT,  BUTTON_0_LONG},
	{ "btnShort1",  "btnLong1",  BUTTON_1_SHORT,  BUTTON_1_LONG},
	{ "btnShort2",  "btnLong2",  BUTTON_2_SHORT,  BUTTON_2_LONG},
	{ "btnShort3",  "btnLong3",  BUTTON_3_SHORT,  BUTTON_3_LONG},
	{ "btnShort4",  "btnLong4",  BUTTON_4_SHORT,  BUTTON_4_LONG},
	{ "btnShort5",  "btnLong5",  BUTTON_5_SHORT,  BUTTON_5_LONG},
	{ "btnShort6",  "btnLong6",  BUTTON_6_SHORT,  BUTTON_6_LONG},
	{ "btnShort7",  "btnLong7",  BUTTON_7_SHORT,  BUTTON_7_LONG},
	{ "btnShort8",  "btnLong8",  BUTTON_8_SHORT,  BUTTON_8_LONG},
	{ "btnShort9",  "btnLong9",  BUTTON_9_SHORT,  BUTTON_9_LONG},
	{"btnShort10", "btnLong10", BUTTON_10_SHORT, BUTTON_10_LONG},
	{"btnShort11", "btnLong11", BUTTON_11_SHORT, BUTTON_11_LONG},
};

static constexpr bool Button_IsValidChannel(uint8_t channel) {
	return (channel <= MAX_GPIO)
#ifdef PORT_EXPANDER_ENABLE
		|| (channel >= 100 && channel <= 115)
#endif
#ifdef ANALOG_BUTTONS_ENABLE
		|| (channel >= ANALOG_BUTTONS_FIRST_CHANNEL && channel < ANALOG_BUTTONS_FIRST_CHANNEL + Port_NumAnalogButtons)
#endif
		;
}

bool Button_IsEnabled(uint8_t buttonIndex) {
	return (buttonIndex < BUTTON_COUNT) && Button_IsValidChannel(buttonHwConfig[buttonIndex].channel);
}

uint8_t Button_GetActionDefault(uint8_t buttonIndex, bool longPress) {
	if (buttonIndex >= BUTTON_COUNT) {
		return CMD_NOTHING;
	}
	return longPress ? buttonCmdConfig[buttonIndex].defaultLong : buttonCmdConfig[buttonIndex].defaultShort;
}

// Allocate gButtons in PSRAM if available
EXT_RAM_BSS_ATTR t_button gButtons[BUTTON_COUNT + 1]; // + dummy-button
uint8_t gShutdownButton = 99; // Helper used for Neopixel: stores button-number of shutdown-button
uint16_t gLongPressTime = 0;

#ifdef PORT_EXPANDER_ENABLE
extern volatile bool Port_AllowReadFromPortExpander;
#endif

static std::atomic<SemaphoreHandle_t> Button_TimerSemaphore;

hw_timer_t *Button_Timer = NULL;
static void IRAM_ATTR onTimer();
static void Button_DoButtonActions(void);

void Button_Init() {
#if (WAKEUP_BUTTON >= 0 && WAKEUP_BUTTON <= MAX_GPIO)
	if (ESP_ERR_INVALID_ARG == esp_sleep_enable_ext0_wakeup((gpio_num_t) WAKEUP_BUTTON, 0)) {
		Log_Printf(LOGLEVEL_ERROR, wrongWakeUpGpio, WAKEUP_BUTTON);
	}
#endif

#ifdef NEOPIXEL_ENABLE // Try to find button that is used for shutdown via longpress-action (only necessary for Neopixel)
	for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
		if (Button_IsEnabled(i) && (buttonCmdConfig[i].defaultLong == CMD_SLEEPMODE)) {
			gShutdownButton = i;
			break;
		}
	}
#endif

	// Activate internal pullups for all enabled buttons connected to GPIOs
	for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
		if (Button_IsEnabled(i) && buttonHwConfig[i].channel <= MAX_GPIO) {
			pinMode(buttonHwConfig[i].channel, buttonHwConfig[i].activeState ? INPUT : INPUT_PULLUP);
		}
	}

	// Create 1000Hz-HW-Timer (currently only used for buttons)
	Button_TimerSemaphore = xSemaphoreCreateBinary();
#if (defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3))
	Button_Timer = timerBegin(1000000); // Prescaler: CPU-clock in MHz
	timerAttachInterrupt(Button_Timer, &onTimer);
	timerAlarm(Button_Timer, 10000, true, 0); // 100 Hz
#else
	Button_Timer = timerBegin(0, 240, true); // Prescaler: CPU-clock in MHz
	timerAttachInterrupt(Button_Timer, &onTimer, true);
	timerAlarmWrite(Button_Timer, 10000, true); // 100 Hz
	timerAlarmEnable(Button_Timer);
#endif
}

// Read current state of all enabled buttons
static void Button_ReadAllStates(void) {
	for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
		if (Button_IsEnabled(i)) {
			gButtons[i].currentState = Port_Read(buttonHwConfig[i].channel) ^ buttonHwConfig[i].activeState;
		}
	}
}

// Update press/release state for a single button with debouncing
static void Button_UpdateState(uint8_t i, unsigned long currentTimestamp) {
	t_button &btn = gButtons[i];
	bool const stateChanged = btn.currentState != btn.lastState;
	bool const debounceElapsed = currentTimestamp - btn.lastPressedTimestamp > buttonDebounceInterval;

	if (stateChanged && debounceElapsed) {
		bool const buttonPressed = !btn.currentState;
		if (buttonPressed) {
			btn.isPressed = true;
			btn.lastPressedTimestamp = currentTimestamp;
			// Tells which button sits on which connector when wiring a case full of them
			if (i < BUTTON_COUNT) {
				Log_Printf(LOGLEVEL_DEBUG, "Button %u pressed (channel %u)", i, buttonHwConfig[i].channel);
			}
			if (!btn.firstPressedTimestamp) {
				btn.firstPressedTimestamp = currentTimestamp;
			}
		} else {
			btn.isReleased = true;
			btn.lastReleasedTimestamp = currentTimestamp;
			btn.firstPressedTimestamp = 0;
		}
	}
	btn.lastState = btn.currentState;
}

// If timer-semaphore is set, read buttons (unless controls are locked)
void Button_Cyclic() {
	if (xSemaphoreTake(Button_TimerSemaphore, 0) != pdTRUE) {
		return;
	}

	unsigned long currentTimestamp = millis();

#if defined(PORT_EXPANDER_ENABLE) || defined(ANALOG_BUTTONS_ENABLE)
	Port_Cyclic();
#endif

	if (System_AreControlsLocked()) {
		return;
	}

	Button_ReadAllStates();

	for (uint8_t i = 0; i < sizeof(gButtons) / sizeof(gButtons[0]); i++) {
		Button_UpdateState(i, currentTimestamp);
	}

	gButtonInitComplete = true;
	Button_DoButtonActions();
}

// Multi-button combination configuration: {btn1, btn2, prefsKey, defaultCmd}
static const struct {
	uint8_t btn1;
	uint8_t btn2;
	const char *prefsKey;
	uint8_t defaultCmd;
} multiButtonCombos[] = {
	{0, 1, "btnMulti01", BUTTON_MULTI_01},
	{0, 2, "btnMulti02", BUTTON_MULTI_02},
	{0, 3, "btnMulti03", BUTTON_MULTI_03},
	{0, 4, "btnMulti04", BUTTON_MULTI_04},
	{0, 5, "btnMulti05", BUTTON_MULTI_05},
	{1, 2, "btnMulti12", BUTTON_MULTI_12},
	{1, 3, "btnMulti13", BUTTON_MULTI_13},
	{1, 4, "btnMulti14", BUTTON_MULTI_14},
	{1, 5, "btnMulti15", BUTTON_MULTI_15},
	{2, 3, "btnMulti23", BUTTON_MULTI_23},
	{2, 4, "btnMulti24", BUTTON_MULTI_24},
	{2, 5, "btnMulti25", BUTTON_MULTI_25},
	{3, 4, "btnMulti34", BUTTON_MULTI_34},
	{3, 5, "btnMulti35", BUTTON_MULTI_35},
	{4, 5, "btnMulti45", BUTTON_MULTI_45},
};

// Check for multi-button combinations and execute corresponding action
static bool Button_HandleMultiButtonPress(void) {
	for (const auto &combo : multiButtonCombos) {
		if (gButtons[combo.btn1].usedAsModifier || gButtons[combo.btn2].usedAsModifier) {
			continue; // held as a rotary modifier, not as half of a combo
		}
		if (gButtons[combo.btn1].isPressed && gButtons[combo.btn2].isPressed) {
			gButtons[combo.btn1].isPressed = false;
			gButtons[combo.btn2].isPressed = false;
			Cmd_Action(gPrefsSettings.getUChar(combo.prefsKey, combo.defaultCmd));
			return true;
		}
	}
	return false;
}

// Handle a single button's short/long press action
static void Button_HandleSinglePress(uint8_t i, unsigned long currentTimestamp) {
	// The button was used to modify a rotary gesture, so it must not also fire its own action. Its short
	// press would otherwise fire on release, and its long press at intervalToLongPress while still held --
	// which for a button whose long action is CMD_SLEEPMODE means the box falls asleep mid-gesture.
	if (gButtons[i].usedAsModifier) {
		if (gButtons[i].lastReleasedTimestamp > gButtons[i].lastPressedTimestamp) {
			gButtons[i].isPressed = false;
			gButtons[i].usedAsModifier = false;
		}
		return;
	}

	uint8_t Cmd_Short = gPrefsSettings.getUChar(buttonCmdConfig[i].prefsKeyShort, buttonCmdConfig[i].defaultShort);
	uint8_t Cmd_Long = gPrefsSettings.getUChar(buttonCmdConfig[i].prefsKeyLong, buttonCmdConfig[i].defaultLong);
	unsigned long const pressDuration = currentTimestamp - gButtons[i].lastPressedTimestamp;
	bool const wasReleased = gButtons[i].lastReleasedTimestamp > gButtons[i].lastPressedTimestamp;

	// A button that can act as a rotary modifier must not fire its long action at intervalToLongPress while
	// it is still held: holding it is also how you start a gesture, and the user has not necessarily begun
	// turning the encoder yet. (Holding NEXT for 700ms to seek would otherwise first fire BUTTON_0_LONG --
	// CMD_LASTTRACK by default -- and jump straight to the end of the book.) Defer it to release, exactly as
	// CMD_SLEEPMODE already is, so that a turn in the meantime can cancel it via usedAsModifier. A plain
	// long-press with no turn still works, it just resolves when the button comes back up.
	bool const isRotaryModifier = (Button_GetRotaryAction(i, true) != CMD_NOTHING) || (Button_GetRotaryAction(i, false) != CMD_NOTHING);

	// Handle button release (short or long press completed)
	if (wasReleased) {
		unsigned long const releaseDuration = gButtons[i].lastReleasedTimestamp - gButtons[i].lastPressedTimestamp;
		bool const wasShortPress = releaseDuration < intervalToLongPress;

		if (wasShortPress) {
			Cmd_Action(Cmd_Short);
		} else if (Cmd_Long == CMD_SLEEPMODE || isRotaryModifier) {
			// Sleep-mode only triggers on release to prevent immediate wake-up; modifier buttons defer for
			// the reason above.
			Cmd_Action(Cmd_Long);
		}

		gButtons[i].isPressed = false;
		return;
	}

	if (isRotaryModifier) {
		return; // Still held: wait for release before deciding whether this was a long press or a gesture
	}

	// Handle volume buttons with repeat functionality
	if (Cmd_Long == CMD_VOLUMEUP || Cmd_Long == CMD_VOLUMEDOWN) {
		if (pressDuration <= intervalToLongPress) {
			return;
		}
		uint16_t remainder = pressDuration % intervalToLongPress;
		if (remainder < gLongPressTime) {
			Cmd_Action(Cmd_Long);
		}
		gLongPressTime = remainder;
		return;
	}

	// Handle other long-press actions (except sleep mode which triggers on release)
	if (Cmd_Long != CMD_SLEEPMODE && pressDuration > intervalToLongPress) {
		gButtons[i].isPressed = false;
		Cmd_Action(Cmd_Long);
	}
}

// settings-override.h, when present, replaces settings.h wholesale -- so an override written before this
// feature existed defines none of the BUTTON_n_ROTARY_* macros. Default them to CMD_NOTHING (= button is
// not a modifier) so those configs keep building and simply have no gestures until they opt in.
#ifndef BUTTON_0_ROTARY_CW
	#define BUTTON_0_ROTARY_CW CMD_NOTHING
#endif
#ifndef BUTTON_0_ROTARY_CCW
	#define BUTTON_0_ROTARY_CCW CMD_NOTHING
#endif
#ifndef BUTTON_1_ROTARY_CW
	#define BUTTON_1_ROTARY_CW CMD_NOTHING
#endif
#ifndef BUTTON_1_ROTARY_CCW
	#define BUTTON_1_ROTARY_CCW CMD_NOTHING
#endif
#ifndef BUTTON_2_ROTARY_CW
	#define BUTTON_2_ROTARY_CW CMD_NOTHING
#endif
#ifndef BUTTON_2_ROTARY_CCW
	#define BUTTON_2_ROTARY_CCW CMD_NOTHING
#endif
#ifndef BUTTON_3_ROTARY_CW
	#define BUTTON_3_ROTARY_CW CMD_NOTHING
#endif
#ifndef BUTTON_3_ROTARY_CCW
	#define BUTTON_3_ROTARY_CCW CMD_NOTHING
#endif
#ifndef BUTTON_4_ROTARY_CW
	#define BUTTON_4_ROTARY_CW CMD_NOTHING
#endif
#ifndef BUTTON_4_ROTARY_CCW
	#define BUTTON_4_ROTARY_CCW CMD_NOTHING
#endif
#ifndef BUTTON_5_ROTARY_CW
	#define BUTTON_5_ROTARY_CW CMD_NOTHING
#endif
#ifndef BUTTON_5_ROTARY_CCW
	#define BUTTON_5_ROTARY_CCW CMD_NOTHING
#endif
#ifndef BUTTON_6_ROTARY_CW
	#define BUTTON_6_ROTARY_CW CMD_NOTHING
#endif
#ifndef BUTTON_6_ROTARY_CCW
	#define BUTTON_6_ROTARY_CCW CMD_NOTHING
#endif
#ifndef BUTTON_7_ROTARY_CW
	#define BUTTON_7_ROTARY_CW CMD_NOTHING
#endif
#ifndef BUTTON_7_ROTARY_CCW
	#define BUTTON_7_ROTARY_CCW CMD_NOTHING
#endif
#ifndef BUTTON_8_ROTARY_CW
	#define BUTTON_8_ROTARY_CW CMD_NOTHING
#endif
#ifndef BUTTON_8_ROTARY_CCW
	#define BUTTON_8_ROTARY_CCW CMD_NOTHING
#endif
#ifndef BUTTON_9_ROTARY_CW
	#define BUTTON_9_ROTARY_CW CMD_NOTHING
#endif
#ifndef BUTTON_9_ROTARY_CCW
	#define BUTTON_9_ROTARY_CCW CMD_NOTHING
#endif
#ifndef BUTTON_10_ROTARY_CW
	#define BUTTON_10_ROTARY_CW CMD_NOTHING
#endif
#ifndef BUTTON_10_ROTARY_CCW
	#define BUTTON_10_ROTARY_CCW CMD_NOTHING
#endif
#ifndef BUTTON_11_ROTARY_CW
	#define BUTTON_11_ROTARY_CW CMD_NOTHING
#endif
#ifndef BUTTON_11_ROTARY_CCW
	#define BUTTON_11_ROTARY_CCW CMD_NOTHING
#endif

// "Hold this button, turn the encoder" -- one action per rotation direction, per button.
// Mirrors buttonCmdConfig: compile-time default in settings.h, overridable at runtime via NVS.
static const struct {
	const char *prefsKeyCw;
	const char *prefsKeyCcw;
	uint8_t defaultCw;
	uint8_t defaultCcw;
} rotaryCmdConfig[BUTTON_COUNT] = {
	{ "btnRotCw0",  "btnRotCcw0",	BUTTON_0_ROTARY_CW,	BUTTON_0_ROTARY_CCW},
	{ "btnRotCw1",  "btnRotCcw1",	BUTTON_1_ROTARY_CW,	BUTTON_1_ROTARY_CCW},
	{ "btnRotCw2",  "btnRotCcw2",	BUTTON_2_ROTARY_CW,	BUTTON_2_ROTARY_CCW},
	{ "btnRotCw3",  "btnRotCcw3",	BUTTON_3_ROTARY_CW,	BUTTON_3_ROTARY_CCW},
	{ "btnRotCw4",  "btnRotCcw4",	BUTTON_4_ROTARY_CW,	BUTTON_4_ROTARY_CCW},
	{ "btnRotCw5",  "btnRotCcw5",	BUTTON_5_ROTARY_CW,	BUTTON_5_ROTARY_CCW},
	{ "btnRotCw6",  "btnRotCcw6",	BUTTON_6_ROTARY_CW,	BUTTON_6_ROTARY_CCW},
	{ "btnRotCw7",  "btnRotCcw7",	BUTTON_7_ROTARY_CW,	BUTTON_7_ROTARY_CCW},
	{ "btnRotCw8",  "btnRotCcw8",	BUTTON_8_ROTARY_CW,	BUTTON_8_ROTARY_CCW},
	{ "btnRotCw9",  "btnRotCcw9",	BUTTON_9_ROTARY_CW,	BUTTON_9_ROTARY_CCW},
	{"btnRotCw10", "btnRotCcw10", BUTTON_10_ROTARY_CW, BUTTON_10_ROTARY_CCW},
	{"btnRotCw11", "btnRotCcw11", BUTTON_11_ROTARY_CW, BUTTON_11_ROTARY_CCW},
};

uint8_t Button_GetRotaryAction(uint8_t buttonIndex, bool clockwise) {
	if (buttonIndex >= (sizeof(rotaryCmdConfig) / sizeof(rotaryCmdConfig[0]))) {
		return CMD_NOTHING;
	}
	const auto &cfg = rotaryCmdConfig[buttonIndex];
	return clockwise
		? gPrefsSettings.getUChar(cfg.prefsKeyCw, cfg.defaultCw)
		: gPrefsSettings.getUChar(cfg.prefsKeyCcw, cfg.defaultCcw);
}

// Compile-time default (settings.h / the override), ignoring any NVS override. Used by the
// web-UI's "reset to factory settings".
uint8_t Button_GetRotaryActionDefault(uint8_t buttonIndex, bool clockwise) {
	if (buttonIndex >= (sizeof(rotaryCmdConfig) / sizeof(rotaryCmdConfig[0]))) {
		return CMD_NOTHING;
	}
	return clockwise ? rotaryCmdConfig[buttonIndex].defaultCw : rotaryCmdConfig[buttonIndex].defaultCcw;
}

// currentState (not isPressed) is the live level: false means physically down right now. isPressed is a
// consumable latch that the long-press handler clears while the finger is still on the button, so it is
// useless as a "still held" signal.
uint8_t Button_GetHeldModifier(void) {
	if (!gButtonInitComplete) {
		return BUTTON_NONE; // gButtons[] is still garbage before the first scan
	}
	for (uint8_t i = 0; i < (sizeof(rotaryCmdConfig) / sizeof(rotaryCmdConfig[0])); i++) {
		if (!Button_IsEnabled(i) || gButtons[i].currentState) {
			continue; // not connected or not pressed
		}
		if (Button_GetRotaryAction(i, true) != CMD_NOTHING || Button_GetRotaryAction(i, false) != CMD_NOTHING) {
			return i;
		}
	}
	return BUTTON_NONE;
}

void Button_MarkModifierUsed(uint8_t buttonIndex) {
	if (buttonIndex < (sizeof(gButtons) / sizeof(gButtons[0]))) {
		gButtons[buttonIndex].usedAsModifier = true;
	}
}

// Do corresponding actions for all buttons
void Button_DoButtonActions(void) {
	if (Button_HandleMultiButtonPress()) {
		return;
	}

	unsigned long currentTimestamp = millis();
	for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
		if (gButtons[i].isPressed) {
			Button_HandleSinglePress(i, currentTimestamp);
		}
	}
}

void IRAM_ATTR onTimer() {
	xSemaphoreGiveFromISR(Button_TimerSemaphore, NULL);
}
