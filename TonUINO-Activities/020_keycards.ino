
/*
 * Arduino Nano supports interrupts only on D2 and D3.
 * In TonUINO, these are used for the DFplayer RX/TX.
 * Thus, only enable this if DFplayer is using different PINs
 */
//#define KEYCARDS_USEIRQ

#define RST_PIN  9        // reset pin: D9
#define SS_PIN  10        // slave-select pin: D10
static MFRC522 mfrc522(SS_PIN, RST_PIN); // create MFRC522
static MFRC522::MIFARE_Key key;  // MIFARE key (we will use the default 0xff...)

/*
 * MIFARE Classic 
 * 
 * Memory organization: Sectors with 4 blocks each, each block has 16 bytes.
 * The last block of each sector contains key information (cannot be used for data)
 * Sector 0, block 0 contains the manufactor block (e.g., the UID).
 * MIFARE Classic 1K: 16 sectors with 4 blocks each
 * MIFARE Classic 4K: 32 sectors with 4 blocks each + 8 sectors with 16 blocks seach
 * 
 * Source: https://www.nxp.com/docs/en/data-sheet/MF1S50YYX_V1.pdf
 * 
 * We are using sector #1, covering block #4 up to and including block #7
 * (which is the trailer block with access information)
 * Max payload size: 3 blocks x 16 bytes = 48 bytes
 * (could be extended if we use additional sectors)
 */
#define MIFARE_CLASSIC_SECTOR       1
#define MIFARE_CLASSIC_DATABLOCK    4
#define MIFARE_CLASSIC_TRAILERBLOCK 7

/*
 * MIFARE Ultralight
 * 
 * Memory organization: 16 pages (0-15), 4 bytes per page
 * Pages 0+1: UID
 * Page 2: Lock configuration
 * Page 3: One-time programmable area / capability container
 * Pages 4-15: Data pages
 * 
 * Source: https://www.nxp.com/docs/en/application-note/AN1303.pdf
 * 
 * We are using page #4 and following.
 * Max payload size: 12 pages x 4 bytes = 48 bytes
 */
#define MIFARE_UL_PAGESIZE 4
#define MIFARE_UL_DATAPAGE_START 4

static byte cardUid[UIDSIZE];        // UID buffer: 6 bytes
static MFRC522::PICC_Type piccType;  // card/tag type (PICC = proximity integrated circuit card)
static bool piccStarted = false;     // did we start communication with a card?

// read/write buffer: 16 bytes plus 2 bytes checksum
#define BUFSIZE_NET 16
#define BUFSIZE     (BUFSIZE_NET+2)
static byte buffer[BUFSIZE];

static_assert(sizeof(CardData) <= BUFSIZE_NET, "Read/write buffer too small!");
static_assert(BUFSIZE_NET % MIFARE_UL_PAGESIZE == 0, "Net buffer size must be a multiple of MIFARE Ultralight page size!");

#ifdef KEYCARDS_USEIRQ
#define IRQ_PIN  2 // interrupt pin: D2
volatile bool newInterrupt = false; // interrupt indicator

/**
 * Handle a new interrupt from the card module
 */
void handleInterrupt()
{
  newInterrupt = true;
}
#endif // KEYCARDS_USEIRQ

/**
 * Start communication with a card/tag (PICC).
 * Return its type.
 */
int keycards_start()
{
  // select one of the PICCs
  if (!mfrc522.PICC_ReadCardSerial()) {
    return 0;
  }
  piccStarted = true;
  
  // show some details of the PICC
  DBG(Serial.print(F("Card UID:")));
  DBG(dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size));
  DBG(Serial.println());

  memset(cardUid, 0, UIDSIZE);
  memcpy(cardUid, mfrc522.uid.uidByte, min(mfrc522.uid.size, UIDSIZE));
  
  DBG(Serial.print(F("PICC type: ")));
  piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  DBG(Serial.println(mfrc522.PICC_GetTypeName(piccType)));

  if (piccType == MFRC522::PICC_TYPE_MIFARE_MINI || piccType == MFRC522::PICC_TYPE_MIFARE_1K || piccType == MFRC522::PICC_TYPE_MIFARE_4K) {
    // MIFARE Classic
    
    // authenticate
    DBG(Serial.println(F("Authenticating using key A...")));
    MFRC522::StatusCode status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, MIFARE_CLASSIC_TRAILERBLOCK, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      DBG(Serial.print(F("PCD_Authenticate() failed: ")));
      DBG(Serial.println(mfrc522.GetStatusCodeName(status)));
      return 0;
    }

  } else if (piccType == MFRC522::PICC_TYPE_MIFARE_UL) {
    // MIFARE Ultralight (nothing to do)

  } else {
    DBG(Serial.println(F("Card type not supported")));
    return 0;
    
  }

  DBG(
    Serial.println(F("Current data in sector:"));
    if (piccType == MFRC522::PICC_TYPE_MIFARE_MINI || piccType == MFRC522::PICC_TYPE_MIFARE_1K || piccType == MFRC522::PICC_TYPE_MIFARE_4K) {
      // MIFARE Classic
      mfrc522.PICC_DumpMifareClassicSectorToSerial(&(mfrc522.uid), &key, MIFARE_CLASSIC_SECTOR);
  
    } else if (piccType == MFRC522::PICC_TYPE_MIFARE_UL) {
      // MIFARE Ultralight  
      mfrc522.PICC_DumpMifareUltralightToSerial();
  
    }
    Serial.println();
  );

  return 1;
}

/**
 * Stop communication with a card/tag (PICC).
 */
void keycards_release()
{
  if (piccStarted) {
    piccStarted = false;

    // Halt PICC (put to sleep)
    mfrc522.PICC_HaltA();
    
    if (piccType == MFRC522::PICC_TYPE_MIFARE_MINI || piccType == MFRC522::PICC_TYPE_MIFARE_1K || piccType == MFRC522::PICC_TYPE_MIFARE_4K) {
      // MIFARE Classic: Stop encryption on PCD
      mfrc522.PCD_StopCrypto1();
  
    } else if (piccType == MFRC522::PICC_TYPE_MIFARE_UL) {
      // MIFARE Ultralight: nothing to do
  
    } 
  }
}

/**
 * Reads payload from the card/tag.
 * 
 * Note: Only a single read (16 bytes) is done. If more data is to be read, 
 * this function needs modification.
 */
int keycards_read()
{
  byte size = sizeof(buffer);
  byte addr = 0;

  if (piccType == MFRC522::PICC_TYPE_MIFARE_MINI || piccType == MFRC522::PICC_TYPE_MIFARE_1K || piccType == MFRC522::PICC_TYPE_MIFARE_4K) {
    // MIFARE Classic
    addr = MIFARE_CLASSIC_DATABLOCK;

  } else if (piccType == MFRC522::PICC_TYPE_MIFARE_UL) {
    // MIFARE Ultralight
    addr = MIFARE_UL_DATAPAGE_START;

  }

  MFRC522::StatusCode status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(addr, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    DBG(Serial.print(F("MIFARE_Read() failed: ")));
    DBG(Serial.println(mfrc522.GetStatusCodeName(status)));
    return 0;
    
  } else {
    DBG(Serial.println(F("Data in block ")));
    DBG(dump_byte_array(buffer, BUFSIZE); Serial.println());
    DBG(Serial.println());
  
    if (memcmp(buffer, magicCode, MAGICCODE_LEN) == 0) {
      const CardData* cardData = (CardData*) buffer;
      DBG(
        Serial.print(F("Version ")); Serial.print(cardData->version); 
        Serial.print(F(", Activity ")); Serial.print(cardData->activity);
        Serial.print(F(", Folder ")); Serial.print(cardData->folder);
        Serial.print(F(", Track ")); Serial.println(cardData->track)
      );
      if (cardData->version == CARDDATA_VERSION) {
        if ((cardData->folder <= 99) &&
            (cardData->track <= 2999))
        {
          if (cardData->activity == ActivityConfig && currentActivity && currentActivity->getActivityCode() != ActivityConfig) {
            switchActivity((ActivityCode) cardData->activity, true, false);
            
          } else {
            if (currentActivity) currentActivity->onNewCard(cardUid, cardData);
            
          }
          
        } else {
          DBG(Serial.println(F("Sanity check failed (assuming new blank card)")));
          if (currentActivity) currentActivity->onNewBlankCard(cardUid);
          
        }
      } else {
        DBG(Serial.print(F("Unsupported payload version ")); Serial.print(cardData->version));
        DBG(Serial.println(F(" (assuming new blank card)")));
        if (currentActivity) currentActivity->onNewBlankCard(cardUid);
        
      }
      
    } else {
      DBG(Serial.println(F("MagicCode does not match (assuming new blank card)")));
      if (currentActivity) currentActivity->onNewBlankCard(cardUid);
      
    }
  }

  return 1;
}

/**
 * Write payload to the card/tag.
 * 
 * Note: Only a single write (16 bytes) is done. If more data is to be written, 
 * this function needs modification.
 */
int keycards_write(CardData *cardData)
{
  int result = 0;
  bool startedHere = false;

  if (!piccStarted) {
    // try to wakeup sleeping cards
    byte size = BUFSIZE;
    MFRC522::StatusCode status = (MFRC522::StatusCode) mfrc522.PICC_WakeupA(buffer, &size);
    if (status != MFRC522::STATUS_OK) {
      DBG(Serial.print(F("PICC_WakeupA() failed: ")));
      DBG(Serial.println(mfrc522.GetStatusCodeName(status)));
      return 0;
      
    } else {
      DBG(Serial.println(F("PICC_WakeupA() successful")));
      
    }
    
    if (!keycards_start()) {
      DBG(Serial.print(F("Cannot start cards")));
      return 0;
      
    }
    startedHere = true;
  }
  
  memcpy(cardData->magicCode, magicCode, MAGICCODE_LEN);
  cardData->version = CARDDATA_VERSION;

  memset(buffer, 0, BUFSIZE);
  memcpy(buffer, cardData, sizeof(CardData));

  DBG(Serial.println(F("Writing data:")));
  dump_byte_array(buffer, BUFSIZE_NET);
  DBG(Serial.println());

  if (piccType == MFRC522::PICC_TYPE_MIFARE_MINI || piccType == MFRC522::PICC_TYPE_MIFARE_1K || piccType == MFRC522::PICC_TYPE_MIFARE_4K) {
    // MIFARE Classic

    MFRC522::StatusCode status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(MIFARE_CLASSIC_DATABLOCK, buffer, BUFSIZE_NET);
    if (status != MFRC522::STATUS_OK) {
      DBG(Serial.print(F("MIFARE_Write() failed: ")));
      DBG(Serial.println(mfrc522.GetStatusCodeName(status)));
      
    } else {
      DBG(Serial.println(F("MIFARE_Write() successful")));
      result = 1;
      
    }

  } else if (piccType == MFRC522::PICC_TYPE_MIFARE_UL) {
    // MIFARE Ultralight
    
    MFRC522::StatusCode status;
    for (int page = 0; page < BUFSIZE_NET/MIFARE_UL_PAGESIZE; page++) {
      status = (MFRC522::StatusCode) mfrc522.MIFARE_Ultralight_Write(MIFARE_UL_DATAPAGE_START+page, &buffer[page*MIFARE_UL_PAGESIZE], MIFARE_UL_PAGESIZE);
      if (status != MFRC522::STATUS_OK) {
        DBG(Serial.print(F("MIFARE_Ultralight_Write() failed: ")));
        DBG(Serial.println(mfrc522.GetStatusCodeName(status)));
        
      } else {
        DBG(Serial.println(F("MIFARE_Ultralight_Write() successful")));
        result = 1;
        
      }
    }

  }

  if (startedHere) {
    keycards_release();
  }
  return result;
}

inline void keycards_powerDown()
{
  mfrc522.PCD_AntennaOff();
  //mfrc522.PCD_SoftPowerDown();
  
  // hard power down
  pinMode(RST_PIN, OUTPUT);     // set reset PIN as output
  digitalWrite(RST_PIN, HIGH);  // make sure we have a clean HIGH state
  delay(1);                     // 8.8.1 Reset timing requirements says about 100ns. Let us be generous: 1ms
  digitalWrite(RST_PIN, LOW);   // power down
}

/**
 * Setup PCD
 */
void keycards_setup() 
{
  // initialize RFID reader
  SPI.begin();        // init SPI bus
  mfrc522.PCD_Init(); // init MFRC522
  DBG(mfrc522.PCD_DumpVersionToSerial());
  
  // Prepare the key (used both as key A and as key B)
  // using 0xFFFFFFFFFFFF which is the default at chip delivery from the factory
  for (byte i = 0; i < MFRC522::MF_KEY_SIZE; i++) {
      key.keyByte[i] = 0xFF;
  }

#ifdef KEYCARDS_USEIRQ
  // setup the IRQ pin
  pinMode(IRQ_PIN, INPUT_PULLUP);

  // select which IRQ to propagate to the IRQ pin
  mfrc522.PCD_WriteRegister(mfrc522.ComIEnReg, 0xA0); // select RX IRQ

  // activate the interrupt
  attachInterrupt(digitalPinToInterrupt(IRQ_PIN), handleInterrupt, FALLING);

  // clear a spurious interrupt at start
  while (!newInterrupt) {};
  newInterrupt = false;
#endif
}

/**
 * Poll PCD for new PICCs
 */
void keycards_loop() 
{
#ifdef KEYCARDS_USEIRQ
  if (!newInterrupt) {
    // keep sending the needed commands to activate the reception
    mfrc522.PCD_WriteRegister(mfrc522.FIFODataReg, mfrc522.PICC_CMD_REQA);
    mfrc522.PCD_WriteRegister(mfrc522.CommandReg, mfrc522.PCD_Transceive);
    mfrc522.PCD_WriteRegister(mfrc522.BitFramingReg, 0x87);
    return;
  }
  
  DBG(Serial.println(F("Got an interrupt!")));

  // clear pending interrupt
  mfrc522.PCD_WriteRegister(mfrc522.ComIrqReg, 0x7F);
  newInterrupt = false;
#else
  // Reset the loop if no new card present on the sensor/reader. 
  // This saves the entire process when idle.
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
#endif

  if (keycards_start()) {
    DBG(Serial.print(F("Reading card was ")));
    if (keycards_read()) {
      DBG(Serial.println(F("successful")));
    } else {
      DBG(Serial.println(F("unsuccessful")));
    }

    lastActivity = millis();
  }

  keycards_release();
}
