
// AIRDOS04
// Compiled with: Arduino 1.8.13
// MightyCore 2.2.2 

#define MAJOR 8   // Data format
#define MINOR 0   // Features
#include "githash.h"

//#define CALIBRATION

#define XSTR(s) STR(s)
#define STR(s) #s

#ifndef CHANNELS
  #define CHANNELS 1024 // number of channels in buffer for histogram, including negative numbers 
#endif
#define ZERO CHANNELS/2 // 3th channel is channel 1 (ussually DCoffset or DCoffset+1, for version with noise reduction transistor)
#define RANGE 1024  //!!!ZERO-12

String FWversion = XSTR(MAJOR)"."XSTR(MINOR)"."XSTR(GHRELEASE)"-"XSTR(GHBUILD)"-"XSTR(GHBUILDTYPE)".A500"; // 500 effective channels for 1024 ADC channels

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

#define CONV        0    // PB0, ADC CONV signal
#define DRESET      22   // PC6, D Reset
#define DSET        21   // PC5, D Set
#define SDpower1    1    // PB1
#define SDpower2    2    // PB2
#define SDpower3    3    // PB3
#define SS          4    // PB4
#define MOSI        5    // PB5
#define MISO        6    // PB6
#define SCK         7    // PB7
#define LED         23   // PC7

String filename = "";
uint16_t fn;
uint16_t count = 0;
uint32_t serialhash = 0;
uint16_t base_offset = ZERO;
uint8_t lo, hi;
uint16_t u_sensor;
boolean SDinserted = true;
uint8_t histogram[RANGE];

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

bool store = false;

// Timer 3 interrupt service routine (ISR)
ISR(TIMER3_COMPA_vect)
{
  store = true;
}

// Data out
void DataOut()
{
  uint16_t noise = 3;
  uint32_t flux=0;

  for(int n=noise; n<(RANGE); n++)  
  {
    flux += histogram[n]; 
  }

  digitalWrite(LED, HIGH); 

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
  
  for(uint16_t n=0; n<(RANGE); n++)  
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
    
    // make sure that the default chip select pin is set to output
    // see if the card is present and can be initialized:
    if (!SD.begin(SS)) 
    {
      Serial.println("#SD init false");
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
        Serial.println("#SD false");
        SDinserted = false;
      }
    }  
  //PORTB = 0b00000000; // SD card power off
    digitalWrite(SS, HIGH);         // Disable SD card
  }          

  Serial.println(dataString);   // print to terminal 
  digitalWrite(LED, LOW);     
  
  count++;
  if (count > MAX_MEASUREMENTS) 
  {
    count = 0;
    fn++;
    filename = String(fn) + ".txt";        
    Serial.print("#Filename,"); 
    Serial.println(filename); 
  } 
}    

void setup()
{
  pinMode(LED, OUTPUT); 
  digitalWrite(LED, HIGH); 

  // Open serial communications and wait for port to open:
  Serial.begin(115200);

  Serial.println("#Cvak...");
  
  pinMode(DRESET, OUTPUT);   // peak detetor
  pinMode(DSET, OUTPUT);   
  pinMode(CONV, INPUT);   

  pinMode(SDpower1, OUTPUT);  // SDcard interface
  pinMode(SDpower2, OUTPUT);     
  pinMode(SDpower3, OUTPUT);     
  pinMode(SS, OUTPUT);     
  pinMode(MOSI, INPUT);     
  pinMode(MISO, INPUT);     
  pinMode(SCK, OUTPUT);  

  digitalWrite(SDpower1, HIGH);   // SD card power on
  digitalWrite(SDpower2, HIGH);  
  digitalWrite(SDpower3, HIGH);  
  digitalWrite(SS, HIGH);         // Disable SD card
  digitalWrite(SCK, LOW);    
  digitalWrite(DSET, HIGH);       // Disable ADC
  digitalWrite(DRESET, LOW);       
  
  Wire.setClock(100000);

  Serial.println("#Hmmm...");

  //!!!! looking for zero
  {
    base_offset = 18500; // Calculate mean of n measurements
  }

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
  String dataString = "$DOS,AIRDOS04," + FWversion + "," + String(base_offset) + "," + githash + ","; // FW version and Git hash
  
  Wire.beginTransmission(0x58);                   // request SN from EEPROM
  Wire.write((int)0x08); // MSB
  Wire.write((int)0x00); // LSB
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)0x58, (uint8_t)16);    
  for (int8_t reg=0; reg<16; reg++)
  { 
    uint8_t serialbyte = Wire.read(); // receive a byte
    if (serialbyte<0x10) dataString += "0";
    dataString += String(serialbyte,HEX);    
    serialhash += serialbyte;
  }

  {    
    // make sure that the default chip select pin is set to output
    // see if the card is present and can be initialized:
    if (!SD.begin(SS)) 
    {
      Serial.println("#SD init false");
      SDinserted = false;
    }
    for (fn = 1; fn<MAXFILES; fn++) // find last file
    {
       filename = String(fn) + ".txt";
       if (SD.exists(filename) == 0) break;
    }
    fn--;
    filename = String(fn) + ".txt";
    
    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    File dataFile = SD.open(filename, FILE_WRITE);

    uint32_t filesize = dataFile.size();
    Serial.print("#Filesize,");
    Serial.println(filesize); 
    if (filesize > MAXFILESIZE)
    {
      dataFile.close();
      fn++;
      filename = String(fn) + ".txt";      
      dataFile = SD.open(filename, FILE_WRITE);
    }
    Serial.print("#Filename,");
    Serial.println(filename); 
  
    // if the file is available, write to it:
    if (dataFile) 
    {
      dataFile.println(dataString);  // write to SDcard (800 ms)     
      dataFile.close();  
    }  
    // if the file isn't open, pop up an error:
    else 
    {
      Serial.println("#SD false");
      SDinserted = false;
    }
    Serial.println(dataString);  // print SN to terminal 
  }    

  
  for( uint8_t n=0; n<5; n++)
  {
    delay(100);  
    pinMode(LED, OUTPUT); 
    digitalWrite(LED, HIGH); 
    delay(100);  
    pinMode(LED, OUTPUT); 
    digitalWrite(LED, LOW); 
  }
 
  cli(); // disable interrupts during setup

  // Configure Timer 3 interrupt
  // F_clock = 8 MHz, prescaler = 1024, Fs = 0.125 Hz
  TCCR3A = 0;
  TCCR3B = 1<<WGM32 | 1<<CS32 | 0<<CS31 | 1<<CS30;
  // OCR3A = ((F_clock / prescaler) / Fs) - 1 
  OCR3AH = 0xF4;      // 62499 Set sampling frequency Fs, period 8 s
  OCR3AL = 0x23;      // 62499 Set sampling frequency Fs, period 8 s
  TCNT3H = 0;          // reset Timer 1 counter
  TCNT3L = 0;          // reset Timer 1 counter
  TIMSK3 = 1<<OCIE3A; // Enable Timer 1 interrupt

 
  // Configure Timer 1 for Input Capture
  // Voltage reference
  ADMUX = INTERNAL2V56 << 6;
  // Comparator
  ACSR = 1<<ACBG | 1<<ACIC;
  DIDR1 = 1<<AIN1D | 1<<AIN0D;
  // F_clock = 8 MHz, prescaler = 1, Fs = 8 MHz
  TCCR1A = 0;
  //TCCR1B = 0;
  TCCR1B = 1<<ICES1 | 0<<CS12 | 0<<CS11 | 1<<CS10;
  TCNT1 = 0;          // reset Timer 1 counter

  store = false;
  sei(); // re-enable interrupts
}



void loop()
{
  for(int n=0; n<CHANNELS; n++)
  {
    histogram[n]=0;
  }

  // dummy conversion
  digitalWrite(DRESET, HIGH);
  SPI.transfer16(0x8000);
  digitalWrite(DRESET, LOW);
  
  // dosimeter integration
  while(true)
  {
    while((PINB & 1)==0) if (store) 
    {
      store = false;
      digitalWrite(DRESET, LOW);
      digitalWrite(DSET, HIGH);
      DataOut();
      digitalWrite(DSET, LOW);
      for(int n=0; n<CHANNELS; n++)
      {
        histogram[n]=0;
      }
      // dummy conversion
      digitalWrite(DRESET, HIGH);
      SPI.transfer16(0x0000); // 0x8000
      digitalWrite(DRESET, LOW);
      continue;
    };
    //delayMicroseconds(4);
    digitalWrite(DRESET, HIGH);
    uint16_t adcVal = SPI.transfer16(0x0000);

    /*
    uint16_t ToT;
    //TIFR1 = 1<<ICF1;
    //while (!(TIFR1 & (1 << ICF1)));
    //TCNT1 = 0;          // reset Timer 1 counter
    //delayMicroseconds(1000);
    ToT = TCNT1 -ICR1;
    Serial.print(adcVal);    
    Serial.print("*");
    //Serial.print(TCNT1);
    //Serial.print("*");
    Serial.println(ToT);
    */
    //if(adcVal>17000) 
    {
      //Serial.println(adcVal);
      //adcVal -= 17001;
      adcVal >>= 6;
      if (histogram[adcVal]<255) histogram[adcVal]++;
    }
    digitalWrite(DRESET, LOW);
  }
}
