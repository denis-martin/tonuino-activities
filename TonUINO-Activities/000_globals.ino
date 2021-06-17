
/**
 * Software switch-off using a Polulu switch (uncomment this if needed).
 * Pin is set to LOW at startup and set to HIGH upon shutdown.
 */
//#define SHUTDOWN_PIN 7

/**
 * Time in millis after which we power down as much as possible, hopefully
 * triggering auto-power-off of the PowerBank
 */
#define STANDBY_TIME (((unsigned long) 30)*60*1000) // 30 minutes

/**
 * Card UID buffer: 6 bytes
 */
#define UIDSIZE 6

/**
 * Magic code. 
 * Used to check whether the keycards and the EEPROM settings
 * contain data we understand.
 */
#define MAGICCODE_LEN 3
const static uint8_t magicCode[MAGICCODE_LEN] = { 0xFB, 0x17, 0xE0 };

/** 
 * Card data struct.
 * The data we write to the RFID cards.
 */
#define CARDDATA_VERSION 0x01
typedef struct __attribute__ ((packed)) {
  uint8_t magicCode[MAGICCODE_LEN];
  uint8_t version;      // struct version (e.g. CARDDATA_VERSION)
  uint8_t activity;     // 0xff: admin, 0x00: default
  uint8_t folder;       // 0x80: global folder; additional bits are used to encode track numbers > 255 in this case
  uint8_t track;        // track within the folder; if 0, then play the whole folder
  uint8_t button;       // associate a button with this card (e.g., a player or a color)
  uint8_t reserved[8];  // reserved for future / activity related extensions
} CardData;
static_assert(sizeof(CardData) == 16, "CardData size unexpected!"); // if we use more, we need to modify the card read/write logic a bit

/**
 * System settings.
 * Read/written to EEPROM.
 */
#define SETTINGS_EEPROM_ADDR 0x00
#define SETTINGS_VERSION 0x01
typedef struct Settings {
  uint8_t magicCode[MAGICCODE_LEN];
  uint8_t version;      // struct version (e.g. SETTINGS_VERSION)
  uint8_t volume;       // main volume
  uint8_t volumeMin;    // main volume min
  uint8_t volumeMax;    // main volume max
  uint8_t hpVolume;     // headphone volume
  uint8_t hpVolumeMin;  // headphone volume min
  uint8_t hpVolumeMax;  // headphone volume max
  uint8_t reserved[22]; // reserved for future use (ensured to be 0)
} Settings;
static_assert(sizeof(Settings) == 32, "Settings size unexpected!");

/**
 * Global settings struct.
 */
static Settings settings;

/**
 * Activity codes
 */
typedef enum ActivityCode {
  ActivityDefault = 0x00,
  ActivityConfig = 0xff
} ActivityCode;
#define MAX_ACTIVITIES ActivityDefault

/**
 * Abstract activity class
 */
class Activity
{
protected:
  byte uid[UIDSIZE];
  ActivityCode activityCode;
  
  virtual void setUid(const byte uid[]) {
    memcpy(this->uid, uid, UIDSIZE);
  }

  virtual bool cmpUid(const byte uid[]) {
    return memcmp(this->uid, uid, UIDSIZE) == 0;
  }

  virtual void rstUid(const byte uid[]) {
    memset(this->uid, 0, UIDSIZE);
  }
public:
  Activity() : activityCode(ActivityDefault) {}
  virtual ~Activity() {}

  /**
   * Loop
   */
  virtual void loop() {}

  /**
   * Called when a new (valid) card is read
   */
  virtual void onNewCard(const byte uid[], const CardData* cardData) {}

  /**
   * Called when a new (empty) card is read
   */
  virtual void onNewBlankCard(const byte uid[]) {}

  /**
   * Called when a track finished playing
   * 
   * track: Global track number
   * playFinished: True if play command finished, false if only track finished
   */
  virtual void onPlayFinished(const uint16_t track, const bool playFinished) {}

  /**
   * Called when headphones are plugged or unplugged
   * 
   * plugged: True if headphones were just plugged, else false
   */
  virtual void onHeadphoneJack(const bool plugged) {}

  /**
   * Called when a button was pressed.
   */
  virtual void onButtonPressed(const uint8_t button) {}

  /**
   * Called when a button was released but not long-pressed.
   */
  virtual void onButtonReleased(const uint8_t button) {}

  /**
   * Called when a button was long-pressed.
   */
  virtual void onButtonLongPressed(const uint8_t button) {}

  /**
   * Return code for this activity.
   */
  virtual ActivityCode getActivityCode() { return activityCode; }

};

/**
 * Current Activity
 */
static Activity* currentActivity = NULL;

/**
 * Idle time
 */
static unsigned long lastActivity = 0;

// some forward declarations
int keycards_write(CardData *cardData);
void player_onHeadphoneJack(const bool plugged);
void saveSettings();
void switchActivity(const ActivityCode actCode, const bool playIntro, const bool enqueueIntro);
