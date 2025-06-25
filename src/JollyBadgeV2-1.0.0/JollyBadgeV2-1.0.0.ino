#include <Adafruit_NeoPixel.h>
#include <FlashAsEEPROM.h>
#include <Wire.h>
#include <ST25DVSensor.h>
#include "LIS3DHTR.h"

/*
  ==========================================
  HARDWARE CONFIGURATION & CONSTANTS
  ==========================================
*/

// Hardware Pin Configuration
#define NEOPIXEL_OUTER_PIN    1
#define NEOPIXEL_INNER_PIN    0
#define NFC_GPIO_PIN         199
#define NFC_LPD_PIN          198
#define ACCEL_ADDRESS        0x19

// NeoPixel Configuration
#define NEOPIXEL_OUTER_COUNT  24
#define NEOPIXEL_INNER_COUNT  8
#define INNER_BRIGHTNESS      10
#define OUTER_BRIGHTNESS      50

// Timing Constants (all in milliseconds)
const unsigned long ANIMATION_INTERVAL = 16;
const unsigned long NFC_CHECK_INTERVAL = 1000;
const unsigned long HOT_COLD_INTERVAL = 50;
const unsigned long CEREAL_INTERVAL = 5000;
const unsigned long ACCEL_DELAY = 33;

// Animation Constants
const int FLASH_INTERVAL = 125;
const int FLASH_COUNT = 10;
const int MAX_BRIGHTNESS = 255;
const int FADE_STEP = 5;
const int CHASE_LENGTH = 3;
const int SWEEP_LENGTH = 3;
const int PROGRESS_INTERVAL = 50;
const int PROGRESS_FADE_STEP = 15;

// Temperature Constants
const int TEMPERATURE_DELTA = 2;
const int TEMP_BRIGHTNESS_MIN = 128;
const int TEMP_BRIGHTNESS_MAX = 255;

// Morse Code Constants
const unsigned long MORSE_OFFSET = 250;
const unsigned long MORSE_DOT_LENGTH = 1;
const unsigned long MORSE_DASH_LENGTH = 3;
const unsigned long MORSE_CHAR_LENGTH = 1;
const unsigned long MORSE_LETTER_LENGTH = 2;
const unsigned long MORSE_WORD_LENGTH = 6;
const unsigned long MORSE_REPEAT_DELAY = 2000;

// Color Definitions
const uint8_t COLOR_RED[3] = {255, 0, 0};
const uint8_t COLOR_GREEN[3] = {0, 255, 0};
const uint8_t COLOR_BLUE[3] = {0, 0, 255};
const uint8_t COLOR_WHITE[3] = {255, 255, 255};
const uint8_t COLOR_PURPLE[3] = {255, 0, 255};
const uint8_t COLOR_OFF[3] = {0, 0, 0};

/*
  ==========================================
  ENUMS & DATA STRUCTURES
  ==========================================
*/

enum GameState {
  BOOTING,
  PUZZLE_FIND_NFC,
  PUZZLE_FIND_SOURCE,
  PUZZLE_MORSE_CODE,
  PUZZLE_BINARY_CODE,
  PUZZLE_HOT_COLD,
  PUZZLE_CEREAL, 
  PUZZLE_EXTRACT,
  COMPLETED
};

enum AnimationState {
  ANIM_OFF,
  ANIM_SUCCESS,
  ANIM_FAILURE,
  ANIM_COUNTDOWN,
  ANIM_INNER_PULSE,
  ANIM_OUTER_CHASE,
  ANIM_MORSE_CODE,
  ANIM_BINARY,
  ANIM_HOT_COLD,
  ANIM_RAINBOW,
  ANIM_CEREAL,
  ANIM_EXTRACT,
  ANIM_COMPLETE
};

struct PuzzleConfig {
  const char* name;
  const char* hint;
  const char* flag;
  AnimationState animation;
};

struct ColorRGB {
  uint8_t r, g, b;
  ColorRGB(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0) : r(red), g(green), b(blue) {}
};

struct AnimationState_t {
  AnimationState current;
  AnimationState next;
  bool changePending;
  bool shouldBlock;
  bool isComplete;
  unsigned long lastUpdate;
  int stepCounter;
};

struct TemperatureState {
  uint16_t first;
  uint16_t current;
  uint16_t highTarget;
  uint16_t lowTarget;
  bool highSuccess;
  bool lowSuccess;
};

/*
  ==========================================
  GLOBAL VARIABLES
  ==========================================
*/

// Hardware Objects
LIS3DHTR<TwoWire> accelerometer;
Adafruit_NeoPixel outerRing(NEOPIXEL_OUTER_COUNT, NEOPIXEL_OUTER_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel innerRing(NEOPIXEL_INNER_COUNT, NEOPIXEL_INNER_PIN, NEO_GRB + NEO_KHZ800);
ST25DV nfcTag = ST25DV(NFC_GPIO_PIN, NFC_LPD_PIN, &Wire);

// Game State
GameState gameState = BOOTING;
String currentHint = "";
String currentFlag = "";
bool puzzleOverrideSolved = false;

// Animation State
AnimationState_t animState = {ANIM_OFF, ANIM_OFF, false, false, true, 0, 0};

// Timing Variables
unsigned long globalTime = 0;
unsigned long nfcCheckTimer = 0;
unsigned long hotColdTimer = 0;
unsigned long cerealTimer = 0;
unsigned long flashStepTimer = 0;
unsigned long progressTimer = 0;
int progressStepCounter = 0;

// Temperature State
TemperatureState tempState = {0, 0, 0, 0, false, false};

// Morse Code State
const char MORSE_STRING[] = "-- --- .-. ... . -- .- ... - . .-. ";
unsigned long morseTimer = 0;
unsigned long morseThreshold = 0;
uint8_t morseIndex = 0;
bool morseTick = false;

// Binary Code Configuration
const int BINARY_GROUPS[8][3] = {
  {0,1,2}, {3,4,5}, {6,7,8}, {9,10,11}, 
  {12,13,14}, {15,16,17}, {18,19,20}, {21,22,23}
};
const char* binaryString = "THISISABINARYFLAG";

// Rainbow Colors for Animations
const ColorRGB RAINBOW_COLORS[] = {
  ColorRGB(255, 0, 0),    // Red
  ColorRGB(255, 165, 0),  // Orange
  ColorRGB(255, 255, 0),  // Yellow
  ColorRGB(0, 255, 0),    // Green
  ColorRGB(0, 0, 255),    // Blue
  ColorRGB(75, 0, 130),   // Indigo
  ColorRGB(238, 130, 238) // Violet
};
const int RAINBOW_COLOR_COUNT = sizeof(RAINBOW_COLORS) / sizeof(RAINBOW_COLORS[0]);

// Temperature Display Configuration
const int TEMP_TOP_PIXELS[] = {19, 20, 21, 22, 23, 0, 1, 2, 3, 4, 5};
const int TEMP_BOTTOM_PIXELS[] = {7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
const int TEMP_TOP_COUNT = sizeof(TEMP_TOP_PIXELS) / sizeof(TEMP_TOP_PIXELS[0]);
const int TEMP_BOTTOM_COUNT = sizeof(TEMP_BOTTOM_PIXELS) / sizeof(TEMP_BOTTOM_PIXELS[0]);
const int TEMP_OFF_PIXELS[] = {6, 18};

// Puzzle Configuration Table
const PuzzleConfig PUZZLE_CONFIGS[] = {
  {"Find NFC", "Go to www.getjollybadge.com/get-started", "HACKTHEPLANET", ANIM_INNER_PULSE},
  {"Find Source", "Go back and dig deeper", "SOURCEWIZARD", ANIM_OUTER_CHASE},
  {"Morse Code", "Follow the beats and the breaks", "MORSEMASTER", ANIM_MORSE_CODE},
  {"Binary Code", "Two values, infinite meanings.", "BINARYBADDIE", ANIM_BINARY},
  {"Hot/Cold", "Seek balance in extremes.", "ICYHOT", ANIM_HOT_COLD},
  {"Cereal", "Cereal is quite delicious.", "2388", ANIM_CEREAL},
  {"Extract", "Deep within lies the hidden truth.", "EXTRACTORREACTOR", ANIM_EXTRACT},
  {"Complete", "ALL PUZZLES COMPLETE!", "CANTSTOPWONTSTOP", ANIM_RAINBOW}
};

/*
  ==========================================
  UTILITY FUNCTIONS
  ==========================================
*/

uint32_t createColor(const ColorRGB& color, bool isOuter = true) {
  return isOuter ? outerRing.Color(color.r, color.g, color.b) 
                 : innerRing.Color(color.r, color.g, color.b);
}

uint32_t createColor(uint8_t r, uint8_t g, uint8_t b, bool isOuter = true) {
  return isOuter ? outerRing.Color(r, g, b) : innerRing.Color(r, g, b);
}

void clearAllPixels() {
  outerRing.clear();
  innerRing.clear();
  outerRing.show();
  innerRing.show();
}

void setAllPixels(const ColorRGB& color, bool outer = true, bool inner = true) {
  if (outer) {
    for (int i = 0; i < NEOPIXEL_OUTER_COUNT; i++) {
      outerRing.setPixelColor(i, createColor(color, true));
    }
    outerRing.show();
  }
  
  if (inner) {
    for (int i = 0; i < NEOPIXEL_INNER_COUNT; i++) {
      innerRing.setPixelColor(i, createColor(color, false));
    }
    innerRing.show();
  }
}

void setPixelRange(const int pixels[], int count, const ColorRGB& color) {
  for (int i = 0; i < count; i++) {
    outerRing.setPixelColor(pixels[i], createColor(color));
  }
}

bool isTimerReady(unsigned long& timer, unsigned long interval) {
  if (globalTime - timer >= interval) {
    timer = globalTime;
    return true;
  }
  return false;
}

int calculateProgress(float value, float min, float max, int maxSteps) {
  float progress = constrain((value - min) / (max - min), 0.0, 1.0);
  return progress * maxSteps;
}

void logStateChange(const char* type, int state) {
  // SerialUSB.print(F("Setting "));
  // SerialUSB.print(type);
  // SerialUSB.print(F(" state: "));
  // SerialUSB.println(state);
}

/*
  ==========================================
  ANIMATION FUNCTIONS
  ==========================================
*/

void animateFlash(bool success) {
  const int maxFlashes = FLASH_COUNT * 2;
  bool ledsOn = (animState.stepCounter % 2 == 0);
  ColorRGB color = success ? ColorRGB(COLOR_GREEN[0], COLOR_GREEN[1], COLOR_GREEN[2]) 
                           : ColorRGB(COLOR_RED[0], COLOR_RED[1], COLOR_RED[2]);

  if (ledsOn) {
    setAllPixels(color);
  } else {
    clearAllPixels();
  }

  if (isTimerReady(flashStepTimer, FLASH_INTERVAL)) {
    animState.stepCounter++;
  }

  if (animState.stepCounter >= maxFlashes) {
    animState.stepCounter = 0;
    animState.isComplete = true;
  }
}

void animateInnerPulse() {
  int brightness = animState.stepCounter * FADE_STEP;

  if (brightness <= MAX_BRIGHTNESS) {
    // Fade in both rings
    ColorRGB color(brightness, brightness, brightness);
    setAllPixels(color);
  } else if (brightness <= MAX_BRIGHTNESS * 2) {
    // Outer ring fades out
    int outerBrightness = MAX_BRIGHTNESS * 2 - brightness;
    setAllPixels(ColorRGB(outerBrightness, outerBrightness, outerBrightness), true, false);
    setAllPixels(ColorRGB(MAX_BRIGHTNESS, MAX_BRIGHTNESS, MAX_BRIGHTNESS), false, true);
  } else if (brightness <= MAX_BRIGHTNESS * 3) {
    // Inner ring fades out
    int innerBrightness = MAX_BRIGHTNESS * 3 - brightness;
    outerRing.clear();
    outerRing.show();
    setAllPixels(ColorRGB(innerBrightness, innerBrightness, innerBrightness), false, true);
  }

  animState.stepCounter++;

  if (brightness >= MAX_BRIGHTNESS * 3) {
    animState.stepCounter = 0;
    animState.isComplete = true;
  }
}

void animateChasingLights() {
  outerRing.clear();
  
  for (int i = 0; i < CHASE_LENGTH; i++) {
    int pixelIndex = (animState.stepCounter + i) % NEOPIXEL_OUTER_COUNT;
    outerRing.setPixelColor(pixelIndex, createColor(ColorRGB(COLOR_RED[0], COLOR_RED[1], COLOR_RED[2])));
  }
  
  outerRing.show();
  animState.stepCounter = (animState.stepCounter + 1) % NEOPIXEL_OUTER_COUNT;
  animState.isComplete = true; // Continuous animation
}

void animateMorseCode() {
  if (!isTimerReady(morseTimer, morseThreshold)) {
    animState.isComplete = true;
    return;
  }

  char character = MORSE_STRING[morseIndex];

  if (morseTick) {
    morseTick = false;
    outerRing.clear();
    outerRing.show();
    morseThreshold = MORSE_CHAR_LENGTH * MORSE_OFFSET;
    animState.isComplete = true;
    return;
  }

  if (character == '.' || character == '-') {
    setAllPixels(ColorRGB(COLOR_WHITE[0], COLOR_WHITE[1], COLOR_WHITE[2]), true, false);
    morseThreshold = (character == '.') ? MORSE_DOT_LENGTH * MORSE_OFFSET 
                                        : MORSE_DASH_LENGTH * MORSE_OFFSET;
  } else if (character == ' ') {
    outerRing.clear();
    outerRing.show();
    morseThreshold = MORSE_LETTER_LENGTH * MORSE_OFFSET;
  } else if (character == '/') {
    outerRing.clear();
    outerRing.show();
    morseThreshold = MORSE_WORD_LENGTH * MORSE_OFFSET;
  }

  if (morseIndex + 1 < strlen(MORSE_STRING)) {
    morseIndex++;
    morseTick = true;
  } else {
    outerRing.clear();
    outerRing.show();
    morseIndex = 0;
    morseTick = false;
    morseThreshold = MORSE_REPEAT_DELAY;
  }

  animState.isComplete = true;
}

void animateBinary() {
  static int fadeDirection = 1;
  static int brightness = 0;

  int stringLength = strlen(binaryString);
  int currentCharIndex = animState.stepCounter / (2 * (MAX_BRIGHTNESS / FADE_STEP));

  if (currentCharIndex >= stringLength) {
    animState.stepCounter = 0;
    animState.isComplete = true;
    return;
  }

  char currentChar = binaryString[currentCharIndex];
  uint8_t binaryValue = (uint8_t)currentChar;

  outerRing.clear();

  for (int bitIndex = 0; bitIndex < 8; bitIndex++) {
    if (binaryValue & (1 << (7 - bitIndex))) {
      for (int j = 0; j < 3; j++) {
        int pixelIndex = BINARY_GROUPS[bitIndex][j];
        outerRing.setPixelColor(pixelIndex, createColor(brightness, 0, brightness));
      }
    }
  }

  outerRing.show();

  brightness += FADE_STEP * fadeDirection;

  if (brightness >= MAX_BRIGHTNESS) {
    fadeDirection = -1;
  } else if (brightness <= 0) {
    fadeDirection = 1;
    brightness = 0;
    animState.stepCounter += 2 * (MAX_BRIGHTNESS / FADE_STEP);
  }

  animState.isComplete = true;
}

void animateTemperatureProgress() {
  float topProgress = (float)(tempState.current - tempState.lowTarget) / 
                     (tempState.highTarget - tempState.lowTarget);
  float bottomProgress = (float)(tempState.highTarget - tempState.current) / 
                        (tempState.highTarget - tempState.lowTarget);

  topProgress = constrain(topProgress, 0.0, 1.0);
  bottomProgress = constrain(bottomProgress, 0.0, 1.0);

  int topPixels = topProgress * TEMP_TOP_COUNT;
  int bottomPixels = bottomProgress * TEMP_BOTTOM_COUNT;

  outerRing.clear();

  // Set top half (red/green)
  for (int i = 0; i < TEMP_TOP_COUNT; i++) {
    if (i < topPixels) {
      int brightness = map(i, 0, max(topPixels - 1, 0), TEMP_BRIGHTNESS_MIN, TEMP_BRIGHTNESS_MAX);
      ColorRGB color = tempState.highSuccess ? ColorRGB(COLOR_GREEN[0], COLOR_GREEN[1], COLOR_GREEN[2]) 
                                             : ColorRGB(brightness, 0, 0);
      outerRing.setPixelColor(TEMP_TOP_PIXELS[i], createColor(color));
    }
  }

  // Set bottom half (blue/green)
  for (int i = 0; i < TEMP_BOTTOM_COUNT; i++) {
    if (i < bottomPixels) {
      int brightness = map(i, 0, max(bottomPixels - 1, 0), TEMP_BRIGHTNESS_MIN, TEMP_BRIGHTNESS_MAX);
      ColorRGB color = tempState.lowSuccess ? ColorRGB(COLOR_GREEN[0], COLOR_GREEN[1], COLOR_GREEN[2]) 
                                            : ColorRGB(0, 0, brightness);
      outerRing.setPixelColor(TEMP_BOTTOM_PIXELS[i], createColor(color));
    }
  }

  // Ensure specific pixels are off
  for (int i = 0; i < 2; i++) {
    outerRing.setPixelColor(TEMP_OFF_PIXELS[i], createColor(ColorRGB(COLOR_OFF[0], COLOR_OFF[1], COLOR_OFF[2])));
  }

  outerRing.show();
  animState.isComplete = true;
}

void animateRainbow() {
  int startIndex = animState.stepCounter % NEOPIXEL_OUTER_COUNT;

  for (int i = 0; i < NEOPIXEL_OUTER_COUNT; i++) {
    int colorIndex = (i + startIndex) % RAINBOW_COLOR_COUNT;
    int pixelIndex = (i + startIndex) % NEOPIXEL_OUTER_COUNT;
    outerRing.setPixelColor(pixelIndex, createColor(RAINBOW_COLORS[colorIndex]));
  }

  outerRing.show();
  animState.stepCounter = (animState.stepCounter + 1) % NEOPIXEL_OUTER_COUNT;
}

void animateExtract() {
  int currentPos = animState.stepCounter % NEOPIXEL_OUTER_COUNT;
  int direction = (animState.stepCounter / NEOPIXEL_OUTER_COUNT) % 2 == 0 ? 1 : -1;

  outerRing.clear();

  for (int i = 0; i < SWEEP_LENGTH; i++) {
    int pixelIndex = (currentPos + direction * i) % NEOPIXEL_OUTER_COUNT;
    if (pixelIndex < 0) pixelIndex += NEOPIXEL_OUTER_COUNT;
    
    int colorIndex = (currentPos / SWEEP_LENGTH + i) % RAINBOW_COLOR_COUNT;
    outerRing.setPixelColor(pixelIndex, createColor(RAINBOW_COLORS[colorIndex]));
  }

  outerRing.show();
  animState.stepCounter++;
  animState.isComplete = true;

  if (animState.stepCounter >= 2 * NEOPIXEL_OUTER_COUNT) {
    animState.stepCounter = 0;
  }
}

void animateCereal() {
  int brightness = animState.stepCounter * FADE_STEP;

  if (brightness <= MAX_BRIGHTNESS) {
    setAllPixels(ColorRGB(brightness, brightness, brightness), true, false);
  } else if (brightness <= MAX_BRIGHTNESS * 2) {
    int fadedBrightness = MAX_BRIGHTNESS * 2 - brightness;
    setAllPixels(ColorRGB(fadedBrightness, fadedBrightness, fadedBrightness), true, false);
  }

  animState.stepCounter++;

  if (brightness >= MAX_BRIGHTNESS * 2) {
    animState.stepCounter = 0;
    animState.isComplete = true;
  }
}

void updateProgressAnimation(int progressIndex) {
  if (!isTimerReady(progressTimer, PROGRESS_INTERVAL)) return;
  
  int brightness = (progressStepCounter % (MAX_BRIGHTNESS / PROGRESS_FADE_STEP)) * PROGRESS_FADE_STEP;

  for (int i = 0; i < NEOPIXEL_INNER_COUNT; i++) {
    ColorRGB color;
    if (i < progressIndex) {
      color = ColorRGB(COLOR_WHITE[0], COLOR_WHITE[1], COLOR_WHITE[2]);
    } else if (i == progressIndex) {
      color = ColorRGB(brightness, brightness, brightness);
    } else {
      color = ColorRGB(COLOR_OFF[0], COLOR_OFF[1], COLOR_OFF[2]);
    }
    innerRing.setPixelColor(i, createColor(color, false));
  }

  innerRing.show();
  progressStepCounter++;
}

/*
  ==========================================
  ANIMATION STATE MANAGEMENT
  ==========================================
*/

void setAnimationState(AnimationState nextState) {
  logStateChange("animation", nextState);

  if (!animState.shouldBlock || animState.isComplete) {
    animState.current = nextState;
    animState.next = nextState;
    animState.stepCounter = 0;
    animState.isComplete = false;
    animState.changePending = false;
  } else {
    animState.next = nextState;
    animState.changePending = true;
  }
}

void performAnimation() {
  if (!isTimerReady(animState.lastUpdate, ANIMATION_INTERVAL)) return;

  switch (animState.current) {
    case ANIM_OFF:           clearAllPixels(); break;
    case ANIM_SUCCESS:       animateFlash(true); break;
    case ANIM_FAILURE:       animateFlash(false); break;
    case ANIM_COUNTDOWN:     break;
    case ANIM_INNER_PULSE:   animateInnerPulse(); break;
    case ANIM_OUTER_CHASE:   animateChasingLights(); break;
    case ANIM_MORSE_CODE:    animateMorseCode(); break;
    case ANIM_BINARY:        animateBinary(); break;
    case ANIM_HOT_COLD:      animateTemperatureProgress(); break;
    case ANIM_RAINBOW:       animateRainbow(); break;
    case ANIM_CEREAL:        animateCereal(); break;
    case ANIM_EXTRACT:       animateExtract(); break;
    case ANIM_COMPLETE:      break;
  }

  // Update progress animation for active puzzles
  if (gameState != BOOTING && gameState != PUZZLE_FIND_NFC && 
      animState.current != ANIM_SUCCESS && animState.current != ANIM_FAILURE) {
    int progressIndex = (int)gameState < 5 ? (int)gameState - 1 : (int)gameState;
    updateProgressAnimation(progressIndex);
  }

  if (animState.isComplete && animState.shouldBlock && animState.changePending) {
    setAnimationState(animState.next);
  }
}

/*
  ==========================================
  PUZZLE IMPLEMENTATIONS
  ==========================================
*/

void updateHotColdPuzzle() {
  if (!isTimerReady(hotColdTimer, HOT_COLD_INTERVAL)) return;

  tempState.current = accelerometer.getTemperature();

  if (tempState.current >= tempState.highTarget) {
    tempState.highSuccess = true;
  }
  if (tempState.current <= tempState.lowTarget) {
    tempState.lowSuccess = true;
  }
  if (tempState.highSuccess && tempState.lowSuccess) {
    puzzleOverrideSolved = true;
  }
}

void updateCerealPuzzle() {
  if (isTimerReady(cerealTimer, CEREAL_INTERVAL)) {
    SerialUSB.println(F("How many miles did JollyBadge [V2] travel to DEF CON?"));
  }
}

void performCurrentPuzzle() {
  switch (gameState) {
    case PUZZLE_HOT_COLD: updateHotColdPuzzle(); break;
    case PUZZLE_CEREAL:   updateCerealPuzzle(); break;
    default: break; // Most puzzles are passive
  }
}

/*
  ==========================================
  GAME STATE MANAGEMENT
  ==========================================
*/

void setGameState(GameState nextState) {
  logStateChange("game", nextState);
  gameState = nextState;
  EEPROM.write(0, nextState);
  EEPROM.commit();
}

void initializePuzzleState() {
  clearAllPixels();

  if (gameState >= PUZZLE_FIND_NFC && gameState <= COMPLETED) {
    int puzzleIndex = gameState - PUZZLE_FIND_NFC;
    const PuzzleConfig& config = PUZZLE_CONFIGS[puzzleIndex];
    
    // SerialUSB.print(F("Puzzle Init: "));
    // SerialUSB.println(config.name);
    
    currentHint = config.hint;
    currentFlag = config.flag;
    setAnimationState(config.animation);

    // Special initialization for specific puzzles
    if (gameState == PUZZLE_BINARY_CODE) {
      binaryString = "BINARYBADDIE";
    } else if (gameState == PUZZLE_HOT_COLD) {
      tempState.first = accelerometer.getTemperature();
      tempState.current = tempState.first;
      tempState.highTarget = tempState.current + TEMPERATURE_DELTA;
      tempState.lowTarget = tempState.current - TEMPERATURE_DELTA;
      tempState.highSuccess = false;
      tempState.lowSuccess = false;
    }
  } else {
    // SerialUSB.println(F("GameState INIT: Unknown Game State!"));
    setAnimationState(ANIM_RAINBOW);
  }
}

void nextState() {
  GameState nextState = (GameState)(gameState + 1);
  setGameState(nextState);
  initializePuzzleState();

  if (nfcTag.writeText(currentHint)) {
    // SerialUSB.println(F("NFC Write Failed!"));
    while(1);
  }
}

bool checkForSolution() {
  if (!isTimerReady(nfcCheckTimer, NFC_CHECK_INTERVAL)) return false;

  String nfcString = "";
  if (nfcTag.readText(&nfcString)) {
    // SerialUSB.println(F("Read NFC Failed!"));
    // SerialUSB.print(F("NDEF Type: "));
    // SerialUSB.println(nfcTag.readNDEFType());
    return false;
  }

  if (puzzleOverrideSolved) {
    puzzleOverrideSolved = false;
    nfcString = currentFlag;
  }

  if (nfcString == currentHint) return false;

  if (nfcString == currentFlag) {
    puzzleOverrideSolved = false;
    animState.shouldBlock = true;
    setAnimationState(ANIM_SUCCESS);
    return true;
  } else {
    AnimationState returnState = animState.current;
    animState.shouldBlock = true;
    setAnimationState(ANIM_FAILURE);
    setAnimationState(returnState);

    if (nfcTag.writeText(currentHint)) {
      // SerialUSB.println(F("NFC Write Failed!"));
      while(1);
    }
  }
  return false;
}

/*
  ==========================================
  SETUP & MAIN LOOP
  ==========================================
*/

void setup() {
  SerialUSB.begin(115200);
  delay(500);
  SerialUSB.println(F("Booting JollyBadge V2..."));

  // Initialize I2C
  SerialUSB.println(F("Setting up I2C..."));
  Wire.begin();
  delay(250);
  
  // Initialize NeoPixels
  SerialUSB.println(F("Setting up NeoPixels..."));
  outerRing.begin();
  outerRing.setBrightness(OUTER_BRIGHTNESS);
  innerRing.begin();
  innerRing.setBrightness(INNER_BRIGHTNESS);

  // Initialize Accelerometer
  SerialUSB.println(F("Setting up Accelerometer..."));
  accelerometer.begin(Wire, ACCEL_ADDRESS);
  accelerometer.openTemp();
  delay(100);
  accelerometer.setOutputDataRate(LIS3DHTR_DATARATE_50HZ);
  accelerometer.setHighSolution(true);

  // Initialize NFC
  SerialUSB.println(F("Setting up NFC..."));
  if (nfcTag.begin() != 0) {
    SerialUSB.println(F("NFC Failed to initialize!"));
    while(1);
  }

  String bootMessage = "H3y 1'm b00t1n' h3r3!";
  if (nfcTag.writeText(bootMessage)) {
    SerialUSB.println(F("Initial NFC Write Failed!"));
    while(1);
  }

  // Initialize game state
  if (!EEPROM.isValid()) {
    SerialUSB.println(F("EEPROM empty, starting new game."));
  } else {
    SerialUSB.println(F("Restoring game state from EEPROM..."));
    GameState storedState = (GameState)EEPROM.read(0);
    gameState = (GameState)(storedState - 1);
  }

  nextState();
}

void loop() {
  globalTime = millis();

  if (checkForSolution()) {
    SerialUSB.println(F("FLAG CAPTURED. ADVANCING TO NEXT PUZZLE."));
    nextState();
  } else {
    performCurrentPuzzle();
  }

  performAnimation();
}