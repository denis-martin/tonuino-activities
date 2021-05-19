
#define NUM_BUTTONS 4
Button buttons[NUM_BUTTONS] = { 
  Button(A0), 
  Button(A1), 
  Button(A2),
  Button(A3)
};

bool longPressConsumed[NUM_BUTTONS];

#define BTN_SELECT 0
#define BTN_DOWN 1
#define BTN_UP 2
#define BTN_HEADPHONES 3

#define BTN_YELLOW BTN_SELECT
#define BTN_BLUE   BTN_DOWN
#define BTN_GREEN  BTN_UP

#define LONGPRESS 1000

void buttons_setup()
{
  for (int i = 0; i < NUM_BUTTONS; ++i) {
    longPressConsumed[i] = false;
    buttons[i].begin();
  }
}

void buttons_loop()
{
  // first, read all so we have a new state for all buttons
  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttons[i].read();
  }

  // second, execute actions
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (buttons[i].wasPressed()) {
      lastActivity = millis();
      if (BTN_HEADPHONES == i) {
        player_onHeadphoneJack(true);
        if (currentActivity) currentActivity->onHeadphoneJack(true);

      } else {
        if (currentActivity) currentActivity->onButtonPressed(i);

      }
      
    } else if (buttons[i].wasReleased()) {
      lastActivity = millis();
      if (BTN_HEADPHONES == i) {
        player_onHeadphoneJack(false);
        if (currentActivity) currentActivity->onHeadphoneJack(false);
        
      } else if (!longPressConsumed[i]) {
        if (currentActivity) currentActivity->onButtonReleased(i);
        
      } else {
        longPressConsumed[i] = false;
        
      }
      
    } else if (buttons[i].pressedFor(LONGPRESS) && !longPressConsumed[i]) {
      longPressConsumed[i] = true;
      if (currentActivity) currentActivity->onButtonLongPressed(i);
      
    }
  }
}
