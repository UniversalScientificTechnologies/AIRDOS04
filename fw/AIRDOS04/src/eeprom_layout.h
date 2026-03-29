// AUTO-GENERATED. Do not edit by hand.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace eeprom {

enum DeviceType : uint16_t {
    USTSIPIN02 = 0,
    AIRDOS04 = 1,
    LABDOS01 = 2,
};

// Packed, little-endian layout. CRC32 covers the whole blob with the crc32
// field zeroed.
struct __attribute__((packed)) EepromRecord {
    uint16_t format_version;
    uint16_t device_type;
    uint32_t crc32;
    uint8_t  hw_rev_major;
    uint8_t  hw_rev_minor;
    char     device_id[10];
    uint32_t config_flags;
    uint8_t  rtc_flags;
    // RTC synchronization fields
    uint32_t init_time;         // Unix timestamp (s) when RTC counter was 0
    uint32_t sync_time;         // Unix timestamp (s) of last synchronization
    uint32_t sync_rtc_seconds;  // RTC counter value (s) at synchronization
    float    calib[3];
    uint32_t calib_ts;
};

} // namespace eeprom
