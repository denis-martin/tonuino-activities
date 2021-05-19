#include <assert.h>

#include <SoftwareSerial.h>
#include <DFMiniMp3.h>

#include <SPI.h>
#include <MFRC522.h>

#include <JC_Button.h>

#include <EEPROM.h>
#include <avr/sleep.h>


#define DEBUG

#ifdef DEBUG
#define DEBUG_MODE 1
#define MEMDEBUG
#else
#define DEBUG_MODE 0
#endif

#define DBG(a) do { if (DEBUG_MODE) { a; } } while (0);
