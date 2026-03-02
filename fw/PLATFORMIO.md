# Firmware build in PlatformIO

## MCU / board

- MCU: `ATmega1284P`
- Core: `MightyCore` (3.0.2)
- Clock: external `8 MHz`
- Bootloader: **Urboot** (MightyCore 3.0.2), UART1, autobaud, no LED

## Build

```bash
cd fw/AIRDOS04
pio run

cd fw/AIRDOS04X
pio run
```

## Upload firmware (urboot/bootloader via UART1)

```bash
cd fw/AIRDOS04
pio run -t upload --upload-port /dev/ttyUSB0

cd fw/AIRDOS04X
pio run -t upload --upload-port /dev/ttyUSB0
```

## Burn bootloader (STK500v2)

This flashes the UART1 bootloader image and writes fuses.

```bash
cd fw/AIRDOS04
pio run -e bootloader_stk500v2 -t upload --upload-port /dev/ttyUSB0
```
