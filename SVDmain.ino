// Includes and global variables remain unchanged
#include <FlickerFreePrint.h>
#include <ComEVesc.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "EEPROMAnything.h"

float trip;
float startup_total_km;
float last_total_km_stored;
float total_km;
float tacho;
float rpm;
float speed;
float watts;
float wheel_diameter;
int maxspeed;
int brightness = 255;
char fmt[10];

#define SPEEDFONT 7
#define DATAFONTSMALL2 2
#define DATAFONTSMALL 4
#define DATAFONTSMALLTEXT 1

#define MOTOR_POLES 30
#define WHEEL_DIAMETER_MM 246
#define GEAR_RAITO 1.0
#define PI 3.141592
#define SCONST 0.12

#define RXD2 22
#define TXD2 27
#define LDR_PIN 34
#define LCD_BACK_LIGHT_PIN 21
#define COOLING_PIN 35

#define DEMO_MODE true
float demoVoltage      = 40.0;
bool  demoVoltageUp    = true;
int   demoWatts        = 0;
bool  demoWattsUp      = true;
int   demoSpeed        = 0;
bool  demoSpeedUp      = true;
int   demoVescT        = 0;
bool  demoVescTUp      = true;
int   demoMotorT       = 0;
bool  demoMotorTUp     = true;
float demoMiles        = 0;
bool  demoMilesUp      = true;
float demoBattPct      = 100;
bool  demoBattPctDown  = true;
float demoPhaseAmps    = 0;
bool  demoPhaseAmpsUp  = true;

int EEPROM_MAGIC_VALUE = 0;
#define EEPROM_UPDATE_EACH_KM 0.1

int COLOR_WARNING_SPEED = TFT_RED;
#define HIGH_SPEED_WARNING 60

#define DO_LOGO_DRAW
#define DEBUG_MODE

#ifdef DO_LOGO_DRAW
#include <PNGdec.h>
#include "startup_image.h"
#include "background_image.h"
#include "menu_image.h"
PNG png;
int16_t xpos = 0;
int16_t ypos = 0;
#define MAX_IMAGE_WDITH 320
#endif

int Screen_refresh_delay = 250;

// Screen state variables
enum ScreenState {
  MAIN_SCREEN,
  MENU_SCREEN
};
ScreenState currentScreen = MAIN_SCREEN;
unsigned long lastButtonTime = 0;
unsigned long doublePressDelay = 500; // 500ms window for double press
bool waitingForSecondPress = false;
bool menuBackgroundDrawn = false;
bool lastButtonState = HIGH;
bool currentButtonState = HIGH;

// Define button pin - you can change this to any available digital pin
#define MENU_BUTTON_PIN 0  // Using GPIO 0 (boot button on many ESP32 boards)

ComEVesc UART;
#define VescSerial Serial1

TFT_eSPI tft = TFT_eSPI();
FlickerFreePrint<TFT_eSPI> Data1(&tft, COLOR_WARNING_SPEED, TFT_BLACK);
FlickerFreePrint<TFT_eSPI> Data4(&tft, TFT_WHITE,      TFT_BLACK);
FlickerFreePrint<TFT_eSPI> Data4t(&tft, TFT_WHITE,     TFT_BLACK);
FlickerFreePrint<TFT_eSPI> Data10(&tft, TFT_WHITE,     TFT_BLACK);
FlickerFreePrint<TFT_eSPI> Data10t(&tft, TFT_WHITE,    TFT_BLACK);
FlickerFreePrint<TFT_eSPI> Data9(&tft, TFT_WHITE,      TFT_BLACK);
FlickerFreePrint<TFT_eSPI> Data9t(&tft, TFT_WHITE,     TFT_BLACK);
FlickerFreePrint<TFT_eSPI> Data2(&tft, TFT_WHITE,      TFT_BLACK);
FlickerFreePrint<TFT_eSPI> Data2t(&tft, TFT_WHITE,     TFT_BLACK);
FlickerFreePrint<TFT_eSPI> Data11(&tft, TFT_WHITE,     TFT_BLACK);
FlickerFreePrint<TFT_eSPI> Data11t(&tft, TFT_WHITE,    TFT_BLACK);
FlickerFreePrint<TFT_eSPI> Data12(&tft, TFT_WHITE,     TFT_BLACK);
FlickerFreePrint<TFT_eSPI> Data12t(&tft, TFT_WHITE,    TFT_BLACK);
FlickerFreePrint<TFT_eSPI> Data13(&tft, TFT_WHITE,     TFT_BLACK);
FlickerFreePrint<TFT_eSPI> Data13t(&tft, TFT_WHITE,    TFT_BLACK);

void pngDraw(PNGDRAW *pDraw) {
  uint16_t lineBuffer[MAX_IMAGE_WDITH];
  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  tft.pushImage(xpos, ypos + pDraw->y, pDraw->iWidth, 1, lineBuffer);
}

void handleButton() {
  currentButtonState = digitalRead(MENU_BUTTON_PIN);
  
  // Detect button press (transition from HIGH to LOW)
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    unsigned long currentTime = millis();
    
    if (waitingForSecondPress && (currentTime - lastButtonTime) <= doublePressDelay) {
      // Double press detected!
      waitingForSecondPress = false;
      
      if (currentScreen == MAIN_SCREEN) {
        currentScreen = MENU_SCREEN;
        menuBackgroundDrawn = false; // Force redraw of menu background
      } else {
        currentScreen = MAIN_SCREEN;
        // Clear screen and redraw main background
        tft.fillScreen(TFT_BLACK);
        int16_t rc_mainbg = png.openFLASH((uint8_t *)background_image, sizeof(background_image), pngDraw);
        if (rc_mainbg == PNG_SUCCESS) {
          tft.startWrite();
          png.decode(NULL, 0);
          tft.endWrite();
        }
        png.close();
      }
    } else {
      // First press
      waitingForSecondPress = true;
      lastButtonTime = currentTime;
    }
  }
  
  lastButtonState = currentButtonState;
  
  // Reset double press if too much time has passed
  if (waitingForSecondPress && (millis() - lastButtonTime) > doublePressDelay) {
    waitingForSecondPress = false;
  }
}

void drawMenuScreen() {
  if (!menuBackgroundDrawn) {
    tft.fillScreen(TFT_BLACK);
    int16_t rc_menubg = png.openFLASH((uint8_t *)menu_image, sizeof(menu_image), pngDraw);
    if (rc_menubg == PNG_SUCCESS) {
      tft.startWrite();
      png.decode(NULL, 0);
      tft.endWrite();
    }
    png.close();
    menuBackgroundDrawn = true;
  }
  
  // Add any menu-specific content here
  // For now, it's just a blank screen with the menu background
}

void checkvalues() {
  total_km = startup_total_km + trip;
  if (total_km - last_total_km_stored >= EEPROM_UPDATE_EACH_KM) {
    last_total_km_stored = total_km;
    EEPROM_writeAnything(EEPROM_MAGIC_VALUE, total_km);
  }
}

void setup(void) {
  if (!DEMO_MODE) {
    VescSerial.begin(115200);
    UART.setSerialPort(&VescSerial);
  }
#ifdef DEBUG_MODE
  Serial.begin(115200);
  if (!DEMO_MODE) UART.setDebugPort(&Serial);
#endif

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  pinMode(COOLING_PIN, OUTPUT);
  digitalWrite(COOLING_PIN, LOW);
  
  // Setup menu button pin
  pinMode(MENU_BUTTON_PIN, INPUT_PULLUP);

  if (!DEMO_MODE) UART.getVescValues();

  EEPROM_readAnything(EEPROM_MAGIC_VALUE, startup_total_km);
  if (isnan(startup_total_km) || startup_total_km < 0 || startup_total_km > 100000) {
    startup_total_km = 0.0;
    last_total_km_stored = 0.0;
    for (int i = 0; i < 100; i++) {
      EEPROM_writeAnything(i, 0);
    }
    EEPROM_writeAnything(EEPROM_MAGIC_VALUE, startup_total_km);
    delay(1000);
  }

  last_total_km_stored = startup_total_km;
  tacho = (UART.data.tachometerAbs / (MOTOR_POLES * 3));
  trip  = tacho / 1000;
  if (startup_total_km != 0) startup_total_km -= trip;

#ifdef DO_LOGO_DRAW
  int16_t rc_bg = png.openFLASH((uint8_t *)startup_image, sizeof(startup_image), pngDraw);
  if (rc_bg == PNG_SUCCESS) {
    tft.startWrite();
    png.decode(NULL, 0);
    tft.endWrite();
  }
  png.close();
  delay(3000);
  tft.fillScreen(TFT_BLACK);
  int16_t rc_mainbg = png.openFLASH((uint8_t *)background_image, sizeof(background_image), pngDraw);
  if (rc_mainbg == PNG_SUCCESS) {
    tft.startWrite();
    png.decode(NULL, 0);
    tft.endWrite();
  }
  png.close();
#endif
}

void loop() {
  // Handle button input first
  handleButton();
  
  if (currentScreen == MENU_SCREEN) {
    drawMenuScreen();
    delay(50); // Small delay for menu screen
    return;
  }
  
  // Main screen code below
  bool vescOk = DEMO_MODE ? true : UART.getVescValues();

  if (DEMO_MODE) {
    // Battery Voltage
    demoVoltage += demoVoltageUp ? 0.5 : -0.5;
    if (demoVoltage >= 140.0) demoVoltageUp = false;
    else if (demoVoltage <= 40.0) demoVoltageUp = true;

    // Watts
    int wattsStep = random(100, 500);
    demoWatts += demoWattsUp ? wattsStep : -wattsStep;
    if (demoWatts >= 10000) { demoWatts = 10000; demoWattsUp = false; }
    if (demoWatts <= 0)     { demoWatts = 0;     demoWattsUp = true;  }

    // Speed
    int speedStep = random(1, 6) * 5;
    demoSpeed += demoSpeedUp ? speedStep : -speedStep;
    if (demoSpeed >= 100) { demoSpeed = 100; demoSpeedUp = false; }
    if (demoSpeed <= 0)   { demoSpeed = 0;   demoSpeedUp = true;  }

    // VescT
    int vescStep = random(5, 16);
    demoVescT += demoVescTUp ? vescStep : -vescStep;
    if (demoVescT >= 150) { demoVescT = 150; demoVescTUp = false; }
    if (demoVescT <= 0)   { demoVescT = 0;   demoVescTUp = true;  }

    // MotorT
    int motorStep = random(5, 16);
    demoMotorT += demoMotorTUp ? motorStep : -motorStep;
    if (demoMotorT >= 150) { demoMotorT = 150; demoMotorTUp = false; }
    if (demoMotorT <= 0)   { demoMotorT = 0;   demoMotorTUp = true;  }

    // Miles (0–100)
    int milesStep = random(1, 6);
    demoMiles += demoMilesUp ? milesStep : -milesStep;
    if (demoMiles >= 100) { demoMiles = 100; demoMilesUp = false; }
    if (demoMiles <= 0)   { demoMiles = 0;   demoMilesUp = true;  }

    // Batt%
    demoBattPct -= 1;
    if (demoBattPct <= 0) demoBattPct = 100;

    // Phase Amps (0–400)
    int phaseStep = random(1, 21);
    demoPhaseAmps += demoPhaseAmpsUp ? phaseStep : -phaseStep;
    if (demoPhaseAmps >= 400) { demoPhaseAmps = 400; demoPhaseAmpsUp = false; }
    if (demoPhaseAmps <= 0)   { demoPhaseAmps = 0;   demoPhaseAmpsUp = true;  }

    // Write back to UART.data for display
    UART.data.inpVoltage      = demoVoltage;
    UART.data.avgInputCurrent = demoWatts / demoVoltage;
    UART.data.tachometerAbs   = demoMiles * 1000;  // used for record only
    UART.data.ampHours        = demoPhaseAmps;     // Phase Amps
    UART.data.tempMosfet      = demoVescT;
    UART.data.tempMotor       = demoMotorT;
    speed                     = demoSpeed;
    watts                     = demoWatts;
  }

  // DEMO label
  tft.setCursor(55, 5);
  tft.setTextFont(2);
  tft.setTextColor(TFT_YELLOW);
  if (DEMO_MODE) tft.print("DEMO");

  // Speed
  int speedINT = max((int)speed, 0);
  COLOR_WARNING_SPEED = speedINT > HIGH_SPEED_WARNING ? TFT_RED : TFT_WHITE;
  tft.setTextFont(8);
  tft.setCursor(110, 50);
  Data1.setTextColor(COLOR_WARNING_SPEED, TFT_BLACK);
  Data1.print(speedINT);
  tft.setCursor(115, 175);
  tft.setTextFont(DATAFONTSMALLTEXT);
  tft.setTextColor(TFT_WHITE);
  tft.print("MPH");

  // Battery Voltage
  tft.setTextFont(DATAFONTSMALL2);
  tft.setCursor(10, 3);
  dtostrf(UART.data.inpVoltage, 3, 1, fmt);
  int batteryColor;
  if      (UART.data.inpVoltage <= 55.0)  batteryColor = TFT_RED;
  else if (UART.data.inpVoltage <= 65.0)  batteryColor = TFT_YELLOW;
  else if (UART.data.inpVoltage <= 120.0) batteryColor = TFT_WHITE;
  else if (UART.data.inpVoltage <= 140.0) batteryColor = TFT_GREEN;
  else                                    batteryColor = TFT_RED;
  Data4.setTextColor(batteryColor, TFT_BLACK);
  Data4.print(fmt);
  tft.setCursor(10, 20);
  tft.setTextFont(1);
  Data4t.setTextColor(TFT_WHITE, TFT_BLACK);
  Data4t.print("Battery");

  // Watts
  tft.setTextFont(DATAFONTSMALL2);
  tft.setCursor(260, 3);
  dtostrf(watts, 5, 0, fmt);
  int wattsColor;
  if      (watts <= 5000)  wattsColor = TFT_WHITE;
  else if (watts <= 8500)  wattsColor = TFT_YELLOW;
  else                      wattsColor = TFT_RED;
  Data10.setTextColor(wattsColor, TFT_BLACK);
  Data10.print(fmt);
  tft.setCursor(270, 22);
  tft.setTextFont(1);
  Data10t.setTextColor(TFT_WHITE, TFT_BLACK);
  Data10t.print("Watts");

  // Miles
  float milesValue = DEMO_MODE ? demoMiles : (startup_total_km + (UART.data.tachometerAbs/(MOTOR_POLES*3))/1000.0) * 0.621371;
  tft.setCursor(145, 200);
  tft.setTextFont(DATAFONTSMALL);
  dtostrf(milesValue, 4, 0, fmt);
  Data9.setTextColor(TFT_WHITE, TFT_BLACK);
  Data9.print(fmt);
  tft.setCursor(150, 224);
  tft.setTextFont(DATAFONTSMALLTEXT);
  Data9t.setTextColor(TFT_WHITE, TFT_BLACK);
  Data9t.print("Miles");

  // VescT
  tft.setCursor(5, 200);
  tft.setTextFont(DATAFONTSMALL);
  dtostrf(UART.data.tempMosfet, 3, 0, fmt);
  int vescColor;
  if      (UART.data.tempMosfet <= 70)  vescColor = TFT_WHITE;
  else if (UART.data.tempMosfet <= 95)  vescColor = TFT_YELLOW;
  else                                   vescColor = TFT_RED;
  Data2.setTextColor(vescColor, TFT_BLACK);
  Data2.print(fmt);
  tft.setCursor(10, 224);
  tft.setTextFont(DATAFONTSMALLTEXT);
  Data2t.setTextColor(TFT_WHITE, TFT_BLACK);
  Data2t.print("VescT");

  // MotorT
  tft.setCursor(65, 200);
  tft.setTextFont(DATAFONTSMALL);
  dtostrf(UART.data.tempMotor, 3, 0, fmt);
  int motorColor;
  if      (UART.data.tempMotor <= 70)  motorColor = TFT_WHITE;
  else if (UART.data.tempMotor <= 95)  motorColor = TFT_YELLOW;
  else                                  motorColor = TFT_RED;
  Data11.setTextColor(motorColor, TFT_BLACK);
  Data11.print(fmt);
  tft.setCursor(70, 224);
  tft.setTextFont(DATAFONTSMALLTEXT);
  Data11t.setTextColor(TFT_WHITE, TFT_BLACK);
  Data11t.print("MotorT");

  // Batt%
  tft.setCursor(200, 200);
  tft.setTextFont(DATAFONTSMALL);
  dtostrf(demoBattPct, 4, 0, fmt);
  int battPctColor;
  if      (demoBattPct >= 95) battPctColor = TFT_GREEN;
  else if (demoBattPct >= 40) battPctColor = TFT_WHITE;
  else if (demoBattPct >= 30) battPctColor = TFT_YELLOW;
  else                         battPctColor = TFT_RED;
  Data12.setTextColor(battPctColor, TFT_BLACK);
  Data12.print(fmt);
  tft.setCursor(205, 224);
  tft.setTextFont(DATAFONTSMALLTEXT);
  Data12t.setTextColor(TFT_WHITE, TFT_BLACK);
  Data12t.print("Batt%");

  // Phase Amps
  tft.setCursor(260, 200);
  tft.setTextFont(DATAFONTSMALL);
  dtostrf(UART.data.ampHours, 4, 0, fmt);
  int phaseAmpsColor;
  if      (UART.data.ampHours <= 300) phaseAmpsColor = TFT_WHITE;
  else if (UART.data.ampHours <= 350) phaseAmpsColor = TFT_YELLOW;
  else                                 phaseAmpsColor = TFT_RED;
  Data13.setTextColor(phaseAmpsColor, TFT_BLACK);
  Data13.print(fmt);
  tft.setCursor(260, 224);
  tft.setTextFont(DATAFONTSMALLTEXT);
  Data13t.setTextColor(TFT_WHITE, TFT_BLACK);
  Data13t.print("Phase Amps");

  delay(Screen_refresh_delay);
  checkvalues();
}
