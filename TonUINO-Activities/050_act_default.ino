
typedef enum PlayMode {
  PlayModeSingleTrack = 0,
  PlayModeAlbum = 1,
  //PlayModeRandom = 2
} PlayMode;

/**
 * Default activity class: Play albums
 */
class DefaultActivity : public Activity
{
private:
  bool paused;

  PlayMode playMode;
  uint8_t currentFolder;
  uint16_t currentTrack;
  uint16_t tracksInFolder;

  void reset();
  
  void skipForward();
  void skipBackward();

public:
  DefaultActivity(bool playIntro = true, bool enqueueIntro = false);
  virtual ~DefaultActivity();

  virtual void onNewCard(const byte uid[], const CardData* cardData);
  virtual void onPlayFinished(const uint16_t track, const bool playFinished);

  virtual void onButtonReleased(const uint8_t button);
  virtual void onButtonLongPressed(const uint8_t button);
};

/**
 * Constructor
 */
DefaultActivity::DefaultActivity(bool playIntro, bool enqueueIntro) :
  paused(false), playMode(PlayModeSingleTrack),
  currentFolder(0), currentTrack(0), tracksInFolder(0)
{
  DBG(Serial.println(F("DefaultActivity()")));
  activityCode = ActivityDefault;
  if (playIntro) {
    player.enqueue(0, SND_STARTUP, !enqueueIntro);
  }
}

/**
 * Destructor - free all memory here!
 */
DefaultActivity::~DefaultActivity()
{
  
}

/**
 * Called when a new (valid) card is read
 */
void DefaultActivity::onNewCard(const byte uid[], const CardData* cardData)
{
  DBG(Serial.print(F("DefaultActivity::onNewCard: ")));
  if (!cmpUid(uid)) {
    DBG(Serial.println(F("new card")));
    reset();
    setUid(uid);
    currentFolder = cardData->folder;
    currentTrack = cardData->track;
    tracksInFolder = player.getTrackCount(currentFolder);
    if (currentTrack == 0) {
      playMode = PlayModeAlbum;
      currentTrack = 1;
    } else {
      playMode = PlayModeSingleTrack;
    }
    player.enqueue(currentFolder, currentTrack, true);
    
  } else {
    DBG(Serial.println(F("same card")));
    if (paused) {
      paused = false;
      player.start();
    }
    
  }
}

/**
 * Called when a track finished playing
 * 
 * track: Global track number
 * playFinished: True if play command finished, false if only track finished
 */
void DefaultActivity::onPlayFinished(const uint16_t track, const bool playFinished)
{
  DBG(Serial.print(F("DefaultActivity::onPlayFinished: ")); Serial.print(track); Serial.print(F(" playFinished ")); Serial.println(playFinished));
  if (playFinished) {
    switch (playMode) {
      case PlayModeSingleTrack: {
        reset(); // forget last card
        break;
      }
      case PlayModeAlbum: {
        currentTrack++;
        if (currentTrack <= tracksInFolder) {
          player.enqueue(currentFolder, currentTrack, true);
        } else {
          DBG(Serial.println(F("Album: all tracks in folder played")));
          reset(); // forget last card
        }
        break;
      }
    }
  }
}

/**
 * Simple button presses
 */
void DefaultActivity::onButtonReleased(const uint8_t button)
{
  switch (button) {
    case BTN_UP: {
      skipForward();
      break;
    }
    case BTN_DOWN: {
      skipBackward();
      break;
    }
    case BTN_SELECT: {
      if (paused) {
        paused = false;
        player.start();
        
      } else if (player.isPlaying()) {
        paused = true;
        player.pause();
        
      }
      break;
    }
    default: {
      // none
    }
  }
}

/**
 * Long button presses
 */
void DefaultActivity::onButtonLongPressed(const uint8_t button)
{
  switch (button) {
    case BTN_UP: {
      player.volumeInc();
      break;
    }
    case BTN_DOWN: {
      player.volumeDec();
      break;
    }
    case BTN_SELECT: {
      player.stop();
      reset();
      break;
    }
  }
}

/**
 * Long button presses
 */
void DefaultActivity::reset()
{
  memset(uid, 0, UIDSIZE);
  playMode = PlayModeSingleTrack;
  currentFolder = 0;
  currentTrack = 0;
  tracksInFolder = 0;
  paused = false;
}

/**
 * Skip to next track
 */
void DefaultActivity::skipForward()
{
  if (playMode == PlayModeAlbum) {
    if (currentTrack < tracksInFolder) {
      currentTrack++;
      player.enqueue(currentFolder, currentTrack, true);
      
    } else {
      DBG(Serial.println(F("skipForward: reached end of album")));
      player.playNotification(SND_ACK);
      
    }
  }
}

/**
 * Skip to previous track
 */
void DefaultActivity::skipBackward()
{
  if (playMode == PlayModeAlbum) {
    if (currentTrack > 1) {
      currentTrack--;
    }
    player.enqueue(currentFolder, currentTrack, true);
    
  } else if (playMode == PlayModeSingleTrack) {
    player.enqueue(currentFolder, currentTrack, true);
    
  }
}
