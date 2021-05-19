unsigned int loopCounter = 0;

/**
 * Reduce power consumption to trigger auto-poweroff of powerbank.
 * 
 * http://discourse.voss.earth/t/intenso-s10000-powerbank-automatische-abschaltung-software-only/805
 */
#define shutdownPin 7
void powerDown()
{
  DBG(Serial.println(F("Powering down")));

  digitalWrite(shutdownPin, LOW);
  delay(500);
  
  keycards_powerDown();
  player.powerDown();

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  cli(); // disable interrupts
  sleep_enable();
  sleep_bod_disable();
  sleep_cpu();
  
  // we will never return here
}

void loop() 
{
  unsigned long looptime = millis();
  
  DBG(
    if (!(loopCounter % 128)) {
      Serial.print(F("a3box loop"));
      Serial.print(F(", avg millis "));
      Serial.print((millis() - timer) / 128);
      setTimer(millis());
      #ifdef MEMDEBUG
      Serial.print(F(", free mem "));
      Serial.print(freeMemory());
      #endif
      Serial.println();
    }
  );

  buttons_loop();
  player.loop();

  if (!(loopCounter % 8)) {
    // we only poll the card reader every nth loop iteration
    // (approx. every 200ms)
    // polling takes about 25ms
    keycards_loop();
  }
  
  if (currentActivity) currentActivity->loop();

  // check for idle time
  if (millis() - lastActivity > (unsigned long) STANDBY_TIME) {
    if (player.isPlaying()) {
      lastActivity = millis();
    } else {
      powerDown();
    }
  }

  loopCounter++; // we don't care about overflows
  
  // we sleep longer if we didn't poll the card reader
  // we will align with the button debounce time
  while (millis() - looptime < 25) {
    sleep_mode(); // returns about every 1ms
  }
}
