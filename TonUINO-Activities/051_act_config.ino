
typedef enum ConfigMenu {
  ConfigMenuGeneral = 0,
  ConfigMenuSetVolume = 1,
  ConfigMenuSetVolumeMin = 2,
  ConfigMenuSetVolumeMax = 3,
  ConfigMenuSetHpVolume = 4,
  ConfigMenuSetHpVolumeMin = 5,
  ConfigMenuSetHpVolumeMax = 6,

  ConfigMenuNewCard = 10,
  ConfigMenuChooseFolder = 11,
  ConfigMenuChooseTrack = 12,
} ConfigMenu;

/**
 * Default activity class: Play albums
 */
class ConfigActivity : public Activity
{
private:
  ConfigMenu step;
  CardData cardData;

  void resetCard();

  void initVolume(uint8_t vol);

  void setStep(ConfigMenu step);
  void nextStep();
  
public:
  ConfigActivity(bool playIntro = true, bool enqueueIntro = false);
  virtual ~ConfigActivity();

  virtual void loop();
  virtual void onNewCard(const byte uid[], const CardData* cardData);
  virtual void onNewBlankCard(const byte uid[]);
  virtual void onPlayFinished(const uint16_t track, const bool playFinished);

  virtual void onButtonReleased(const uint8_t button);
  virtual void onButtonLongPressed(const uint8_t button);
};

/**
 * Constructor
 */
ConfigActivity::ConfigActivity(bool playIntro, bool enqueueIntro) :
  step(ConfigMenuSetVolume)
{
  activityCode = ActivityConfig;
  DBG(Serial.println(F("ConfigActivity()")));
  resetCard();
  if (playIntro) {
    player.enqueue(0, SND_CONFIG_MENU, !enqueueIntro);
  }
}

/**
 * Destructor - free all memory here!
 */
ConfigActivity::~ConfigActivity()
{
  
}

/**
 * Loop
 */
void ConfigActivity::loop()
{
  
}

/**
 * Called when a new (valid) card is read
 */
void ConfigActivity::onNewCard(const byte uid[], const CardData* cardData)
{
  onNewBlankCard(uid);
}

/**
 * Called when a new (empty) card is read
 */
void ConfigActivity::onNewBlankCard(const byte uid[])
{
  DBG(Serial.println(F("ConfigActivity: onNewBlankCard")));
  if (step < ConfigMenuNewCard) {
    resetCard();
    setStep(ConfigMenuChooseFolder);
  }
}

/**
 * Called when a track finished playing
 * 
 * track: Global track number
 * playFinished: True if play command finished, false if only track finished
 */
void ConfigActivity::onPlayFinished(const uint16_t track, const bool playFinished)
{
  DBG(Serial.print(F("ConfigActivity::onPlayFinished: ")); Serial.print(track); Serial.print(F(" playFinished ")); Serial.println(playFinished));
}

#define getVolumeSound(step) (SND_CONFIG_VOLUME + ((uint16_t) step) - ((uint16_t) ConfigMenuSetVolume))

#ifdef DEBUG
void errUnhandledState(uint8_t state)
{
  Serial.print(F("ERR: Unhandled state: "));
  Serial.println(state);
}
#else
#define errUnhandledState(state) {}
#endif

/**
 * Simple button presses
 */
void ConfigActivity::onButtonReleased(const uint8_t button)
{
  switch (step) {
    case ConfigMenuSetVolume:      // no break
    case ConfigMenuSetVolumeMin:   // no break
    case ConfigMenuSetVolumeMax:   // no break
    case ConfigMenuSetHpVolume:    // no break
    case ConfigMenuSetHpVolumeMin: // no break
    case ConfigMenuSetHpVolumeMax: {
      switch (button) {
        case BTN_UP: {
          player.volumeInc(step != ConfigMenuSetVolume && step != ConfigMenuSetHpVolume);
          player.enqueue(0, getVolumeSound(step), true);
          player.enqueue(0, SND_NUMBERS_BASE + player.getVolume());
          break;
        }
        case BTN_DOWN: {
          player.volumeDec(step != ConfigMenuSetVolume && step != ConfigMenuSetHpVolume);
          player.enqueue(0, getVolumeSound(step), true);
          player.enqueue(0, SND_NUMBERS_BASE + player.getVolume());
          break;
        }
        case BTN_SELECT: {
          switch (step) {
            case ConfigMenuSetVolume:      settings.volume = player.getVolume(); break;
            case ConfigMenuSetVolumeMin:   settings.volumeMin = player.getVolume(); break;
            case ConfigMenuSetVolumeMax:   settings.volumeMax = player.getVolume(); break;
            case ConfigMenuSetHpVolume:    settings.hpVolume = player.getVolume(); break;
            case ConfigMenuSetHpVolumeMin: settings.hpVolumeMin = player.getVolume(); break;
            case ConfigMenuSetHpVolumeMax: settings.hpVolumeMax = player.getVolume(); break;
            default: {
              errUnhandledState(step);
            }
          }
        }
      }
      break;
    }
    case ConfigMenuChooseFolder: {
      switch (button) {
        case BTN_UP: {
          if (cardData.folder <= 99) {
            cardData.folder += 1;
          }
          player.enqueue(0, SND_NUMBERS_BASE + cardData.folder, true);
          break;
        }
        case BTN_DOWN: {
          if (cardData.folder > 1) {
            cardData.folder -= 1;
          }
          player.enqueue(0, SND_NUMBERS_BASE + cardData.folder, true);
          break;
        }
      }
      break;
    }
    case ConfigMenuChooseTrack: {
      switch (button) {
        case BTN_UP: {
          if (cardData.track <= 255) {
            cardData.track += 1;
            player.enqueue(0, SND_NUMBERS_BASE + cardData.track, true);
          }
          break;
        }
        case BTN_DOWN: {
          if (cardData.track > 1) {
            cardData.track -= 1;
            player.enqueue(0, SND_NUMBERS_BASE + cardData.track, true);
          }
          break;
        }
      }
      break;
    }
    default: {
      errUnhandledState(step);
    }
  }

  if (button == BTN_SELECT) {
    nextStep();
  }
}

/**
 * Long button presses
 */
void ConfigActivity::onButtonLongPressed(const uint8_t button)
{
  switch (button) {
    case BTN_UP: {
      break;
    }
    case BTN_DOWN: {
      player.enqueue(0, SND_ABORTED, true);
      switchActivity(ActivityDefault, true, true);
      return;
    }
    case BTN_SELECT: {
      switch (step) {
        case ConfigMenuChooseFolder: // no break
        case ConfigMenuChooseTrack: {
          // save card
          cardData.activity = (uint8_t) ActivityDefault;
          if (keycards_write(&cardData)) {
            player.enqueue(0, SND_SUCCESS, true);
          } else {
            player.enqueue(0, SND_ERROR, true);
          }
          break;
        }
        default: {
          // save general settings
          saveSettings();
          player.enqueue(0, SND_SAVED, true);
        }
      }
    }
  }
}

void ConfigActivity::resetCard()
{
  memset(&cardData, 0, sizeof(CardData));
  cardData.folder = 1;
}

void ConfigActivity::initVolume(uint8_t vol)
{
  player.setVolume(vol);
  player.enqueue(0, getVolumeSound(step), true);
  player.enqueue(0, SND_NUMBERS_BASE + vol);
}

void ConfigActivity::setStep(ConfigMenu nextStep)
{
  step = nextStep;
  
  switch (step) {
    case ConfigMenuSetVolume:      initVolume(settings.volume); break;
    case ConfigMenuSetVolumeMin:   initVolume(settings.volumeMin); break;
    case ConfigMenuSetVolumeMax:   initVolume(settings.volumeMax); break;
    case ConfigMenuSetHpVolume:    initVolume(settings.hpVolume); break;
    case ConfigMenuSetHpVolumeMin: initVolume(settings.hpVolumeMin); break;
    case ConfigMenuSetHpVolumeMax: initVolume(settings.hpVolumeMax); break;
    case ConfigMenuChooseFolder:   player.enqueue(0, SND_CONFIG_SELECT_FOLDER, true); break;
    case ConfigMenuChooseTrack:    player.enqueue(0, SND_CONFIG_SELECT_TRACK, true); break;
    default: {
      DBG(Serial.print(F("ERROR: ConfigActivity::setStep() step not handled: ")); Serial.println(step));
    }
  }
}

void ConfigActivity::nextStep()
{ 
  switch (step) {
    case ConfigMenuSetVolume:      setStep(ConfigMenuSetVolumeMin); break;
    case ConfigMenuSetVolumeMin:   setStep(ConfigMenuSetVolumeMax); break;
    case ConfigMenuSetVolumeMax:   setStep(ConfigMenuSetHpVolume); break;
    case ConfigMenuSetHpVolume:    setStep(ConfigMenuSetHpVolumeMin); break;
    case ConfigMenuSetHpVolumeMin: setStep(ConfigMenuSetHpVolumeMax); break;
    case ConfigMenuSetHpVolumeMax: setStep(ConfigMenuSetVolume); break;
    
    case ConfigMenuChooseFolder:   setStep(ConfigMenuChooseTrack); break;
    case ConfigMenuChooseTrack:    setStep(ConfigMenuChooseFolder); break;
    default: {
      DBG(Serial.print(F("ERROR: ConfigActivity::nextStep() step not handled: ")); Serial.println(step));
    }
  }
}
