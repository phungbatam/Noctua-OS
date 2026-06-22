#include "rtc.h"
#include "ports.h"
#include "string.h"

/* Convert BCD to binary */
static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

/* Convert binary to BCD */
static uint8_t bin_to_bcd(uint8_t bin) {
    return ((bin / 10) << 4) | (bin % 10);
}

/* Read from RTC register */
static uint8_t rtc_read_register(uint8_t reg) {
    outb(RTC_PORT, reg);
    return inb(RTC_DATA);
}

/* Write to RTC register */
static void rtc_write_register(uint8_t reg, uint8_t value) {
    outb(RTC_PORT, reg);
    outb(RTC_DATA, value);
}

/* Initialize RTC */
void rtc_init(void) {
    /* Disable NMI (Non-Maskable Interrupt) during initialization */
    uint8_t prev = rtc_read_register(RTC_STATUS_B);
    
    /* Set 24-hour mode and binary format (not BCD) */
    rtc_write_register(RTC_STATUS_B, prev | 0x02 | 0x04);
}

/* Read current time from RTC */
void rtc_read_time(rtc_time_t *time) {
    if (!time) return;
    
    /* Read in binary mode (if supported) or convert from BCD */
    uint8_t status_b = rtc_read_register(RTC_STATUS_B);
    int is_bcd = !(status_b & 0x04);
    
    uint8_t sec = rtc_read_register(RTC_SECONDS);
    uint8_t min = rtc_read_register(RTC_MINUTES);
    uint8_t hour = rtc_read_register(RTC_HOURS);
    uint8_t day = rtc_read_register(RTC_DAY);
    uint8_t month = rtc_read_register(RTC_MONTH);
    uint8_t year = rtc_read_register(RTC_YEAR);
    
    /* Convert from BCD if needed */
    if (is_bcd) {
        time->second = bcd_to_bin(sec);
        time->minute = bcd_to_bin(min);
        time->hour = bcd_to_bin(hour);
        time->day = bcd_to_bin(day);
        time->month = bcd_to_bin(month);
        time->year = bcd_to_bin(year);
    } else {
        time->second = sec;
        time->minute = min;
        time->hour = hour;
        time->day = day;
        time->month = month;
        time->year = year;
    }
    
    /* Try to read century if available */
    time->century = 20; /* Default to 20xx for modern systems */
}

/* Write time to RTC */
void rtc_write_time(const rtc_time_t *time) {
    if (!time) return;
    
    uint8_t status_b = rtc_read_register(RTC_STATUS_B);
    int is_bcd = !(status_b & 0x04);
    
    /* Convert to BCD if needed */
    uint8_t sec = is_bcd ? bin_to_bcd(time->second) : time->second;
    uint8_t min = is_bcd ? bin_to_bcd(time->minute) : time->minute;
    uint8_t hour = is_bcd ? bin_to_bcd(time->hour) : time->hour;
    uint8_t day = is_bcd ? bin_to_bcd(time->day) : time->day;
    uint8_t month = is_bcd ? bin_to_bcd(time->month) : time->month;
    uint8_t year = is_bcd ? bin_to_bcd(time->year) : time->year;
    
    /* Write to RTC registers */
    rtc_write_register(RTC_SECONDS, sec);
    rtc_write_register(RTC_MINUTES, min);
    rtc_write_register(RTC_HOURS, hour);
    rtc_write_register(RTC_DAY, day);
    rtc_write_register(RTC_MONTH, month);
    rtc_write_register(RTC_YEAR, year);
}

/* Get day name */
const char *rtc_get_day_name(uint8_t day) {
    static const char *days[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday", 
        "Thursday", "Friday", "Saturday"
    };
    if (day >= 1 && day <= 7) {
        return days[day - 1];
    }
    return "Unknown";
}

/* Get month name */
const char *rtc_get_month_name(uint8_t month) {
    static const char *months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    if (month >= 1 && month <= 12) {
        return months[month - 1];
    }
    return "Unknown";
}

/* Get Unix timestamp (simplified) */
uint32_t rtc_get_timestamp(void) {
    rtc_time_t time;
    rtc_read_time(&time);
    
    /* Simplified timestamp calculation (not accounting for leap years, etc.) */
    uint32_t days = time.day;
    uint32_t year = time.century * 100 + time.year;
    
    /* Add days for previous months (simplified) */
    static const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int i = 0; i < time.month - 1; i++) {
        days += days_in_month[i];
    }
    
    /* Add days for previous years (simplified, ignoring leap years) */
    days += (year - 1970) * 365;
    
    /* Convert to seconds */
    uint32_t seconds = days * 86400;
    seconds += time.hour * 3600;
    seconds += time.minute * 60;
    seconds += time.second;
    
    return seconds;
}

/* System uptime tracking */
static uint32_t uptime_seconds = 0;
static uint32_t last_tick = 0;

/* Initialize uptime counter */
void uptime_init(void) {
    uptime_seconds = 0;
    last_tick = 0;
}

/* Get uptime in seconds */
uint32_t uptime_get_seconds(void) {
    return uptime_seconds;
}

/* Update uptime (should be called periodically) */
void uptime_update(void) {
    uptime_seconds++;
}
