#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library
#include <SPI.h>
#include <math.h>            // Required for GasLevel (sin/cos)


// 16-bit Color Codes (5-bit R, 6-bit G, 5-bit B)
#define ST77XX_BLACK   0xFFFF
#define ST77XX_WHITE   0x0000
#define ST77XX_CYAN    0xF800
#define ST77XX_BLUE    0xFFE0
#define ST77XX_MAGENTA 0x07E0
#define ST77XX_GREEN   0xF81F
#define ST77XX_YELLOW  0x001F
#define ST77XX_RED     0x07FF

// -----------------------------------------------------------------


// --- DISPLAY PIN DEFINITIONS ---
#define TFT_CS    5  // Chip Select
#define TFT_DC    0  // Data/Command
#define TFT_RST   4  // Reset (Optional, but recommended)

// The display is initialized with its native 240x320 resolution
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST); 

// --- GUI DIMENSIONS (Based on 320x240 Logical Screen) ---
#define SCREEN_W    320
#define SCREEN_H    240

#define HEADER_H    30  
#define PANEL_W     (SCREEN_W / 2) 
#define PANEL_H     ((SCREEN_H - HEADER_H) / 2) 

// --- COLORS (Strictly using the forced definitions above) ---
#define MAIN_BG_COLOR   ST77XX_BLACK        
#define HEADER_BG_COLOR ST77XX_CYAN         

#define GUI_HEADER_TEXT ST77XX_RED        
#define GUI_TEXT_FG     ST77XX_WHITE        
#define GUI_LINE_COLOR  ST77XX_WHITE        
#define GUI_WATER_COLOR ST77XX_BLUE         
#define GUI_GAS_LOW     ST77XX_GREEN        
#define GUI_GAS_HIGH    ST77XX_RED          
#define GUI_TEMP_NORMAL ST77XX_GREEN
#define GUI_TEMP_HIGH   ST77XX_RED
#define GUI_TEMP_LOW    ST77XX_CYAN

#define LAYOUT_SETUP  0
#define UPDATE_VALUE  1



// --- FUNCTION PROTOTYPES ---
void drawHeader(const char* title);
void drawLayoutLines();
void TankLevel(int tankId, uint8_t percentage, bool IsDataUpdate);
void Temperature(uint8_t temp, bool IsDataUpdate);
void GasLevel(uint8_t level, bool IsDataUpdate);

// =====================================================================
// === CORE ARDUINO FUNCTIONS ==========================================
// =====================================================================

void setup(void) {
  tft.init(240, 320); 
  tft.setRotation(1); 
  
  tft.fillScreen(MAIN_BG_COLOR); 

  drawHeader("    MONITORING SYSTEM");
  drawLayoutLines();
  
  TankLevel(1, 75.2, LAYOUT_SETUP);
  TankLevel(2, 30.1, LAYOUT_SETUP);
  Temperature(25.5, LAYOUT_SETUP);
  GasLevel(15, LAYOUT_SETUP);
}

void loop() {


  // --- Simulation/Update ---
  static uint8_t simulatedTank1 = 100;
  static uint8_t simulatedTank2 = 5;
  static uint8_t simulatedTemp = 20;
  static int simulatedGas = 15;

  // Tank 1: Decreasing
  simulatedTank1 -= 5;
  if (simulatedTank1 < 5) simulatedTank1 = 100;

    simulatedTank2 += 5;
  if (simulatedTank2 > 99) simulatedTank2 = 5;
  
  // Gas: Increasing
  simulatedGas += 1;
  if (simulatedGas > 80) simulatedGas = 5;

  simulatedTemp +=1;
  if(simulatedTemp > 35)
  {
    simulatedTemp = 20;
  }
  TankLevel(1, simulatedTank1, UPDATE_VALUE );
  TankLevel(2, simulatedTank2, UPDATE_VALUE);
  Temperature(simulatedTemp, UPDATE_VALUE);
  GasLevel(simulatedGas, UPDATE_VALUE);
  delay(200);
  
}

// =====================================================================
// === GUI DRAWING FUNCTIONS ===========================================
// =====================================================================

/**
 * Draws the static header bar at the top.
 */
void drawHeader(const char* title) {
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, HEADER_BG_COLOR);
  
  tft.drawLine(0, HEADER_H - 1, SCREEN_W, HEADER_H - 1, GUI_TEXT_FG); 
  
  tft.setCursor(5, 8);
  tft.setTextColor(GUI_HEADER_TEXT); 
  tft.setTextSize(2);
  tft.print(title);
}

/**
 * Draws the dividing lines for the four panels (the main grid).
 */
void drawLayoutLines() {
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
void TankLevel(int tankId, uint8_t percentage, bool IsDataUpdate) {
  int x = (tankId == 1) ? 0 : PANEL_W;
  int y = HEADER_H;

  // --- Bar Graph (Tank Icon) ---
  int barX = x + 100;
  int barY = y + 10;
  int barW = 40;
  int barH = 80;
  
  if(!(IsDataUpdate))
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

    if(percentage == 100)
    {
      tft.setCursor(x + 1, y + 45);
    }
    else
    {
      tft.setCursor(x + 15, y + 45);
    }
    if(percentage < 25)
    {
      tft.setTextColor(ST77XX_RED, MAIN_BG_COLOR);
    }
    else
    {
      tft.setTextColor(ST77XX_GREEN, MAIN_BG_COLOR);
    }
    tft.setTextSize(4);
    // Clear the Display before Printing
    
    tft.fillRect(x + 1, y + 45, 95, 30, MAIN_BG_COLOR);

    tft.print(percentage, 1);
    tft.print("%");

    // Clear and Fill (Background BLACK, Water color BLUE)
    tft.fillRect(barX + 1, barY + 1, barW - 2, barH - 2, MAIN_BG_COLOR);
    tft.fillRect(barX + 1, fillY+1, barW - 2, fillHeight - 2, GUI_WATER_COLOR);

  }


  

}


/**
 * Draws or updates the Temperature Panel (Bottom Left).
 */
void Temperature(uint8_t temp, bool IsDataUpdate) {

  int x = 0;
  int y = HEADER_H + PANEL_H;
  // --- Icon/Value ---
  int tempX = x + 10;
  int tempY = y + 35;


  if(!IsDataUpdate)
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
      else if(temp < 25)
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
void GasLevel(int level, bool IsDataUpdate) {
  int x = PANEL_W;
  int y = HEADER_H + PANEL_H;
    // --- Radial Gauge (Simplified Half Circle) ---
  int centerX = x + 80;
  int centerY = y + 70;
  int radius = 45;

  // Static variables to store the previous needle end position
  static int prev_x2 = -1; 
  static int prev_y2 = -1;

  const int THICKNESS = 3; // Define desired thickness
  const int NEEDLE_LENGTH_PX = radius - 3; // *** NEW: 3 pixels shorter needle length ***

  uint16_t arcColor = (level < 20) ? GUI_GAS_LOW : GUI_GAS_HIGH;

  if(!IsDataUpdate)
  {
    tft.fillRect(x + 1, y + 1, PANEL_W - 2, PANEL_H - 2, MAIN_BG_COLOR);
    // --- Title (White text) ---
    tft.setCursor(x + 5, y + 5);
    tft.setTextColor(ST77XX_YELLOW, MAIN_BG_COLOR);
    tft.setTextSize(2);
    tft.print("  GAS LEVEL");
    // Draw full circle outline (White)
    tft.drawCircle(centerX, centerY, radius, GUI_TEXT_FG);
  }
  else
  {
    // Map 0-100 to 180 to 0 degrees (180=min, 0=max on top half)
    float angle = map(level, 0, 100, 180, 0); 
    float rad = angle * 0.0174533; 
    
    int x2 = centerX + (int)(NEEDLE_LENGTH_PX * cos(rad));
    int y2 = centerY - (int)(NEEDLE_LENGTH_PX * sin(rad)); 

    // ===============================================
  // *** CLEAR OLD LINE ***
  // ===============================================
  if (prev_x2 != -1) {
    // Draw the previous thick line using the background color (clearing it)
    for (int i = 0; i < THICKNESS; i++) {
        tft.drawLine(centerX, centerY + i, prev_x2, prev_y2 + i, MAIN_BG_COLOR);
        tft.drawLine(centerX + i, centerY, prev_x2 + i, prev_y2, MAIN_BG_COLOR);
    }
    // Clear the center pivot area more aggressively
    tft.fillCircle(centerX, centerY, 6, MAIN_BG_COLOR);
  }
  
  // ===============================================
  // *** DRAW NEW THICK LINE ***
  // ===============================================
  for (int i = 0; i < THICKNESS; i++) {
      // Draw the line and offset lines
      tft.drawLine(centerX, centerY + i, x2, y2 + i, arcColor);
      tft.drawLine(centerX + i, centerY, x2 + i, y2, arcColor);
  }

  tft.fillCircle(centerX, centerY, 5, arcColor); // Needle pivot
  // ===============================================

  // Update static variables for the next loop iteration
  prev_x2 = x2;
  prev_y2 = y2;
    


    // 3. Display Value (White text)
    tft.setCursor(centerX - 32, y + 75);
    tft.setTextSize(2);
    tft.fillRect(centerX - 32, y + 75, 70, 32, MAIN_BG_COLOR);
    tft.setTextColor(GUI_TEXT_FG);
    tft.print(level);
    tft.print(" PPM");

    // 1. Outer Border (White)
    int monitorY = HEADER_H;
    int monitorH = SCREEN_H - HEADER_H;  
    tft.drawRect(0, monitorY, SCREEN_W, monitorH, GUI_LINE_COLOR); 
  }
  
} // <-- MISSING BRACE ADDED HERE