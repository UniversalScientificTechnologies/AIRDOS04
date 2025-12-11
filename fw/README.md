## How to compile firmware

### Prerequisites

Install arduino-cli and required libraries:

```bash
# Install arduino-cli (if not already installed)
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# Install MightyCore board support
arduino-cli core install MightyCore:avr

# Install required libraries
arduino-cli lib install "SHT31@0.5.0"
arduino-cli lib install "MS5611@0.4.0"
```

### Compile firmware

Navigate to the firmware directory and compile:

```bash
cd AIRDOS04

arduino-cli compile --fqbn MightyCore:avr:1284:bootloader=uart1,eeprom=keep,BOD=2v7,LTO=Os_flto,clock=8MHz_external \
  --build-property "compiler.cpp.extra_flags=-DGHRELEASE=0 -DGHBUILD=0 -DGHBUILDTYPE=0" \
  --export-binaries
```

The compiled `.hex` file will be in `build/MightyCore.avr.1284/` directory.

## How to update firmware

### Prerequisites

Install avrdude:

```bash
sudo apt update
sudo apt -y install avrdude
```

### Programming AIRDOS by avrdude

Please provide a correct path to .hex file. 

Also correct name of ttyUSB interface has to be provided. Then run avrdude.

#### Using Arduino bootloader (via serial):

```bash
avrdude -v -patmega1284p -carduino -P/dev/ttyUSB0 -b57600 -D -Uflash:w:./build/fw_AIRDOS04_AIRDOS04.latest-CIBuild.hex:i
```

#### Using STK500v2 programmer (for uploading with bootloader):

> SD card must be removed from reader

```bash
avrdude -v -patmega1284p -cstk500v2 -P/dev/ttyUSB0 -Uflash:w:./build/MightyCore.avr.1284/AIRDOS04.ino.with_bootloader.hex:i
```

