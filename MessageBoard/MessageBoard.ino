// PROJECT  :MessageBoard
// PURPOSE  :Native hardware SPI version of DES Message Board code
// COURSE   :*
// AUTHOR   :C. D'Arcy
// DATE     :2020 03 18
// HARDWARE :328P (Nano) on a Breadboard with Chronodot, MSGEQ7 BoB, TC74
//          :5V/2A Adapter for power.
//          :Shared common Ground between 5V Supply and Nano
// STATUS   :Working
// REFERENCE:https://www.best-microcontroller-projects.com/max7219.html
// DISPLAY  :http://mail.rsgc.on.ca/~cdarcy/Datasheets/LEDMatrixforDESScrollingDisplay.pdf
// DRIVER   :https://mail.rsgc.on.ca/~cdarcy/Datasheets/COM-09622-MAX7219-MAX7221.pdf
// NOTES    :Native hardware SPI implementation is the fastest
//          :Much of the data has been moved into Program Memory
#define NUMMATRICES 16
#define NUMTRANSITIONS 11
#define SCROLL_SPEED 30
#include <Wire.h>
#define TC74ADDRESS 0x48    //aka B01001000 
#include <avr/pgmspace.h>   //data arrays placed in PROGMEM 
#include "Support.h"        //local header file stuffed with data arrays
#include <SPI.h>            //hardware->Fastest SCK, MOSI & SS defs from pin_arduino.h 

#define MAX7219_DECODE_MODE 0x09 //MAX7219 SPI LED Driver Command Addresses 
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
uint8_t celsius, fahrenheit;
// REFERENCE:http://nuewire.com/info-archive/msgeq7-by-j-skoba/
// NOTES    :http://darcy.rsgc.on.ca/ACES/PCBs/index.html#AudioDock
#define ANALOGPIN  7      // read from multiplexer using analog input 7
#define STROBEPIN  2      // strobe is attached to digital pin 2
#define RESETPIN   3      // reset is attached to digital pin 3
uint8_t spectrumValues[7];           // to hold A2D values

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
  MSGEQ7_Setup();
  // MSGEQ7_Test();
  //SPI preparations
  //for (uint8_t i = 0; i < 7; i++)
  //  Serial.print(pgm_read_byte(&charToPrint[94][i]) + String(" "));
  //while (1);

  randomSeed(analogRead(0));
  SPI.begin();                //sets SCK, MOSI pins for output (MISO not req'd)
  pinMode(SS, OUTPUT);        //must explicitly declare SS for output
  configMax7219s();           //sets registers for all 16 7219s
  uint8_t num = 0;
  uint8_t numItems = 7;
  while (true) {              //start rotation
    now = rtc.now();
    uint8_t h = now.hour();
    char m = h < 12 ? 'A' : 'P';
    if (h != 12) h %= 12;
    uint8_t dayNum = getDayNum();
    switch (num) {
      case 0:     //Home
        sprintf(message, "HOME OF THE RSGC ACES"); break;
      case 1:     //Date...
        sprintf(message, "%s %s %d", daysOfTheWeek[now.dayOfTheWeek()], months[now.month() - 1], now.day());
        break;
      case 2:     //Time
        sprintf(message, "%d:%02d:%02d %cM", h, now.minute(), now.second(), m);
        break;
      case 3:     //Day
        sprintf(message, "DAY %c", dayNum); break;
      case 4:     //Temp
        getTemp(celsius, fahrenheit);
        sprintf(message, "TEMP: %2dC  %2dF", celsius, fahrenheit);
        break;
      case 5:     //Weekly Note(s)
        sprintf(message, "%s: ISP PROPOSAL DUE", "4U"); break;
      case 6:     //Equalizer
        showEqualizer();
        break;
      default: break;   //should never happen
    }
    //Serial.println(message);
    //No way to turn brightness completely off ? (min: 1/32)
    //so, only way to go blank is to erase Display RAM ?
    eraseAll();           //be sure display is off before updating
    if (num != 6)
      assembleActive();     //update the buffer for the next message
    showActive();         //update the display while blank
    uint8_t which = random(NUMTRANSITIONS);
    if (which != 3) {      //for all transitions except Fade in
      brightness();        //full brightness
      if (num != 6)
        delay(3000);         //hold for 3s before beginning transition out...
    }
    switch (which) {       //currently 4 transitions
      case 1: transitionRise(); break;
      case 2: transitionFall(); break;
      case 3: transitionFadeIn(); delay(3000); break;  //bring up the lights!
      case 4: transitionFadeOut(); break;
      case 5: transitionL2R(); break;
      case 6: transitionR2L(); break;
      case 7: transitionPacManL2R(); break;
      case 8: transitionBlinkyL2R(); break;
      case 9: transitionMelt(); break;
      case 10: transitionSlice(); break;
      default: break;                       //None
    }
    delay(500);                             //hold off for 0.5s
    num = (num + 1) % numItems;             //next...
  }
}

void transitionSlice() {
  for (uint8_t a = 0; a < 128; a++)
    for (uint8_t b = 0; b < 8; b++) {
      active[a] <<= 1;
      showActive();
      delay(1);
    }

}

void transitionMelt() {
  uint16_t times = 0;
  while (times++ < 1500) {
    active[random(128)] <<= 1;
    showActive();
    delayMicroseconds(100);
  }
}


void transitionPacManL2R() {
  for (uint8_t a = 1; a < 129; a++) {
    for (uint8_t b = 0; b < 8; b++)
      active[a + b] = pgm_read_byte(&pacMan[a % 2][b]);
    active[a - 1] = 0;
    showActive();
    delay(SCROLL_SPEED);
  }
}

void transitionBlinkyL2R() {
  for (uint8_t a = 1; a < 129; a++) {
    for (uint8_t b = 0; b < 8; b++)
      active[a + b] = pgm_read_byte(&blinky[a % 2][b]);
    active[a - 1] = 0;
    showActive();
    delay(SCROLL_SPEED);
  }
}


void transitionL2R() {
  for (uint8_t a = 0; a < 128; a++) {
    for (int8_t b = 126; b > 0; b--)
      active[b + 1] = active[b];
    active[0] = 0;
    showActive();
    delay(SCROLL_SPEED);
  }
}
void transitionR2L() {
  for (uint8_t a = 0; a < 128; a++) {
    for (int8_t b = 1; b < 127; b++)
      active[b - 1] = active[b];
    active[127] = 0;
    showActive();
    delay(SCROLL_SPEED);
  }
}

void transitionFall() {
  for (uint8_t shifts = 0; shifts < 8; shifts++) {
    for (uint8_t b = 0; b < 128; b++)
      active[b] <<= 1;
    showActive();
    delay(SCROLL_SPEED);
  }
}
void transitionRise() {
  for (uint8_t shifts = 0; shifts < 8; shifts++) {
    for (uint8_t b = 0; b < 128; b++)
      active[b] >>= 1;
    showActive();
    delay(SCROLL_SPEED);
  }
}

void transitionFadeIn() {
  //timed ramp down the brightness level to 1/32 (at 0x00)
  for (int8_t level = 0; level <= 0x0F; level++) {
    digitalWrite(SS, LOW);
    for (int i = 0; i < NUMMATRICES; i++) {
      SPI.transfer(MAX7219_BRIGHTNESS);
      SPI.transfer(level);
    }
    digitalWrite(SS, HIGH);
    delay(SCROLL_SPEED << 1);
  }
}

void transitionFadeOut() {
  //timed ramp down the brightness level to 1/32 (at 0x00)
  for (int8_t level = 0x0F; level >= 0; level--) {
    digitalWrite(SS, LOW);
    for (int i = 0; i < NUMMATRICES; i++) {
      SPI.transfer(MAX7219_BRIGHTNESS);
      SPI.transfer(level);
    }
    digitalWrite(SS, HIGH);
    delay(SCROLL_SPEED);
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

void getTemp(uint8_t &celsius, uint8_t &fahrenheit) {
  Wire.beginTransmission(TC74ADDRESS); //alert the device a request is imminent
  Wire.write(0);          //inform device that the master is interested in Register 0
  Wire.endTransmission(); //terminate transmission

  Wire.requestFrom(TC74ADDRESS, 1); //request 1 byte from the device with specified address
  while (Wire.available() == 0);    //wait for response
  celsius = Wire.read();     //read 1 byte from SDA buffer into a variable
  fahrenheit = round(celsius * 1.8 + 32.0);  //convert from celsius to fahrenheit
  Serial.print(String(celsius) + "C,\t");         //echo data to the Serial Monitor
  Serial.print(String(fahrenheit, 1) + "F\n");
}

void showEqualizer() {
  sprintf(message, "EQUALIZER: M"); //M to be replaced by active audio data
  assembleActive();                 //update the buffer for the next message

  uint32_t stamp = millis();

  while (millis() - stamp < 7000) {
    digitalWrite(RESETPIN, HIGH);   //reset to first frequency..
    digitalWrite(RESETPIN, LOW);
    for (int i = 0; i < 7; i++)  {
      digitalWrite(STROBEPIN, LOW);
      delayMicroseconds(30); // to allow the output to settle
      spectrumValues[i] = analogRead(ANALOGPIN) >> 7;  //0..7
      digitalWrite(STROBEPIN, HIGH);
    }
    // update active buffer with spectrum values..
    for (uint8_t i = 0; i < 7; i++)
      active[89 + i] =  pgm_read_byte_near(&eqLevels[spectrumValues[i]]);
    showActive();           //update the display while blank
  }
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
    digitalWrite(SS, HIGH);     //Finish transfer...latch all 16 similar columns
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
      active[pos++] = pgm_read_byte_near(&charToPrint[ch][j]);
    active[pos++] = 0;
  }
  for (uint8_t i = 0; i < pad; i++)
    active[pos++] = 0;
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

void MSGEQ7_Setup() {   //MSGEQ7 Preparations
  pinMode(ANALOGPIN, INPUT);
  pinMode(STROBEPIN, OUTPUT);
  pinMode(RESETPIN, OUTPUT);
  analogReference(DEFAULT);
  digitalWrite(RESETPIN, LOW);
  digitalWrite(STROBEPIN, HIGH);
}

void MSGEQ7_Test() {
  while (1) {
    digitalWrite(RESETPIN, HIGH);   //reset to first frequency..
    digitalWrite(RESETPIN, LOW);

    for (int i = 0; i < 7; i++)  {
      digitalWrite(STROBEPIN, LOW);
      delayMicroseconds(30); // to allow the output to settle
      //     spectrumValue[i] = analogRead(analogPin);
      spectrumValues[i] = (1 << (analogRead(ANALOGPIN) >> 7)) - 1;
      Serial.print(spectrumValues[i], HEX);
      Serial.print('\t');
      digitalWrite(STROBEPIN, HIGH);
    }
    Serial.println();
  }
}
