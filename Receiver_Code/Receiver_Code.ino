// Receiver Code
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <stdint.h>
#include <Servo.h>
#include <math.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library
#include <EEPROM.h>

// TFT Display related Variables

// 16-bit Color Codes (5-bit R, 6-bit G, 5-bit B)
#define ST77XX_BLACK 0xFFFF
#define ST77XX_WHITE 0x0000
#define ST77XX_CYAN 0xF800
#define ST77XX_BLUE 0xFFE0
#define ST77XX_MAGENTA 0x07E0
#define ST77XX_GREEN 0xF81F
#define ST77XX_YELLOW 0x001F
#define ST77XX_RED 0x07FF

// --- DISPLAY PIN DEFINITIONS ---
#define TFT_CS 6  // Chip Select
#define TFT_DC 3  // Data/Command
#define TFT_RST 5 // Reset (Optional, but recommended)

// The display is initialized with its native 240x320 resolution
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// --- GUI DIMENSIONS (Based on 320x240 Logical Screen) ---
#define SCREEN_W 320
#define SCREEN_H 240

#define HEADER_H 30
#define PANEL_W (SCREEN_W / 2)
#define PANEL_H ((SCREEN_H - HEADER_H) / 2)

// --- COLORS (Strictly using the forced definitions above) ---
#define MAIN_BG_COLOR ST77XX_BLACK
#define HEADER_BG_COLOR ST77XX_CYAN

#define GUI_HEADER_TEXT ST77XX_BLACK
#define GUI_TEXT_FG ST77XX_WHITE
#define GUI_LINE_COLOR ST77XX_WHITE
#define GUI_WATER_COLOR ST77XX_BLUE
#define GUI_GAS_LOW ST77XX_GREEN
#define GUI_GAS_HIGH ST77XX_RED
#define GUI_TEMP_NORMAL ST77XX_GREEN
#define GUI_TEMP_HIGH ST77XX_RED
#define GUI_TEMP_LOW ST77XX_CYAN

#define LAYOUT_SETUP 0
#define UPDATE_VALUE 1

// --- FUNCTION PROTOTYPES ---
void SetUpTftDisplayLayout(void);
void drawHeader(const char *title);
void drawLayoutLines();
void TankLevel(int tankId, uint8_t percentage, bool IsDataUpdate);
void Temperature(uint8_t temp, bool IsDataUpdate);
void GasLevel(uint8_t level, bool IsDataUpdate);

// NRF Module related Variables

struct DataPacket
{
  uint16_t tank1_depth;
  uint16_t tank2_depth;
  float temperature;
  float humidity;
};

#define CE_PIN 2
#define CSN_PIN 4

RF24 radio(CE_PIN, CSN_PIN); // CE, CSN

const byte address[6] = "00001";

void SetUpNrfModule(void);

/* System Variables*/

#define ALARM A3
#define RED_BUTTON A2
#define YELLOW_BUTTON A1
#define GREEN_BUTTON A0
#define NUM_OF_TANKS 2

#define gAlarmActiveTank1 EEPROM[20]
#define gAlarmActiveTank2 EEPROM[21]

bool gChangeThreshold = 0;
bool gChangeTrim = 0;
bool gChangeSystemVar = 0;

uint8_t gtank1fillper = 0;
uint8_t gtank2fillper = 0;

// Used to receive the data
struct DataPacket received_data;

typedef enum
{

  System_Connected,
  System_DisConnected,
  System_UpdateThresHolds,
  System_UpdateTrimVal,
  System_ChangeRelatedVar,

} tSystemState;

tSystemState gNextState;

void SystemDisconnected(void);
void SystemConnected(void);
void SystemUpdateThresholds(void);
void ChangeSystemVariables(void);
void StateMachineUpdate(tSystemState State);
void ProcessDataDisplayit(void);
void TurnOnAlarm(uint8_t tank1fillper, uint8_t tank2fillper);

void setup()
{

  Serial.begin(9600);

  // TFT Module Setup
  tft.init(240, 320);
  tft.setRotation(1);

  // NRF Module Setup
  SetUpNrfModule();

  // Hardware Setup
  pinMode(ALARM, OUTPUT);
  pinMode(RED_BUTTON, INPUT_PULLUP);
  pinMode(YELLOW_BUTTON, INPUT_PULLUP);
  pinMode(GREEN_BUTTON, INPUT_PULLUP);

  // Set the gAlarmActiveTank1 & 2 variables to true as  part of Setup
  // gAlarmActiveTank1 = 1;
  // gAlarmActiveTank2 = 1;
}

void loop()
{

  if (radio.available())
  {
    // radio.read(&received_data, sizeof(DataPacket));
    gNextState = System_Connected;
  }
  else
  {
    gNextState = System_DisConnected;
    StateMachineUpdate(gNextState);
  }

  // we will be here if the System is in Connected State/UpdateThreshold state
  StateMachineUpdate(gNextState);
}

void SystemDisconnected(void)
{

  // Until we Get Data Stay here
  while (!radio.available())
  {

    Serial.println("System is in Dis-Connected State");
    tft.fillScreen(ST77XX_BLACK);
    // Draw Thick Red X
    int cx = SCREEN_W / 2;
    int cy = 90;
    int sz = 40;
    for (int i = -5; i <= 5; i++)
    {
      tft.drawLine(cx - sz + i, cy - sz, cx + sz + i, cy + sz, ST77XX_RED);
      tft.drawLine(cx + sz + i, cy - sz, cx - sz + i, cy + sz, ST77XX_RED);
    }

    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);

    // Thick "SYSTEM DISCONNECTED"
    int tx = 45, ty = 170;
    tft.setCursor(tx + 1, ty);
    tft.print("SYSTEM DISCONNECTED");
    tft.setCursor(tx, ty + 1);
    tft.print("SYSTEM DISCONNECTED");
    tft.setCursor(tx, ty);
    tft.print("SYSTEM DISCONNECTED");

    tft.setCursor(65, 200);
    tft.print("CHECK CONNECTION");
    // // Turn On the Alarm
    // tone(ALARM, 1000, 500); // 1 kHz tone
    delay(1000);
  }
  gNextState = System_Connected;
}

void SystemConnected(void)
{
  // Clear the Screen
  // Setup the Layout
  tft.fillScreen(ST77XX_BLACK);
  SetUpTftDisplayLayout();

  // Variable to Count the disconnect of nrf
  uint8_t NRFDisconCount = 0x00;

  // Initial value to enter into while loop
  gChangeSystemVar = false;
  // Stay here until system is connected and update threshold button not pressed
  while (!(gChangeSystemVar))
  {
    // Read the value
    if (radio.available())
    {
      radio.read(&received_data, sizeof(DataPacket));
      NRFDisconCount = 0;
    }
    else
    {
      NRFDisconCount++;
    }

    // if NRF Disconnected for long while then change the state
    if (NRFDisconCount > 100)
    {
      // Change the State
      gNextState = System_DisConnected;
      break;
    }

    Serial.println("System is in Connected State");
    // Process the Data Here
    // Add the code here
    ProcessDataDisplayit(&received_data);

    // Process the alarm

    // Take the Thresholds
    // Add some error here so that we can avoid some sensor flucations
    uint8_t sensor_error = 3;
    uint8_t tank1upthres = (EEPROM.get(1, tank1upthres) - sensor_error);
    uint8_t tank1lowthres = (EEPROM.get(3, tank1lowthres) + sensor_error);

    uint8_t tank2upthres = (EEPROM.get(2, tank2upthres) - sensor_error);
    uint8_t tank2lowthres = (EEPROM.get(4, tank2lowthres) + sensor_error);

    // Set the ALarm only when we are in normal range
    // else TurnOnAlarm will trgigger and user will clear Alarm
    // Process for Tank1
    if (((gtank1fillper <= tank1upthres) && (gtank1fillper >= tank1lowthres)))
    {
      Serial.println("Entered into Tank1 alarm active");
      gAlarmActiveTank1 = true;
    }

    // Process for Tank2
    if (((gtank2fillper <= tank2upthres) && (gtank2fillper >= tank2lowthres)))
    {
      Serial.println("Entered into Tank2 alarm active");
      gAlarmActiveTank2 = true;
    }

    // Show the alarm status Active if any one tank is active
    AlarmStatus((gAlarmActiveTank1 || gAlarmActiveTank2), UPDATE_VALUE);

    // Turn on the Alarm based on the Thresholds
    TurnOnAlarm(gtank1fillper, gtank2fillper);

    bool button_state = digitalRead(YELLOW_BUTTON);
    if (button_state == 0)
    {
      // Hold the Button for Two Seconds
      // to change the state to update threshold.
      delay(2000);
      // still the button pressed
      button_state = digitalRead(YELLOW_BUTTON);
      if (button_state == 0)
      {
        gChangeSystemVar = true;
        tone(ALARM, 1583, 2000);
        gNextState = System_ChangeRelatedVar;
      }
    }
  }

  // if not update System Variables then it must be nrf disconnect
  if (!(gChangeSystemVar))
  {
    gNextState = System_DisConnected;
  }
}

void SystemUpdateThresholds(void)
{
  // Update the Layout here
  drawThresholdUI();

  // Iterate through both tanks
  for (uint8_t tank = 1; tank <= NUM_OF_TANKS; tank++)
  {
    // Update the Layout with respect to tank here
    uint8_t UpLimit = EEPROM.get(tank, UpLimit);
    uint8_t LowLimit = EEPROM.get(tank + 2, LowLimit);

    // Print TANK-1 or TANK-2 based on selection
    updateActiveTank(tank);

    Serial.println("System is in Update Threshold state");
    while ((gChangeThreshold))
    {

      bool uplimit = digitalRead(RED_BUTTON);
      bool lowlimit = digitalRead(GREEN_BUTTON);

      if (uplimit == 0)
      {
        tone(ALARM, 1000, 200);
        UpLimit++;
        delay(150);
      }

      if (lowlimit == 0)
      {
        tone(ALARM, 1000, 200);
        LowLimit++;
        delay(150);
      }

      UpLimit = UpLimit % 100; // rounding the values to 100
      LowLimit = LowLimit % 100;

      updateThresholdValues(UpLimit, LowLimit);

      Serial.print("Upper val : ");
      Serial.print(UpLimit);
      Serial.print(" | Lower val : ");
      Serial.println(LowLimit);

      bool save = digitalRead(YELLOW_BUTTON);
      if (save == 0)
      {
        // Hold the button for two seconds to save the data into eeprom
        delay(2000);
        save = digitalRead(YELLOW_BUTTON);
        if (save == 0)
        {
          EEPROM.put(tank + 2, LowLimit);
          EEPROM.put(tank, UpLimit);
          gChangeThreshold = false;
          tone(ALARM, 1500, 200);
          delay(500);
          tone(ALARM, 1500, 200);
          delay(500);
        }
      }
    }
    // for tank2
    if (!(gChangeThreshold))
    {
      gChangeThreshold = true;
    }
  }

  tone(ALARM, 1500, 2000);
  // If the nrf disconnected
  if (!radio.available())
  {
    gNextState = System_DisConnected;
  }
  else
  {
    gNextState = System_Connected;
  }
}

void SystemUpdateTrim(void)
{
  uint8_t NRFDisconCount = 0x00;
  // Iterate through both tanks
  // Clear the GUI and add the new gui
  Serial.println("System is in Updated Trim");
  for (uint8_t tank = 1; tank <= NUM_OF_TANKS; tank++)
  {
    // Update the Layout with respect to tank here
    uint16_t Trim = EEPROM.get(5 * tank, Trim);

    // Print TANK-1 or TANK-2 based on selection

    Serial.println("System is in Update Trim state");
    drawTankTrimView(tank, received_data.tank1_depth, Trim, LAYOUT_SETUP);
    delay(1000);
    while ((radio.available() && gChangeTrim))
    {

      if (radio.available())
      {
        radio.read(&received_data, sizeof(DataPacket));
        NRFDisconCount = 0;
      }
      else
      {
        NRFDisconCount++;
        Serial.println("NRF Disconnect with Count");
      }

      // if NRF Disconnected for long while then change the state
      if (NRFDisconCount > 100)
      {
        // Change the State
        gNextState = System_DisConnected;
        break;
      }

      bool lowlimit = digitalRead(RED_BUTTON);
      bool uplimit = digitalRead(GREEN_BUTTON);

      if (uplimit == 0)
      {
        tone(ALARM, 1000, 200);
        Trim++;
        delay(100);
      }

      if (lowlimit == 0)
      {
        tone(ALARM, 1000, 200);
        Trim--;
        delay(100);
      }

      Trim = Trim % 400; // bound the value to 400;
      uint16_t current_depth;
      if (tank == 1)
      {
        current_depth = received_data.tank1_depth;
      }
      else
      {
        current_depth = received_data.tank2_depth;
      }
      drawTankTrimView(tank, current_depth, Trim, UPDATE_VALUE);

      Serial.print("Upper val : ");
      Serial.println(Trim);

      bool save = digitalRead(YELLOW_BUTTON);
      if (save == 0)
      {
        // Hold the button for two seconds to save the data into eeprom
        delay(2000);
        save = digitalRead(YELLOW_BUTTON);
        if (save == 0)
        {
          Serial.println("EXIT Because of the button press");
          EEPROM.put(5 * tank, Trim);
          gChangeTrim = false;
          tone(ALARM, 1500, 200);
          delay(500);
          tone(ALARM, 1500, 200);
          delay(500);
        }
      }
    }
    // for tank2
    if (!(gChangeTrim))
    {
      gChangeTrim = true;
    }
  }

  tone(ALARM, 1500, 2000);
  // If the nrf disconnected
  if (!radio.available())
  {
    gNextState = System_DisConnected;
  }
  else
  {
    gNextState = System_Connected;
  }
}

void ChangeSystemVariables(void)
{
  // Initial value to enter into while loop
  bool UpdateThresholds = true;
  bool UpdateTrim = true;
  // clear the Display
  // Update the Layout
  tft.fillScreen(ST77XX_BLACK);
  drawButtonInterface();
  Serial.println("System is in Change System Variables");
  // Stay here until we press a button
  while (UpdateThresholds && UpdateTrim)
  {

    UpdateTrim = digitalRead(RED_BUTTON);
    UpdateThresholds = digitalRead(GREEN_BUTTON);

    if ((UpdateThresholds == 0) || (UpdateTrim == 0))
    {

      delay(1000);

      UpdateTrim = digitalRead(RED_BUTTON);
      UpdateThresholds = digitalRead(GREEN_BUTTON);

      if ((UpdateThresholds == 0) && (UpdateTrim == 1))
      {
        tone(ALARM, 1500, 1000);
        gChangeThreshold = true;
        gChangeTrim = false;
        Serial.println("////////////////////");
        Serial.println("System Update Thresholds");
        Serial.println("///////////////////");
        gNextState = System_UpdateThresHolds;
      }

      if ((UpdateThresholds == 1) && (UpdateTrim == 0))
      {
        tone(ALARM, 1500, 1000);
        gChangeTrim = true;
        gChangeThreshold = false;
        Serial.println("////////////////////");
        Serial.println("System Update Trim");
        Serial.println("///////////////////");
        gNextState = System_UpdateTrimVal;
      }
    }
  }
}

void ProcessDataDisplayit(struct DataPacket *data)
{

  uint16_t Tank1Trim = EEPROM.get(5 * 1, Tank1Trim);
  uint16_t Tank2Trim = EEPROM.get(5 * 2, Tank2Trim);

  gtank1fillper = map((Tank1Trim - data->tank1_depth), 0, (Tank1Trim - 10), 0, 100);

  gtank2fillper = map((Tank2Trim - data->tank2_depth), 0, (Tank2Trim - 10), 0, 100);

  if (gtank2fillper > 100 || gtank1fillper > 100)
  {
    // Dont process the incorrect values Just process the temperatur alone
    // and Initialize the global variables with some safe values instead of Zero, So you wont get any incorrect alarm
    gtank1fillper = 50;
    gtank2fillper = 50;
    Temperature(data->temperature, UPDATE_VALUE);
    return;
  }

  TankLevel(1, gtank1fillper, UPDATE_VALUE);
  TankLevel(2, gtank2fillper, UPDATE_VALUE);
  Temperature(data->temperature, UPDATE_VALUE);
}

void TurnOnAlarm(uint8_t tank1fillper, uint8_t tank2fillper)
{

  // Take the Thresholds
  uint8_t tank1upthres = EEPROM.get(1, tank1upthres);
  uint8_t tank1lowthres = EEPROM.get(3, tank1lowthres);

  uint8_t tank2upthres = EEPROM.get(2, tank2upthres);
  uint8_t tank2lowthres = EEPROM.get(4, tank2lowthres);

  // Process for Tank1

  if (gAlarmActiveTank1)
  {

    if (((tank1fillper > tank1upthres) || (tank1fillper < tank1lowthres)))
    {

      // Turn on the Alarm here
      // Once we reach here we need to turn on the Alarm util user is acknowledged
      bool red_button = digitalRead(RED_BUTTON);
      while (red_button)
      {
        tone(ALARM, 1000, 300);
        delay(500);
        Serial.println("Alaram Activated");
        red_button = digitalRead(RED_BUTTON);
        if (red_button == 0)
        {
          delay(500);
          red_button = digitalRead(RED_BUTTON);
        }
      }

      // clear the Alarm User is Acknowledged
      gAlarmActiveTank1 = false;
    }
  }

  // Process for Tank2
  if (gAlarmActiveTank2)
  {

    if (((tank2fillper > tank2upthres) || (tank2fillper < tank2lowthres)))
    {

      // Turn on the Alarm here
      // Once we reach here we need to turn on the Alarm util user is acknowledged
      bool red_button = digitalRead(RED_BUTTON);
      while (red_button)
      {
        tone(ALARM, 1000, 300);
        delay(500);
        Serial.println("Alaram Activated");
        red_button = digitalRead(RED_BUTTON);
        if (red_button == 0)
        {
          delay(500);
          red_button = digitalRead(RED_BUTTON);
        }
      }

      // clear the Alarm User is Acknowledged
      gAlarmActiveTank2 = false;
    }
  }
}

void StateMachineUpdate(tSystemState State)
{
  while (1)
  {
    State = gNextState;

    switch (State)
    {
    case System_DisConnected:
    {
      SystemDisconnected();
      break;
    }
    case System_Connected:
    {
      SystemConnected();
      break;
    }

    case System_ChangeRelatedVar:
    {
      ChangeSystemVariables();
      break;
    }
    case System_UpdateThresHolds:
    {
      SystemUpdateThresholds();
      break;
    }
    case System_UpdateTrimVal:
    {
      SystemUpdateTrim();
      break;
    }

    default:
      break;
    }
  }
}

// Tft Display related function definations

// =====================================================================
// === GUI DRAWING FUNCTIONS ===========================================
// =====================================================================

void SetUpTftDisplayLayout(void)
{

  tft.setRotation(1);
  tft.fillScreen(MAIN_BG_COLOR);

  drawHeader("    MONITORING SYSTEM");
  drawLayoutLines();

  TankLevel(1, 0, LAYOUT_SETUP);
  TankLevel(2, 0, LAYOUT_SETUP);
  Temperature(0, LAYOUT_SETUP);
  AlarmStatus(0, LAYOUT_SETUP);
}

/**
 * Draws the static header bar at the top with thickened text.
 */
void drawHeader(const char *title)
{
  // 1. Draw header background (CYAN)
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, HEADER_BG_COLOR);

  // 2. Draw a highlight line below the header (White)
  tft.drawLine(0, HEADER_H - 1, SCREEN_W, HEADER_H - 1, GUI_TEXT_FG);

  // Set the text properties
  tft.setTextColor(GUI_HEADER_TEXT); // Black text
  tft.setTextSize(2);                // Text size 2 (already large)

  // 3. Draw the thickened text
  const int START_X = 5;
  const int START_Y = 8;

  // Draw the text four times with slight offsets to simulate thickness
  tft.setCursor(START_X + 1, START_Y); // Offset Right
  tft.print(title);

  tft.setCursor(START_X, START_Y + 1); // Offset Down
  tft.print(title);

  tft.setCursor(START_X + 1, START_Y + 1); // Offset Diagonal (Right & Down)
  tft.print(title);

  // Draw the original text last for sharpest finish (or just one of the offsets)
  tft.setCursor(START_X, START_Y); // Original Position
  tft.print(title);
}

/**
 * Draws the dividing lines for the four panels (the main grid).
 */
void drawLayoutLines()
{
  int monitorY = HEADER_H;
  int monitorH = SCREEN_H - HEADER_H;

  // 1. Outer Border (White)
  tft.drawRect(0, monitorY, SCREEN_W, monitorH, GUI_LINE_COLOR);

  // 2. Vertical Center Divider (White)
  tft.drawLine(PANEL_W, monitorY, PANEL_W, SCREEN_H, GUI_LINE_COLOR);

  // 3. Horizontal Center Divider (White)
  tft.drawLine(0, monitorY + PANEL_H, SCREEN_W, monitorY + PANEL_H, GUI_LINE_COLOR);
}

/**
 * Draws or updates a Tank Level Panel (Top Half).
 */
void TankLevel(int tankId, uint8_t percentage, bool IsDataUpdate)
{
  int x = (tankId == 1) ? 0 : PANEL_W;
  int y = HEADER_H;

  // --- Bar Graph (Tank Icon) ---
  int barX = x + 100;
  int barY = y + 10;
  int barW = 40;
  int barH = 80;

  if (!(IsDataUpdate))
  {
    tft.fillRect(x + 1, y + 1, PANEL_W - 2, 35, MAIN_BG_COLOR);

    // --- Title and Percentage (White text) ---
    tft.setCursor(x + 12, y + 5);
    tft.setTextColor(ST77XX_YELLOW, MAIN_BG_COLOR);
    tft.setTextSize(2);
    tft.print("TANK-");
    tft.print(tankId);

    tft.drawRect(barX, barY, barW, barH, GUI_TEXT_FG);
  }

  else

  {

    int fillHeight = map(percentage * 10, 0, 1000, 0, barH);
    int fillY = barY + barH - fillHeight;

    if (percentage == 100)
    {
      tft.setCursor(x + 1, y + 45);
    }
    else
    {
      tft.setCursor(x + 15, y + 45);
    }

    tft.setTextColor(ST77XX_GREEN, MAIN_BG_COLOR);

    tft.setTextSize(4);
    // Clear the Display before Printing

    tft.fillRect(x + 1, y + 45, 95, 30, MAIN_BG_COLOR);

    tft.print(percentage, 1);
    tft.print("%");

    // Clear and Fill (Background BLACK, Water color BLUE)
    tft.fillRect(barX + 1, barY + 1, barW - 2, barH - 2, MAIN_BG_COLOR);
    tft.fillRect(barX + 1, fillY + 1, barW - 2, fillHeight - 2, GUI_WATER_COLOR);
  }
}

/**
 * Draws or updates the Temperature Panel (Bottom Left).
 */
void Temperature(uint8_t temp, bool IsDataUpdate)
{

  int x = 0;
  int y = HEADER_H + PANEL_H;
  // --- Icon/Value ---
  int tempX = x + 10;
  int tempY = y + 35;

  if (!IsDataUpdate)
  {

    tft.fillRect(x + 1, y + 1, PANEL_W - 2, PANEL_H - 2, MAIN_BG_COLOR);
    // --- Title (White text) ---
    tft.setCursor(x + 5, y + 5);
    tft.setTextColor(ST77XX_YELLOW, MAIN_BG_COLOR);
    tft.setTextSize(2);
    tft.print(" TEMPERATURE");

    // 1. Draw Thermometer Outline (White)
    tft.drawRect(tempX, tempY, 15, 50, GUI_TEXT_FG);
    tft.fillCircle(tempX + 7, tempY + 55, 7, GUI_TEXT_FG);
  }

  else
  {
    uint16_t tempColor = GUI_TEMP_NORMAL;

    if (temp >= 30.0)
    {
      tempColor = GUI_TEMP_HIGH;
    }
    else if (temp < 25)
    {
      tempColor = GUI_TEMP_LOW;
    }

    // 2. Draw Temperature Fill (Background BLACK)
    int fillH = map(temp * 10, 0, 500, 0, 48);
    int fillY = tempY + 50 - fillH;

    tft.fillRect(tempX + 3, tempY + 1, 9, 48, MAIN_BG_COLOR);
    tft.fillCircle(tempX + 7, tempY + 55, 7, tempColor);
    tft.fillRect(tempX + 3, fillY + 1, 9, fillH, tempColor);

    // 3. Display Value (Colored text)
    tft.setCursor(tempX + 30, y + 50);
    tft.setTextSize(3);
    tft.setTextColor(tempColor);
    tft.fillRect(x + 30, y + 50, 90, 50, MAIN_BG_COLOR);
    tft.print(temp, 1);
    tft.print((char)247);
    tft.print("C");
  }
}

/**
 * Draws or updates the Gas Level Panel (Bottom Right).
 */
/**
 * Draws or updates the Alarm Status Panel (Bottom Right).
 * Replaces the old GasLevel function.
 */

/**
 * Helper function to draw a LARGE, filled Bell Icon
 * x, y: Top-left corner of the bell area
 * color: The fill color
 */
void drawBigBell(int x, int y, uint16_t color)
{
  // 1. Bell Top (Small loop/handle)
  tft.fillRoundRect(x + 15, y, 10, 6, 2, color);

  // 2. Main Bell Body (Trapezoid-like shape using rounded rect)
  tft.fillRoundRect(x + 5, y + 5, 30, 25, 8, color);

  // 3. Bell Flare (The wide bottom part)
  tft.fillRoundRect(x, y + 25, 40, 8, 3, color);

  // 4. Clapper (The ball at the bottom)
  tft.fillCircle(x + 20, y + 36, 5, color);
}

/**
 * Updated Alarm Status Panel with Large Bell
 */
void AlarmStatus(bool isActive, bool IsDataUpdate)
{
  int x = PANEL_W;
  int y = HEADER_H + PANEL_H;

  if (!IsDataUpdate)
  {
    // Initial Setup: Clear panel and draw title
    tft.fillRect(x + 1, y + 1, PANEL_W - 2, PANEL_H - 2, MAIN_BG_COLOR);

    tft.setCursor(x + 8, y + 10);
    tft.setTextColor(ST77XX_YELLOW, MAIN_BG_COLOR);
    tft.setTextSize(2);
    tft.print("ALARM STATUS");
  }
  else
  {
    // Area to clear (covers bell and text)
    tft.fillRect(x + 10, y + 40, PANEL_W - 20, 55, MAIN_BG_COLOR);

    if (isActive)
    {
      // --- ACTIVE STATE (RED) ---
      uint16_t color = ST77XX_GREEN;

      // Draw Large Red Bell
      drawBigBell(x + 15, y + 45, color);

      // Print "ACTIVE" (Reduced size for better fit)
      tft.setTextColor(color);
      tft.setTextSize(2); // Reduced from 3 to 2 to look professional

      // Thick Text Effect
      int textX = x + 65;
      int textY = y + 55;
      tft.setCursor(textX, textY);
      tft.print("ACTIVE");
      tft.setCursor(textX + 1, textY);
      tft.print("ACTIVE");
    }
    else
    {
      // --- NORMAL STATE (GREEN) ---
      uint16_t color = ST77XX_RED;

      // Draw Large Green Bell
      drawBigBell(x + 15, y + 45, color);

      tft.setTextColor(color);
      tft.setTextSize(2);

      int textX = x + 65;
      int textY = y + 55;
      tft.setCursor(textX, textY);
      tft.print("OFF");
      tft.setCursor(textX + 1, textY);
      tft.print("OFF");
    }

    // Refresh border to prevent artifacts
    tft.drawRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H, GUI_LINE_COLOR);
    tft.drawLine(PANEL_W, HEADER_H, PANEL_W, SCREEN_H, GUI_LINE_COLOR);
  }
}

#define HEADER_H 35
#define TANK_ROW_H 30 // Height for the TANK-1 / TANK-2 row
void drawThresholdUI()
{
  tft.fillScreen(ST77XX_BLACK);

  // 1. MAIN HEADER
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, ST77XX_CYAN);
  drawThickText("THRESHOLD SETTINGS", 50, 8, 2, ST77XX_BLACK);

  // 2. NEW ROW: TANK SELECTOR
  tft.fillRect(0, HEADER_H, SCREEN_W, TANK_ROW_H, ST77XX_BLACK);
  tft.drawLine(0, HEADER_H + TANK_ROW_H, SCREEN_W, HEADER_H + TANK_ROW_H, ST77XX_WHITE);

  // 3. THRESHOLD CARDS (Adjusted Y to 75 to fit the new row)
  int cardY = HEADER_H + TANK_ROW_H + 15;
  drawSettingCard(10, cardY, 145, 140, "HIGH LEVEL", ST77XX_RED);
  drawSettingCard(165, cardY, 145, 140, "LOW LEVEL", ST77XX_GREEN);
}

/**
 * Draws the background boxes with THICK labels
 */
void drawSettingCard(int x, int y, int w, int h, const char *label, uint16_t accentColor)
{
  // Main Border
  tft.drawRoundRect(x, y, w, h, 8, ST77XX_YELLOW); // Yellow border

  // Accent Top Bar
  tft.fillRoundRect(x + 2, y + 2, w - 4, 30, 6, accentColor);

  // PRINTING "HIGH LEVEL" / "LOW LEVEL" THICKER
  // Using offset trick inside the card header
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_BLACK);
  int textX = x + 12;
  int textY = y + 8;

  tft.setCursor(textX + 1, textY);
  tft.print(label);
  tft.setCursor(textX, textY + 1);
  tft.print(label);
  tft.setCursor(textX, textY);
  tft.print(label);
}

/**
 * Updates numbers only
 */
void updateThresholdValues(uint8_t highThreshold, uint8_t lowThreshold)
{

  int cardY = HEADER_H + TANK_ROW_H + 15;
  int textY = cardY + 55;

  // High Value
  tft.fillRect(25, textY, 110, 50, ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(5);
  tft.setCursor(45, textY + 5);
  tft.print(highThreshold);
  tft.setTextSize(2);
  tft.print("%");

  // Low Value
  tft.fillRect(180, textY, 110, 50, ST77XX_BLACK);
  tft.setTextSize(5);
  tft.setCursor(200, textY + 5);
  tft.print(lowThreshold);
  tft.setTextSize(2);
  tft.print("%");
}

void updateActiveTank(int newTank)
{

  // 1. Clear ONLY the Tank Selection Row area
  // We use COLOR_GREY to match the bar color we defined
  tft.fillRect(0, HEADER_H, SCREEN_W, TANK_ROW_H, ST77XX_BLACK);

  // 2. Redraw the Divider Line (to keep the UI sharp)
  tft.drawLine(0, HEADER_H + TANK_ROW_H, SCREEN_W, HEADER_H + TANK_ROW_H, ST77XX_WHITE);

  // 3. Prepare the string
  char tankLabel[12];
  sprintf(tankLabel, "TANK - %d", newTank);

  // 4. Print the new text (using our thick text trick)
  // Note: I added a background color to setTextColor as a secondary safety
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  drawThickText(tankLabel, 110, HEADER_H + 7, 2, ST77XX_YELLOW);
}

/**
 * Bold Text Helper
 */
void drawThickText(const char *text, int x, int y, int size, uint16_t color)
{
  tft.setTextSize(size);
  tft.setTextColor(color);
  tft.setCursor(x + 1, y);
  tft.print(text);
  tft.setCursor(x, y + 1);
  tft.print(text);
  tft.setCursor(x, y);
  tft.print(text);
}

/**
 * Helper to print "Smooth" text by over-drawing with offsets
 */
void printSmoothText(const char *text, int x, int y, uint8_t size, uint16_t color)
{
  tft.setTextSize(size);
  tft.setTextColor(color);

  // Offset printing to remove the "pixelated" look
  tft.setCursor(x + 1, y);
  tft.print(text);
  tft.setCursor(x, y + 1);
  tft.print(text);
  tft.setCursor(x + 1, y + 1);
  tft.print(text);
  tft.setCursor(x, y);
  tft.print(text);
}

void drawLargeButton(int x, int y, int w, int h, uint16_t color, const char *topText, const char *bottomText)
{
  // 1. Draw main colored block (Red or Green)
  tft.fillRoundRect(x, y, w, h, 12, color);
  tft.drawRoundRect(x, y, w, h, 12, ST77XX_WHITE);

  // 2. Print Headers in Black (Size 3 for smooth thickness)
  printSmoothText(topText, x + 18, y + 20, 3, ST77XX_BLACK);
  printSmoothText(bottomText, x + 25, y + 50, 3, ST77XX_BLACK);

  // 3. ENLARGED "PRESS BUTTON" Section
  // We increase the height and width to make it the focus
  int labelW = 130;
  int labelH = 70; // Increased height significantly
  int labelX = x + (w / 2) - (labelW / 2);
  int labelY = y + 95; // Positioned lower to give headers space

  // Draw the Black Container
  tft.fillRoundRect(labelX, labelY, labelW, labelH, 8, ST77XX_BLACK);
  tft.drawRoundRect(labelX, labelY, labelW, labelH, 8, ST77XX_WHITE);

  // 4. Print "PRESS" and "BUTTON" in Large White Text
  // Using Size 2 for both to keep them big and clear
  printSmoothText("PRESS", labelX + 35, labelY + 15, 2, ST77XX_WHITE);
  printSmoothText("BUTTON", labelX + 28, labelY + 40, 2, ST77XX_WHITE);
}

void drawButtonInterface()
{
  // Background and Outer White Frame
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRect(0, 0, 320, 240, ST77XX_WHITE);
  tft.drawRect(1, 1, 318, 238, ST77XX_WHITE);

  // Draw the two main selection blocks
  drawLargeButton(15, 25, 140, 190, ST77XX_RED, "UPDATE", "TRIM");
  drawLargeButton(165, 25, 140, 190, ST77XX_GREEN, "UPDATE", "LIMITS");
}

/**
 * Trim Adjustment View for Tank 1 and Tank 2
 * @param tankID: 1 or 2
 * @param currentVal: The live sensor reading
 * @param trimVal: The current offset
 * @param IsDataUpdate: True to update only the numbers, False for full redraw
 */
void drawTankTrimView(int tankID, int currentVal, int trimVal, bool IsDataUpdate)
{
  // Layout Coordinates
  int leftX = 15;
  int rightX = 165;
  int cardY = 70;
  int cardW = 140;
  int cardH = 140;

  if (!IsDataUpdate)
  {
    // --- DRAW STATIC LAYOUT ---
    tft.fillScreen(ST77XX_BLACK);

    // 1. White Main Borders
    tft.drawRect(0, 0, 320, 240, ST77XX_WHITE);
    tft.drawRect(1, 1, 318, 238, ST77XX_WHITE);

    // 2. Dynamic Tank Header Tab
    uint16_t tabColor = (tankID == 1) ? ST77XX_BLUE : 0xF81F; // Blue for T1, Magenta for T2
    tft.fillRoundRect(10, 5, 120, 35, 5, tabColor);
    tft.drawRoundRect(10, 5, 120, 35, 5, ST77XX_WHITE);

    char tankLabel[10];
    sprintf(tankLabel, "TANK %d", tankID);
    printSmoothText(tankLabel, 25, 12, 2, ST77XX_WHITE);

    // 3. Main Title
    printSmoothText("TRIM ADJUST", 145, 12, 2, ST77XX_CYAN);
    tft.drawFastHLine(2, 45, 316, ST77XX_WHITE);

    // 4. Draw Cards
    // Left: CURRENT
    tft.drawRoundRect(leftX, cardY, cardW, cardH, 8, ST77XX_WHITE);
    tft.fillRoundRect(leftX + 2, cardY + 2, cardW - 4, 30, 5, 0x4208);
    printSmoothText("CURRENT", leftX + 22, cardY + 8, 2, ST77XX_WHITE);

    // Right: TRIM
    tft.drawRoundRect(rightX, cardY, cardW, cardH, 8, ST77XX_WHITE);
    tft.fillRoundRect(rightX + 2, cardY + 2, cardW - 4, 30, 5, ST77XX_RED);
    printSmoothText("OFFSET", rightX + 35, cardY + 8, 2, ST77XX_WHITE);

    // Vertical Center Divider
    tft.drawFastVLine(160, 46, 192, ST77XX_WHITE);
  }
  else
  {
    // --- UPDATE DYNAMIC VALUES ---

    // Clear and update Current Value
    tft.fillRect(leftX + 5, cardY + 45, cardW - 10, 80, ST77XX_BLACK);
    char curStr[10];
    sprintf(curStr, "%d", currentVal);
    printSmoothText(curStr, leftX + 35, cardY + 55, 5, ST77XX_YELLOW);
    printSmoothText("CM", leftX + 60, cardY + 105, 1, 0x7BEF); // Assuming tank depth in CM

    // Clear and update Trim Value
    tft.fillRect(rightX + 5, cardY + 45, cardW - 10, 80, ST77XX_BLACK);
    char trimStr[10];
    sprintf(trimStr, "%d", trimVal);
    printSmoothText(trimStr, rightX + 25, cardY + 55, 5, ST77XX_RED);
    printSmoothText("ADJUST", rightX + 45, cardY + 105, 1, 0x7BEF);
  }
}

// Nrf Module Defination

void SetUpNrfModule(void)
{

  // NRF Module Settings
  radio.begin();
  // (2400MHz + 100MHz) Frequency Channel
  // both Transimitter and Receiver should be on same channel
  // helps when multiple NRF modules communicating in same space
  radio.setChannel(120);
  radio.setDataRate(RF24_250KBPS);
  radio.openReadingPipe(1, address);
  radio.setPALevel(RF24_PA_HIGH);
  radio.powerUp();
  radio.setPayloadSize(sizeof(DataPacket));
  radio.startListening();
}
