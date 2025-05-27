
#include <zephyr/drivers/rtc.h>

// Fixed length timestamp.
#define TIMESTRING_LENGTH 28
// DAY DD-MM-YYYY HH:MM:SS:MIL\0
// 28 characters.

// Both rtc_time and RTCC_DATE store the # of years since 1900.
#define BASE_YEAR 1900

// String representations of each weekday name.
extern const char* wday_name[7];

// Initialize RTCC hardware
int rtcc_setup(void);

// Use RTCC to set RTC time
int rtcc_set_rtc_time(struct rtc_time *timeptr);

// Use RTCC to get RTC time
int rtcc_get_rtc_time(struct rtc_time *timeptr);

// Use RTCC to log RTC time
int rtcc_log_rtc_time(struct rtc_time *timeptr);

// Create a timestamp of the RTC time
void rtcc_timestring_rtc_time(char* timestring, struct rtc_time *timeptr);