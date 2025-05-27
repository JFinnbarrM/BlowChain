
#include <mylib_clock_rtcc.h>

#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include <string.h>

// Print an rtc_time struct value to the shell.
static int shell_print_rtc_time(const struct shell *sh, struct rtc_time *timeptr)
{
    // Construct string representation of rtc_time struct and print it to shell
    shell_print(sh, "DAY: %s | DATE: %02d-%02d-%04d | TIME: %02d:%02d:%02d.%03d",
            wday_name[timeptr->tm_wday],
            timeptr->tm_mday,
            timeptr->tm_mon + 1, // rtc_time uses [0, 11] for months.
            timeptr->tm_year + BASE_YEAR, // rtc_time uses Year-1900.
            timeptr->tm_hour,
            timeptr->tm_min,
            timeptr->tm_sec,
            timeptr->tm_nsec /1000000 // Only display millisecond accuracy.
        );

    return 0; // Success.
}

// Sets the clock to a user specified day.
static int handle_rtc_write_day(const struct shell *sh, size_t argc, char **argv) 
{   
    // Check for valid argument.
    int day = atoi(argv[1]);
    if (day >= 0 && day <= 6) {

        // Get the current time.
        struct rtc_time timeptr = {0};
        rtcc_get_rtc_time(&timeptr);
        // Update the current time.
        timeptr.tm_wday = day;
        rtcc_set_rtc_time(&timeptr);
        shell_print_rtc_time(sh, &timeptr);
        return 0;
    };

    // Error message if invalid argument.
    shell_error(sh, 
        "\"%s\" is an invalid weekday number.\
        \nDays must be numbered: [0-6] (Sun to Sat)", 
        argv[1]);
    return -1;
}

// Sets the clock to a user specified date.
static int handle_rtc_write_date(const struct shell *sh, size_t argc, char **argv) 
{   
    // Check for valid argument.
    int d, m, y;
    if (sscanf(argv[1], "%2d-%2d-%4d", &d, &m, &y) == 3) {

        // Get the current time.
        struct rtc_time timeptr = {0};
        rtcc_get_rtc_time(&timeptr);
        // Update the current time.
        timeptr.tm_mday = d;
        timeptr.tm_mon = m - 1;
        timeptr.tm_year = y - BASE_YEAR;
        rtcc_set_rtc_time(&timeptr);
        shell_print_rtc_time(sh, &timeptr);
        return 0;
    };

    // Error message if invalid argument.
    shell_error(sh, 
        "\"%s\" is an invalid date format.\n\
        Dates must be formatted: DD-MM-YYYY", 
        argv[1]);
    return -1;
}

// Sets the clock to a user specified time.
static int handle_rtc_write_time(const struct shell *sh, size_t argc, char **argv) 
{   
    // Check for valid argument.
    int h, m, s;
    if (sscanf(argv[1], "%2d:%2d:%2d", &h, &m, &s) == 3) {

        // Get the current time.
        struct rtc_time timeptr = {0};
        rtcc_get_rtc_time(&timeptr);
        // Update the current time.
        timeptr.tm_hour = h;
        timeptr.tm_min = m;
        timeptr.tm_sec = s;
        rtcc_set_rtc_time(&timeptr);
        shell_print_rtc_time(sh, &timeptr);
        return 0;
    };
    
    // Error message if invalid argument.
    shell_error(sh, 
        "\"%s\" is an invalid time format.\
        \nTimes must be formatted: HH:MM:SS", 
        argv[1]);
    return -1;
}

// Create clock write subcommands.
SHELL_STATIC_SUBCMD_SET_CREATE(rtc_write_cmds,
    SHELL_CMD_ARG(day, NULL, "[0-6](Sun-Sat)", handle_rtc_write_day, 2, 0),
    SHELL_CMD_ARG(date, NULL, "DD-MM-YY", handle_rtc_write_date, 2, 0),
    SHELL_CMD_ARG(time, NULL, "HH:MM:SS", handle_rtc_write_time, 2, 0),
    SHELL_SUBCMD_SET_END
);

// Gets the current clock value and prints it to shell.
static int handle_rtc_read(const struct shell *sh, size_t argc, char **argv)
{
    struct rtc_time timeptr = {0};
    rtcc_get_rtc_time(&timeptr);
    shell_print_rtc_time(sh, &timeptr);
    return 0;
}

// Create clock subcommands.
SHELL_STATIC_SUBCMD_SET_CREATE(rtc_cmds,
    SHELL_CMD_ARG(r, NULL, "read the time", handle_rtc_read, 1, 0),
    SHELL_CMD_ARG(w, &rtc_write_cmds, "write the time", NULL, 1, 0),
    SHELL_SUBCMD_SET_END
);

// Register clock command.
SHELL_CMD_REGISTER(rtc, &rtc_cmds, "Use the real time clock", NULL);