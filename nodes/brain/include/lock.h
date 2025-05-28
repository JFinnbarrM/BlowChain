#include <stdbool.h>

typedef enum {
    LOCK_CLOSED = 0,
    LOCK_OPEN = 1
} lock_state_t;

void lock_init(void);
void lock_open(void);
void lock_close(void);
bool lock_is_open(void);
bool lock_is_closed(void);