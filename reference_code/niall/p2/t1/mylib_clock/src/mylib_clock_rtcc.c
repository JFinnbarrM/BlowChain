
#include "mylib_clock_rtcc.h"
#include "zephyr/drivers/rtc.h"

#include <em_rtcc.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/logging/log.h>

// Registers the logging name of this file.
LOG_MODULE_REGISTER(mylib_rtcc);

// Using a 32768Hz counter means there is 32768 ticks per 1000000000 nanoseconds.
#define NSECS_PER_TICK 1000000000ULL/32768ULL
// This gives ~30 microsecond (us) accuracy.

// String representations of each weekday name.
const char* wday_name[7] = {
    "Sun",
    "Mon", 
    "Tue", 
    "Wed", 
    "Thu", 
    "Fri", 
    "Sat"
};

// Initialize RTCC hardware
int rtcc_setup(void)
{
    // Configure RTCC_CTRL register (0x000).
    RTCC_Init_TypeDef rtccInitConfig = {0}; // Includes setting all reserve bits to 0.

    // Enable once initial calendar time is set.
    rtccInitConfig.enable = true; 
    // External syncronisation requires keeping real time while debugging.
    // Disabled.
    rtccInitConfig.debugRun = false; 
    // Disable pre-counter wrap on channel 0 CCV value.
    rtccInitConfig.precntWrapOnCCV0 = false;
    // Disable counter wrap on channel 1 CCV value.
    rtccInitConfig.cntWrapOnCCV1 = false;
    // Divides counter by 32768 as it operates at 32768Hz.
    rtccInitConfig.presc = rtccCntPresc_32768;
    // Counter incriments according to prescaler value set above.
    rtccInitConfig.prescMode = rtccCntTickPresc;
    // Only using the internal clock so no external clock failure detection required.
    rtccInitConfig.enaOSCFailDetect = false;
    // Set the counter to calendar mode.
    rtccInitConfig.cntMode = rtccCntModeCalendar;
    // Dont disable correction for leap years.
    rtccInitConfig.disLeapYearCorr = false;
    
    // Initialise RTCC_CTRL register.
    RTCC_Init(&rtccInitConfig);

    return 0; // Success.
}

// Use RTCC to set RTC time
int rtcc_set_rtc_time(struct rtc_time *timeptr)
{
    // Convert rtc_time struct values into RTCC_DATE register values.
    uint8_t dayow  = ( timeptr->tm_wday       ) & 0b111;
    uint8_t yeart  = ( timeptr->tm_year   / 10) & 0b1111; // RTCC_DATE and rtc_time both use year-1900.
    uint8_t yearu  = ( timeptr->tm_year   % 10) & 0b1111; // RTCC_DATE and rtc_time both use year-1900.
    uint8_t montht = ((timeptr->tm_mon+1) / 10) & 0b1; // rtc_time uses [0, 11] for months.
    uint8_t monthu = ((timeptr->tm_mon+1) % 10) & 0b1111; // rtc_time uses [0, 11] for months.
    uint8_t dayomt = ( timeptr->tm_mday   / 10) & 0b11;
    uint8_t dayomu = ( timeptr->tm_mday   % 10) & 0b1111;
    // Construct the RTCC_DATE register value.
    uint32_t rtcc_date_reg = (dayow  << 24) 
                           | (yeart  << 20) 
                           | (yearu  << 16) 
                           | (montht << 12) 
                           | (monthu << 8 ) 
                           | (dayomt << 4 ) 
                           | (dayomu << 0 );

    // Convert rtc_time struct values into RTCC_TIME register values.
    uint8_t hourt = (timeptr->tm_hour / 10) & 0b11;
    uint8_t houru = (timeptr->tm_hour % 10) & 0b1111;
    uint8_t mint  = (timeptr->tm_min  / 10) & 0b111;
    uint8_t minu  = (timeptr->tm_min  % 10) & 0b1111;
    uint8_t sect  = (timeptr->tm_sec  / 10) & 0b111;
    uint8_t secu  = (timeptr->tm_sec  % 10) & 0b1111;
    // Construct the RTCC_TIME register value.
    uint32_t rtcc_time_reg = (hourt << 20) 
                           | (houru << 16) 
                           | (mint  << 12) 
                           | (minu  << 8 ) 
                           | (sect  << 4 ) 
                           | (secu  << 0 );

    // Construct the RTCC_PRECNT register value.
    uint16_t precnt_reg = timeptr->tm_nsec / NSECS_PER_TICK;


    RTCC_Enable(false); // Disable RTCC while updating registers.

    RTCC_TimeSet(rtcc_time_reg);
    RTCC_DateSet(rtcc_date_reg);
    RTCC_PreCounterSet(precnt_reg);

    RTCC_Enable(true); // Enable RTCC after updating registers.

    return 0; // Success.
}

// Use RTCC to get RTC time
int rtcc_get_rtc_time(struct rtc_time *timeptr)
{
    // Get the RTCC_PRECNT register value.
    uint32_t rtcc_precnt_reg = RTCC_PreCounterGet();
    // Extract values from the RTCC_PRECNT register.
    uint16_t precnt = (rtcc_precnt_reg >> 0) & 0b111111111111111;
    
    // Get the RTCC_DATE register value.
    uint32_t rtcc_date_reg = RTCC_DateGet();
    // Extract the values from the RTCC_DATE register.
    uint8_t dayow  = (rtcc_date_reg >> 24) & 0b111;
    uint8_t yeart  = (rtcc_date_reg >> 20) & 0b1111;
    uint8_t yearu  = (rtcc_date_reg >> 16) & 0b1111;
    uint8_t montht = (rtcc_date_reg >> 12) & 0b1;
    uint8_t monthu = (rtcc_date_reg >> 8)  & 0b1111;
    uint8_t dayomt = (rtcc_date_reg >> 4)  & 0b11;
    uint8_t dayomu = (rtcc_date_reg >> 0)  & 0b1111;

    // Get the RTCC_TIME register value.
    uint32_t rtcc_time_reg = RTCC_TimeGet();
    // Extract the values from the RTCC_TIME register.
    uint8_t hourt = (rtcc_time_reg >> 20) & 0b11;
    uint8_t houru = (rtcc_time_reg >> 16) & 0b1111;
    uint8_t mint  = (rtcc_time_reg >> 12) & 0b111;
    uint8_t minu  = (rtcc_time_reg >> 8)  & 0b1111;
    uint8_t sect  = (rtcc_time_reg >> 4)  & 0b111;
    uint8_t secu  = (rtcc_time_reg >> 0)  & 0b1111;

    // Set the rtc_time struct using extracted values.
    timeptr->tm_sec = (10*sect) + secu;
    timeptr->tm_min = (10*mint) + minu;
    timeptr->tm_hour = (10*hourt) + houru;
    timeptr->tm_mday = (10*dayomt) + dayomu;
    timeptr->tm_mon = (10*montht) + monthu - 1; // rtc_time uses [0, 11] for months.
    timeptr->tm_year = (10*yeart) + yearu; // RTCC_DATE and rtc_time both use year-1900.
    timeptr->tm_wday = dayow;
    timeptr->tm_yday = -1;
    timeptr->tm_isdst = -1;
    timeptr->tm_nsec = precnt * NSECS_PER_TICK; // accurate to the nearest 30517th nanosecond.

    return 0; // Success.
}

// Use RTCC to log RTC time
int rtcc_log_rtc_time(struct rtc_time *timeptr)
{
    // Construct string representation of rtc_time struct and log it.
    char timestring[64];
    rtcc_timestring_rtc_time(timestring, timeptr);
    LOG_INF("%s", timestring);

    return 0; // Success.
}

// Create a timestamp of the RTC time
void rtcc_timestring_rtc_time(char* timestring, struct rtc_time *timeptr)
{
    snprintf(timestring, 64,
        "%02d-%02d-%04d %s %02d:%02d:%02d.%03d",
            timeptr->tm_mday,
            timeptr->tm_mon + 1, // rtc_time uses [0, 11] for months.
            timeptr->tm_year + BASE_YEAR, // rtc_time uses Year-1900.
            wday_name[timeptr->tm_wday],
            timeptr->tm_hour,
            timeptr->tm_min,
            timeptr->tm_sec,
            timeptr->tm_nsec /1000000 // Only display millisecond accuracy.
        );
}