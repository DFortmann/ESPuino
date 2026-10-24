#pragma once

typedef struct {
	bool lastState		: 1;
	bool currentState	: 1;
	bool isPressed		: 1;
	bool isReleased		: 1;
	bool usedAsModifier : 1; // A rotary gesture was performed while this button was held: swallow its normal short/long action
	unsigned long lastPressedTimestamp;
	unsigned long lastReleasedTimestamp;
	unsigned long firstPressedTimestamp;
} t_button;

// next + prev + pplay + rotEnc + button4 ... button11
constexpr uint8_t BUTTON_COUNT = 12;

extern t_button gButtons[BUTTON_COUNT + 1]; // + dummy-button
extern uint8_t gShutdownButton;
extern bool gButtonInitComplete;

constexpr uint8_t BUTTON_NONE = 0xFFu;

// True if the button's GPIO/channel is configured (and supported by the enabled modules).
bool Button_IsEnabled(uint8_t buttonIndex);
// Compile-time default action (settings.h / the override) for a short or long press, ignoring any NVS override.
uint8_t Button_GetActionDefault(uint8_t buttonIndex, bool longPress);

// "Hold a button, turn the rotary encoder" gestures. Button_GetHeldModifier() returns the index of a
// button that is physically down right now AND has a rotary action configured, or BUTTON_NONE.
uint8_t Button_GetHeldModifier(void);
uint8_t Button_GetRotaryAction(uint8_t buttonIndex, bool clockwise);
uint8_t Button_GetRotaryActionDefault(uint8_t buttonIndex, bool clockwise);
void Button_MarkModifierUsed(uint8_t buttonIndex);

void Button_Init(void);
void Button_Cyclic(void);
