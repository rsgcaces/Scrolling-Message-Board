// PROJECT  :MessageBoard
// PURPOSE  :Native hardware SPI version of DES Message Board code
// COURSE   :*
// AUTHOR   :C. D'Arcy
// DATE     :2020 03 15
// HARDWARE :328P (Nano) on a Breadboard with Chronodot
//          :5V/2A Adapter for power.
//          :Shared common Ground between 5V Supply and Nano
// STATUS   :Working
// REFERENCE:https://www.best-microcontroller-projects.com/max7219.html
// DISPLAY  :http://mail.rsgc.on.ca/~cdarcy/Datasheets/LEDMatrixforDESScrollingDisplay.pdf
// DRIVER   :https://mail.rsgc.on.ca/~cdarcy/Datasheets/COM-09622-MAX7219-MAX7221.pdf
// NOTES    :Native hardware SPI implementation is the fastest
//          :Much of the data has been moved into Program Memory
#define NUMMATRICES 16
#include <avr/pgmspace.h>   //arrays placed in PROGMEM where necessary
#include "Support.h"
#include <SPI.h>            //hardare->Fastest. SCK,MOSI, & SS from pin_arduino.h 
// MAX7219 SPI LED Driver Command Addresses
#define MAX7219_DECODE_MODE 0x09 // 
#define MAX7219_BRIGHTNESS  0x0A // 
#define MAX7219_SCAN_LIMIT  0x0B // 
#define MAX7219_SHUTDOWN    0x0C // 
#define MAX7219_DISPLAYTEST 0x0F // 
#define DISPLAY_NORMAL      0x00 //DISPLAYTEST Constants...
#define DISPLAY_TEST        0x01 //

#include "RTClib.h"              //Chronodot support
RTC_DS3231 rtc;                  //
const char daysOfTheWeek[7][10] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
const char months[12][10] = {"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};
DateTime now;
//Place this monster array in Program Memory
const byte schoolYear[12][32] PROGMEM = { // Day Calendar 2019 - 2020
  {9, 9, 9, 9, 9, 9, 0, 8, 1, 2, 3, 9, 9, 4, 5, 6, 7, 8, 9, 9, 1, 2, 3, 4, 5, 9, 9, 6, 7, 8, 1, 2}, //january
  {9, 9, 9, 3, 4, 5, 6, 7, 9, 9, 8, 1, 2, 3, 9, 9, 9, 9, 4, 5, 6, 7, 9, 9, 8, 1, 2, 3, 4, 9}, //february
  {9, 9, 5, 6, 7, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 1, 2, 3, 4, 9, 9, 5, 6}, //march
  {9, 7, 8, 1, 9, 9, 2, 3, 4, 5, 9, 9, 9, 9, 6, 7, 8, 1, 9, 9, 2, 3, 4, 5, 6, 9, 9, 7, 8, 1, 2}, //april
  {9, 3, 9, 9, 4, 5, 6, 7, 8, 9, 9, 1, 2, 3, 4, 5, 9, 9, 9, 6, 7, 8, 1, 9, 9, 2, 3, 4, 5, 6, 9, 9}, // may
  {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9}, //june
  {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9}, // july
  {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9}, //august
  {9, 9, 9, 9, 9, 1, 2, 9, 9, 3, 4, 5, 6, 7, 9, 9, 8, 1, 2, 3, 0, 9, 9, 4, 5, 6, 7, 8, 9, 9, 1}, // september
  {9, 2, 3, 4, 5, 9, 9, 6, 7, 8, 1, 2, 9, 9, 9, 3, 4, 5, 6, 9, 9, 7, 8, 1, 2, 3, 9, 9, 4, 5, 6, 7}, //october
  {9, 8, 9, 9, 1, 2, 3, 4, 9, 9, 9, 9, 5, 6, 7, 8, 9, 9, 1, 2, 3, 4, 5, 9, 9, 6, 7, 8, 1, 2, 9}, //november
  {9, 9, 3, 4, 5, 6, 7, 9, 9, 8, 1, 2, 3, 4, 9, 9, 5, 6, 7, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9} //december
};

char active[128];                // Display data arrays...
char message[128];               //

void setup() {
  Serial.begin(9600);
  if (!rtc.begin()) {
    //F() macro places String in PROGMEM
    Serial.println(F("Couldn't find RTC"));
    while (1);
  }
  //rtc.adjust(DateTime(2020, 3, 15, 16, 45, 0));
  //printDate();
  //SPI preparations
  SPI.begin();                //sets SCK, MOSI pins for output (MISO not req'd)
  pinMode(SS, OUTPUT);        //must explicitly declare SS for output
  configMax7219s();           //
  uint8_t num = 0;
  uint8_t numItems = 5;
  while (true) {              //start rotation
    //Serial.println(num);
    now = rtc.now();
    uint8_t h = now.hour();
    char m = h < 12 ? 'A' : 'P';
    if (h != 12) h %= 12;
    uint8_t dayNum = getDayNum();
    switch (num) {            //Home
      case 0: sprintf(message, "HOME OF THE RSGC ACES"); break;
      case 1:     //Date...
        sprintf(message, "%s %s %d, %d", daysOfTheWeek[now.dayOfTheWeek()], months[now.month() - 1], now.day(), now.year());
        break;
      case 2:     //Time
        sprintf(message, "%d:%02d:%02d %cM", h, now.minute(), now.second(), m);
        break;
      case 3:     //Day
        sprintf(message, "DAY %c", dayNum); break;
      case 4:
        sprintf(message, "4U: %s", "ISP PROPOSALS DUE"); break;
      default: break;
    }
    Serial.println(message);
    assembleActive();
    brightness();
    showActive();
    delay(3000);                  //hold for 3s
    fadeToBlack();
    delay(500);                   //hold off for 0.5s
    num = (num + 1) % numItems;   //next...
  }
}

void fadeToBlack() {
  //timed ramp down the brightness level to 1/32 (at 0x00)
  for (int8_t level = 0x0F; level >= 0; level--) {
    digitalWrite(SS, LOW);
    for (int i = 0; i < NUMMATRICES; i++) {
      SPI.transfer(MAX7219_BRIGHTNESS);
      SPI.transfer(level);
    }
    digitalWrite(SS, HIGH);
    delay(30);
  }
  //only way to go to black is to erase Display RAM
  eraseAll();
}

void brightness() {
  digitalWrite(SS, LOW);
  for (int i = 0; i < NUMMATRICES; i++) {
    SPI.transfer(MAX7219_BRIGHTNESS);
    SPI.transfer(0x0F);
  }
  digitalWrite(SS, HIGH);
  clearPrevious();            //? req'd when changing columns...
}

char getDayNum() {
  uint8_t dayNum = pgm_read_byte(&schoolYear[now.month() - 1][now.day()]);
  return dayNum < 9 ? '0' + dayNum : 'H';
}

void eraseAll() {
  for (uint8_t col = 0; col < 8; col++) {
    for (uint8_t m = 0; m < NUMMATRICES; m++) {
      digitalWrite(SS, LOW);
      SPI.transfer(col + 1);
      SPI.transfer(0);
      digitalWrite(SS, HIGH);
    }
    clearPrevious();            //? req'd when changing columns...
  }
}

void showActive() {
  for (uint8_t col = 1; col < 9; col++) {
    digitalWrite(SS, LOW);  //Initiate transfer...
    //load back to front...
    for (int8_t i = 119 + col; i >= 0; i -= 8) {
      SPI.transfer(col);         // Send (digit) address (eff. column)
      SPI.transfer(active[i]);   // Send (segment) data  (eff. row)
    }
    digitalWrite(SS, HIGH); //Finish transfer...latch all 16 similar columns
    clearPrevious();            //? req'd when changing columns...
  }
}

void clearPrevious() {
  digitalWrite(SS, LOW);
  for (int i = 0; i < NUMMATRICES; i++) {
    SPI.transfer(0);
    SPI.transfer(0);
  }
  digitalWrite(SS, HIGH);
}

void assembleActive() {
  uint8_t messageLength = 0;
  for (uint8_t i = 0; i < strlen(message); i++) {
    char ch = message[i] - 32;
    //   messageLength += pgm_read_byte_near(charWidth[ch]);
    messageLength += pgm_read_byte_near(charWidth + ch);
  }
  //  Serial.print(F("Message Length: "));
  //  Serial.println(messageLength);
  uint8_t pad = (128 - messageLength) >> 1;
  //  Serial.println(pad);
  uint8_t pos = 0;
  for (uint8_t i = 0; i < pad; i++)
    active[pos++] = 0;
  for (uint8_t i = 0; i < strlen(message); i++) {
    char ch = message[i] - 32;
    for (uint8_t j = 0; j < pgm_read_byte_near(charWidth + ch) - 1; j++)
      //    for (uint8_t j = 0; j < charWidth[ch] - 1; j++)
      active[pos++] = charToPrint[ch][j];
    active[pos++] = 0;
  }
  for (uint8_t i = 0; i < pad; i++)
    active[pos++] = 0;
  // Serial.println(pos);
}

void printActive() {
  for (uint8_t i = 0; i < 128; i++) {
    Serial.print(active[i] + String("\t"));
    if (i % 8 == 7)
      Serial.println();
  }
}

void configMax7219s() {
  // First, all LED segments lit to verify
  digitalWrite(SS, LOW);
  for (uint8_t i = 0; i < NUMMATRICES; i++) {
    SPI.transfer(MAX7219_DISPLAYTEST);
    SPI.transfer(DISPLAY_TEST);
  }
  digitalWrite(SS, HIGH);
  //hold for a period...
  delay(1000);
  //SETS THE SCAN RATE OF THE MAX'S
  digitalWrite(SS, LOW);
  for (int i = 0; i < NUMMATRICES; i++)  {
    SPI.transfer(MAX7219_SCAN_LIMIT);
    SPI.transfer(0x07);
  }
  digitalWrite(SS, HIGH);
  //SETS THERE TO BE NO DECODE MODE THAT IS MEANT FOR DRIVING 7 SEGMENT DISPLAYS
  digitalWrite(SS, LOW);
  for (int i = 0; i < NUMMATRICES; i++) {
    SPI.transfer(MAX7219_DECODE_MODE);
    SPI.transfer(0x00);
  }
  digitalWrite(SS, HIGH);

  //TAKING THE MAX'S OUT OF SHUTDOWN MODE
  digitalWrite(SS, LOW);
  for (int i = 0; i < NUMMATRICES; i++) {
    SPI.transfer(MAX7219_SHUTDOWN);
    SPI.transfer(0x01);
  }
  digitalWrite(SS, HIGH);

  //SETTING THE DISPLAY TEST TO NONE ALL THIS DOES IS LIGHT UP ALL OF THE LEDS
  digitalWrite(SS, LOW);
  for (int i = 0; i < NUMMATRICES; i++) {
    SPI.transfer(MAX7219_DISPLAYTEST);
    SPI.transfer(DISPLAY_NORMAL);
  }
  digitalWrite(SS, HIGH);

  //THIS SETS THE INTENSITY OF THE LEDS BRIGHTNESS
  digitalWrite(SS, LOW);
  for (int i = 0; i < NUMMATRICES; i++) {
    SPI.transfer(MAX7219_BRIGHTNESS);
    SPI.transfer(0x0F);
  }
  digitalWrite(SS, HIGH);

  //THIS CLEARS ALL OF THE DISPLAYS
  for (int i = 0; i < 8; i++) {
    digitalWrite(SS, LOW);
    for (int j = 0; j < NUMMATRICES; j++) {
      SPI.transfer(i);
      SPI.transfer(0);
    }
    digitalWrite(SS, HIGH);
  }
}

void loop() {}

void printDate() {
  DateTime now = rtc.now();
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  //  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
}
