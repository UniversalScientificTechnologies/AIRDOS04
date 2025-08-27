## How to update firmware
### Prerequisites

Install avrdude.
```
sudo apt update
```

```
sudo apt -y install avrdude
```

### Programming AIRDOS by avrdude

Please provide a correct path to .hex file. 

Also correct name of ttyUSB interface has to be provided. Then run avrdude.

```
avrdude -v -patmega1284p -carduino -P/dev/ttyUSB0 -b57600 -D -Uflash:w:./build/fw_AIRDOS04_AIRDOS04.latest-CIBuild.hex:i
```

