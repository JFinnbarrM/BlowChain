

#include <mylib_clock_rtcc.h>

int log_loop(struct rtc_time *timeptr)
{
    while (1) {
        
        rtcc_get_rtc_time(&*timeptr);

        rtcc_log_rtc_time(&*timeptr);
        
        k_sleep(K_MSEC(1000));
    }
}

int main(void)
{
    rtcc_setup();

    struct rtc_time timeptr = {
        0, 
        0,
        0,
        4,
        4 - 1,
        2025 - BASE_YEAR,
        5,
        -1,
        -1,
        0
    };

    rtcc_set_rtc_time(&timeptr);

    log_loop(&timeptr);

    return 0;
}