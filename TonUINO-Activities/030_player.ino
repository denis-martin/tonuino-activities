
#define PLAYER_BUSYPIN  4
#define PLAYER_POWERPIN 6     // Pin for switching player module off using a MOSFET.
                              // Gate of MOSFET should be connected to a pullup resistor so it is 
                              // default-on.

#define VOLUME_MIN 0          // as given by sound chip
#define VOLUME_MAX 30         // as given by sound chip
#define VOLUME_INC 5          // increment/decrement step
#define VOLUME_DEFAULT_MIN 5  // for new settings
#define VOLUME_DEFAULT_MAX 25 // for new settings
#define VOLUME_DEFAULT 15     // for new settings

#define PLAYER_QUEUESIZE 32   // Number of max items in player queue. Watch memory consumption!

/*
 * Number of milliseconds after starting playing before the actual busy pin is is evaluated.
 * Background: After starting playing, it may take a few loops until the player actually plays 
 * the item and thus sets the busy pin. If we keep pushing the forward button, we be counting
 * wrong.
 */
#define BUSY_LOCK_MS 250

/**
 * Player management.
 * Singleton class.
 */
class Player
{
public:
  static Player* instance;
  
  /**
   * Notifications from dfplayer.
   * Class is never instantiated.
   */
  class PlayerNotifications 
  {
  public:
    static void OnError(uint16_t errorCode) 
    {
      DBG(
        Serial.print(F("DFPlayer error ")); 
        Serial.print(errorCode);
        switch (errorCode) {
          case DfMp3_Error_Busy: Serial.println(F(": No media found")); break;
          case DfMp3_Error_Sleeping: Serial.println(F(": Chip is in sleep mode")); break;
          case DfMp3_Error_SerialWrongStack: Serial.println(F(": SerialWrongStack")); break;
          case DfMp3_Error_CheckSumNotMatch: Serial.println(F(": Communication error, check wiring")); break;
          case DfMp3_Error_FileIndexOut: Serial.println(F(": File index out of bounds")); break;
          case DfMp3_Error_FileMismatch: Serial.println(F(": Cannot find file")); break;
          case DfMp3_Error_Advertise: Serial.println(F(": In advertisement or no track is playing while trying to advertise")); break;
          case DfMp3_Error_RxTimeout: Serial.println(F(": RX timeout, check wiring")); break;
          case DfMp3_Error_PacketSize: Serial.println(F(": Packet from device has incorrect size")); break;
          case DfMp3_Error_PacketHeader: Serial.println(F(": Packet from device has incorrect header")); break;
          case DfMp3_Error_PacketChecksum: Serial.println(F(": Packet from device has incorrect checksum")); break;
          case DfMp3_Error_General: Serial.println(F(": General error")); break;
          default: Serial.println(F(": Unknown error"));
        }
      );
    }
      
    static void OnPlayFinished(DfMp3_PlaySources source, uint16_t track) 
    {
      if (!Player::instance) return;
      Player* player = Player::instance;
      
      //DBG(Serial.print(F("Track finished: ")); Serial.println(track));
      player->trackFinished = !player->trackFinished;
      if (!player->trackFinished) { // this means play command is finished (from DFplayer mini's perspective)
        // play next item from queue
        if (player->queueItems > 0) {
          player->queueCursor = player->nextQueueItem();
          player->queueItems--;
          if (player->queueItems > 0) {
            player->playTrack(player->queue[player->queueCursor].folder, player->queue[player->queueCursor].track);
          } else {
            if (currentActivity) currentActivity->onPlayFinished(track, true);
          }
        } else {
          DBG(Serial.println(F("Warning: Item was played without a queue")));
          if (currentActivity) currentActivity->onPlayFinished(track, true);
        }
      } else {
        if (currentActivity) currentActivity->onPlayFinished(track, false);
      }
    }
      
    static void OnPlaySourceOnline(DfMp3_PlaySources source) 
    {
      DBG(Serial.print(F("Source online: ")); Serial.println(source));
    }
    
    static void OnPlaySourceInserted(DfMp3_PlaySources source) 
    {
      DBG(Serial.print(F("Source inserted: ")); Serial.println(source));
    }
    
    static void OnPlaySourceRemoved(DfMp3_PlaySources source) 
    {
      DBG(Serial.print(F("Source removed: ")); Serial.println(source));
    }
    
  };
  
private:
  SoftwareSerial dfplayerSerial; // TX, RX
  DFMiniMp3<SoftwareSerial, Player::PlayerNotifications> dfplayer;

  uint8_t volume = VOLUME_DEFAULT;
  bool headphonesPlugged = false;
  bool playing = false;
  bool trackFinished = false;

  unsigned long playingSince = 0;

  typedef struct QueueItem {
    uint8_t folder;
    uint16_t track;
  } QueueItem;
  
  QueueItem queue[PLAYER_QUEUESIZE];
  uint8_t queueCursor = 0;
  uint8_t queueItems = 0;
  uint8_t nextQueueItem() { return (queueCursor+1) % PLAYER_QUEUESIZE; }
  uint8_t nextFreeQueueItem() { return  (queueCursor+queueItems) % PLAYER_QUEUESIZE; }

  /**
   * Play a track. Use is discouraged, use Player::enqueue() instead.
   */
  void playTrack(const uint8_t folder, const uint16_t track)
  {
    DBG(Serial.print(F("Playing track ")); Serial.print(track); Serial.print(F(" from folder ")); Serial.println(folder));
    if (folder > 0) {
      dfplayer.playFolderTrack(folder, track);
    } else {
      dfplayer.playMp3FolderTrack(track);
    }
    playing = true;
    playingSince = millis();
    lastActivity = playingSince;
  }
  
public:
  /**
   * Constructor
   */
  Player();

  /**
   * Returns true if headphones are currently plugged, false otherwise.
   */
  bool areHeadphonesPlugged() { return headphonesPlugged; }

  /**
   * Returns true if a track is currently playing, false otherwise.
   */
  bool isPlaying() { return playing; }

  /**
   * Return track count for a given folder.
   */
  uint16_t getTrackCount(const uint8_t folder) { return dfplayer.getFolderTrackCount(folder); }

  /**
   * Play a notification. 
   * Uses advertisement in case a track is currently playing.
   */
  void playNotification(const uint16_t soundIndex);

  /**
   * Enqueues a track.
   * Use resetQueue = true to play the track immediately. This also removes all tracks from queue.
   */
  void enqueue(const uint8_t folder, const uint16_t track, const bool resetQueue = false);

  /**
   * Enqueues a number.
   * Use resetQueue = true to play the track immediately. This also removes all tracks from queue.
   */
  void enqueueNumber(const uint8_t number, const bool resetQueue = false);

  /**
   * Increase the volume by one step, respecting configured limits.
   */
  void volumeInc(const bool nolimits = false);

  /**
   * Decrease the volume by one step, respecting configured limits.
   */
  void volumeDec(const bool nolimits = false);

  /**
   * Get current volume.
   */
  uint8_t getVolume()  { return volume; }

  /**
   * Set the volume to a given value. Ignores limits.
   */
  void setVolume(const uint8_t vol);

  /**
   * Notify on headphones plugged.
   * Switches volumes as configured.
   */
  void onHeadphoneJack(const bool plugged);

  /**
   * Pause playback.
   */
  void pause() { dfplayer.pause(); }

  /**
   * Resume playback.
   */
  void start() { dfplayer.start(); }

  /**
   * Stop playback.
   */
  void stop() { dfplayer.stop(); }

  /**
   * Power down
   */
  void powerDown() { 
    dfplayer.sleep();
    digitalWrite(PLAYER_POWERPIN, LOW);
  }

  void setupA();
  void setupB();
  void loop();
};

Player* Player::instance = NULL;

/**
 * Constructor.
 */
Player::Player() :
  dfplayerSerial(2, 3), // TX, RX pins for DFPlayer
  dfplayer(dfplayerSerial)
{
  if (Player::instance) {
    DBG(Serial.println(F("Error: Player class can only exist once")));
  } else {
    Player::instance = this;
  }
}

/**
 * Play a notification. 
 * Uses advertisement in case a track is currently playing.
 */
void Player::playNotification(const uint16_t soundIndex)
{
  if (isPlaying()) {
    DBG(Serial.println(F("Playing notification as advertisment")));
    dfplayer.playAdvertisement(soundIndex);
  } else {
    DBG(Serial.println(F("Playing notification as normal track")));
    enqueue(0, soundIndex, true);
  }
}

/**
 * Enqueues a track.
 */
void Player::enqueue(const uint8_t folder, const uint16_t track, const bool resetQueue)
{
  if (resetQueue) {
    queueCursor = 0;
    queueItems = 1;
    playTrack(folder, track);
    
  } else {
    if (queueItems < PLAYER_QUEUESIZE) {
      uint8_t qi = nextFreeQueueItem();
      queue[qi].folder = folder;
      queue[qi].track = track;
      queueItems++;
      DBG(Serial.print(F("Enqueuing ")); Serial.print(folder); Serial.print(F("/")); Serial.print(track); Serial.print(F(" at ")); Serial.println(qi));
      if (!isPlaying()) {
        playTrack(folder, track);
      }
  
    } else {
      DBG(Serial.println(F("Queue is full, cannot queue more items")));
      playNotification(SND_QUEUE_FULL);
      
    }
  }
}

/**
 * Enqueues a number.
 */
void Player::enqueueNumber(const uint8_t number, const bool resetQueue)
{
  if (number == 0) {
    enqueue(0, SND_NUMBERS_NULL, resetQueue);
  } else {
    enqueue(0, SND_NUMBERS_BASE + number, resetQueue);
  }
}

void Player::volumeInc(const bool nolimits)
{
  const uint8_t maxVol = nolimits ? VOLUME_MAX : (headphonesPlugged ? settings.hpVolumeMax : settings.volumeMax);
  if (volume + VOLUME_INC <= maxVol) {
    setVolume(volume + VOLUME_INC);
  } else {
    DBG(Serial.println(F("Volume maximum reached")));
  }
}

void Player::volumeDec(const bool nolimits)
{
  const uint8_t minVol = nolimits ? VOLUME_MIN : (headphonesPlugged ? settings.hpVolumeMin : settings.volumeMin);
  if (volume - VOLUME_INC >= minVol) {
    setVolume(volume - VOLUME_INC);
  } else {
    DBG(Serial.println(F("Volume minimum reached")));
  }
}

void Player::setVolume(const uint8_t vol)
{
  DBG(Serial.print(F("Setting volume to ")); Serial.println(vol));
  volume = vol;
  dfplayer.setVolume(volume);
}

void Player::onHeadphoneJack(const bool plugged)
{
  DBG(Serial.print(F("Headphone jack ")); Serial.println(plugged));
  headphonesPlugged = plugged;
  setVolume(headphonesPlugged ? settings.hpVolume : settings.volume);
}

/**
 * Setup player (1)
 */
inline void Player::setupA()
{
  pinMode(PLAYER_POWERPIN, OUTPUT);
  digitalWrite(PLAYER_POWERPIN, HIGH);
  // no delay since we assume that the dfplayer was already on due to a pull-up resistor at the
  // gate of the switching MOSFET
  dfplayer.begin(); // initialize DFPlayer Mini
}

/**
 * Setup player (2)
 */
void Player::setupB()
{
  memset(queue, 0, sizeof(QueueItem) * PLAYER_QUEUESIZE);
  DBG(Serial.print(F("Player queue size in bytes: ")); Serial.println(sizeof(QueueItem) * PLAYER_QUEUESIZE));

  headphonesPlugged = buttons[BTN_HEADPHONES].isPressed();
  setVolume(headphonesPlugged ? settings.hpVolume : settings.volume);
}

/**
 * Player loop
 */
void Player::loop()
{
  playing = (millis() - playingSince < BUSY_LOCK_MS) || !digitalRead(PLAYER_BUSYPIN);
  headphonesPlugged = buttons[BTN_HEADPHONES].isPressed();
  dfplayer.loop();
}

/**
 * Static callback
 */
void player_onHeadphoneJack(const bool plugged)
{
  if (Player::instance) Player::instance->onHeadphoneJack(plugged);
}

/**
 * Our player class
 */
static Player player;
