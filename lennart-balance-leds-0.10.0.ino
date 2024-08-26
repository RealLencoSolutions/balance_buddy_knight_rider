#include <Arduino.h>

#include "balance_beeper.cpp"
#include "esc.cpp"

#include <FastLED.h>

#define FLASHING_LED_RED 230
#define FLASHING_LED_GREEN 230
#define FLASHING_LED_BLUE 255

#define CONSTANT_LED_RED 255
#define CONSTANT_LED_GREEN 0
#define CONSTANT_LED_BLUE 0

#define THRESHOLD 5000
#define FAST_DELAY 14
#define SLOW_DELAY 50
#define STARTUP_BRIGHTNESS 30 
#define NORMAL_BRIGHTNESS 255 

#define NUM_LEDS 20 
#define FORWARD_PIN 5
#define REVERSE_PIN 6
#define FORWARD 0
#define REVERSE 1

#define BRAKE_IDLE_THRESHOLD 200
#define BRAKE_THRESHOLD 15
#define BRAKE_ON_DEBOUNCE_COUNT 3 
#define BRAKE_OFF_DEBOUNCE_COUNT 3

CRGB forward_leds[NUM_LEDS];
CRGB reverse_leds[NUM_LEDS];

ESC esc;
BalanceBeeper balanceBeeper;

unsigned long lastKnightRiderUpdate = 0;
unsigned long lastBrakeCheckMillis = 0;
const unsigned long brakeCheckInterval = 50;
unsigned long previousMillis = 0;
unsigned long delayStartTime = 0;
unsigned long delayDuration = 10;
bool isReturnDelayActive = false;

int currentLEDIndex = 0;
int direction = FORWARD;
int animationDirFlag = 1;
int previousErpm = 0;

bool startupState = true; 
bool movingState = false; 
bool isBraking = false;
bool returningToStartup = false;
bool returningToStartupFlag = false;

void knightRider(int red, int green, int blue, int ridingWidth);


void setup() {
 // Serial.begin(115200);

  previousMillis = millis();

  esc.setup();
  balanceBeeper.setup();
  FastLED.addLeds<WS2812B, FORWARD_PIN, GRB>(forward_leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);
  FastLED.addLeds<WS2812B, REVERSE_PIN, GRB>(reverse_leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);

  FastLED.setMaxPowerInVoltsAndMilliamps(5, 1500);
  FastLED.setBrightness(STARTUP_BRIGHTNESS); 
  FastLED.clear(); 

  for (int i = 0; i < NUM_LEDS; i++) {
    forward_leds[i] = CRGB(FLASHING_LED_RED, FLASHING_LED_GREEN, FLASHING_LED_BLUE);
    if (i % 2 == 0) { // Only light up every other LED
        reverse_leds[i] = CRGB(CONSTANT_LED_RED, CONSTANT_LED_GREEN, CONSTANT_LED_BLUE);
    } else {
        reverse_leds[i] = CRGB(0, 0, 0); // Turn off every other LED
    }
  }
  FastLED.show();
}

void startup() {
    // If moving or braking, do not execute the startup logic
  if (movingState || isBraking) {
    return;
  }
  FastLED.setBrightness(STARTUP_BRIGHTNESS);
  for (int i = 0; i < NUM_LEDS; i++) {
    if (direction == FORWARD) {
        forward_leds[i] = CRGB(FLASHING_LED_RED, FLASHING_LED_GREEN, FLASHING_LED_BLUE);
        if (i % 2 == 0) {
            reverse_leds[i] = CRGB(CONSTANT_LED_RED, CONSTANT_LED_GREEN, CONSTANT_LED_BLUE);
        } else {
            reverse_leds[i] = CRGB(0, 0, 0); // Turn off every other LED
        }
    } else { // direction is REVERSE
        reverse_leds[i] = CRGB(FLASHING_LED_RED, FLASHING_LED_GREEN, FLASHING_LED_BLUE);
        if (i % 2 == 0) {
            forward_leds[i] = CRGB(CONSTANT_LED_RED, CONSTANT_LED_GREEN, CONSTANT_LED_BLUE);
        } else {
            forward_leds[i] = CRGB(0, 0, 0); // Turn off every other LED
        }
    }
  }
  FastLED.show();
}

void loop() {
  esc.loop();
  balanceBeeper.loop(esc.dutyCycle, esc.erpm, esc.switchState, esc.voltage);

  // Determine the state and direction
  if (esc.erpm > 200) {
    startupState = false;
    movingState = true;
    direction = FORWARD;
    FastLED.setBrightness(NORMAL_BRIGHTNESS); // Set brightness to normal level
  } else if (esc.erpm < -200) {
    startupState = false;
    movingState = true;
    direction = REVERSE;
    FastLED.setBrightness(NORMAL_BRIGHTNESS); // Set brightness to normal level
  } else { // When ERPM is between -200 and 200
    startupState = true;
    movingState = false;
    delayDuration = SLOW_DELAY; // Set to slow delay when in startup state
    FastLED.setBrightness(STARTUP_BRIGHTNESS); // Set brightness to startup level
  }

  // Additional condition to set fast delay or slow delay based on the threshold
  if (abs(esc.erpm) > THRESHOLD) {
    delayDuration = FAST_DELAY; // Set to fast delay when ERPM is above the threshold
  } else {
    delayDuration = SLOW_DELAY; // Set to slow delay when ERPM is below the threshold
  }

  // Handle the states
  if (startupState) {
    startup(); // Call the startup function when in startup state
  } else if (movingState) {
    knightRider(FLASHING_LED_RED, FLASHING_LED_GREEN, FLASHING_LED_BLUE, 5);
  }

  if (millis() - lastBrakeCheckMillis >= brakeCheckInterval) {
    checkBraking();
    lastBrakeCheckMillis = millis();
  }
}



void checkBraking() {
    static int debounceOnCount = 0;
    static int debounceOffCount = 0;
    int erpmDifference = previousErpm - esc.erpm;

    if ((direction == FORWARD && erpmDifference > BRAKE_THRESHOLD && esc.erpm > BRAKE_IDLE_THRESHOLD) ||
        (direction == REVERSE && erpmDifference < -BRAKE_THRESHOLD && esc.erpm < -BRAKE_IDLE_THRESHOLD)) {
        debounceOnCount++;
        debounceOffCount = 0;
        if (debounceOnCount >= BRAKE_ON_DEBOUNCE_COUNT) {
            isBraking = true;
            debounceOnCount = 0;
        }
    } else {
        debounceOffCount++;
        debounceOnCount = 0;
        if (debounceOffCount >= BRAKE_OFF_DEBOUNCE_COUNT) {
            isBraking = false;
            debounceOffCount = 0;
        }
    }

    previousErpm = esc.erpm;

    CRGB *leds_const = nullptr;
    if (direction == FORWARD) {
        leds_const = reverse_leds;
    } else if (direction == REVERSE) {
        leds_const = forward_leds;
    }

    if (isBraking) {
        // Set all LEDs to braking color when braking
        for (int i = 0; i < NUM_LEDS; i++) {
            leds_const[i].setRGB(CONSTANT_LED_RED, CONSTANT_LED_GREEN, CONSTANT_LED_BLUE);
        }
    } else {
        // Set 50% LEDs to color and the rest off when not braking
        for (int i = 0; i < NUM_LEDS; i++) {
            if (i % 2 == 0) {
                leds_const[i].setRGB(CONSTANT_LED_RED, CONSTANT_LED_GREEN, CONSTANT_LED_BLUE);
            } else {
                leds_const[i] = CRGB(0, 0, 0);
            }
        }
    }

    FastLED.show();
}




void knightRider(int red, int green, int blue, int ridingWidth) {
  CRGB *leds = nullptr;

  if (direction == FORWARD) {
    leds = forward_leds;
  } else if (direction == REVERSE) {
    leds = reverse_leds;
  } else {
    return; // Exit if direction is not FORWARD or REVERSE
  }

  // Fade existing LEDs before setting new ones
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i].fadeToBlackBy(80); // Adjust the fade value as needed
  }

  // Ensure the currentLEDIndex is within bounds
  currentLEDIndex = constrain(currentLEDIndex, 0, NUM_LEDS - 1 - ridingWidth - 2);

  // Check if it's time to update the LEDs
  if (millis() - lastKnightRiderUpdate >= delayDuration) {

    // Update LEDs for the current position
    leds[currentLEDIndex].setRGB(red / 10, green / 10, blue / 10);
    for (int j = 1; j <= ridingWidth; j++) {
      leds[currentLEDIndex + j].setRGB(red, green, blue);
    }
    leds[currentLEDIndex + ridingWidth + 1].setRGB(red / 10, green / 10, blue / 10);

    // Move to the next LED position
    currentLEDIndex += animationDirFlag;

    // Check if the animation is completed
    if (currentLEDIndex >= NUM_LEDS - ridingWidth - 2) {
      // Change the direction
      animationDirFlag = -1;
    } else if (currentLEDIndex < 0) {
      // Change the direction
      animationDirFlag = 1;
    }

    FastLED.show();

    // Save the last update time
    lastKnightRiderUpdate = millis();
  }
}