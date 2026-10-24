#pragma once

constexpr uint8_t cardIdSize = 4u;
constexpr uint8_t cardIdStringSize = (cardIdSize * 3u) + 1u;

extern char gCurrentRfidTagId[cardIdStringSize];

// PN5180's LPCD wakes ESPuino from deep sleep via RFID_IRQ. If the analog buttons' resistor ladder uses the same pin,
// LPCD can't work: the ladder's pullup is powered by the switched 3.3 V, so the pin is low in deep sleep and would
// wake ESPuino again immediately. LPCD is ignored in that case, whatever is stored in NVS.
#if defined(ANALOG_BUTTONS_ENABLE) && defined(ANALOG_BUTTONS_PIN) && defined(RFID_IRQ) && (ANALOG_BUTTONS_PIN == RFID_IRQ)
constexpr bool Rfid_LpcdAvailable = false;
#else
constexpr bool Rfid_LpcdAvailable = true;
#endif

void Rfid_ResetOldRfid(void);
void Rfid_ResetLastTag(void);
// Reader-task internal: returns true once after Rfid_ResetLastTag() was called.
bool Rfid_ConsumeLastTagReset(void);
void Rfid_Init(void);
void Rfid_WakeupHandling(void);
void Rfid_StartTask(void);
void Rfid_Cyclic(void);
void Rfid_Exit(void);
void Rfid_TaskPause(void);
void Rfid_TaskResume(void);
void Rfid_TaskReset(void);
void Rfid_WakeupCheck(void);
void Rfid_PreferenceLookupHandler(void);
// Writes one card assignment to NVS, see RfidCommon.cpp.
bool Rfid_SaveAssignment(const char *cardId, const char *fileOrUrl, uint8_t playModeOrModId,
	uint32_t lastPlayPos = 0, uint16_t trackLastPlayed = 0);
