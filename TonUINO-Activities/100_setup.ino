
#ifdef DEBUG
static uint32_t timer = 0;
#define setTimer(x) { timer = x; }
#else
#define timer 0
#define setTimer(x) {}
#endif

/**
 * Save settings to EEPROM
 */
void saveSettings()
{
  DBG(Serial.println(F("Saving settings")));
  EEPROM.put(SETTINGS_EEPROM_ADDR, settings);
}

/**
 * Reset/initialize settings
 */
void resetSettings()
{
  DBG(Serial.println(F("Initializing settings")));
  memset(&settings, 0, sizeof(Settings));
  memcpy(settings.magicCode, magicCode, MAGICCODE_LEN);
  settings.version     = SETTINGS_VERSION;
  settings.volume      = VOLUME_DEFAULT;
  settings.volumeMin   = VOLUME_DEFAULT_MIN;
  settings.volumeMax   = VOLUME_DEFAULT_MAX;
  settings.hpVolume    = VOLUME_DEFAULT;
  settings.hpVolumeMin = VOLUME_DEFAULT_MIN;
  settings.hpVolumeMax = VOLUME_DEFAULT_MAX;
}

/**
 * Load settings from EEPROM
 */
void loadSettings()
{
  EEPROM.get(SETTINGS_EEPROM_ADDR, settings);
  if (memcmp(settings.magicCode, magicCode, MAGICCODE_LEN) != 0) {
    resetSettings();
    
  } else {
    DBG(Serial.println(F("Settings loaded")));
    DBG(
      Serial.print(F("  Volume/min/max ")); Serial.print(settings.volume); 
      Serial.print('/'); Serial.print(settings.volumeMin); 
      Serial.print('/'); Serial.println(settings.volumeMax);
      Serial.print(F("  Headphones volume/min/max ")); Serial.print(settings.hpVolume); 
      Serial.print('/'); Serial.print(settings.hpVolumeMin); 
      Serial.print('/'); Serial.println(settings.hpVolumeMax)
    );
    
    if (settings.version != SETTINGS_VERSION) {
      DBG(Serial.println(F("Version mismatch!")));
      resetSettings();
    }
    if (settings.volumeMin > VOLUME_MAX || settings.volumeMax > VOLUME_MAX || settings.volumeMin > settings.volumeMax ||
      settings.volume < settings.volumeMin || settings.volume > settings.volumeMax)
    {
      DBG(Serial.println(F("Volume/min/max inconsistent, resetting")));
      settings.volume    = VOLUME_DEFAULT;
      settings.volumeMin = VOLUME_DEFAULT_MIN;
      settings.volumeMax = VOLUME_DEFAULT_MAX;
    }
    if (settings.hpVolumeMin > VOLUME_MAX || settings.hpVolumeMax > VOLUME_MAX || settings.hpVolumeMin > settings.hpVolumeMax ||
      settings.hpVolume < settings.hpVolumeMin || settings.hpVolume > settings.hpVolumeMax)
    {
      DBG(Serial.println(F("Headphones volume/min/max inconsistent, resetting")));
      settings.hpVolume    = VOLUME_DEFAULT;
      settings.hpVolumeMin = VOLUME_DEFAULT_MIN;
      settings.hpVolumeMax = VOLUME_DEFAULT_MAX;
    }
  }
}

/**
 * Switch activties.
 * Ensures that the old activity is destroyed before creating a new one.
 */
void switchActivity(const ActivityCode actCode, const bool playIntro, const bool enqueueIntro)
{
  if (currentActivity) delete currentActivity;
  switch (actCode) {
    case ActivityConfig: {
      currentActivity = new ConfigActivity(playIntro, enqueueIntro); 
      break;
    }
    default: {
      currentActivity = new DefaultActivity(playIntro, enqueueIntro);
    }
  }
}

void setup()
{
  DBG(Serial.begin(115200));
  DBG(Serial.println(F("a3box is starting")));
  DBG(setTimer(millis()));

#ifdef SHUTDOWN_PIN
  pinMode(SHUTDOWN_PIN, OUTPUT);
  digitalWrite(SHUTDOWN_PIN, LOW);
#endif

  player.setupA();

  loadSettings();

  keycards_setup();
  buttons_setup();
  player.setupB();

  switchActivity(ActivityDefault, true, false);

  set_sleep_mode(SLEEP_MODE_IDLE);

  lastActivity = millis();
}
