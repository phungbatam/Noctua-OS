#ifndef RTC_H
#define RTC_H

#include <stdint.h>

/* RTC (Real Time Clock) registers */
#define RTC_PORT 0x70
#define RTC_DATA 0x71

/* RTC register addresses */
#define RTC_SECONDS    0x00
#define RTC_MINUTES    0x02
#define RTC_HOURS      0x04
#define RTC_DAY        0x07
#define RTC_MONTH      0x08
#define RTC_YEAR       0x09
#define RTC_CENTURY    0x32  /* Some systems use this */
#define RTC_STATUS_A   0x0A
#define RTC_STATUS_B   0x0B

/* Time structure */
typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t century;
} rtc_time_t;

/* RTC API */
void rtc_init(void);
void rtc_read_time(rtc_time_t *time);
void rtc_write_time(const rtc_time_t *time);
const char *rtc_get_day_name(uint8_t day);
const char *rtc_get_month_name(uint8_t month);
uint32_t rtc_get_timestamp(void);

/* System uptime */
void uptime_init(void);
uint32_t uptime_get_seconds(void);
void uptime_update(void);

#endif
