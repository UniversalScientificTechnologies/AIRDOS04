#define TYPE "AIRDOS04X"
#define DIGTYPE "BATDATUNIT01B"
#define ADCTYPE "USTSIPIN03A"
// Compiled with: Arduino 1.8.13
// MightyCore 2.2.2 

#define MAJOR 1   // Data format
#define MINOR 1   // Features
#include "githash.h"

//#define CALIBRATION

#define XSTR(s) STR(s)
#define STR(s) #s

#define CHANNELS 1024 // number of channels in buffer for histogram 

String FWversion = XSTR(MAJOR)"."XSTR(MINOR)"."XSTR(GHRELEASE)"-"XSTR(GHBUILD)"-"XSTR(GHBUILDTYPE); 

#define MAXFILESIZE MAX_MEASUREMENTS * BYTES_MEASUREMENT // in bytes, 4 MB per day, 28 MB per week, 122 MB per month
#define MAX_MEASUREMENTS 11000ul // in measurement cycles, 5 500 per day
#define BYTES_MEASUREMENT 531ul // number of bytes per one measurement
#define MAXFILES 200 // maximal number of files on SD card

/* 
ISP
---
PD0     RX
PD1     TX
RESET#  through 50M capacitor to RST#


                     Mighty 1284p    
                      +---\/---+
           (D 0) PB0 1|        |40 PA0 (AI 0 / D24)
           (D 1) PB1 2|        |39 PA1 (AI 1 / D25)
      INT2 (D 2) PB2 3|        |38 PA2 (AI 2 / D26)
       PWM (D 3) PB3 4|        |37 PA3 (AI 3 / D27)
    PWM/SS (D 4) PB4 5|        |36 PA4 (AI 4 / D28)
      MOSI (D 5) PB5 6|        |35 PA5 (AI 5 / D29)
  PWM/MISO (D 6) PB6 7|        |34 PA6 (AI 6 / D30)
   PWM/SCK (D 7) PB7 8|        |33 PA7 (AI 7 / D31)
                 RST 9|        |32 AREF
                VCC 10|        |31 GND
                GND 11|        |30 AVCC
              XTAL2 12|        |29 PC7 (D 23)
              XTAL1 13|        |28 PC6 (D 22)
      RX0 (D 8) PD0 14|        |27 PC5 (D 21) TDI
      TX0 (D 9) PD1 15|        |26 PC4 (D 20) TDO
RX1/INT0 (D 10) PD2 16|        |25 PC3 (D 19) TMS
TX1/INT1 (D 11) PD3 17|        |24 PC2 (D 18) TCK
     PWM (D 12) PD4 18|        |23 PC1 (D 17) SDA
     PWM (D 13) PD5 19|        |22 PC0 (D 16) SCL
     PWM (D 14) PD6 20|        |21 PD7 (D 15) PWM
                      +--------+
*/

#include "wiring_private.h"
#include <Wire.h>           
#include <SD.h>             
#include <SPI.h>

#define CONV        0    // PB0, MOLEX B0, D Q, ADC CONV signal
#define DRESET      22   // PC6, MOLEX C0, D #Reset
#define DSET        23   // PC7, MOLEX C1, D #Set
#define SDpower     19   // PC3
#define SDmode      3    // PB3
#define SS          4    // PB4
#define MOSI        5    // PB5
#define MISO        6    // PB6
#define SCK         7    // PB7
#define LED1        12   // PD4
#define LED2        13   // PD5
#define LED3        14   // PD6
#define BUZZER      15   // PD7
#define POWER5V     26   // PA2
#define POWER3V3    2    // PB2
#define SPI_MUX_SEL 18   // PC2
#define EXT_I2C_EN  20   // PC4
#define ACONNECT    27   // PA3 = LOW = analogue frontend connected
#define CTS         28   // PA4
#define RTS         29   // PA5
#define BTN_USER_A  30   // PA6
//#define BTN_USER_B  31   // PA7
#define ENUM_FTDI_USB 21 // PC5 = LOW = USB connected

#define BQ34Z100 0x55

String filename = "";
uint16_t fn;
uint16_t count = 0;
boolean SDinserted = true;
uint8_t histogram[CHANNELS];
uint8_t ADCconf1;
uint8_t ADCconf2;
uint8_t DIGconf1;
uint8_t DIGconf2;

void(* resetFunc) (void) = 0; //declare reset function at address 0

uint8_t bcdToDec(uint8_t b)
{
  return ( ((b >> 4)*10) + (b%16) );
}

uint32_t tm;
uint8_t tm_s100;

void readRTC()
{
  Wire.beginTransmission(0x51);
  Wire.write(0);
  Wire.endTransmission();
  
  Wire.requestFrom(0x51, 6);
  tm_s100 = bcdToDec(Wire.read());
  uint8_t tm_sec = bcdToDec(Wire.read() & 0x7f);
  uint8_t tm_min = bcdToDec(Wire.read() & 0x7f);
  tm = bcdToDec(Wire.read());
  tm += bcdToDec(Wire.read()) * 100;
  tm += bcdToDec(Wire.read()) * 10000;
  tm = tm * 60 * 60 + tm_min * 60 + tm_sec;
}

int16_t readBat(int8_t regaddr)
{
  Wire.beginTransmission(BQ34Z100);
  Wire.write(regaddr);
  Wire.endTransmission();

  Wire.requestFrom(BQ34Z100,1);

  unsigned int low = Wire.read();

  Wire.beginTransmission(BQ34Z100);
  Wire.write(regaddr+1);
  Wire.endTransmission();

  Wire.requestFrom(BQ34Z100,1);

  unsigned int high = Wire.read();

  unsigned int high1 = high<<8;

  return (high1 + low);
}


uint8_t store = 0;
uint8_t batt = 0;
uint8_t ainserted = 0;

// Timer 1 interrupt service routine (ISR)
ISR(TIMER1_COMPA_vect)
{
  store++;
  batt++;
  TCNT1 = 0; // Reset Counter
}

// Battery status out
void BattOut()
{
  digitalWrite(SDpower, HIGH);   // SD card power on
  digitalWrite(SPI_MUX_SEL, LOW); // SDcard    

  uint16_t noise = 9;
  uint32_t flux=0;

  for(uint16_t n=noise; n<(CHANNELS); n++)  
  {
    flux += histogram[n]; 
  }

  // make a string for assembling the data to log:
  String dataString = "";

  readRTC();
  
  // make a string for assembling the data to log:
  dataString += "$BATT,";
  dataString += String(count); 
  dataString += ",";  
  dataString += String(tm); 
  dataString += ".";
  dataString += String(tm_s100); 
  dataString += ",";
  dataString += String(readBat(0x8));   // mV - U
  dataString += ",";
  dataString += String(readBat(0xa));  // mA - I
  dataString += ",";
  dataString += String(readBat(0x4));   // mAh - remaining capacity
  dataString += ",";
  dataString += String(readBat(0x6));   // mAh - full charge
  dataString += ",";
  dataString += String(readBat(0xc) * 0.1 - 273.15);   // temperature
  
  if (SDinserted)
  {    
    // make sure that the default chip select pin is set to output
    // see if the card is present and can be initialized:
    if (!SD.begin(SS))//, SPI_HALF_SPEED)) 
    {
      Serial1.println("#SD init false");
      SDinserted = false;
      // don't do anything more:
    }
    else
    {
      // open the file. note that only one file can be open at a time,
      // so you have to close this one before opening another.
      File dataFile = SD.open(filename, FILE_WRITE);
    
      // if the file is available, write to it:
      if (dataFile) 
      {
        dataFile.println(dataString);  // write to SDcard (800 ms)     
        dataFile.close();
      }  
      // if the file isn't open, pop up an error:
      else 
      {
        Serial1.println("#SD false");
        SDinserted = false;
      }
    }  
    digitalWrite(SS, HIGH);         // Disable SD card
  }          

  digitalWrite(SPI_MUX_SEL, HIGH); // ADC    
  digitalWrite(SDpower, LOW);   // SD card power off
  delay(1);
}    

// Data out
void DataOut()
{
  digitalWrite(SDpower, HIGH);   // SD card power on
  digitalWrite(SPI_MUX_SEL, LOW); // SDcard    

  uint16_t noise = 4;
  uint32_t flux=0;

  for(uint16_t n=noise; n<(CHANNELS); n++)  
  {
    flux += histogram[n]; 
  }

  // make a string for assembling the data to log:
  String dataString = "";

  readRTC();
  
  // make a string for assembling the data to log:
  dataString += "$HIST,";
  dataString += String(count); 
  dataString += ",";  
  dataString += String(tm); 
  dataString += ".";
  dataString += String(tm_s100); 
  dataString += ",";
  dataString += String(flux);
  
  for(uint16_t n=0; n<(CHANNELS); n++)  
  {
/* 
 if (n>600)
    {
    dataString += "\t,";
    dataString += String(n);
    dataString += "*";      
      
    }
*/
#ifdef CALIBRATION
    dataString += "\t,";
    dataString += String(n);
    dataString += "*";      
#else
    dataString += ",";
#endif      
    dataString += String(histogram[n]); 
  }

  if (SDinserted)
  {
    //PORTB = 0b11111110; // SD card power on
    digitalWrite(LED3, HIGH); 
    
    // make sure that the default chip select pin is set to output
    // see if the card is present and can be initialized:
    if (!SD.begin(SS))//, SPI_HALF_SPEED)) 
    {
      Serial1.println("#SD init false");
      SDinserted = false;
      // don't do anything more:
    }
    else
    {
      // open the file. note that only one file can be open at a time,
      // so you have to close this one before opening another.
      File dataFile = SD.open(filename, FILE_WRITE);
    
      // if the file is available, write to it:
      if (dataFile) 
      {
        dataFile.println(dataString);  // write to SDcard (800 ms)     
        dataFile.close();
      }  
      // if the file isn't open, pop up an error:
      else 
      {
        Serial1.println("#SD false");
        SDinserted = false;
      }
    }  
  //PORTB = 0b00000000; // SD card power off
    digitalWrite(SS, HIGH);         // Disable SD card
  }   
  else
  {       
    digitalWrite(LED2, HIGH); 
    // Debug output if SD card is not inserted
    uint16_t i=0;
    uint16_t len = dataString.length();
    while(true)
    {
      for(uint8_t n=0; n<255; n++) if (!digitalRead(RTS)) break;
      {delayMicroseconds(50);Serial.print(dataString[i++]);}
      if (i>len) break;
      //delay(2);    
    }
    Serial.println();             // print to HID
    Serial1.println(dataString);  // print to debug terminal 
  }
  digitalWrite(LED3, LOW);     
  digitalWrite(LED2, LOW);     
  
  count++;
  if (count > MAX_MEASUREMENTS) 
  {
    count = 0;
    fn++;
    filename = String(fn) + ".txt";        
    Serial1.print("#Filename,"); 
    Serial1.println(filename); 
  } 
  digitalWrite(SPI_MUX_SEL, HIGH); // ADC    
  digitalWrite(SDpower, LOW);   // SD card power off
  delay(1);
}    

void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  Serial1.begin(115200);
  Wire.setClock(100000);

  Serial1.println("#Cvak...");
    
  pinMode(ACONNECT, INPUT);      // detection of analog frontend
  pinMode(ENUM_FTDI_USB, INPUT); // detection of USB
  pinMode(RTS, INPUT);           // UART handshake

  pinMode(BTN_USER_A, INPUT);   // Button st the front panel
  
  pinMode(DRESET, OUTPUT);   // peak detetor
  pinMode(DSET, OUTPUT);   
  pinMode(CONV, INPUT);   

  pinMode(SPI_MUX_SEL, OUTPUT);   // SDcard/ADC
  digitalWrite(SPI_MUX_SEL, HIGH); // ADC    
  
  pinMode(SDpower, OUTPUT);  // SDcard interface
  pinMode(SDmode, OUTPUT);  
  pinMode(SS, OUTPUT);     
  pinMode(MOSI, INPUT);     
  pinMode(MISO, INPUT);     
  pinMode(SCK, OUTPUT);  

  digitalWrite(SDpower, HIGH);   // SD card power on
  digitalWrite(SS, HIGH);        // Disable SD card
  digitalWrite(SCK, LOW);    
  digitalWrite(SDmode, LOW);     // SD card reader oscilator off

  // // Setup battery charger
  // Wire.beginTransmission(0x6A); // I2C address
  // Wire.write((uint8_t)0x15); // Start register
  // Wire.write((uint8_t)0b10011001); // Disable charging LED
  // Wire.endTransmission();

  
  // Setup battery charger
  Wire.beginTransmission(0x6A); // I2C address
  Wire.write((uint8_t)0x02); // Start register
  Wire.write((uint8_t)(int(440/40))<<5); // 440 mA
  Wire.endTransmission();
  Wire.beginTransmission(0x6A); // I2C address
  Wire.write((uint8_t)0x14); // Start register
  Wire.write((uint8_t)0b00100110);
  Wire.write((uint8_t)0b10011001);
  Wire.write((uint8_t)0b00000000);
  // Wire.write((uint8_t)0b01010110);
  // Wire.write((uint8_t)0b00000000);
  // Wire.write((uint8_t)0b00000001);
  Wire.endTransmission();
  Wire.beginTransmission(0x6A); // I2C address
  Wire.write((uint8_t)0x1a); // Start register
  Wire.write((uint8_t)0b10111111); // NTC
  Wire.endTransmission();
  Wire.beginTransmission(0x6A); // I2C address
  Wire.write((uint8_t)0x26); // Start register
  Wire.write((uint8_t)0b10001100); // ADC
  Wire.endTransmission();


  
/*
  for (uint16_t MSB=0; MSB<=256; MSB++)
  {
    uint8_t c=0;
    Serial1.print(MSB, HEX);
    Serial1.print("\t");
    for (uint16_t LSB=0; LSB<256; LSB++)
    {
      Wire.beginTransmission(0x50);                   // request configuration from EEPROM - digital board
      Wire.write(MSB); // MSB
      Wire.write(LSB); // LSB
      Wire.endTransmission();
      Wire.requestFrom((uint8_t)0x50, (uint8_t)1);    
      Serial1.print(" ");
      uint8_t ble;
      ble = (uint8_t) Wire.read();
      Serial1.print(ble, HEX);
      if (ble < 0xFF) delay(200); 
      //Serial1.println('....');
      c++;
      if (c == 32) 
      {
        Serial1.println();
        Serial1.print("\t"); 
        c=0;       
      }
    }
    Serial1.println();  
  }
*/
  /* DEBUG VBUS voltage
uint8_t vbus;
while(true)
{
  // Is VBUS (USB) present?
  Wire.beginTransmission(0x6A);      // ADC of VBUS
  //Wire.write(0x2D); // MSB 0.264 V/bit
  Wire.write(0x1E); 
  Wire.endTransmission();
  Wire.requestFrom(0x6A, 1);    
  vbus = Wire.read();
  Serial1.println(vbus, HEX);
  delay(1000);
}
   //*/

  if (digitalRead(ACONNECT))  // Analog board disconnected
  {  
    boolean SDreader = true;    // wanted SD reader mode  
    boolean USBchanged = true;  // USB devaci need to be changed

    while(true)
    {    
      if (USBchanged)
      {
        USBchanged = false;
        if (SDreader)
        {
          // SD card reader ON
          digitalWrite(SDmode, HIGH);   // SD card reader oscilator on
          
          pinMode(LED1, OUTPUT); 
          digitalWrite(LED1, HIGH); 
          for( uint16_t n=0; n<200; n++)
          {
            delayMicroseconds(250);
            pinMode(BUZZER, OUTPUT); 
            digitalWrite(BUZZER, HIGH); 
            delayMicroseconds(250);
            pinMode(BUZZER, OUTPUT); 
            digitalWrite(BUZZER, LOW); 
          };
          for( uint16_t n=0; n<200; n++)
          {
            delayMicroseconds(180);
            pinMode(BUZZER, OUTPUT); 
            digitalWrite(BUZZER, HIGH); 
            delayMicroseconds(180);
            pinMode(BUZZER, OUTPUT); 
            digitalWrite(BUZZER, LOW); 
          }
          // SD card reader on
          Wire.beginTransmission(0x71); // card reader address
          Wire.write((uint8_t)0x00); // Start register
          Wire.write((uint8_t)0b00010011); // 0b0001 0 01 1
          Wire.endTransmission();  
        }
        else
        {
          pinMode(LED1, OUTPUT); 
          digitalWrite(LED1, LOW); 
          for( uint16_t n=0; n<200; n++)
          {
            delayMicroseconds(180);
            pinMode(BUZZER, OUTPUT); 
            digitalWrite(BUZZER, HIGH); 
            delayMicroseconds(180);
            pinMode(BUZZER, OUTPUT); 
            digitalWrite(BUZZER, LOW); 
          }
          for( uint16_t n=0; n<200; n++)
          {
            delayMicroseconds(250);
            pinMode(BUZZER, OUTPUT); 
            digitalWrite(BUZZER, HIGH); 
            delayMicroseconds(250);
            pinMode(BUZZER, OUTPUT); 
            digitalWrite(BUZZER, LOW); 
          };
          // SD card reader off
          Wire.beginTransmission(0x71); // card reader address
          Wire.write((uint8_t)0x00); // Start register
          Wire.write((uint8_t)0b00010000); // 0b0001 0 00 0
          Wire.endTransmission();
          // SD card reader OFF
          digitalWrite(SDmode, LOW);   // SD card reader oscilator off
        }
        delay(1000);
      };
            
      if (!digitalRead(BTN_USER_A))
      {
        SDreader = !SDreader;
        USBchanged = true;
      }
    }
  }
    
  pinMode(EXT_I2C_EN, OUTPUT);    // Enable external I2C
  digitalWrite(EXT_I2C_EN, HIGH);   

  for( uint8_t n=0; n<5; n++)
  {
    delay(80);  
    pinMode(LED1, OUTPUT); 
    digitalWrite(LED1, HIGH); 
    delay(80);  
    digitalWrite(LED1, LOW); 
  }
  digitalWrite(LED1, HIGH); 
  
  for( uint16_t n=0; n<200; n++)
  {
    delayMicroseconds(180);
    pinMode(BUZZER, OUTPUT); 
    digitalWrite(BUZZER, HIGH); 
    delayMicroseconds(180);
    pinMode(BUZZER, OUTPUT); 
    digitalWrite(BUZZER, LOW); 
  }

  Serial1.println("#Hmmm...");

  pinMode(POWER3V3, OUTPUT);    // Analog power 3.3 V
  digitalWrite(POWER3V3, HIGH); // on 
  pinMode(POWER5V, OUTPUT);     // Analog power 5 V
  digitalWrite(POWER5V, HIGH);  // on

  digitalWrite(DSET, LOW);       // Disable ADC
  digitalWrite(DRESET, HIGH);       
  
  // Initiation of RTC
  Wire.beginTransmission(0x51); // init clock
  Wire.write((uint8_t)0x23); // Start register
  Wire.write((uint8_t)0x00); // 0x23
  Wire.write((uint8_t)0x00); // 0x24 Two's complement offset value
  Wire.write((uint8_t)0b00000101); // 0x25 Normal offset correction, disable low-jitter mode, set load caps to 6 pF
  Wire.write((uint8_t)0x00); // 0x26 Battery switch reg, same as after a reset
  Wire.write((uint8_t)0x00); // 0x27 Enable CLK pin, using bits set in reg 0x28
  Wire.write((uint8_t)0x97); // 0x28 stop watch mode, no periodic interrupts, CLK pin off
  Wire.write((uint8_t)0x00); // 0x29
  Wire.write((uint8_t)0x00); // 0x2a
  Wire.endTransmission();
  Wire.beginTransmission(0x51); // reset clock
  Wire.write(0x2f); 
  Wire.write(0x2c);
  Wire.endTransmission();
  Wire.beginTransmission(0x51); // start stop-watch
  Wire.write(0x28); 
  Wire.write(0x97);
  Wire.endTransmission();
  Wire.beginTransmission(0x51); // reset stop-watch
  Wire.write((uint8_t)0x00); // Start register
  Wire.write((uint8_t)0x00); // 0x00
  Wire.write((uint8_t)0x00); // 0x01 
  Wire.write((uint8_t)0x00); // 0x02 
  Wire.write((uint8_t)0x00); // 0x03
  Wire.write((uint8_t)0x00); // 0x04
  Wire.write((uint8_t)0x00); // 0x05
  Wire.endTransmission();
  
  // make a string for device identification output
  String dataString = "$DOS,"TYPE"," + FWversion + ",0," + githash + ","; // FW version and Git hash
  
  Wire.beginTransmission(0x5B);                   // request SN from EEPROM - analog board
  Wire.write((int)0x08); // MSB
  Wire.write((int)0x00); // LSB
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)0x5B, (uint8_t)16);    
  for (int8_t reg=0; reg<16; reg++)
  { 
    uint8_t serialbyte = Wire.read(); // receive a byte
    if (serialbyte<0x10) dataString += "0";
    dataString += String(serialbyte,HEX);    
  }

  dataString += "\r\n$DIG,"DIGTYPE","; 
  Wire.beginTransmission(0x58);                   // request SN from EEPROM - digital board
  Wire.write((int)0x08); // MSB
  Wire.write((int)0x00); // LSB
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)0x58, (uint8_t)16);    
  for (int8_t reg=0; reg<16; reg++)
  { 
    uint8_t serialbyte = Wire.read(); // receive a byte
    if (serialbyte<0x10) dataString += "0";
    dataString += String(serialbyte,HEX);    
  }
  dataString += ","; 
  Wire.beginTransmission(0x50);                   // request configuration from EEPROM - digital board
  Wire.write((int)0x00); // MSB
  Wire.write((int)0x00); // LSB
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)0x50, (uint8_t)2);    
  DIGconf1 = Wire.read();
  DIGconf2 = Wire.read();
  dataString += String(DIGconf1,HEX);    
  dataString += String(DIGconf2,HEX);    

  dataString += "\r\n$ADC,"ADCTYPE","; 
  Wire.beginTransmission(0x5B);                   // request SN from EEPROM - analog board
  Wire.write((int)0x08); // MSB
  Wire.write((int)0x00); // LSB
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)0x5B, (uint8_t)16);    
  for (int8_t reg=0; reg<16; reg++)
  { 
    uint8_t serialbyte = Wire.read(); // receive a byte
    if (serialbyte<0x10) dataString += "0";
    dataString += String(serialbyte,HEX);    
  };
  dataString += ","; 
  Wire.beginTransmission(0x53);                   // request configuration from EEPROM - analog board
  Wire.write((int)0x00); // MSB
  Wire.write((int)0x00); // LSB
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)0x53, (uint8_t)2);    
  ADCconf1 = Wire.read();
  ADCconf2 = Wire.read();
  dataString += String(ADCconf1,HEX);    
  dataString += String(ADCconf2,HEX);    

  // Filename selection and initial write to SD card
  {    
    digitalWrite(SPI_MUX_SEL, LOW); // SD card    
    // make sure that the default chip select pin is set to output
    // see if the card is present and can be initialized:
    if (!SD.begin(SS))//, SPI_HALF_SPEED)) 
    {
      Serial1.println("#SD init false");
      SDinserted = false;
    }
    for (fn = 1; fn<MAXFILES; fn++) // find last file
    {
       filename = String(fn) + ".txt";
       if (SD.exists(filename) == 0) break;
    }
    fn--;
    filename = String(fn) + ".txt";
  
    for( uint8_t n=0; n<5; n++)
    {
      delay(80);  
      pinMode(LED2, OUTPUT); 
      digitalWrite(LED2, HIGH); 
      delay(80);  
      digitalWrite(LED2, LOW); 
    }
    digitalWrite(LED2, HIGH); 
    
    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    File dataFile = SD.open(filename, FILE_WRITE);

    uint32_t filesize = dataFile.size();
    Serial1.print("#Filesize,");
    Serial1.println(filesize); 
    if (filesize > MAXFILESIZE)
    {
      dataFile.close();
      fn++;
      filename = String(fn) + ".txt";      
      dataFile = SD.open(filename, FILE_WRITE);
    }
    Serial1.print("#Filename,");
    Serial1.println(filename); 
  
    // if the file is available, write to it:
    if (dataFile) 
    {
      dataFile.println(dataString);  // write to SDcard (800 ms)     
      dataFile.close();  
    }  
    // if the file isn't open, pop up an error:
    else 
    {
      Serial1.println("#SD false");
      SDinserted = false;
    }
    Serial1.println(dataString);  // print SN to terminal 
    digitalWrite(SPI_MUX_SEL, HIGH); // ADC    
  }    

  pinMode(EXT_I2C_EN, OUTPUT);    // Disable external I2C
  digitalWrite(EXT_I2C_EN, LOW);   

  for( uint8_t n=0; n<5; n++)
  {
    delay(80);  
    pinMode(LED3, OUTPUT); 
    digitalWrite(LED3, HIGH); 
    delay(80);  
    digitalWrite(LED3, LOW); 
  }
  digitalWrite(LED1, LOW); 
  digitalWrite(LED2, LOW); 

  pinMode(POWER3V3, OUTPUT);   // Analog power 3.3 V
  digitalWrite(POWER3V3, LOW); // off
  
  cli(); // disable interrupts during setup
  // Configure Timer 1 interrupt
  // F_clock = 8 MHz, prescaler = 1024, Fs = 0.125 Hz
  TCCR1A = 0;
  //TCCR1B = 1<<WGM12 | 0<<CS12 | 1<<CS11 | 1<<CS10;
  TCCR1B = 1<<WGM12 | 1<<CS12 | 0<<CS11 | 1<<CS10;
  // OCR1A = ((F_clock / prescaler) / Fs) - 1 
  OCR1A = 39063;      // Set sampling frequency Fs, period 5 s
  //OCR1A = (62500/2)-1;      // Set sampling frequency Fs, period 4 s
  TCNT1 = 0;          // reset Timer 1 counter
  TIMSK1 = 1<<OCIE1A; // Enable Timer 1 interrupt
  sei(); // re-enable interrupts
}



void loop()
{
  for(int n=0; n<CHANNELS; n++) // reset histogram
  {
    histogram[n]=0;
  }

  // dummy conversion
  digitalWrite(DSET, HIGH);
  digitalWrite(DRESET, LOW);
  SPI.transfer16(0x0000);
  digitalWrite(DRESET, HIGH);
  
  store = 0;
  batt = 0;
  // dosimeter integration
  while(true)
  {
    while((PINB & 1)==0) // Waiting for signal drop
    {     
      if (store >= 2) // Data out every 10 s
      {
        store = 0;
        digitalWrite(LED2, digitalRead(ACONNECT)); 
        if (digitalRead(ACONNECT))  // Analog part is disconnected?
        {
          Wire.beginTransmission(0x51); // 1 kHz to #INTA
          Wire.write(0x28); 
          Wire.write(0x05);             // COF
          Wire.endTransmission();

          delay(3000);
          
          for( uint16_t n=0; n<200; n++)
          {
            delayMicroseconds(250);
            pinMode(BUZZER, OUTPUT); 
            digitalWrite(BUZZER, HIGH); 
            delayMicroseconds(250);
            pinMode(BUZZER, OUTPUT); 
            digitalWrite(BUZZER, LOW); 
          }
          // Power off
          Wire.beginTransmission(0x6A); // I2C address
          Wire.write((uint8_t)0x18); // Start register
          Wire.write((uint8_t)0x0A); // 
          Wire.endTransmission();
          delay(10000);
          //while(true);
        };
  
        digitalWrite(DRESET, HIGH);
        digitalWrite(DSET, LOW);
        DataOut();
        for(int n=0; n<CHANNELS; n++) // reset histogram
        {
          histogram[n]=0;
        }
  
        if (batt >= 12) // Battery status every 60 s
        {
          batt = 0;
          BattOut();
        };

        // dummy conversion
        digitalWrite(DSET, HIGH);
        digitalWrite(DRESET, LOW); // L on CONV
        SPI.transfer16(0x0000); 
        digitalWrite(DRESET, HIGH);

        TCNT1 = 0;          // reset Timer 1 counter
      }

    };
    // Signal is going down, we can run ADC
    // delayMicroseconds(4); // This delay is done in cycle overhead
    digitalWrite(DRESET, LOW); // L on CONV
    uint16_t adcVal = SPI.transfer16(0x0000); // 0c8000 +/GND, 0x0000 +/-
    adcVal >>= 6;
    if (histogram[adcVal]<255) histogram[adcVal]++;
    digitalWrite(DRESET, HIGH);
  }
}
