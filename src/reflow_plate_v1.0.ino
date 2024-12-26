#include <PID_v1.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <stdint.h>
#include <max6675.h>
#include "Generic.h"              // Include the header file for generics

//-------------------------------------------------//
// Thermocouple MAX6675
//-------------------------------------------------//
MAX6675 thermocouple(MAX6675_CLK, MAX6675_CS, MAX6675_DO);
float temp = 0.0;
//-------------------------------------------------//
// Touch Controller
//-------------------------------------------------//
Adafruit_FT6206 ts = Adafruit_FT6206();
//-------------------------------------------------//
// TFT Screen Controller
//-------------------------------------------------//
Adafruit_ILI9341 tft = Adafruit_ILI9341(ILI9341_CS, ILI9341_DC, ILI9341_MOSI, ILI9341_SCLK, ILI9341_RST, ILI9341_MISO);
//-------------------------------------------------//
// PID Controller
//-------------------------------------------------//
// Specify the coefficients and initial tuning parameters for PID
double Kp = 2, Ki = 5, Kd = 1;
// Define Variables we'll be connecting to
double Setpoint, Input, Output;
PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT);
//-------------------------------------------------//
// Variables
//-------------------------------------------------//
// Grid drawing:
const int displayWidth = 320, displayHeight = 240;
const int gridSize = 53;
// Screen enbablers
bool mainMenu = false, editMenu = false, reflowMenu = false, pidMenu = false;
// Debouncing for touch buttons 
const int touchHoldLimit = 250;
// Timer
unsigned long timeSinceReflowStarted;
// Check temperature every 1sec
unsigned long timeTempCheck = 1000;
unsigned long lastTimeTempCheck = 0;
// Defaults
double preheatTemp = 180, soakTemp = 150, reflowTemp = 230, cooldownTemp = 25;
unsigned long preheatTime = 120000, soakTime = 60000, reflowTime = 120000, cooldownTime = 120000, totalTime = preheatTime + soakTime + reflowTime + cooldownTime;
// States
bool preheating = false, soaking = false, reflowing = false, coolingDown = false, newState = false;
// Theme colors
uint16_t gridColor = 0x7BEF;
uint16_t preheatColor = 0xF800, soakColor = 0xFBE0, reflowColor = 0xDEE0, cooldownColor = 0x4A7E;          // colors for plotting
uint16_t preheatColor_d = 0xC000, soakColor_d = 0xC2E0, reflowColor_d = 0xC600, cooldownColor_d = 0x0018;  // desaturated colors

void buzzerBeep(int times, int duration)
{
  for (int i= 0; i<times; i++){
    digitalWrite(BUZZER_PIN, HIGH);  // turn the beep on
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);  // turn the beep off
    delay(duration);
  }
}

void setup() {
  //Start Serial
  Serial.begin(115200);
  while (!Serial)
    delay(10);
  Serial.println("Hot Plate Controller  V1.0");
  pinMode(FAN_PIN, OUTPUT);
  pinMode(SSR_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  delay(100);
  // Disable the fan
  digitalWrite(FAN_PIN, LOW);  // turn the FAN off
  digitalWrite(SSR_PIN, LOW); // turn the plate off
  // init the TFT
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(1);
  tft.cp437(true);
  // Initialise the touch controller
  if (!ts.begin(40)) {
    Serial.println("Unable to start touchscreen.");
  } else {
    Serial.println("Touchscreen started.");
  }

  Serial.print("Initial Plate Temperature: ");
  Input = thermocouple.readCelsius();
  Serial.print(Input);
  Serial.println(" °C");

  Setpoint = cooldownTemp;
  // tell the PID to range between 0 and the full window size
  myPID.SetOutputLimits(0, 1);
  // turn the PID on
  myPID.SetMode(AUTOMATIC);

  drawMainMenu();
  mainMenu = true;
  //buzzerBeep(1, 250);
}

void loop() {
  ///* Setup Menu *///
  while (mainMenu) {
    if (ts.touched()) {
      TS_Point touchpoint = ts.getPoint();
      touchpoint.x = map(touchpoint.x, 0, 240, 0, 240);
      touchpoint.y = map(touchpoint.y, 0, 320, 0, 320);
      int mainMenuXPos = getGridCellY(touchpoint), mainMenuYPos = getGridCellX(touchpoint);
      Serial.print("Setup menu touch: (");
      Serial.print(mainMenuXPos);
      Serial.print(",");
      Serial.print(mainMenuYPos);
      Serial.print(") -> ");
      if (mainMenuYPos == 3 && mainMenuXPos > 3) {  // Confirm button grid area pressed
        Serial.println("Confirm button");
        mainMenu = false;
      } else if (mainMenuYPos > 0) {  // Somewhere other than the confirm button and top bar
        editMenu = true;
        bool editingPreheat = false, editingSoak = false, editingReflow = false, editingPID = false;
        if (mainMenuYPos == 3) { // somewhere within the PID zone
          editingPID = true;
          Serial.println("Edit PID Menu");
          drawEditMenu("PID");
          drawText(64, 74, ILI9341_WHITE, ILI9341_BLACK, String(Kp));
          drawText(224, 74, ILI9341_WHITE, ILI9341_BLACK, String(Ki));
          drawText(64, 148, ILI9341_WHITE, ILI9341_BLACK, String(Kd));
        } else if (mainMenuXPos < 2) {  // Somwhere within the preheat zone
          editingPreheat = true;
          Serial.println("Edit Preheat Menu");
          drawEditMenu("Preheat");
          drawTemperature(63, 74, ILI9341_WHITE, ILI9341_BLACK, int(preheatTemp));
          drawText(223, 74, ILI9341_WHITE, ILI9341_BLACK, formatTime(preheatTime));
        } else if (mainMenuXPos > 3) {  // Somwhere within the reflow zone
          editingReflow = true;
          Serial.println("Edit Reflow Menu");
          drawEditMenu("Reflow");
          drawTemperature(63, 74, ILI9341_WHITE, ILI9341_BLACK, int(reflowTemp));
          drawText(223, 74, ILI9341_WHITE, ILI9341_BLACK, formatTime(reflowTime));
        } else {  // Somwhere within the soak zone
          editingSoak = true;
          Serial.println("Edit Soak Menu");
          drawEditMenu("Soak");
          drawTemperature(63, 74, ILI9341_WHITE, ILI9341_BLACK, int(soakTemp));
          drawText(223, 74, ILI9341_WHITE, ILI9341_BLACK, formatTime(soakTime));
        }
        while (editMenu) {  // Stay in this loop until the save button is pressed
          if (ts.touched()) {
            TS_Point touchpoint = ts.getPoint();
            touchpoint.x = map(touchpoint.x, 0, 240, 0, 240);
            touchpoint.y = map(touchpoint.y, 0, 320, 0, 320);
            int editMenuXPos = getGridCellY(touchpoint), editMenuYPos = getGridCellX(touchpoint);
            Serial.print("Edit menu touch at (");
            Serial.print(editMenuXPos);
            Serial.print(",");
            Serial.print(editMenuYPos);
            Serial.print(") -> ");
            if (editMenuYPos == 1) {    // One of the first row arrows was pressed
              if (editMenuXPos == 0) {  // The Temp down arrow was pressed
                Serial.println("Left down arrow");
                if (editingPreheat) {
                  if (preheatTemp > 100)
                    preheatTemp -= 10;
                  drawTemperature(63, 74, ILI9341_WHITE, ILI9341_BLACK, int(preheatTemp));
                }
                if (editingSoak) {
                  if (soakTemp > 100)
                    soakTemp -= 10;
                  drawTemperature(63, 74, ILI9341_WHITE, ILI9341_BLACK, int(soakTemp));
                }
                if (editingReflow) {
                  if (reflowTemp > 100)
                    reflowTemp -= 10;
                  drawTemperature(63, 74, ILI9341_WHITE, ILI9341_BLACK, int(reflowTemp));
                }
                if (editingPID) {
                  if (Kp > 0)
                    Kp -= 0.01;
                  drawText(64, 74, ILI9341_WHITE, ILI9341_BLACK, String(Kp));
                }
              } else if (editMenuXPos == 2) {  // The Temp up arrow was pressed
                Serial.println("Left up arrow");
                if (editingPreheat) {
                  if (preheatTemp < 300)
                    preheatTemp += 10;
                  drawTemperature(63, 74, ILI9341_WHITE, ILI9341_BLACK, int(preheatTemp));
                }
                if (editingSoak) {
                  if (soakTemp < 300)
                    soakTemp += 10;
                  drawTemperature(63, 74, ILI9341_WHITE, ILI9341_BLACK, int(soakTemp));
                }
                if (editingReflow) {
                  if (reflowTemp < 300)
                  reflowTemp += 10;
                  drawTemperature(63, 74, ILI9341_WHITE, ILI9341_BLACK, int(reflowTemp));
                }
                if (editingPID) {
                  if (Kp < 10)
                    Kp += 0.01;
                  drawText(64, 74, ILI9341_WHITE, ILI9341_BLACK, String(Kp));
                }
              } else if (editMenuXPos == 3) {
                Serial.println("Right down arrow");
                if (editingPreheat) {
                  if (preheatTime > 30000)
                    preheatTime -= 10000;
                  drawText(223, 74, ILI9341_WHITE, ILI9341_BLACK, formatTime(preheatTime));
                } else if (editingSoak) {
                  if (soakTime > 30000)
                    soakTime -= 10000;
                  drawText(223, 74, ILI9341_WHITE, ILI9341_BLACK, formatTime(soakTime));
                } else if (editingReflow) {
                  if (reflowTime > 30000)
                    reflowTime -= 10000;
                  drawText(223, 74, ILI9341_WHITE, ILI9341_BLACK, formatTime(reflowTime));
                } else if (editingPID) {
                  if (Ki > 1)
                    Ki -= 0.01;
                  drawText(224, 74, ILI9341_WHITE, ILI9341_BLACK, String(Ki));
                }

              } else if (editMenuXPos == 5) {  // The Time up arrow was pressed
                Serial.println("Right up arrow");
                if (editingPreheat) {
                  if (preheatTime < 300000)
                    preheatTime += 10000;
                  drawText(223, 74, ILI9341_WHITE, ILI9341_BLACK, formatTime(preheatTime));
                }
                if (editingSoak) {
                  if (soakTime < 300000)
                    soakTime += 10000;
                  drawText(223, 74, ILI9341_WHITE, ILI9341_BLACK, formatTime(soakTime));
                }
                if (editingReflow) {
                  if (reflowTime < 300000)
                    reflowTime += 10000;
                  drawText(223, 74, ILI9341_WHITE, ILI9341_BLACK, formatTime(reflowTime));
                }
                 if (editingPID) {
                  if (Ki < 10)
                    Ki += 0.01;
                  drawText(224, 74, ILI9341_WHITE, ILI9341_BLACK, String(Ki));
                }
              }
            } else if (editMenuYPos == 2) { // 2nd row arrows were pressed 
              if (editMenuXPos == 0) {  // The down arrow was pressed
                Serial.println("2nd row down arrow");
                if (Kd > 0)
                  Kd -= 0.01;
                drawText(64, 148, ILI9341_WHITE, ILI9341_BLACK, String(Kd));
              } else if (editMenuXPos == 2) { // The up arrow was pressed
                Serial.println("2nd row up arrow");
                if (Kd < 10)
                  Kd += 0.01;
                drawText(64, 148, ILI9341_WHITE, ILI9341_BLACK, String(Kd));
              }

            } else if (editMenuYPos == 3 && (editMenuXPos == 2 || editMenuXPos == 3)) {  // Save button was pressed
              Serial.println("Save button");
              //update totalTime
              totalTime = preheatTime + soakTime + reflowTime + cooldownTime;
              //update PID Tunings:
              myPID.SetTunings(Kp, Ki, Kd);
              tft.fillScreen(ILI9341_BLACK);
              drawMainMenu();
              editMenu = false;
            }
            delay(touchHoldLimit);  // so holding the button down doesn't read multiple presses
          }
        }
      }
      delay(touchHoldLimit);  // so holding the button down doesn't read multiple presses
    }
  }
  ///* Reflow Menu *///
  tft.fillScreen(ILI9341_BLACK);
  drawReflowMenu();
  drawButton(235, 195, 80, 40, ILI9341_GREEN_DK2, ILI9341_GREEN_DK1, ILI9341_WHITE, "Start");
  bool start = false;
  while (!start) {
    if (ts.touched()) {
      TS_Point touchpoint = ts.getPoint();
      touchpoint.x = map(touchpoint.x, 0, 240, 0, 240);
      touchpoint.y = map(touchpoint.y, 0, 320, 0, 320);
      if (getGridCellX(touchpoint) == 3 && getGridCellY(touchpoint) > 3) {
        start = true;
      }
      delay(touchHoldLimit);  // so holding the button down doesn't read multiple presses
    }
  }
  drawButton(235, 195, 80, 40, ILI9341_RED_DK2, ILI9341_RED_DK1, ILI9341_WHITE, "Stop");
  Serial.println("Reflow Menu");
  unsigned long reflowStarted = millis();
  reflowMenu = true;
  while (reflowMenu) {
    timeSinceReflowStarted = millis() - reflowStarted;
    if (timeSinceReflowStarted - lastTimeTempCheck > timeTempCheck) {
      lastTimeTempCheck = timeSinceReflowStarted;
      printState();
      Serial.print("\tSetpoint:");
      Serial.print(Setpoint);
      Input = thermocouple.readCelsius();
      Serial.print("\tInput:");
      Serial.print(Input);
      myPID.Compute();
      if (Output < 0.5) {
        digitalWrite(SSR_PIN, LOW);
      }
      if (Output > 0.5) {
        digitalWrite(SSR_PIN, HIGH);
      }
      Serial.print("\tOutput:");
      Serial.println(Output);
      plotDataPoint();
    }
    if (timeSinceReflowStarted > totalTime) {
      reflowMenu = false;
      digitalWrite(SSR_PIN, LOW); // turn the plate off
    } else if (timeSinceReflowStarted > (preheatTime + soakTime + reflowTime)) {  // preheat and soak and reflow are complete. Start cooldown
      if (!coolingDown) {
        newState = true;
        preheating = false, soaking = false, reflowing = false, coolingDown = true;
      }
      digitalWrite(FAN_PIN, HIGH);  // turn the FAN ON
      drawText(65, 220, ILI9341_WHITE, ILI9341_BLACK, "FAN ON ");
      Setpoint = cooldownTemp;
    } else if (timeSinceReflowStarted > (preheatTime + soakTime)) {  // preheat and soak are complete. Start reflow
      if (!reflowing) {
        newState = true;
        preheating = false, soaking = false, reflowing = true, coolingDown = false;
      }
      Setpoint = reflowTemp;
    } else if (timeSinceReflowStarted > preheatTime) {  // preheat is complete. Start soak
      if (!soaking) {
        newState = true;
        preheating = false, soaking = true, reflowing = false, coolingDown = false;
      }
      Setpoint = soakTemp;
    } else {  // cycle is starting. Start preheat
      if (!preheating) {
        newState = true;
        preheating = true, soaking = false, reflowing = false, coolingDown = false;
      }
      Setpoint = preheatTemp;
    }
    if (ts.touched()) {
      TS_Point touchpoint = ts.getPoint();
      touchpoint.x = map(touchpoint.x, 0, 240, 0, 240);
      touchpoint.y = map(touchpoint.y, 0, 320, 0, 320);
      //abort the reflow
      if (getGridCellX(touchpoint) == 3 && getGridCellY(touchpoint) > 3) {
        reflowMenu = false;
        digitalWrite(SSR_PIN, LOW); // turn the plate off
        digitalWrite(FAN_PIN, HIGH);  // turn the FAN ON
        drawText(65, 220, ILI9341_WHITE, ILI9341_BLACK, "FAN ON ");
      }
      delay(touchHoldLimit);  // so holding the button down doesn't read multiple presses
    }
  }
  // cooling done
  drawButton(235, 195, 80, 40, ILI9341_BLUE_DK2, ILI9341_BLUE_DK1, ILI9341_YELLOWISH, "Done");
  buzzerBeep(5, 100);
  bool done = false;
  while (!done) {
    if (ts.touched()) {
      TS_Point touchpoint = ts.getPoint();
      touchpoint.x = map(touchpoint.x, 0, 240, 0, 240);
      touchpoint.y = map(touchpoint.y, 0, 320, 0, 320);
      if (getGridCellX(touchpoint) == 3 && getGridCellY(touchpoint) > 3) {
        done = true;
        //turn off the fan
        digitalWrite(FAN_PIN, LOW);  // turn the FAN OFF
        drawMainMenu();
        mainMenu = true;
      }
      delay(touchHoldLimit);  // so holding the button down doesn't read multiple presses
    }
  }
}

void printState() {
  String time = formatTime(timeSinceReflowStarted);
  Serial.print("Current time: ");
  Serial.print(time);
  Serial.print("\t");
  drawTemperature(11, 220, ILI9341_RED_1, ILI9341_BLACK, Input);
  tft.setTextSize(3);
  drawText(120, 205, ILI9341_YELLOWISH, ILI9341_BLACK, time);
  tft.setTextSize(1);
  String currentState;
  if (preheating) {
    currentState = "Preheating";
  }
  if (soaking) {
    currentState = "Soaking   ";
  }
  if (reflowing) {
    currentState = "Reflowing ";
  }
  if (coolingDown) {
    currentState = "Cooldown  ";
  }
  Serial.print(currentState);
  if (newState) {
    newState = false;
    drawText(11, 200, ILI9341_WHITE, ILI9341_BLACK, currentState);
  }
}

void drawGrid() {
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.drawRect(0, 0, displayWidth, displayHeight - gridSize, gridColor);
  for (int i = 1; i < 6; i++) {
    tft.drawFastVLine(i * gridSize, 0, displayHeight - gridSize, gridColor);
  }
  for (int j = 1; j < 4; j++) {
    tft.drawFastHLine(0, j * gridSize, displayWidth, gridColor);
  }
  tft.setCursor(4, 4);
  tft.print("300");
  tft.setCursor(4, 1 * gridSize + 4);
  tft.print("200");
  tft.setCursor(4, 2 * gridSize + 4);
  tft.print("100");

  tft.setCursor(0 * gridSize + 4, 3 * gridSize + 7);
  tft.print("0");
  tft.setCursor(1 * gridSize + 4, 3 * gridSize + 7);
  tft.print(formatTime(totalTime / 6));
  tft.setCursor(2 * gridSize + 4, 3 * gridSize + 7);
  tft.print(formatTime(2 * totalTime / 6));
  tft.setCursor(3 * gridSize + 4, 3 * gridSize + 7);
  tft.print(formatTime(3 * totalTime / 6));
  tft.setCursor(4 * gridSize + 4, 3 * gridSize + 7);
  tft.print(formatTime(4 * totalTime / 6));
  tft.setCursor(5 * gridSize + 4, 3 * gridSize + 7);
  tft.print(formatTime(5 * totalTime / 6));

  plotReflowProfile();
}

void drawButton(int x, int y, int w, int h, uint16_t backgroundColor, uint16_t frameColor, uint16_t labelColor, String label) {
  //Draw the rectangular button
  tft.fillRect(x, y, w, h, backgroundColor);
  //draw the frame
  tft.drawRect(x - 1, y - 1, w + 1, h + 1, frameColor);
  //add the Label
  tft.setTextColor(labelColor, backgroundColor);
  if (label == "UP_ARROW") {
    tft.fillTriangle(int(x + (w/2)), int(y + (h/4)), int(x + (w/4)), int(y + h*3/4), int(x + (w*3/4)), int(y + h*3/4), labelColor);
  } else if (label == "DOWN_ARROW") {
    tft.fillTriangle(int(x + (w/4)), int(y + h/4), int(x + (w*3/4)), int(y + h/4), int(x + (w/2)), int(y + h*3/4), labelColor);
  } else {
    int16_t textBoundX, textBoundY;
    uint16_t textBoundWidth, textBoundHeight;
    //determine the smallest rectangle that could house the text
    tft.getTextBounds(label, 0, 0, &textBoundX, &textBoundY, &textBoundWidth, &textBoundHeight);
    tft.setCursor(x + (w - textBoundWidth) / 2, y + (h - textBoundHeight) / 2);
    tft.setTextSize(1);
    tft.print(label);
  }
}

void drawText(int x, int y, uint16_t textColor, uint16_t textBgColor, String text) {
  tft.setCursor(x, y);
  tft.setTextColor(textColor, textBgColor);
  tft.print(text);
}

void drawTemperature(int x, int y, uint16_t color, uint16_t bgColor, int value) {
  tft.setCursor(x, y);
  tft.setTextColor(color, bgColor);
  tft.printf("%d%c C", value, 0xF8);
}

void drawStatusBar(bool sdCardIn) {
  tft.drawRect(10, 10, 300, 30, ILI9341_GRAY);
  //draw IP Address
  drawText(20, 21, ILI9341_WHITE, ILI9341_BLACK, "Hot Plate Controller V1.0");
  //draw sd card logo
  if (sdCardIn) {
    tft.fillRect(280, 15, 25, 20, ILI9341_WHITE);
  } else tft.fillRect(280, 15, 25, 20, ILI9341_BLACK);
}

void drawMainMenu() {
  Serial.println("Entering Main Menu");
  tft.fillScreen(ILI9341_BLACK);
  drawStatusBar(true);
  drawButton(10, 46, 90, 135, preheatColor, ILI9341_RED_DK1, ILI9341_WHITE, "");
  drawButton(115, 46, 90, 135, soakColor, ILI9341_ORANGE_DK1, ILI9341_WHITE, "");
  drawButton(220, 46, 90, 135, reflowColor, ILI9341_YELLOW_DK1, ILI9341_WHITE, "");
  drawText(33, 60, ILI9341_WHITE, preheatColor, "PREHEAT");
  drawText(147, 60, ILI9341_WHITE, soakColor, "SOAK");
  drawText(246, 60, ILI9341_WHITE, reflowColor, "REFLOW");
  drawTemperature(40, 120, ILI9341_WHITE, preheatColor, int(preheatTemp));
  drawTemperature(144, 120, ILI9341_WHITE, soakColor, int(soakTemp));
  drawTemperature(250, 120, ILI9341_WHITE, reflowColor, int(reflowTemp));
  drawText(30, 130, ILI9341_WHITE, preheatColor, String(formatTime(preheatTime)) + " min");
  drawText(134, 130, ILI9341_WHITE, soakColor, String(formatTime(soakTime)) + " min");
  drawText(240, 130, ILI9341_WHITE, reflowColor, String(formatTime(reflowTime)) + " min");
  drawButton(10, 190, 195, 40, ILI9341_BLUE_DK2, ILI9341_BLUE_DK1, ILI9341_YELLOWISH, "PID Tunings");
  drawButton(220, 190, 90, 40, ILI9341_GREEN_DK2, ILI9341_GREEN_DK1, ILI9341_YELLOWISH, "CONFIRM");
}

void drawReflowMenu() {
  Serial.println("Entering Reflow Menu");
  drawGrid();
  tft.drawRect(5, 195, 220, 40, ILI9341_GRAY);
  drawText(65, 220, ILI9341_WHITE, ILI9341_BLACK, "FAN OFF");
  Input = thermocouple.readCelsius();
  drawTemperature(11, 220, ILI9341_RED_1, ILI9341_BLACK, Input);
  drawText(11, 200, ILI9341_WHITE, ILI9341_BLACK, "Ready");
  tft.setTextSize(3);
  drawText(120, 205, ILI9341_YELLOWISH, ILI9341_BLACK, formatTime(0));
  tft.setTextSize(1);
}

void drawEditMenu(String stage) {
  Serial.println("Entering Settings Menu");
  //clear screen
  tft.fillScreen(ILI9341_BLACK);
  //draw the texts
  tft.setFont(&FreeMonoBold9pt7b);
  drawText(65, 13, ILI9341_WHITE, ILI9341_BLACK, "[" + stage + " Settings]");
  //PID Menu require 3rd selector
  if (stage == "PID") {
    drawText(1, 45, ILI9341_WHITE, ILI9341_BLACK, " Kp: ");
    drawText(161, 45, ILI9341_WHITE, ILI9341_BLACK, " Ki: ");
    drawText(1, 125, ILI9341_WHITE, ILI9341_BLACK, " Kd: ");
  } else {
    drawText(1, 45, ILI9341_WHITE, ILI9341_BLACK, " Temperature: ");
    drawText(161, 45, ILI9341_WHITE, ILI9341_BLACK, " Time: ");
  }
  tft.setFont();
  //draw the frames
  tft.drawRect(4, 50, 150, 50, ILI9341_GRAY);
  tft.drawRect(164, 50, 150, 50, ILI9341_GRAY);
  //black bg
  tft.fillRect(5, 51, 148, 48, ILI9341_BLACK);
  tft.fillRect(165, 51, 148, 48, ILI9341_BLACK);
  //text frames
  tft.drawRect(54, 55, 50, 40, ILI9341_GRAY);
  tft.drawRect(214, 55, 50, 40, ILI9341_GRAY);
  //draw the buttons
  drawButton(10, 55, 40, 40, ILI9341_BLUE_DK2, ILI9341_BLUE_DK1, ILI9341_WHITE, "DOWN_ARROW");
  drawButton(108, 55, 40, 40, ILI9341_BLUE_DK2, ILI9341_BLUE_DK1, ILI9341_WHITE, "UP_ARROW");
  drawButton(170, 55, 40, 40, ILI9341_BLUE_DK2, ILI9341_BLUE_DK1, ILI9341_WHITE, "DOWN_ARROW");
  drawButton(268, 55, 40, 40, ILI9341_BLUE_DK2, ILI9341_BLUE_DK1, ILI9341_WHITE, "UP_ARROW");

  //PID Menu require 3rd selector
  if (stage == "PID") {
    //draw a frame
    tft.drawRect(4, 130, 150, 50, ILI9341_GRAY);
    //draw bg
    tft.fillRect(5, 131, 148, 48, ILI9341_BLACK);
    tft.drawRect(54, 135, 50, 40, ILI9341_GRAY);
    //draw the buttons
    drawButton(10, 135, 40, 40, ILI9341_BLUE_DK2, ILI9341_BLUE_DK1, ILI9341_WHITE, "DOWN_ARROW");
    drawButton(108, 135, 40, 40, ILI9341_BLUE_DK2, ILI9341_BLUE_DK1, ILI9341_WHITE, "UP_ARROW");
  }

  //TODO: this dummy button is used because somehow the display shows the label misplaced 
  // if we send only one drawButton !!!! FIX IT :)
  //drawButton(120, 190, 80, 40, ILI9341_GREEN_DK2, ILI9341_GREEN_DK1, ILI9341_WHITE, "");
  drawButton(120, 190, 80, 40, ILI9341_GREEN_DK2, ILI9341_GREEN_DK1, ILI9341_WHITE, "SAVE");
}

int getGridCellX(TS_Point touchpoint) {
  int xpoint = touchpoint.x;
  Serial.print("x: ");
  Serial.print(xpoint);
  Serial.print(" ");
  if (xpoint > 160)
    return 0;
  else if (xpoint > 120)
    return 1;
  else if (xpoint > 60)
    return 2;
  else
    return 3;
}

int getGridCellY(TS_Point touchpoint) {
  int ypoint = touchpoint.y;
  Serial.print("y: ");
  Serial.print(ypoint);
  Serial.print(" ");
  if (ypoint > 268)
    return 5;
  else if (ypoint > 214)
    return 4;
  else if (ypoint > 160)
    return 3;
  else if (ypoint > 108)
    return 2;
  else if (ypoint > 54)
    return 1;
  else
    return 0;
}

String formatTime(unsigned long milliseconds) {
  // Calculate the number of minutes and seconds
  unsigned long totalSeconds = milliseconds / 1000;
  unsigned int minutes = totalSeconds / 60;
  unsigned int seconds = totalSeconds % 60;
  // Format the time as a string with a leading zero if necessary
  String formattedTime = (minutes < 10 ? "0" : "") + String(minutes) + ":" + (seconds < 10 ? "0" : "") + String(seconds);
  return formattedTime;
}

void plotDataPoint() {
  uint16_t color;
  if (preheating) {
    color = preheatColor;
  }
  if (soaking) {
    color = soakColor;
  }
  if (reflowing) {
    color = reflowColor;
  }
  if (coolingDown) {
    color = cooldownColor;
  }
  tft.fillCircle(map(timeSinceReflowStarted, 0, totalTime, 0, displayWidth), map(Input, 0, 300, 3 * gridSize, 0), 2, color);
}

void plotReflowProfile() {
  int xMin, xMax, amp;
  xMin = 0;
  xMax = map(preheatTime, 0, totalTime, 0, displayWidth);
  amp = map(preheatTemp, 0, 300, 3 * gridSize, 0) - map(cooldownTemp, 0, 300, 3 * gridSize, 0);
  for (int i = 0; i <= (xMax - xMin); i++) {
    tft.fillCircle(xMin + i, -amp / 2 * cos(PI * i / (xMax - xMin)) + map(cooldownTemp, 0, 300, 3 * gridSize, 0) + amp / 2, 2, preheatColor_d);
  }

  xMin = map(preheatTime, 0, totalTime, 0, displayWidth);
  xMax = map(preheatTime + soakTime, 0, totalTime, 0, displayWidth);
  amp = map(soakTemp, 0, 300, 3 * gridSize, 0) - map(preheatTemp, 0, 300, 3 * gridSize, 0);
  //amp = 80;
  for (int i = 0; i <= (xMax - xMin); i++) {
    tft.fillCircle(xMin + i, -amp / 2 * cos(PI * i / (xMax - xMin)) + map(preheatTemp, 0, 300, 3 * gridSize, 0) + amp / 2, 2, soakColor_d);
  }

  xMin = map(preheatTime + soakTime, 0, totalTime, 0, displayWidth);
  xMax = map(preheatTime + soakTime + reflowTime, 0, totalTime, 0, displayWidth);
  amp = map(reflowTemp, 0, 300, 3 * gridSize, 0) - map(soakTemp, 0, 300, 3 * gridSize, 0);
  //amp = 80;
  for (int i = 0; i <= (xMax - xMin); i++) {
    tft.fillCircle(xMin + i, -amp / 2 * cos(PI * i / (xMax - xMin)) + map(soakTemp, 0, 300, 3 * gridSize, 0) + amp / 2, 2, reflowColor_d);
  }

  xMin = map(preheatTime + soakTime + reflowTime, 0, totalTime, 0, displayWidth);
  xMax = map(totalTime, 0, totalTime, 0, displayWidth);
  amp = map(cooldownTemp, 0, 300, 3 * gridSize, 0) - map(reflowTemp, 0, 300, 3 * gridSize, 0);
  //amp = 80;
  for (int i = 0; i <= (xMax - xMin); i++) {
    tft.fillCircle(xMin + i, -amp / 2 * cos(PI * i / (xMax - xMin)) + map(reflowTemp, 0, 300, 3 * gridSize, 0) + amp / 2, 2, cooldownColor_d);
  }
}