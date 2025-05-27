

#include <stdbool.h>
#include <stdint.h>

enum did_nums {
    TEMP = 0,
    HUME = 1,
    PRES = 2,
    LITE = 5,
    ALL = 15
};

int sampling_state(void);

int sampling_set_on(void);
int sampling_set_off(void);

int sampling_set_frequency(int freq);

void sample_did_set(uint8_t did, bool set_to);