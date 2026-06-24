#include <MD_MAX72xx.h>
#include <SPI.h>

#define MAX_DEVICES 4
#define CLK_PIN 11
#define DATA_PIN 12
#define CS_PIN 10

MD_MAX72XX matrix = MD_MAX72XX(MD_MAX72XX::FC16_HW, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

#define BUTTON_PIN 2
#define RESET_PIN 3
#define SPEED_LOW_PIN 8
#define SPEED_MED_PIN 7
#define SPEED_HIGH_PIN 4

#define WIDTH 8
#define HEIGHT 32

enum Speed { LOW_SPEED, MED_SPEED, HIGH_SPEED };
Speed currentSpeed = MED_SPEED;

const int INITIAL_DELAY_LOW = 260;
const int INITIAL_DELAY_MED = 200;
const int INITIAL_DELAY_HIGH = 160;

const int MIN_DELAY_LOW = 90;
const int MIN_DELAY_MED = 50;
const int MIN_DELAY_HIGH = 25;

const int ACCEL_LOW = 5;
const int ACCEL_MED = 10;
const int ACCEL_HIGH = 18;

int currentInitialDelay = INITIAL_DELAY_MED;
int currentMinDelay = MIN_DELAY_MED;
int currentAccel = ACCEL_MED;

int blockPos = 0;
const int INITIAL_BLOCK_WIDTH = 4;
int currentBlockWidth = INITIAL_BLOCK_WIDTH;
int blockRow = 0;
bool blockMoving = true;
int stack[HEIGHT][WIDTH];
int stackHeight = 0;
bool gameOver = false;
unsigned long lastMove = 0;
int moveDelay = 200;
bool moveRight = true;
bool testMode = false;
int testRow = 0;
const bool ROTATE_DISPLAY = true;

const int GAMEOVER_BLINK_COUNT = 25;
const unsigned long GAMEOVER_BLINK_INTERVAL = 300;
int gameOverBlinkStep = 0;
unsigned long lastGameOverBlink = 0;
bool gameOverLedsOn = true;
bool wasGameOver = false;

const unsigned long DEBOUNCE_MS = 50;
unsigned long lastDebounceReset = 0;
bool resetLatch = false;
unsigned long lastDebounceLow = 0, lastDebounceMed = 0, lastDebounceHigh = 0;
bool lowLatch = false, medLatch = false, highLatch = false;
unsigned long lastDebounceButton = 0;
bool buttonLatch = false;

void setup() {
  Serial.begin(9600);

  matrix.begin();
  matrix.clear();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RESET_PIN, INPUT_PULLUP);
  pinMode(SPEED_LOW_PIN, INPUT_PULLUP);
  pinMode(SPEED_MED_PIN, INPUT_PULLUP);
  pinMode(SPEED_HIGH_PIN, INPUT_PULLUP);

  randomSeed(analogRead(A0));

  applySpeed(MED_SPEED);
  resetGame();
}

void applySpeed(Speed speed) {
  currentSpeed = speed;
  switch (speed) {
    case LOW_SPEED:
      currentInitialDelay = INITIAL_DELAY_LOW;
      currentMinDelay = MIN_DELAY_LOW;
      currentAccel = ACCEL_LOW;
      break;
    case HIGH_SPEED:
      currentInitialDelay = INITIAL_DELAY_HIGH;
      currentMinDelay = MIN_DELAY_HIGH;
      currentAccel = ACCEL_HIGH;
      break;
    case MED_SPEED:
    default:
      currentInitialDelay = INITIAL_DELAY_MED;
      currentMinDelay = MIN_DELAY_MED;
      currentAccel = ACCEL_MED;
      break;
  }
}

void resetGame() {
  for (int row = 0; row < HEIGHT; row++) {
    for (int col = 0; col < WIDTH; col++) {
      stack[row][col] = 0;
    }
  }
  currentBlockWidth = INITIAL_BLOCK_WIDTH;
  blockPos = random(0, WIDTH - currentBlockWidth + 1);
  blockRow = 0;
  stackHeight = 0;
  gameOver = false;
  blockMoving = true;
  moveDelay = currentInitialDelay;
  moveRight = true;
  testMode = false;
  testRow = 0;
  gameOverBlinkStep = 0;
  wasGameOver = false;
  updateDisplay();
}

void updateDisplay() {
  matrix.clear();

  if (testMode) {
    for (int col = 0; col < WIDTH; col++) {
      if (ROTATE_DISPLAY) {
        matrix.setPoint(col, testRow, true);
      } else {
        matrix.setPoint(testRow, col, true);
      }
    }
    return;
  }

  for (int row = 0; row < HEIGHT; row++) {
    for (int col = 0; col < WIDTH; col++) {
      if (stack[row][col]) {
        if (ROTATE_DISPLAY) {
          matrix.setPoint(col, row, true);
        } else {
          matrix.setPoint(row, col, true);
        }
      }
    }
  }

  if (!gameOver && blockMoving) {
    for (int col = blockPos; col < blockPos + currentBlockWidth; col++) {
      if (col >= 0 && col < WIDTH) {
        if (ROTATE_DISPLAY) {
          matrix.setPoint(col, blockRow, true);
        } else {
          matrix.setPoint(blockRow, col, true);
        }
      }
    }
  }
}

void dropBlock() {
  blockMoving = false;
  int alignedCount = 0;

  for (int col = blockPos; col < blockPos + currentBlockWidth; col++) {
    if (col >= 0 && col < WIDTH) {
      if (blockRow == 0 || stack[blockRow - 1][col] == 1) {
        stack[blockRow][col] = 1;
        alignedCount++;
      }
    }
  }

  if (alignedCount == 0 && blockRow > 0) {
    gameOver = true;
    return;
  }

  currentBlockWidth = alignedCount;

  if (currentBlockWidth <= 0) {
    gameOver = true;
    return;
  }

  if (blockRow >= stackHeight) {
    stackHeight = blockRow + 1;
  }

  if (stackHeight >= HEIGHT) {
    gameOver = true;
    return;
  }

  blockRow++;
  blockPos = random(0, WIDTH - currentBlockWidth + 1);
  blockMoving = true;

  moveDelay = max(currentMinDelay, moveDelay - currentAccel);
}

bool buttonPressed(int pin, unsigned long &lastDebounce, bool &latch) {
  bool pressedNow = (digitalRead(pin) == LOW);
  if (pressedNow && !latch) {
    if (millis() - lastDebounce > DEBOUNCE_MS) {
      latch = true;
      lastDebounce = millis();
      return true;
    }
  } else if (!pressedNow) {
    latch = false;
    lastDebounce = millis();
  }
  return false;
}

void loop() {
  if (buttonPressed(RESET_PIN, lastDebounceReset, resetLatch)) {
    resetGame();
    return;
  }

  if (buttonPressed(SPEED_LOW_PIN, lastDebounceLow, lowLatch)) {
    applySpeed(LOW_SPEED);
    resetGame();
    return;
  }
  if (buttonPressed(SPEED_MED_PIN, lastDebounceMed, medLatch)) {
    applySpeed(MED_SPEED);
    resetGame();
    return;
  }
  if (buttonPressed(SPEED_HIGH_PIN, lastDebounceHigh, highLatch)) {
    applySpeed(HIGH_SPEED);
    resetGame();
    return;
  }

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't') {
      testMode = !testMode;
      if (testMode) {
        testRow = 0;
      } else {
        resetGame();
      }
    } else if (testMode && c == 'n') {
      testRow = (testRow + 1) % HEIGHT;
    }
    updateDisplay();
  }

  if (testMode) {
    if (buttonPressed(BUTTON_PIN, lastDebounceButton, buttonLatch)) {
      testRow = (testRow + 1) % HEIGHT;
      updateDisplay();
    }
    return;
  }

  if (gameOver) {
    if (!wasGameOver) {
      wasGameOver = true;
      gameOverBlinkStep = 0;
      lastGameOverBlink = millis();
      gameOverLedsOn = true;
    }

    if (gameOverBlinkStep < GAMEOVER_BLINK_COUNT * 2) {
      unsigned long now = millis();
      if (now - lastGameOverBlink >= GAMEOVER_BLINK_INTERVAL) {
        lastGameOverBlink = now;
        gameOverLedsOn = !gameOverLedsOn;
        gameOverBlinkStep++;
        if (gameOverLedsOn) {
          updateDisplay();
        } else {
          matrix.clear();
        }
      }
    } else if (gameOverLedsOn) {
      gameOverLedsOn = false;
      matrix.clear();
    }
    return;
  }
  wasGameOver = false;

  if (blockMoving && millis() - lastMove >= moveDelay) {
    blockPos += moveRight ? 1 : -1;
    if (blockPos <= 0) {
      blockPos = 0;
      moveRight = true;
    } else if (blockPos >= WIDTH - currentBlockWidth) {
      blockPos = WIDTH - currentBlockWidth;
      moveRight = false;
    }
    lastMove = millis();
    updateDisplay();
  }

  if (buttonPressed(BUTTON_PIN, lastDebounceButton, buttonLatch)) {
    dropBlock();
    updateDisplay();
  }
}
