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
#include "eeprom_layout.h"

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
#define RTC_ADDR 0x51
#define SD_READER_ADDR 0x71
#define EEPROM_ANALOG_ADDR 0x5B
#define EEPROM_DIGITAL_ADDR 0x58
#define EEPROM_DIGITAL_CFG_ADDR 0x50
#define EEPROM_ANALOG_CFG_ADDR 0x53
#define SHT31_ADDR_PRIMARY 0x44
#define SHT31_ADDR_SECONDARY 0x45
#define MS5611_ADDR 0x77
#define VBAT_ADC_LSB_0P01_MV     199u      // 1.99 mV = 199 × 0.01 mV
#define VBAT_PRESENT_THRESHOLD_MV 3000u    // např. 3.0 V jako hranice „baterie je přítomna“


String filename = "";
uint16_t fn;
uint16_t count = 0;
boolean SDinserted = true;
String logHeader = "";              // Device identification header ($DOS, $DIG, $ADC, $BATP); $TIME is appended fresh per file
uint32_t eeprom_sync_rtc_seconds = 0; // RTC seconds at last sync, used to compute sync_age on file rotation
uint32_t measurement_start_unix_time = 0; // Unix timestamp of measurement start (for $FSEQ)
uint16_t file_seq = 0;              // File sequence number within one measurement (0 = first file)
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
uint32_t rtc_current_time;  // Current RTC time in seconds
uint32_t eeprom_sync_time;  // Last sync time from EEPROM

void readRTC()
{
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0);
  Wire.endTransmission();

  Wire.requestFrom((uint8_t)RTC_ADDR, (uint8_t)6);
  tm_s100 = bcdToDec(Wire.read());
  uint8_t tm_sec = bcdToDec(Wire.read() & 0x7f);
  uint8_t tm_min = bcdToDec(Wire.read() & 0x7f);
  tm = bcdToDec(Wire.read());
  tm += bcdToDec(Wire.read()) * 100;
  tm += bcdToDec(Wire.read()) * 10000;
  tm = tm * 60 * 60 + tm_min * 60 + tm_sec;
}

// Read single register from RTC (PCF? at address RTC_ADDR)
uint8_t readRTCreg(uint8_t reg)
{
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)RTC_ADDR, (uint8_t)1);
  if (Wire.available()) return (uint8_t)Wire.read();
  return 0xFF; // error
}

// Write single register to RTC
void writeRTCreg(uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

// Reset RTC into stopwatch mode using the provided initialization sequence
// and append a log entry to SD card (assumes SD is already initialized and `filename` is set).
void ResetRTC(uint8_t prev07, uint8_t prev28)
{
  // Use the exact sequence provided in AIRDOS04X for initialization
  Wire.beginTransmission(RTC_ADDR); // init clock
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

  Wire.beginTransmission(RTC_ADDR); // reset clock
  Wire.write(0x2f);
  Wire.write(0x2c);
  Wire.endTransmission();

  Wire.beginTransmission(RTC_ADDR); // start stop-watch
  Wire.write(0x28);
  Wire.write(0x97);
  Wire.endTransmission();

  Wire.beginTransmission(RTC_ADDR); // reset stop-watch
  Wire.write((uint8_t)0x00); // Start register
  Wire.write((uint8_t)0x00); // 0x00
  Wire.write((uint8_t)0x00); // 0x01
  Wire.write((uint8_t)0x00); // 0x02
  Wire.write((uint8_t)0x00); // 0x03
  Wire.write((uint8_t)0x00); // 0x04
  Wire.write((uint8_t)0x00); // 0x05
  Wire.endTransmission();

  // Log action to SD card and UART1
  readRTC();
  String s = "$RTCCHK,";
  s += String(tm);
  s += ".";
  s += String(tm_s100);
  s += ",INIT,reg07=0x";
  if (prev07 < 16) s += "0";
  s += String(prev07, HEX);
  s += ",reg28=0x";
  if (prev28 < 16) s += "0";
  s += String(prev28, HEX);

  Serial1.println(s);  // Output to debug UART

  if (SDinserted)
  {
    if (SD.begin(SS))
    {
      File logf = SD.open(filename, FILE_WRITE);
      if (logf)
      {
        logf.println(s);
        logf.close();
      }
    }
  }
}

// Verify RTC registers: reg 0x07 bit7 == 0 and reg 0x28 bit4 (RTCM) == 1.
// If not in required mode, initialize to stopwatch and log the change.
void CheckRTCconfig()
{
  uint8_t r07 = readRTCreg(0x07);
  uint8_t r28 = readRTCreg(0x28);

  bool ok07 = ((r07 & 0x80) == 0); // bit7 must be zero
  bool okRTCM = ((r28 & (1<<4)) != 0); // RTCM (bit4) must be 1

  if (!ok07 || !okRTCM)
  {
    ResetRTC(r07, r28);
  } else {
    // RTC config is OK - log to SD and UART1
    readRTC();
    String s = "$RTCCHK,";
    s += String(tm);
    s += ".";
    s += String(tm_s100);
    s += ",OK,reg07=0x";
    if (r07 < 16) s += "0";
    s += String(r07, HEX);
    s += ",reg28=0x";
    if (r28 < 16) s += "0";
    s += String(r28, HEX);

    Serial1.println(s);  // Output to debug UART

    if (SDinserted)
    {
      if (SD.begin(SS))
      {
        File logf = SD.open(filename, FILE_WRITE);
        if (logf)
        {
          logf.println(s);
          logf.close();
        }
      }
    }
  }
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

uint16_t readChargerReg16(uint8_t regaddr)
{
  Wire.beginTransmission(CHARGER_ADDR);
  Wire.write(regaddr);
  Wire.endTransmission(false);

  Wire.requestFrom((uint8_t)CHARGER_ADDR, (uint8_t)2);
  if (Wire.available() < 2) return 0;

  uint16_t lsb = (uint16_t)Wire.read();  // první byte = LSB
  uint16_t msb = (uint16_t)Wire.read();  // druhý byte = MSB

  return (msb << 8) | lsb;               // složit správně
}

void writeChargerReg(uint8_t regaddr, uint8_t value)
{
  Wire.beginTransmission(CHARGER_ADDR);
  Wire.write(regaddr);
  Wire.write(value);
  Wire.endTransmission();
}

// Single charger configuration function: enable or disable charging
void configCharger(bool EnCharging)
{
  // Input current limit register (same in both modes)
  Wire.beginTransmission(CHARGER_ADDR);
  Wire.write((uint8_t)0x02); // Input current limit register
  Wire.write((uint8_t)(int(440/40))<<5); // 440 mA
  Wire.endTransmission();

  // Charge control block: different payload depending on desired state
  Wire.beginTransmission(CHARGER_ADDR);
  Wire.write((uint8_t)0x14); // Charge control block
  if (EnCharging)
  {
    Wire.write((uint8_t)0b00100110);
    Wire.write((uint8_t)0b00011001);
    Wire.write((uint8_t)0b10100000); // Enable charger
    Wire.write((uint8_t)0b01010110);
    Wire.write((uint8_t)0b00000000);
    Wire.write((uint8_t)0b00000001);
  }
  else
  {
    Wire.write((uint8_t)0b00100110);
    Wire.write((uint8_t)0b10011001);
    Wire.write((uint8_t)0b00000000); // Disable charger
    Wire.write((uint8_t)0b01010110);
    Wire.write((uint8_t)0b00000000);
    Wire.write((uint8_t)0b00000001);
  }
  Wire.endTransmission();
  // NTC configuration (same in both modes)
  Wire.beginTransmission(CHARGER_ADDR); // NTC
  Wire.write((uint8_t)0x1a);
  Wire.write((uint8_t)0b10111111);
  Wire.endTransmission();

  // ADC configuration (same in both modes)
  Wire.beginTransmission(CHARGER_ADDR); // ADC configuration
  Wire.write((uint8_t)0x26);
  Wire.write((uint8_t)0b10001100);
  Wire.endTransmission();
}



// Convert Unix timestamp to human readable format (UTC)
void unixToDateTime(uint32_t unix_time, uint16_t &year, uint8_t &month, uint8_t &day, 
                    uint8_t &hour, uint8_t &minute, uint8_t &second)
{
  second = unix_time % 60;
  unix_time /= 60;
  minute = unix_time % 60;
  unix_time /= 60;
  hour = unix_time % 24;
  unix_time /= 24;
  
  uint16_t days = unix_time;
  year = 1970;
  
  while (true)
  {
    uint16_t days_in_year = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 366 : 365;
    if (days < days_in_year) break;
    days -= days_in_year;
    year++;
  }
  
  uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) days_in_month[1] = 29;
  
  month = 1;
  while (days >= days_in_month[month - 1])
  {
    days -= days_in_month[month - 1];
    month++;
  }
  day = days + 1;
}

// Build a fresh $TIME line from the current RTC reading. Called both for the
// initial log header and on each file rotation so the timestamp reflects when
// the file was actually created, not when the measurement started.
void appendTimeLine(String &s)
{
  readRTC();
  rtc_current_time = tm;
  uint32_t current_unix_time = 0;
  uint32_t sync_age = 0;
  if (eeprom_sync_time > 0)
  {
    current_unix_time = rtc_current_time + eeprom_sync_time;
    sync_age = rtc_current_time - eeprom_sync_rtc_seconds;
  }
  uint16_t year;
  uint8_t month, day, hour, minute, second;
  unixToDateTime(current_unix_time, year, month, day, hour, minute, second);

  s += "\r\n$TIME,";
  s += String(rtc_current_time);
  s += ",";
  s += String(eeprom_sync_time);
  s += ",";
  s += String(current_unix_time);
  s += ",";
  s += String(sync_age);
  s += ",";
  s += String(year);
  s += "-";
  if (month < 10) s += "0";
  s += String(month);
  s += "-";
  if (day < 10) s += "0";
  s += String(day);
  s += " ";
  if (hour < 10) s += "0";
  s += String(hour);
  s += ":";
  if (minute < 10) s += "0";
  s += String(minute);
  s += ":";
  if (second < 10) s += "0";
  s += String(second);
}

// Read EEPROM structure from internal EEPROM
bool readEEPROMRecord(uint8_t eeprom_addr, eeprom::EepromRecord &record)
{
  Wire.beginTransmission(eeprom_addr);
  Wire.write((uint8_t)0x00); // MSB - start from address 0
  Wire.write((uint8_t)0x00); // LSB
  Wire.endTransmission();
  
  uint8_t* ptr = (uint8_t*)&record;
  uint8_t bytesRead = 0;
  
  // Read in chunks due to Wire buffer limitations
  while (bytesRead < sizeof(eeprom::EepromRecord))
  {
    uint8_t toRead = min(sizeof(eeprom::EepromRecord) - bytesRead, 16);
    Wire.requestFrom(eeprom_addr, toRead);
    
    while (Wire.available() && bytesRead < sizeof(eeprom::EepromRecord))
    {
      ptr[bytesRead++] = Wire.read();
    }
    
    if (bytesRead < sizeof(eeprom::EepromRecord))
    {
      // Set address for next chunk
      Wire.beginTransmission(eeprom_addr);
      Wire.write((uint8_t)(bytesRead >> 8));   // MSB
      Wire.write((uint8_t)(bytesRead & 0xFF)); // LSB
      Wire.endTransmission();
    }
  }
  
  return (bytesRead == sizeof(eeprom::EepromRecord));
}

bool detectBatteryPresence(uint16_t &batteryMv)
{
  // Step 1: Disable charging (EN_CHG = 0)
  uint8_t reg16 = readChargerReg(0x16);
  reg16 &= ~(1u << 5);
  writeChargerReg(0x16, reg16);
  delay(50);

  // Step 2: vynutit vybíjecí proud IBAT (FORCE_IBATDIS = 1) 
  reg16 |= (1u << 6);                      // bit6 = FORCE_IBATDIS
  writeChargerReg(0x16, reg16);
  delay(200);                               // krátký vybíjecí „pulz“

  // Step 3: zrušit vybíjení IBAT (FORCE_IBATDIS = 0) 
  reg16 &= ~(1u << 6);
  writeChargerReg(0x16, reg16);
  delay(20);                               // mezera po pulzu

  // Step 4: povolit VBAT ADC a nastavit one-shot 
  // (jen pro jistotu zajistíme, že VBAT ADC není zakázaný v REG0x27)
  uint8_t reg27 = readChargerReg(0x27);    // ADC_FUNCTION_DISABLE_0
  reg27 &= ~(1u << 4);                     // bit4 = VBAT_ADC_DIS -> 0 = enable
  writeChargerReg(0x27, reg27);

  uint8_t reg26 = readChargerReg(0x26);    // ADC_CONTROL
  reg26 |= (1u << 7);                      // bit7 = ADC_EN
  reg26 |= (1u << 6);                      // bit6 = ADC_RATE = 1 (one-shot)
  reg26 &= ~(3u << 4);    // ADC_SAMPLE = 00
  reg26 |= (1u << 1);     // ADC_AVG_INIT = 1
  writeChargerReg(0x26, reg26);

  // dle datasheetu ~30 ms pro prevod ADC
  delay(50);

  //  Step 5: přečíst VBAT_ADC (REG0x30, bity 12:1) 
  uint16_t vbatRaw  = readChargerReg16(0x30);
  uint16_t vbatCode = (vbatRaw >> 1) & 0x0FFF;   // bity 12:1, bit0 je reserved

  uint32_t tmp = (uint32_t)vbatCode * VBAT_ADC_LSB_0P01_MV;
  batteryMv = (uint16_t)(tmp / 100u);               // mV

  // //  Debug výpis 
  // Serial1.print("#BATDEBUG raw=0x");
  // Serial1.print(vbatRaw, HEX);
  // Serial1.print(" bin=0b");
  // Serial1.print(vbatRaw, BIN);
  // Serial1.print(" code=");
  // Serial1.print(vbatCode);
  // Serial1.print(" mv=");
  // Serial1.print(batteryMv);

  bool present = (batteryMv >= VBAT_PRESENT_THRESHOLD_MV);

  return present;
}


uint8_t store = 0;
uint8_t batt = 0;
uint8_t env = 0;
uint8_t ainserted = 0;

void playModeChangeTone()
{
  // Signal: USB reader mode activated or normal mode resumed
  // First tone sequence: 2 kHz (high pitched)
  for( uint16_t n=0; n<200; n++)
  {
    delayMicroseconds(250);
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, HIGH);
    delayMicroseconds(250);
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);
  }
  // Second tone sequence: ~2.8 kHz (higher pitch)
  for( uint16_t n=0; n<200; n++)
  {
    delayMicroseconds(180);
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, HIGH);
    delayMicroseconds(180);
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);
  }
}

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

  SHT31 sht(SHT31_ADDR_PRIMARY);
  sht.begin();
  sht.read();         //  default = true/fast       slow = false

  dataString += String(sht.getTemperature(), 1);
  dataString += String(",");
  dataString += String(sht.getHumidity(), 1);
  dataString += String(",");

  SHT31 sht2(SHT31_ADDR_SECONDARY);
  sht2.begin();
  sht2.read();         //  default = true/fast       slow = false

  dataString += String(sht2.getTemperature(), 1);
  dataString += String(",");
  dataString += String(sht2.getHumidity(), 1);
  dataString += String(",");

  MS5611 MS5611(MS5611_ADDR);
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
    filename = String(fn) + ".TXT";
    file_seq++;
    Serial1.print("#Filename,");
    Serial1.println(filename);

    // Write detector identification header + $FSEQ line to the new file,
    // so each rotated file can be identified on its own.
    if (SDinserted)
    {
      if (!SD.begin(SS))
      {
        Serial1.println("#SD init false");
        SDinserted = false;
      }
      else
      {
        File headerFile = SD.open(filename, FILE_WRITE);
        if (headerFile)
        {
          String hdr = logHeader;
          appendTimeLine(hdr);
          hdr += "\r\n$FSEQ,";
          hdr += String(file_seq);
          hdr += ",";
          hdr += String(measurement_start_unix_time);
          headerFile.println(hdr);
          headerFile.close();
        }
        else
        {
          Serial1.println("#SD false");
          SDinserted = false;
        }
        digitalWrite(SS, HIGH);       // Disable SD card
      }
    }
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

  // Detect battery presence and configure charger accordingly
  configCharger(false);
  delay(100);
  batteryPresent = detectBatteryPresence(detectedBatteryMv);
  configCharger(batteryPresent);
  
  // Indicate battery status with LED1: ON if no battery, OFF if battery present
  // pinMode(LED1, OUTPUT);
  // digitalWrite(LED1, batteryPresent ? LOW : HIGH);

  /* DEBUG VBUS voltage
uint8_t vbus;
while(true)
{
  // Is VBUS (USB) present?
  Wire.beginTransmission(CHARGER_ADDR);      // ADC of VBUS
  //Wire.write(0x2D); // MSB 0.264 V/bit
  Wire.write(0x1E);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)CHARGER_ADDR, (uint8_t)1);
  vbus = Wire.read();
  Serial1.println(vbus, HEX);
  delay(1000);
}
   //*/

  if (digitalRead(ACONNECT))  // Analog board disconnected
  {


    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    pinMode(LED3, OUTPUT);
  for( uint8_t n=0; n<5; n++)
  {
    delay(80);
    digitalWrite(LED1, HIGH);
    digitalWrite(LED2, HIGH);
    digitalWrite(LED3, HIGH);
    delay(80);
    digitalWrite(LED1, LOW);
    digitalWrite(LED2, LOW);
    digitalWrite(LED3, LOW);
  }

    boolean SDreader = true;    // wanted SD reader mode
    boolean USBchanged = true;  // USB device need to be changed
    uint32_t usbLedTickMs = 0;
    bool usbSdLedState = false;

    wdt_enable(WDTO_2S);  // watchdog for preventing I2C hanging
    
    Wire.beginTransmission(CHARGER_ADDR);      // ADC of VBUS
    Wire.write(0x2D); // MSB 0.264 V/bit
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)CHARGER_ADDR, (uint8_t)1);
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
      uint8_t switch_status;
      bool i2c_switch_to_usb = false;
      bool usb_phy_connected = (digitalRead(ENUM_FTDI_USB) == LOW);

      // USB mode LED indication:
      // USB-SD  -> slow blink on LED1
      // USB-I2C -> double blink on LED2+LED3
      uint32_t nowMs = millis();
      if (SDreader)
      {
        if ((uint32_t)(nowMs - usbLedTickMs) >= 400)
        {
          usbLedTickMs = nowMs;
          usbSdLedState = !usbSdLedState;
        }
        digitalWrite(LED1, usbSdLedState ? HIGH : LOW);
        digitalWrite(LED2, LOW);
        digitalWrite(LED3, LOW);
      }
      else
      {
        uint16_t phase = (uint16_t)(nowMs % 1000ul);
        bool ledOn = (phase < 90) || ((phase >= 180) && (phase < 270));
        digitalWrite(LED1, LOW);
        digitalWrite(LED2, ledOn ? HIGH : LOW);
        digitalWrite(LED3, ledOn ? HIGH : LOW);
      }

      // Check PCA9541A control register (0x01):
      // bit0 = MYBUS (master0), bit1 = NMYBUS (master1 mirrored).
      // Different values mean master0 does not own bus => USB CH-1 is active.
      Wire.beginTransmission((uint8_t)0x70);
      Wire.write((uint8_t)0x01);
      Wire.endTransmission();
      Wire.requestFrom((uint8_t)0x70, (uint8_t)1);
      if (Wire.available())
      {
        switch_status = Wire.read();
        uint8_t bit0 = (switch_status & (1 << 0)) ? 1 : 0;
        uint8_t bit1 = (switch_status & (1 << 1)) ? 1 : 0;
        i2c_switch_to_usb = (bit0 != bit1);
      }
      else
      {
        // On read failure, assume USB mode to avoid false power-off.
        i2c_switch_to_usb = true;
      }
      wdt_reset();

      {
        // Never evaluate local power-off while USB cable is physically connected
        // or while USB master owns I2C switch (USB-I2C mode).
        if (!usb_phy_connected && !i2c_switch_to_usb)
        {
          // Is VBUS (USB) present?
          Wire.beginTransmission(CHARGER_ADDR);      // ADC of VBUS
          Wire.write(0x2D); // MSB 0.264 V/bit
          Wire.endTransmission();
          Wire.requestFrom((uint8_t)CHARGER_ADDR, (uint8_t)1);
          vbus = Wire.read() & 0x7F;
        }
        else
        {
          // USB CH-1 owns I2C bus, skip local VBUS decision.
          vbus = 0xFF;
        }
      }
      wdt_reset();

      if (vbus < 17) // < 4.5 V
      {
        digitalWrite(LED2, digitalRead(ACONNECT));
        // discharge analog board detection signal
        Wire.beginTransmission(RTC_ADDR); // 1 kHz to #INTA
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
        Wire.beginTransmission(RTC_ADDR); // High-Z on #INTA
        Wire.write((uint8_t)0x27); // Start register
        Wire.write((uint8_t)0x03); // 0x27 High-Z on INTA pin.
        Wire.write(0x95);             // COF
        Wire.endTransmission();

        // Power off
        Wire.beginTransmission(CHARGER_ADDR); // I2C address
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
          // USB-SD mode: keep external 3V3 I2C supply off.
          digitalWrite(EXT_I2C_EN, LOW);

          // SD card reader ON
          digitalWrite(SDmode, HIGH);   // SD card reader oscilator on
          playModeChangeTone();          // Signal mode change to user

          // SD card reader on (charger stays as default enabled)
          Wire.beginTransmission(SD_READER_ADDR); // card reader address
          Wire.write((uint8_t)0x00); // Start register
          Wire.write((uint8_t)0b00010011); // 0b0001 0 01 1
          Wire.endTransmission();
        }
        else
        {
          // USB-I2C mode: enable external 3V3 I2C supply.
          digitalWrite(EXT_I2C_EN, HIGH);

          pinMode(LED1, OUTPUT);
          digitalWrite(LED1, LOW);
          playModeChangeTone();          // Signal mode change to user
          // SD card reader off
          Wire.beginTransmission(SD_READER_ADDR); // card reader address
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

  wdt_reset();

  Serial1.println("#Hmmm...");

  digitalWrite(DSET, LOW);       // Disable ADC
  digitalWrite(DRESET, HIGH);


  Wire.beginTransmission(RTC_ADDR); // disable output n INTA
  Wire.write((uint8_t)0x27); // Start register
  Wire.write((uint8_t)0x03); // 0x27 High-Z on INTA pin
  Wire.write((uint8_t)0x97); // 0x28 stop-watch mode, no periodic interrupts, INTA in high-Z
  Wire.endTransmission();

  wdt_reset();

  // Initiation of RTC
  Wire.beginTransmission(RTC_ADDR); // init clock
  Wire.write((uint8_t)0x23); // Start register
  Wire.write((uint8_t)0x00); // 0x23
  Wire.write((uint8_t)0x00); // 0x24 Two's complement offset value
  Wire.write((uint8_t)0b00000101); // 0x25 Normal offset correction, disable low-jitter mode, set load caps to 6 pF
  Wire.write((uint8_t)0x00); // 0x26 Battery switch reg, same as after a reset
  Wire.write((uint8_t)0x03); // 0x27 Enable CLK pin, using bits set in reg 0x28
  Wire.write((uint8_t)0x97); // 0x28 stop watch mode, no periodic interrupts, CLK pin off
  Wire.write((uint8_t)0x00); // 0x29
  Wire.write((uint8_t)0x00); // 0x2a
  Wire.endTransmission();
  

  /*Wire.beginTransmission(RTC_ADDR); // reset clock
  Wire.write(0x2f);
  Wire.write(0x2c);
  Wire.endTransmission();
  Wire.beginTransmission(RTC_ADDR); // start stop-watch
  Wire.write(0x28);
  Wire.write(0x97);
  Wire.endTransmission();
  Wire.beginTransmission(RTC_ADDR); // reset stop-watch
  Wire.write((uint8_t)0x00); // Start register
  Wire.write((uint8_t)0x00); // 0x00
  Wire.write((uint8_t)0x00); // 0x01
  Wire.write((uint8_t)0x00); // 0x02
  Wire.write((uint8_t)0x00); // 0x03
  Wire.write((uint8_t)0x00); // 0x04
  Wire.write((uint8_t)0x00); // 0x05
  Wire.endTransmission();*/

  wdt_disable();

  wdt_enable(WDTO_8S);  // watchdog for preventing I2C hanging

  // Read current RTC time
  readRTC();
  rtc_current_time = tm;
  Serial1.print("#RTC_TIME,");
  Serial1.println(rtc_current_time);

  // Read sync_time and device name from internal EEPROM (digital cfg)
  eeprom::EepromRecord eeprom_record = {};
  if (readEEPROMRecord(EEPROM_DIGITAL_CFG_ADDR, eeprom_record))
  {
    eeprom_sync_time = eeprom_record.sync_time;
    eeprom_sync_rtc_seconds = eeprom_record.sync_rtc_seconds;
    Serial1.print("#EEPROM_SYNC_TIME,");
    Serial1.println(eeprom_sync_time);
    Serial1.print("#EEPROM_INIT_TIME,");
    Serial1.println(eeprom_record.init_time);
    Serial1.print("#EEPROM_SYNC_RTC_SECONDS,");
    Serial1.println(eeprom_record.sync_rtc_seconds);
    Serial1.print("#DIG_NAME,");
    for (uint8_t i = 0; i < sizeof(eeprom_record.device_id); i++)
    {
      char c = eeprom_record.device_id[i];
      if (c == '\0') break;
      Serial1.print(c);
    }
    Serial1.println();
  }
  else
  {
    Serial1.println("#EEPROM read failed");
    eeprom_sync_time = 0;
    eeprom_sync_rtc_seconds = 0;
  }

  // Read device name from analog cfg EEPROM
  eeprom::EepromRecord eeprom_adc_record = {};
  if (readEEPROMRecord(EEPROM_ANALOG_CFG_ADDR, eeprom_adc_record))
  {
    Serial1.print("#ADC_NAME,");
    for (uint8_t i = 0; i < sizeof(eeprom_adc_record.device_id); i++)
    {
      char c = eeprom_adc_record.device_id[i];
      if (c == '\0') break;
      Serial1.print(c);
    }
    Serial1.println();
  }
  else
  {
    Serial1.println("#EEPROM analog read failed");
  }

  // Calculate current Unix time: RTC_time + sync_time
  uint32_t current_unix_time = 0;
  uint32_t sync_age = 0;  // Age of synchronization in seconds
  if (eeprom_sync_time > 0)
  {
    current_unix_time = rtc_current_time + eeprom_sync_time;
    sync_age = rtc_current_time - eeprom_record.sync_rtc_seconds;
  }

  // Convert to human readable format
  uint16_t year;
  uint8_t month, day, hour, minute, second;
  unixToDateTime(current_unix_time, year, month, day, hour, minute, second);

  // Output current time to Serial1 (debug)
  Serial1.print("#SYNC_AGE,");
  Serial1.println(sync_age);
  Serial1.print("#CURRENT_UNIX_TIME,");
  Serial1.println(current_unix_time);
  Serial1.print("#CURRENT_TIME,");
  Serial1.print(year);
  Serial1.print("-");
  if (month < 10) Serial1.print("0");
  Serial1.print(month);
  Serial1.print("-");
  if (day < 10) Serial1.print("0");
  Serial1.print(day);
  Serial1.print(" ");
  if (hour < 10) Serial1.print("0");
  Serial1.print(hour);
  Serial1.print(":");
  if (minute < 10) Serial1.print("0");
  Serial1.print(minute);
  Serial1.print(":");
  if (second < 10) Serial1.print("0");
  Serial1.println(second);

  // make a string for device identification output
  String dataString = "$DOS,"TYPE"," + FWversion + ",0," + githash + ","; // FW version and Git hash

  Wire.beginTransmission(EEPROM_ANALOG_ADDR);                   // request SN from EEPROM - analog board
  Wire.write((int)0x08); // MSB
  Wire.write((int)0x00); // LSB
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)EEPROM_ANALOG_ADDR, (uint8_t)16);
  for (int8_t reg=0; reg<16; reg++)
  {
    uint8_t serialbyte = Wire.read(); // receive a byte
    if (serialbyte<0x10) dataString += "0";
    dataString += String(serialbyte,HEX);
  }

  dataString += "\r\n$DIG,"DIGTYPE",";
  Wire.beginTransmission(EEPROM_DIGITAL_ADDR);                   // request SN from EEPROM - digital board
  Wire.write((int)0x08); // MSB
  Wire.write((int)0x00); // LSB
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)EEPROM_DIGITAL_ADDR, (uint8_t)16);
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
  Wire.beginTransmission(EEPROM_DIGITAL_CFG_ADDR);                   // request configuration from EEPROM - digital board
  Wire.write((int)0x00); // MSB
  Wire.write((int)0x00); // LSB
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)EEPROM_DIGITAL_CFG_ADDR, (uint8_t)2);
  DIGconf1 = Wire.read();
  DIGconf2 = Wire.read();
  dataString += String(DIGconf1,HEX);
  dataString += String(DIGconf2,HEX);

  // Digital module name from EEPROM digital cfg (up to 10 chars, may not be null-terminated).
  // Empty if EEPROM read failed (eeprom_record is zero-initialized).
  dataString += "\r\n$DIG_NAME,";
  for (uint8_t i = 0; i < sizeof(eeprom_record.device_id); i++)
  {
    char c = eeprom_record.device_id[i];
    if (c == '\0') break;
    dataString += c;
  }

  dataString += "\r\n$ADC,"ADCTYPE",";
  Wire.beginTransmission(EEPROM_ANALOG_ADDR);                   // request SN from EEPROM - analog board
  Wire.write((int)0x08); // MSB
  Wire.write((int)0x00); // LSB
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)EEPROM_ANALOG_ADDR, (uint8_t)16);
  for (int8_t reg=0; reg<16; reg++)
  {
    uint8_t serialbyte = Wire.read(); // receive a byte
    if (serialbyte<0x10) dataString += "0";
    dataString += String(serialbyte,HEX);
  };
  dataString += ",";
  Wire.beginTransmission(EEPROM_ANALOG_CFG_ADDR);                   // request configuration from EEPROM - analog board
  Wire.write((int)0x00); // MSB
  Wire.write((int)0x00); // LSB
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)EEPROM_ANALOG_CFG_ADDR, (uint8_t)2);
  ADCconf1 = Wire.read();
  ADCconf2 = Wire.read();
  dataString += String(ADCconf1,HEX);
  dataString += String(ADCconf2,HEX);

  // Analog module name from EEPROM analog cfg (up to 10 chars, may not be null-terminated).
  // Empty if EEPROM read failed (eeprom_adc_record is zero-initialized).
  dataString += "\r\n$ADC_NAME,";
  for (uint8_t i = 0; i < sizeof(eeprom_adc_record.device_id); i++)
  {
    char c = eeprom_adc_record.device_id[i];
    if (c == '\0') break;
    dataString += c;
  }

  // Calibration coefficients from analog board EEPROM
  dataString += "\r\n$CALIB,";
  dataString += String(eeprom_adc_record.calib[0], 6);
  dataString += ",";
  dataString += String(eeprom_adc_record.calib[1], 6);
  dataString += ",";
  dataString += String(eeprom_adc_record.calib[2], 6);
  dataString += ",";
  dataString += String(eeprom_adc_record.calib_ts);

  dataString += "\r\n$BATP,";
  dataString += batteryPresent ? "1" : "0";
  dataString += ",";
  dataString += String(detectedBatteryMv);

  // Snapshot of the device identification header (without $TIME) for reuse on
  // later file rotations. $TIME is appended fresh per file from the RTC.
  logHeader = dataString;

  appendTimeLine(dataString);
  measurement_start_unix_time = current_unix_time;
  file_seq = 0;

  // Append $FSEQ line to the first file header (file sequence 0, measurement start unix time)
  dataString += "\r\n$FSEQ,";
  dataString += String(file_seq);
  dataString += ",";
  dataString += String(measurement_start_unix_time);

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
       filename = String(fn) + ".TXT";
       if (SD.exists(filename) == 0) break;
    }
//    fn--;
    filename = String(fn) + ".TXT";

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
      filename = String(fn) + ".TXT";
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

    // Check RTC registers and initialize to stopwatch mode if necessary.
    // SD is already initialized here and `filename` set, so logging is possible.
    CheckRTCconfig();

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
    Wire.beginTransmission(RTC_ADDR); // 1024 Hz to #INTA
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

    Wire.beginTransmission(RTC_ADDR); // High-Z on #INTA
    Wire.write((uint8_t)0x27); // Start register
    Wire.write((uint8_t)0x03); // 0x27 High-Z on INTA pin.
    Wire.write(0x95);             // COF
    Wire.endTransmission();


    // Power off
    Wire.beginTransmission(CHARGER_ADDR); // I2C address
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
