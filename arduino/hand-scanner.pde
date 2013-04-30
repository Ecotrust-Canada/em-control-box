////////////////////////////////////////////////////////////////////////
// RFID Reader for Ecotrust Canada
// Written by: Steven Stuber
// April 17th 2012
// Memory, processing, SD, and power optimizations
// by Tom Karpik, May 10th 2012
////////////////////////////////////////////////////////////////////////
#include <avr/sleep.h>
#include <SoftwareSerial.h>
#include <SD.h>

#define WAKE_PIN            2
#define LCD_TX_PIN          3
#define RFID_RX_PIN         4

#define LCD_CLEAR           0
#define LCD_LINE1           1
#define LCD_LINE2           2
#define LCD_BACKLIGHT_ON    3
#define LCD_BACKLIGHT_OFF   4

byte LCD_commands[] = { 0x01, 128, 192, 157, 128 };

#define RFID_DATA_BYTES     10
#define RFID_CHK_BYTES      2
#define RFID_START_BYTE     0x3A
#define RFID_STOP_BYTE      0x0D

#define SLEEP_DELAY         5 // seconds

//int sleepStatus = 0; // variable to store a request for sleep

// since the LCD does not send data back to the Arduino
// we should only define the txPin
// the third argument being passed inverts the logic levels if true
// the RFID reader board was sending inverted RS232
SoftwareSerial LCD = SoftwareSerial(-1, LCD_TX_PIN , false);
SoftwareSerial RFID = SoftwareSerial(RFID_RX_PIN, -1, true);

boolean usingSD = false;
File fp;

///////Checksum Explained///////////////////////////////////////////////
// The data sent by the RFID scanner looks like the following
//                   : 0 4 1 2 A D 9 6 A 6 3 2 \cr
// this represents a total of 14 bytes of data
// the : represents the start byte
// the \cr represents the end byte
// the first 10 bytes (0 4 1 2 A D 9 6 A 6) are sent as characters
// they represent the hex value of the rfid tag
// the next 2 bytes (3 2) represent the hex characters of the checksum

// the proper way to do the checksum is to add the character values of
// all of the first 10 bytes together and then mod this value with 256
// this modulus operation gets us the last two hex digits which is what
// we are interested in.

// then we take the last two bytes and convert the value (32 in hex) from
// the characters '3' and '2' into 0x32 (hex) and then compare that to
// the two hex digits we got previously.
////////////////////////////////////////////////////////////////////////
byte c = 0;  // main character buffer reading rfid rs232 data
byte rfid_data[RFID_DATA_BYTES + RFID_CHK_BYTES],
     prev_data1[RFID_DATA_BYTES + RFID_CHK_BYTES],
     prev_data2[RFID_DATA_BYTES + RFID_CHK_BYTES];  // buffer containing the rfid's 10 letter code and chksum
unsigned int rsum; // running checksum
unsigned long time;

void setup() {
  pinMode(WAKE_PIN, INPUT);
  pinMode(LCD_TX_PIN, OUTPUT);
  pinMode(RFID_RX_PIN, INPUT);

  digitalWrite(WAKE_PIN, HIGH); // LOW will trigger interrupt

  LCD.begin(9600);
  RFID.begin(9600);
  delay(50);

  // clear the screen and print the ecotrust splash
  LCDCommand(LCD_BACKLIGHT_ON);
  LCDCommand(LCD_CLEAR);
  LCDCommand(LCD_LINE1);
  LCD.print("Ecotrust");
  LCDCommand(LCD_LINE2);
  LCD.print("RFID Scan 1.2");

  memset(prev_data1, ' ', RFID_DATA_BYTES); // set previous tags to blank spaces so that there is no gibberish on LCD screen
  memset(prev_data2, ' ', RFID_DATA_BYTES); // on first scan (only affects OLD tag)
  delay(1000);
  
  LCDCommand(LCD_CLEAR);
  LCDCommand(LCD_LINE1);

  if (!SD.begin()) {
    LCD.print("SD card failure");
    LCDCommand(LCD_LINE2);
    LCD.print("Not logging data");
    delay(2000);
    LCDCommand(LCD_CLEAR);
    LCDCommand(LCD_LINE1);
  } else {
    if ((fp = SD.open("tags.txt", O_CREAT | O_APPEND | O_WRITE)) == false) {
      LCD.print("Log file failure");
      LCDCommand(LCD_LINE2);
      LCD.print("Not logging data");
      delay(2000);
      LCDCommand(LCD_CLEAR);
      LCDCommand(LCD_LINE1);
    } else {
      usingSD = true;
    }
  }

  LCD.print("Ready ...");
  
  LCDCommand(LCD_LINE2);
  if (usingSD) { 
    LCD.print("Logging ENABLED");
  } else {
    LCD.print("Logging DISABLED");
  }

  attachInterrupt(0, wakeUpNow, LOW); // use interrupt 0 (pin 2) and run wakeUpNow when pin2 gets LOW

  time = millis();
}

void loop() {
  // while there are no bytes available from the RFID scanner
  while (RFID.available() == 0) {
    // has SLEEP_DELAY seconds elapsed?
    if (millis() - time >= SLEEP_DELAY * 1000) {
      time = millis(); // set timestamp
      sleepNow();
    }

    delay(100);
  }

  // if we got here that means there is data in the serial buffer; keep eating data from the RFID until start byte is encountered
  while ((c = RFID.read()) == RFID_START_BYTE) {
    // got start byte, we can now start reading the rest of the data
    rsum = 0; 

    // read the next RFID_DATA_BYTES + RFID_CHK_BYTES (10 + 2)
    for (int currentByte = 0; currentByte < RFID_DATA_BYTES + RFID_CHK_BYTES; currentByte++) {
      rfid_data[currentByte] = RFID.read();
      
      // rsum everything but the RFID_CHK_BYTES
      if (currentByte < RFID_DATA_BYTES) rsum += rfid_data[currentByte];
    }
    
    // if we got a stop byte let's check the data
    if ((c = RFID.read()) == RFID_STOP_BYTE) {
      if (rsum % 256 == GetHexByte(rfid_data[RFID_DATA_BYTES], rfid_data[RFID_DATA_BYTES + 1])) {
        // checksum successful
        time = millis();

        // compare this new (good) data with the previous data
        for (int i = 0; i < RFID_DATA_BYTES; i++) {
          // if something differs
          if (rfid_data[i] != prev_data1[i]) {
            // store the new result, move the old one over
            memcpy(prev_data2, prev_data1, RFID_DATA_BYTES);
            memcpy(prev_data1, rfid_data, RFID_DATA_BYTES);
            UpdateLCD(); // and update the screen
            
            if (usingSD) {
              fp.write(rfid_data, 10);
              fp.println();
              fp.flush();
            }

            break;
          }
        }
      } else {
        // bad checksum
        //LCDCommand(LCD_CLEAR);
        //LCD.print("Bad Checksum");
      }
    } else {
      // after RFID_DATA_BYTES + RFID_CHK_BYTES the next byte was not a stop byte
      //LCDCommand(LCD_CLEAR);
      //LCD.print("No Stop Byte");
    }
    
    break; // done one data read attempt, give first while loop a chance to run (if there turns out to be no data in the buffer)
  }
}

void UpdateLCD() {
  LCDCommand(LCD_LINE1);
  LCD.write("New ");
  LCD.write(prev_data1, RFID_DATA_BYTES);
  if (!usingSD) LCD.write(" !");
  
  LCDCommand(LCD_LINE2);
  LCD.write("Old ");
  LCD.write(prev_data2, RFID_DATA_BYTES);
  LCD.write("  ");
}

// given an ASCII char c that represents a hex (0-f) value, return the value
byte ASCIIToHex(byte c) {
  if (c >= '0' && c <= '9')      return c - '0';
  else if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  else if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  else return -1;   // getting here is bad: it means the character was invalid
}

// given two ASCII-encoded hex chars, return the full byte
byte GetHexByte(byte a1, byte a2) {
  byte a = ASCIIToHex(a1);
  byte b = ASCIIToHex(a2);

  if (a<0 || b<0) return -1;  // an invalid hex character was encountered
  else return (a*16) + b;
}

// put the Arduino to sleep
void sleepNow() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();  // enables the sleep bit in the mcucr register so sleep is possible; just a safety pin

  LCDCommand(LCD_BACKLIGHT_OFF);
  attachInterrupt(0, wakeUpNow, LOW);
  sleep_mode();  // here the device is actually put to sleep!!

  // THE PROGRAM CONTINUES FROM HERE AFTER WAKING UP

  sleep_disable();
  detachInterrupt(0); // disables interrupt so wakeUpNow will not be executed during normal running time
  LCDCommand(LCD_BACKLIGHT_ON);
}

void wakeUpNow() {
  // here the interrupt is handled after wakeup

  // execute code here after wake-up before returning to the loop() function
  // timers and code using timers (serial.print and more...) will not work here.
  // we don't really need to execute any special functions here, since we
  // just want the thing to wake up
}

/*boolean doesFileExist(String name) {
  File root = SD.open("/"), file;

  while((file = root.openNextFile()) != false) {
    if ((String)file.name() == name && !file.isDirectory()) return true;
  }
  
  return false;
}*/

// the following functions operate the LCD screen by issueing the special
// command 0xFE to the screen which then is followed by several different
// possible values allowing us to clear the screen, or select cursor locations
void LCDCommand(int command) {
  switch(command) {
  case LCD_CLEAR:
  case LCD_LINE1:
  case LCD_LINE2:
    LCD.write(0xFE);
    break;

  case LCD_BACKLIGHT_ON:
  case LCD_BACKLIGHT_OFF:
    LCD.write(0x7C);
    break;
  }

  LCD.write(LCD_commands[command]);

  delay(10);
}
