#define TYPE "AIRDOS04C"
#define DIGTYPE "BATDATUNIT01B"
#define ADCTYPE "USTSIPIN03A"
// Compiled with: Arduino 1.8.13
// MightyCore 2.2.2

#define MAJOR 2   // Data format
#define MINOR 0   // Features
#include "githash.h"

#define RADIATION_CLICK
#define DEBUG

#define XSTR(s) STR(s)
#define STR(s) #s

#define CHANNELS 4 // number of channels in the buffer for histogram
#define MAX_EVENTS 300 // number of events per integration time

String FWversion = XSTR(MAJOR)"."XSTR(MINOR)"."XSTR(GHRELEASE)"-"XSTR(GHBUILD)"-"XSTR(GHBUILDTYPE);

#define MAXFILESIZE MAX_MEASUREMENTS * BYTES_MEASUREMENT // in bytes, 4 MB per day, 28 MB per week, 122 MB per month
#define MAX_MEASUREMENTS 11000ul // in measurement cycles, 5 500 per day
#define BYTES_MEASUREMENT 400ul // number of bytes per one measurement
#define MAXFILES 200 // maximal number of files on the SD card
#define _5S 39063  // ticks of timer during 5 s

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

/*
Using library Wire at version 1.1 in folder: /home/kacer/.arduino15/packages/MightyCore/hardware/avr/2.2.2/libraries/Wire
Using library SD at version 1.2.4 in folder: /home/kacer/Arduino/libraries/SD
Using library SPI at version 1.0 in folder: /home/kacer/.arduino15/packages/MightyCore/hardware/avr/2.2.2/libraries/SPI

Using library SHT31 at version 0.5.0 in folder: /home/kacer/Arduino/libraries/SHT31
https://github.com/RobTillaart/SHT31

Using library MS5611 at version 0.4.0 in folder: /home/kacer/Arduino/libraries/MS5611
https://github.com/RobTillaart/MS5611

 */

#include "wiring_private.h"
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <SHT31.h>
#include <MS5611.h>
#include <avr/wdt.h>

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
#define CHARGER_ADDR 0x6A
#define VBAT_ADC_BASE_MV 2304   // BQ2562x VBAT ADC base in mV (2.304 V)
#define VBAT_ADC_LSB_MV 20      // 20 mV per LSB
#define VBAT_PRESENT_THRESHOLD_MV 3000

String filename = "";
uint16_t fn;
uint16_t count = 0;
boolean SDinserted = true;
uint16_t histogram[CHANNELS];
uint16_t event_time[MAX_EVENTS];
uint8_t event_time2[MAX_EVENTS];
uint16_t event_channel[MAX_EVENTS];
uint16_t events_counter;
uint8_t ADCconf1;
uint8_t ADCconf2;
uint8_t DIGconf1;
uint8_t DIGconf2;
boolean clicks = false;
bool batteryPresent = true;
uint16_t detectedBatteryMv = 0;

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

uint8_t readChargerReg(uint8_t regaddr)
{
  Wire.beginTransmission(CHARGER_ADDR);
  Wire.write(regaddr);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)CHARGER_ADDR, (uint8_t)1);
  if (!Wire.available()) return 0;
  return (uint8_t)Wire.read();
}

void writeChargerReg(uint8_t regaddr, uint8_t value)
{
  Wire.beginTransmission(CHARGER_ADDR);
  Wire.write(regaddr);
  Wire.write(value);
  Wire.endTransmission();
}

void configureChargerEnabled()
{
  Wire.beginTransmission(CHARGER_ADDR);
  Wire.write((uint8_t)0x02); // Input current limit register
  Wire.write((uint8_t)(int(440/40))<<5); // 440 mA
  Wire.endTransmission();

  Wire.beginTransmission(CHARGER_ADDR);
  Wire.write((uint8_t)0x14); // Charge control block
  Wire.write((uint8_t)0b00100110);
  Wire.write((uint8_t)0b00011001);
  Wire.write((uint8_t)0b10100000); // Enable charger
  Wire.write((uint8_t)0b01010110);
  Wire.write((uint8_t)0b00000000);
  Wire.write((uint8_t)0b00000001);
  Wire.endTransmission();

  Wire.beginTransmission(CHARGER_ADDR); // NTC
  Wire.write((uint8_t)0x1a);
  Wire.write((uint8_t)0b10111111);
  Wire.endTransmission();

  Wire.beginTransmission(CHARGER_ADDR); // ADC configuration
  Wire.write((uint8_t)0x26);
  Wire.write((uint8_t)0b10001100);
  Wire.endTransmission();
}

void configureChargerDisabled()
{
  Wire.beginTransmission(CHARGER_ADDR);
  Wire.write((uint8_t)0x02); // Input current limit register
  Wire.write((uint8_t)(int(440/40))<<5); // 440 mA
  Wire.endTransmission();

  Wire.beginTransmission(CHARGER_ADDR);
  Wire.write((uint8_t)0x14); // Charge control block
  Wire.write((uint8_t)0b00100110);
  Wire.write((uint8_t)0b10011001);
  Wire.write((uint8_t)0b00000000); // Disable charger (matches AIRDOS04X)
  Wire.write((uint8_t)0b01010110);
  Wire.write((uint8_t)0b00000000);
  Wire.write((uint8_t)0b00000001);
  Wire.endTransmission();

  Wire.beginTransmission(CHARGER_ADDR); // NTC
  Wire.write((uint8_t)0x1a);
  Wire.write((uint8_t)0b10111111);
  Wire.endTransmission();

  Wire.beginTransmission(CHARGER_ADDR); // ADC configuration
  Wire.write((uint8_t)0x26);
  Wire.write((uint8_t)0b10001100);
  Wire.endTransmission();
}

bool detectBatteryPresence(uint16_t &batteryMv)
{
  // Step 1: disable charging and apply 30 mA discharge
  uint8_t reg16 = readChargerReg(0x16);
  reg16 &= ~(1<<5);          // EN_CHG = 0
  writeChargerReg(0x16, reg16 | (1<<6)); // FORCE_IBATDIS = 1

  delay(5);

  // Step 2: stop discharge
  writeChargerReg(0x16, reg16 & ~(1<<6));

  // Step 3: one-shot ADC for VBAT
  uint8_t reg26 = readChargerReg(0x26);
  reg26 |= (1<<7) | (1<<6); // ADC_EN=1, ADC_RATE=1 (one-shot)
  writeChargerReg(0x26, reg26);

  delay(10); // allow conversion to finish

  uint8_t vbatCode = readChargerReg(0x30); // VBAT ADC result
  batteryMv = VBAT_ADC_BASE_MV + (uint16_t)vbatCode * VBAT_ADC_LSB_MV;

  // Battery considered present when VBAT is above undervoltage area
  return batteryMv >= VBAT_PRESENT_THRESHOLD_MV;
}


uint8_t store = 0;
uint8_t batt = 0;
uint8_t env = 0;
uint8_t ainserted = 0;

// Timer 1 interrupt service routine (ISR)
ISR(TIMER1_COMPA_vect)
{
  store++;
  TCNT1 = 0; // Reset Counter
}

// Enviromental sensors out
void EnvOut()
{
  wdt_enable(WDTO_2S);  // watchdog for preventing I2C hanging

  digitalWrite(SDpower, HIGH);   // SD card power on
  digitalWrite(SPI_MUX_SEL, LOW); // SDcard

  pinMode(POWER3V3, OUTPUT);    // Analog power 3.3 V
  digitalWrite(POWER3V3, HIGH); // on
  pinMode(EXT_I2C_EN, OUTPUT);    // Enable external I2C
  digitalWrite(EXT_I2C_EN, HIGH);

  // make a string for assembling the data to log:
  String dataString = "";

  readRTC();

  // make a string for assembling the data to log:
  dataString += "$ENV,";
  dataString += String(count);
  dataString += ",";
  dataString += String(tm);
  dataString += ".";
  dataString += String(tm_s100);
  dataString += ",";

  SHT31 sht(0x44);
  sht.begin();
  sht.read();         //  default = true/fast       slow = false

  dataString += String(sht.getTemperature(), 1);
  dataString += String(",");
  dataString += String(sht.getHumidity(), 1);
  dataString += String(",");

  SHT31 sht2(0x45);
  sht2.begin();
  sht2.read();         //  default = true/fast       slow = false

  dataString += String(sht2.getTemperature(), 1);
  dataString += String(",");
  dataString += String(sht2.getHumidity(), 1);
  dataString += String(",");

  MS5611 MS5611(0x77);
  MS5611.begin();
  MS5611.read();           //  note no error checking => "optimistic".

  dataString += String(MS5611.getTemperature(), 2);
  dataString += String(",");
  dataString += String(MS5611.getPressure(), 2);

  pinMode(EXT_I2C_EN, OUTPUT);    // Disable external I2C
  digitalWrite(EXT_I2C_EN, LOW);
  pinMode(POWER3V3, OUTPUT);    // Analog power 3.3 V
  digitalWrite(POWER3V3, LOW);  // off

  wdt_disable();

  #ifdef DEBUG
    Serial1.println(dataString);  // Debug output to debug terminal        
  #endif

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

// Battery status out
void BattOut()
{
  digitalWrite(SDpower, HIGH);   // SD card power on
  digitalWrite(SPI_MUX_SEL, LOW); // SDcard

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

  #ifdef DEBUG
    Serial1.println(dataString);  // Debug output to debug terminal        
  #endif

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

  readRTC();

  // make a string for assembling the data to log:
  String dataString = "";
  
  // Output of system time of stat the integration
  dataString += "$START,";
  dataString += String(count);
  dataString += ",";
  dataString += String(event_time[0]);

  // Evens out
  for(uint16_t n=1; n<events_counter; n++)
  {
     if (n>=MAX_EVENTS) break;
     uint32_t long_event_time;
     if (0 == event_time2[n])  
     {
       //long_event_time =  2 * _5S - event_time[n];
       long_event_time =  event_time[n];
     }
     else
     {
       //long_event_time =  _5S - event_time[n];
       long_event_time =  _5S + event_time[n];
     };
     
     dataString += "\r\n";
     dataString += "$E,";
     dataString += String(long_event_time);
     dataString += ",";
     dataString += String(event_channel[n]);
  }

  
  // End of integration
  dataString += "\r\n$STOP,";
  dataString += String(count);
  dataString += ",";
  dataString += String(tm);
  dataString += ".";
  dataString += String(tm_s100);
  dataString += ",";
  {
    uint16_t systime = TCNT3L; // read system time
    systime |= TCNT3H<<8;
    dataString += String(systime);
  };
  dataString += ",";
  dataString += String(events_counter-1);

  for(uint16_t n=0; n<CHANNELS; n++)
  {
    dataString += ",";
    dataString += String(histogram[n]);
  }

  #ifdef DEBUG
    Serial1.println(dataString);  // Debug output to debug terminal        
  #endif

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

  pinMode(BUZZER, OUTPUT); // Set the buzzer pin as an output

  pinMode(SPI_MUX_SEL, OUTPUT);   // SDcard/ADC
  digitalWrite(SPI_MUX_SEL, HIGH); // ADC

  pinMode(POWER3V3, OUTPUT);    // Analog power 3.3 V
  digitalWrite(POWER3V3, HIGH); // on
  pinMode(POWER5V, OUTPUT);     // Analog power 5 V
  digitalWrite(POWER5V, HIGH);  // on

  pinMode(SDpower, OUTPUT);  // SDcard interface
  pinMode(SDmode, OUTPUT);
  pinMode(SS, OUTPUT);
  pinMode(MOSI, INPUT);
  pinMode(MISO, INPUT);
  pinMode(SCK, OUTPUT);

  digitalWrite(SDpower, HIGH);   // SD card power on
  digitalWrite(SS, HIGH);        // Disable SD card
  digitalWrite(SCK, LOW);        
  delay(100); 

  digitalWrite(SDmode, LOW);     // SD card reader oscilator off

  pinMode(EXT_I2C_EN, OUTPUT);    // Disable external I2C
  digitalWrite(EXT_I2C_EN, LOW);

  // Setup battery charger (default to enabled)
  configureChargerEnabled();


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

    wdt_enable(WDTO_2S);  // watchdog for preventing I2C hanging
    
    Wire.beginTransmission(0x6A);      // ADC of VBUS
    Wire.write(0x2D); // MSB 0.264 V/bit
    Wire.endTransmission();
    Wire.requestFrom(0x6A, 1);
    Wire.read() & 0x7F;
    wdt_reset();
    delay(1000); // Vaiting for stable voltage
    wdt_reset();
    delay(1000); // Vaiting for stable voltage
    wdt_reset();
    delay(1000); // Vaiting for stable voltage
    wdt_reset();
    while(true)
    {
      uint8_t vbus;

      {
        // Is VBUS (USB) present?
        Wire.beginTransmission(0x6A);      // ADC of VBUS
        Wire.write(0x2D); // MSB 0.264 V/bit
        Wire.endTransmission();
        Wire.requestFrom(0x6A, 1);
        vbus = Wire.read() & 0x7F;
      }
      wdt_reset();

      if (vbus < 17) // < 4.5 V
      {
        digitalWrite(LED2, digitalRead(ACONNECT));
        // discharge analog board detection signal
        Wire.beginTransmission(0x51); // 1 kHz to #INTA
        Wire.write(0x28);
        Wire.write(0x95);             // COF
        Wire.endTransmission();

        wdt_reset();
        delay(1000); // Vaiting for capacitor discharge
        wdt_reset();
        delay(1000); 
        wdt_reset();
        delay(1000); 
        wdt_reset();

        for( uint16_t n=0; n<200; n++)
        {
          delayMicroseconds(250);
          pinMode(BUZZER, OUTPUT);
          digitalWrite(BUZZER, HIGH);
          delayMicroseconds(250);
          pinMode(BUZZER, OUTPUT);
          digitalWrite(BUZZER, LOW);
        }
        // end discharging of analog board detection signal
        Wire.beginTransmission(0x51); // High-Z on #INTA
        Wire.write((uint8_t)0x27); // Start register
        Wire.write((uint8_t)0x03); // 0x27 High-Z on INTA pin.
        Wire.write(0x95);             // COF
        Wire.endTransmission();

        // Power off
        Wire.beginTransmission(0x6A); // I2C address
        Wire.write((uint8_t)0x18); // Start register
        Wire.write((uint8_t)0x0A); //
        Wire.endTransmission();

        while(true); // Waiting for reset

      }

      wdt_reset();

      if (USBchanged)
      {
        USBchanged = false;
        if (SDreader)
        {
          // SD card reader ON
          digitalWrite(SDmode, HIGH);   // SD card reader oscilator on

          // SD card reader on (charger stays as default enabled)
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

  digitalWrite(DSET, LOW);       // Disable ADC
  digitalWrite(DRESET, HIGH);


  Wire.beginTransmission(0x51); // disable output n INTA
  Wire.write((uint8_t)0x27); // Start register
  Wire.write((uint8_t)0x03); // 0x27 High-Z on INTA pin
  Wire.write((uint8_t)0x97); // 0x28 stop-watch mode, no periodic interrupts, INTA in high-Z

  // Initiation of RTC
  /*Wire.beginTransmission(0x51); // init clock
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
  Wire.endTransmission();*/

  wdt_enable(WDTO_8S);  // watchdog for preventing I2C hanging

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
    pinMode(POWER3V3, OUTPUT);    // Analog power 3.3 V
    digitalWrite(POWER3V3, HIGH); // on
    pinMode(POWER5V, OUTPUT);     // Analog power 5 V
    digitalWrite(POWER5V, HIGH);  // on
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

  dataString += "\r\n$BATP,";
  dataString += batteryPresent ? "1" : "0";
  dataString += ",";
  dataString += String(detectedBatteryMv);

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
//    fn--;
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

  wdt_disable();

  cli(); // disable interrupts during setup
  // Configure Timer 1 interrupt
  // F_clock = 8 MHz, prescaler = 1024, Fs = 7.8125 kHz (128 us / tick)
  TCCR1A = 0;
  //TCCR1B = 1<<WGM12 | 0<<CS12 | 1<<CS11 | 1<<CS10;
  TCCR1B = 1<<WGM12 | 1<<CS12 | 0<<CS11 | 1<<CS10;
  // OCR1A = ((F_clock / prescaler) / Fs) - 1
  OCR1A = _5S;      // Set sampling frequency Fs, period 5 s
  TCNT1 = 0;          // reset Timer 1 counter
  TIMSK1 = 1<<OCIE1A; // Enable Timer 1 interrupt

  // Configure Timer 3 interrupt
  // F_clock = 8 MHz, prescaler = 1024, Fs = 7.8125 kHz (128 us / tick)
  TCCR3A = 0;
  TCCR3B = 1<<CS32 | 0<<CS31 | 1<<CS30;
  TCNT3H = 0;          // reset Timer 1 counter
  TCNT3L = 0;          // reset Timer 1 counter
  
  sei(); // re-enable interrupts
}

inline void PostIntegration() __attribute__((always_inline));

inline void PostIntegration()
{
  store = 0;
  batt++;
  env++;

  digitalWrite(LED2, digitalRead(ACONNECT));
  if (digitalRead(ACONNECT))  // Analog part is disconnected?
  {
    Wire.beginTransmission(0x51); // 1024 Hz to #INTA
    Wire.write((uint8_t)0x27); // Start register
    Wire.write((uint8_t)0x00); // 0x27 Enable CLX output on INTA pin, using bits set in reg 0x28
    Wire.write(0x95);             // COF
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

    Wire.beginTransmission(0x51); // High-Z on #INTA
    Wire.write((uint8_t)0x27); // Start register
    Wire.write((uint8_t)0x03); // 0x27 High-Z on INTA pin.
    Wire.write(0x95);             // COF
    Wire.endTransmission();


    // Power off
    Wire.beginTransmission(0x6A); // I2C address
    Wire.write((uint8_t)0x18); // Start register
    Wire.write((uint8_t)0x0A); //
    Wire.endTransmission();
    
    while(true)
    {
      delay(5000);
      for( uint16_t n=0; n<200; n++)
      {
        delayMicroseconds(250);
        pinMode(BUZZER, OUTPUT);
        digitalWrite(BUZZER, HIGH);
        delayMicroseconds(250);
        pinMode(BUZZER, OUTPUT);
        digitalWrite(BUZZER, LOW);
      }
    }
  };

  digitalWrite(DRESET, HIGH);
  digitalWrite(DSET, LOW);

  wdt_enable(WDTO_8S);  // watchdog for preventing I2C hanging

  DataOut();  // Save data from integration time
  for(int n=0; n<CHANNELS; n++) // reset histogram
  {
    histogram[n]=0;
  };
  events_counter = 1; // Start events from 1 (because 0 is record of death time)

  if (env >= 5*6) // Environment out every 5 minutes
  {
    env = 0;
    EnvOut();
  };

  if (batt >= 30*6) // Battery status every 30 minutes
  {
    batt = 0;
    BattOut();
  };
  
  // dummy conversion (reset ADC)
  digitalWrite(DSET, HIGH);
  digitalWrite(DRESET, LOW); // L on CONV
  SPI.transfer16(0x0000);
  digitalWrite(DRESET, HIGH);

  wdt_disable();

  count++;            // Next integration
  TCNT1 = 0;          // reset Timer 1 counter
  uint16_t systime = TCNT3L; // read system time
  systime |= TCNT3H<<8;
  event_time[0] = systime;
}


void loop()
{
  for(int n=0; n<CHANNELS; n++) // reset histogram
  {
    histogram[n]=0;
  };
  events_counter = 1; // reset event counter

  // dummy conversion
  digitalWrite(DSET, HIGH);
  digitalWrite(DRESET, LOW);
  SPI.transfer16(0x0000);
  digitalWrite(DRESET, HIGH);

  {
    TCNT1 = 0;          // reset Timer 1 counter
    uint16_t systime = TCNT3L; // read system time
    systime |= TCNT3H<<8;
    event_time[0] = systime;
  }
  
  store = 0;
  batt = 0;
  env = 0;
  // dosimeter integration
  while(true)
  {
    while((PINB & 1)==0) // Waiting for signal drop
    {
      if (store >= 2) // Data out every 10 s
      {
        if (!digitalRead(BTN_USER_A))
        {
          for( uint16_t n=0; n<200; n++)
          {
            delayMicroseconds(150);
            pinMode(BUZZER, OUTPUT);
            digitalWrite(BUZZER, HIGH);
            delayMicroseconds(150);
            pinMode(BUZZER, OUTPUT);
            digitalWrite(BUZZER, LOW);
          };
          clicks = !clicks;
        };

        PostIntegration();
      }
    };
    // Signal is going down, we can run ADC
    // delayMicroseconds(4); // This delay is done in cycle overhead
    digitalWrite(DRESET, LOW); // L on CONV
    uint16_t adcVal = SPI.transfer16(0x0000); // 0c8000 +/GND, 0x0000 +/-

    if (clicks)
    {
      //if (adcVal>=CHANNELS) PORTD ^= 0x80; // digitalWrite(BUZZER, !digitalRead(BUZZER)); // buzzer click
      if (adcVal>=CHANNELS) PORTD ^= 0x20; // LED2 blick
      //if (adcVal>=CHANNELS) PORTD ^= 0xA0; // LED2 blick & BUZZER click
    };
    
    if (adcVal<CHANNELS)  // Record single event if energy is above threshold
    {   
      histogram[adcVal]++;
    }
    else
    {
//adcVal = 65535;
      if (events_counter<MAX_EVENTS) 
      {
        uint16_t reltime = TCNT1; //uint16_t reltime = TCNT1L; reltime |= TCNT1H<<8;
//reltime = 40000;
        event_time[events_counter] = reltime;
        event_time2[events_counter] = store;
        event_channel[events_counter] = adcVal;
      }
      events_counter++;
    }
    digitalWrite(DRESET, HIGH);
  }
}
