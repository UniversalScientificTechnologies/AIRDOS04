# AIRDOS04 output format specification

The AIRDOS04 firmware writes data to the SD card (files `1.TXT`, `2.TXT`, …) and, when `DEBUG` is defined, also to the debug serial port (Serial1, 115200 baud). Each record is a text line terminated by newline (`\n`; within measurement blocks `\r\n` may be used, but real dumps can also show plain `\n`). Lines start with `$` for data or `#` for debug; data fields are comma-separated.



## Format version and constants

| Constant | Value | Meaning |
|----------|--------|---------|
| Data format | MAJOR.MINOR = 2.0 | Version of the log format |
| Device type | `AIRDOS04C` | DOSimeter unit identifier |
| Digital module type | `BATDATUNIT01B` | Battery/data unit board |
| Analog module type | `USTSIPIN03A` | ADC / analog front-end board |
| CHANNELS | 4 | Number of histogram energy channels |
| MAX_EVENTS | 300 | Maximum number of above-threshold events per integration |
| Integration interval | 10 s | Two 5 s halves; timer tick for 5 s = 39063 (_5S) |
| Timer tick | 128 µs | TCNT1/TCNT3 at 8 MHz / 1024 prescaler = 7.8125 kHz |



## Message types overview

| Prefix | Description | When emitted | Est. max line length |
|--------|-------------|--------------|----------------------|
| `$DOS` | DOSimeter identification | At start of every log file | \~140 chars |
| `$DIG` | Digital module info | At start of every log file | \~60 chars |
| `$DIG_NAME` | Digital module name (from EEPROM) | At start of every log file | \~20 chars |
| `$ADC` | Analog module info | At start of every log file | \~58 chars |
| `$ADC_NAME` | Analog module name (from EEPROM) | At start of every log file | \~20 chars |
| `$BATP` | Battery presence | At start of every log file | \~20 chars |
| `$TIME` | Time and sync info | At start of every log file | \~65 chars |
| `$FSEQ` | File sequence info | At start of every log file | \~25 chars |
| `$RTCCHK` | RTC check/init status | On RTC check (start or when needed) | \~50 chars |
| `$ENV` | Environmental sensors | Every 5 minutes | \~75 chars |
| `$BATT` | Battery status | Every 30 minutes | \~65 chars |
| `$START` | Start of integration block | Every 10 s | \~22 chars |
| `$E` | Single above-threshold event | Per event, within block | \~22 chars |
| `$STOP` | End of integration block | Every 10 s, after events | \~58 chars |



## 1. File header (written at the start of every log file)

The header is written at the beginning of every SD log file — both the initial file opened at startup and every subsequent file created when the previous one reaches `MAX_MEASUREMENTS` cycles. At boot it is also printed to Serial1. Lines within the header are separated by `\r\n`.

The `$DOS`, `$DIG`, `$DIG_NAME`, `$ADC`, `$ADC_NAME`, `$BATP` and `$TIME` lines are identical across all files of a single measurement session (they reflect the state captured at the initial startup). The `$FSEQ` line is updated per-file to indicate the file's order within the measurement.



### 1.1 `$DOS` — DOSimeter identification

Identifies the device type, firmware version, Git identity, and the analog board serial number (from the same EEPROM as used for ADC).

**Format:**

```
$DOS,<TYPE>,<FWversion>,0,<git_hash>,<build_type>,<serial_analog_16B_hex>
```

**Parameters:**

| Parameter | Type / source | Description |
|-----------|----------------|-------------|
| TYPE | Literal string | Always `AIRDOS04C`. |
| FWversion | String | Build version: `MAJOR.MINOR.GHRELEASE-GHBUILD-GHBUILDTYPE` (e.g. `2.0.0-0-User`). MAJOR.MINOR = data format version; rest from build. |
| 0 | Literal | Reserved field. |
| git_hash | String | 40-character Git commit hash from `githash.h`. |
| build_type | String | Build type from same source (e.g. `User`). |
| serial_analog_16B_hex | 32 hex digits | 16-byte serial number from analog board EEPROM (I²C addr. 0x5B), address 0x0800. Each byte as two lowercase hex digits, no separators. |

**Example (real device):**

```
$DOS,AIRDOS04C,2.0.0-0-User,0,a3e23b543a4de5dc3d057462bb6109bf3db0b44b,User,0910410874100851c40ba080a08000b3
```

**Maximum line length (estimate):** prefix \~5 + TYPE 9 + 2 + FWversion \~30 + 3 + githash \~50 + 1 + 32 ⇒ **\~140 characters**.



### 1.2 `$DIG` — Digital module

Identifies the digital (battery/data) board and its configuration.

**Format:**

```
$DIG,<DIGTYPE>,<serial_digital_16B_hex>,<DIGconf1_hex><DIGconf2_hex>
```

**Parameters:**

| Parameter | Type / source | Description |
|-----------|----------------|-------------|
| DIGTYPE | Literal string | Always `BATDATUNIT01B`. |
| serial_digital_16B_hex | 32 hex digits | 16-byte serial number from digital board EEPROM (0x58), address 0x0800. |
| DIGconf1_hex | 2 hex digits | Configuration byte 1 from digital config EEPROM (0x50), address 0x0000. |
| DIGconf2_hex | 2 hex digits | Configuration byte 2 from same EEPROM. No comma between the two hex bytes. |

**Example (real device):**

```
$DIG,BATDATUNIT01B,09104108741008520c0ca080a080005e,ffff
```

**Maximum line length (estimate):** prefix \~5 + DIGTYPE 14 + 2 + 32 + 1 + 4 ⇒ **\~60 characters**.



### 1.3 `$DIG_NAME` — Digital module name

User-assigned name of the digital (detector) module, read from the digital configuration EEPROM (I²C addr. 0x50, `device_id` field of the EEPROM record, up to 10 bytes). Printed as an ASCII string, truncated at the first `\0` byte. If the EEPROM read fails at startup, the name is written as an empty string (the record is zero-initialized).

**Format:**

```
$DIG_NAME,<dig_name>
```

**Parameters:**

| Parameter | Type / source | Description |
|-----------|----------------|-------------|
| dig_name | String (≤ 10 chars) | Digital module name from EEPROM `device_id`. Empty if not programmed or EEPROM read failed. |

**Example:**

```
$DIG_NAME,DIG-001
```

**Maximum line length (estimate):** prefix \~10 + 10 ⇒ **\~20 characters**.



### 1.4 `$ADC` — Analog module (ADC)

Identifies the analog front-end board and its ADC configuration.

**Format:**

```
$ADC,<ADCTYPE>,<serial_analog_16B_hex>,<ADCconf1_hex><ADCconf2_hex>
```

**Parameters:**

| Parameter | Type / source | Description |
|-----------|----------------|-------------|
| ADCTYPE | Literal string | Always `USTSIPIN03A`. |
| serial_analog_16B_hex | 32 hex digits | Same 16-byte serial as in `$DOS`, from analog EEPROM (0x5B). |
| ADCconf1_hex | 2 hex digits | Configuration byte 1 from analog config EEPROM (0x53), address 0x0000. |
| ADCconf2_hex | 2 hex digits | Configuration byte 2. No comma between the two hex bytes. |

**Example (real device):**

```
$ADC,USTSIPIN03A,0910410874100851c40ba080a08000b3,ffff
```

**Maximum line length (estimate):** prefix \~5 + ADCTYPE 12 + 2 + 32 + 1 + 4 ⇒ **\~58 characters**.



### 1.5 `$ADC_NAME` — Analog module name

User-assigned name of the analog module, read from the analog configuration EEPROM (I²C addr. 0x53, `device_id` field of the EEPROM record, up to 10 bytes). Printed as an ASCII string, truncated at the first `\0` byte. If the EEPROM read fails at startup, the name is written as an empty string (the record is zero-initialized).

**Format:**

```
$ADC_NAME,<adc_name>
```

**Parameters:**

| Parameter | Type / source | Description |
|-----------|----------------|-------------|
| adc_name | String (≤ 10 chars) | Analog module name from EEPROM `device_id`. Empty if not programmed or EEPROM read failed. |

**Example:**

```
$ADC_NAME,ADC-001
```

**Maximum line length (estimate):** prefix \~10 + 10 ⇒ **\~20 characters**.



### 1.6 `$BATP` — Battery presence

Reports whether a battery was detected at startup and the voltage measured during detection (charger ADC).

**Format:**

```
$BATP,<present>,<battery_mV>
```

**Parameters:**

| Parameter | Type / source | Description |
|-----------|----------------|-------------|
| present | `0` or `1` | `1` = battery present, `0` = no battery (below threshold). |
| battery_mV | uint16_t | Battery voltage in mV at detection (e.g. 0–12000). From charger VBAT ADC. |

**Example:**

```
$BATP,1,4150
```

**Maximum line length (estimate):** prefix \~6 + 1 + 1 + 5 (mV up to 65535) ⇒ **\~20 characters**.



### 1.7 `$TIME` — Time and synchronization

Reports RTC value, EEPROM sync data, computed Unix time, sync age, and human-readable UTC time.

**Format:**

```
$TIME,<rtc_seconds>,<eeprom_sync_time>,<current_unix_time>,<sync_age>,<YYYY-MM-DD HH:MM:SS>
```

**Parameters:**

| Parameter | Type / source | Description |
|-----------|----------------|-------------|
| rtc_seconds | uint32_t | Current RTC counter in seconds (stopwatch). |
| eeprom_sync_time | uint32_t | Unix timestamp of last synchronization, from EEPROM (`sync_time`). |
| current_unix_time | uint32_t | Computed current Unix time: `rtc_seconds + eeprom_sync_time` (or 0 if not synced). |
| sync_age | uint32_t | Age of sync in seconds: `rtc_seconds - sync_rtc_seconds`. |
| YYYY-MM-DD HH:MM:SS | String | Human-readable UTC time (zero-padded; e.g. `2025-02-25 14:30:00`). |

**Example:**

```
$TIME,1234567,1708862400,1708863634,0,2025-02-25 14:30:34
```

**Maximum line length (estimate):** prefix \~6 + 4×10 (four 32-bit values) + 4 commas + 19 (datetime) ⇒ **\~65 characters**.



### 1.8 `$FSEQ` — File sequence within a measurement

Reports the order of the current log file within a single measurement session and the Unix timestamp of the measurement start. The Unix timestamp is the same across all files of one session; `file_seq` starts at `0` for the first file opened at startup and is incremented by `1` each time the firmware rotates to a new file (after `MAX_MEASUREMENTS` integration cycles).

**Format:**

```
$FSEQ,<file_seq>,<measurement_start_unix_time>
```

**Parameters:**

| Parameter | Type / source | Description |
|-----------|----------------|-------------|
| file_seq | uint16_t | Index of the log file within the current measurement session. `0` for the first file, `1` for the second, and so on. |
| measurement_start_unix_time | uint32_t | Computed current Unix time at startup (same as `current_unix_time` in `$TIME`). `0` if the device was not time-synced. |

**Example:**

```
$FSEQ,0,1708863634
$FSEQ,1,1708863634
```

**Maximum line length (estimate):** prefix \~6 + 5 (file_seq) + 1 + 10 (uint32) ⇒ **\~25 characters**.



## 2. `$RTCCHK` — RTC check / initialization

Emitted on Serial1 and optionally appended to the SD file when the RTC is checked or re-initialized to stopwatch mode.

**Format (OK):**

```
$RTCCHK,<tm>.<tm_s100>,OK,reg07=0x<hex>,reg28=0x<hex>
```

**Format (after init):**

```
$RTCCHK,<tm>.<tm_s100>,INIT,reg07=0x<hex>,reg28=0x<hex>
```

**Parameters:**

| Parameter | Type / source | Description |
|-----------|----------------|-------------|
| tm | uint32_t | RTC time in seconds (integer part). |
| tm_s100 | uint8_t | Hundredths of second (0–99), from RTC BCD. |
| Status | `OK` or `INIT` | Whether RTC was already correct or was re-initialized. |
| reg07 | 2 hex digits | RTC register 0x07 value (bit 7 must be 0). |
| reg28 | 2 hex digits | RTC register 0x28 value (bit 4 RTCM must be 1 for stopwatch). |

**Example:**

```
$RTCCHK,1234567.50,OK,reg07=0x00,reg28=0x97
```

**Maximum line length (estimate):** prefix \~9 + 10 + 1 + 2 + 5 + 8 + 2 + 9 + 2 ⇒ **\~50 characters**.



## 3. `$ENV` — Environmental sensors

Emitted every 5 minutes (every 30 integration cycles). Contains two SHT31 sensors and one MS5611 (temperature and pressure).

**Format:**

```
$ENV,<count>,<tm>.<tm_s100>,<T1>,<H1>,<T2>,<H2>,<T_MS5611>,<P_MS5611>
```

**Parameters:**

| Parameter | Type / source | Description |
|-----------|----------------|-------------|
| count | uint16_t | Current measurement cycle index (0 … MAX_MEASUREMENTS). |
| tm | uint32_t | RTC time in seconds. |
| tm_s100 | uint8_t | Hundredths of second. |
| T1 | float, 1 decimal | Temperature (°C) from primary SHT31 (I²C 0x44). |
| H1 | float, 1 decimal | Relative humidity (%) from primary SHT31. |
| T2 | float, 1 decimal | Temperature (°C) from secondary SHT31 (0x45). |
| H2 | float, 1 decimal | Relative humidity (%) from secondary SHT31. |
| T_MS5611 | float, 2 decimals | Temperature (°C) from MS5611 (0x77). |
| P_MS5611 | float, 2 decimals | Pressure (hPa) from MS5611. |

**Example (real device):**

```
$ENV,179,4275399683.0,29.1,44.0,27.5,45.5,29.31,989.05
```

**Maximum line length (estimate):** prefix \~5 + count 5 + 10 + 1 + 2 + 4×(5 for T/H) + 6 + 7 + 8 commas ⇒ **\~75 characters** (allowing for negative temps and 3-digit pressure).



## 4. `$BATT` — Battery status

Emitted every 30 minutes (every 180 integration cycles). Values from BQ34Z100 gas gauge.

**Format:**

```
$BATT,<count>,<tm>.<tm_s100>,<voltage_mV>,<current_mA>,<remaining_mAh>,<full_charge_mAh>,<temperature_C>
```

**Parameters:**

| Parameter | Type / source | Description |
|-----------|----------------|-------------|
| count | uint16_t | Measurement cycle index. |
| tm, tm_s100 | As above | RTC time. |
| voltage_mV | int16_t | Battery voltage in mV (register 0x08). Can be negative if gauge invalid. |
| current_mA | int16_t | Current in mA (0x0A); positive = discharge, negative = charge. |
| remaining_mAh | int16_t | Remaining capacity in mAh (0x04). Can be negative if gauge invalid. |
| full_charge_mAh | int16_t | Full charge capacity in mAh (0x06). Can be negative if gauge invalid. |
| temperature_C | float | Cell temperature in °C: `register_0x0C * 0.1 - 273.15`. Can be negative (e.g. when gauge not available). |

**Example (normal):**

```
$BATT,720,12345.50,4150,-120,1800,2000,25.3
```

**Example (real device, gauge unavailable or no battery):**

```
$BATT,179,4275399683.0,-257,-257,-257,-257,-298.85
```

**Maximum line length (estimate):** prefix \~6 + 5 + 10 + 1 + 2 + 5 + 6 (signed current) + 5 + 5 + 6 (temp) + 7 commas ⇒ **\~65 characters**.



## 5. Measurement block — `$START`, `$E`, `$STOP`

Emitted every 10 s (one integration period). The block can span multiple lines; lines within the block are separated by `\r\n`. The first line has no leading `\r\n`.



### 5.1 `$START` — Start of integration

**Format:**

```
$START,<count>,<event_time_0>
```

**Parameters:**

| Parameter | Type / source | Description |
|-----------|----------------|-------------|
| count | uint16_t | Measurement cycle index. |
| event_time_0 | uint16_t | System time at start of integration (TCNT3). Unit: 128 µs per tick (7.8125 kHz). Used as reference for “time of first event” and for dead-time accounting. |

**Example (real device):**

```
$START,0,1
$START,1,13034
$START,18,35623
```

**Maximum line length (estimate):** prefix \~8 + count 5 + systime 5 + 2 commas ⇒ **\~22 characters**.



### 5.2 `$E` — Single above-threshold event

One line per event where the ADC value was ≥ CHANNELS (i.e. above the energy threshold). Index n = 1 … events_counter−1. Each line is prefixed with `\r\n`.

**Format:**

```
\r\n$E,<long_event_time>,<event_channel>
```

**Parameters:**

| Parameter | Type / source | Description |
|-----------|----------------|-------------|
| long_event_time | uint32_t (in ticks) | Event time in timer ticks. If `event_time2[n] == 0`: same 5 s window as start, so value = `event_time[n]` (TCNT1). If `event_time2[n] != 0`: second 5 s window, value = `_5S + event_time[n]` (with _5S = 39063). Tick = 128 µs. |
| event_channel | uint16_t | Raw ADC value for this event (adcVal; ≥ CHANNELS). In practice can be 3 digits (e.g. 4, 24, 69, 704, 401). |

**Example (real device):**

```
$E,488,24
$E,5807,5
$E,7617,4
$E,55683,69
$E,38278,704
```

**Maximum line length (estimate):** `\r\n` 2 + prefix \~4 + long_event_time up to 39063+65535 → 6 digits + comma + event_channel 5 digits ⇒ **\~22 characters** (excluding CR/LF).



### 5.3 `$STOP` — End of integration

**Format:**

```
\r\n$STOP,<count>,<tm>.<tm_s100>,<systime>,<events_count>,<histogram_0>,<histogram_1>,<histogram_2>,<histogram_3>
```

**Parameters:**

| Parameter | Type / source | Description |
|-----------|----------------|-------------|
| count | uint16_t | Same measurement cycle index as in `$START`. |
| tm, tm_s100 | As above | RTC time at end of integration. Printed as `<tm>.<tm_s100>` (e.g. `4275397709.0`). |
| systime | uint16_t | TCNT3 at end of integration (128 µs per tick). |
| events_count | uint16_t | Number of above-threshold events in this integration (`events_counter - 1`). Can exceed 299 when the event buffer overflows: only the first 300 events are stored as `$E` lines; the total count is still reported here. |
| histogram_0 … histogram_3 | uint16_t | Count of events in energy channels 0–3 (ADC value &lt; CHANNELS). Channel index = adcVal. |

**Example (normal):**

```
$STOP,100,12345.75,78125,2,150,200,180,50
```

**Example (real device, event buffer overflow — events_count &gt; 299, only 300 events have `$E` lines):**

```
$STOP,179,4275399681.0,31359,427,19373,11,24,7
```

**Maximum line length (estimate):** prefix \~7 + count 5 + 10 + 1 + 2 + systime 5 + events_count 3–5 + 4×5 (histogram) + 9 commas ⇒ **\~58–60 characters** (excluding optional CR/LF).



## 6. Debug / service messages (Serial1)

Lines starting with `#` are for diagnostics. They are sent on Serial1 only and are not necessarily written to the SD file.

| Message | Meaning |
|---------|--------|
| `#Cvak...` | Firmware started. |
| `#Hmmm...` | Initialization done, before main measurement loop. |
| `#RTC_TIME,<sec>` | Current RTC value in seconds. |
| `#EEPROM_SYNC_TIME,<t>` | `sync_time` read from EEPROM. |
| `#EEPROM_INIT_TIME,<t>` | `init_time` from EEPROM. |
| `#EEPROM_SYNC_RTC_SECONDS,<t>` | `sync_rtc_seconds` from EEPROM. |
| `#DIG_NAME,<name>` | Digital module name (`device_id`) from digital cfg EEPROM (0x50). |
| `#ADC_NAME,<name>` | Analog module name (`device_id`) from analog cfg EEPROM (0x53). |
| `#EEPROM read failed` | Failed to read digital EEPROM record. |
| `#EEPROM analog read failed` | Failed to read analog EEPROM record. |
| `#SYNC_AGE,<sec>` | Sync age in seconds. |
| `#CURRENT_UNIX_TIME,<t>` | Computed current Unix timestamp. |
| `#CURRENT_TIME,YYYY-MM-DD HH:MM:SS` | Human-readable current time. |
| `#Filename,<name>` | Current SD file name (e.g. `5.TXT`). |
| `#Filesize,<bytes>` | Size of the open SD file in bytes. |
| `#SD init false` | SD card initialization failed. |
| `#SD false` | SD write or open failed. |
