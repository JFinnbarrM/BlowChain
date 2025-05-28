

#include "lock.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(brain);

int main(void)
{
    lock_init();
    
    while (1) {
        LOG_INF("Opening servo");
        lock_open();
        k_sleep(K_SECONDS(2));
        
        LOG_INF("Current state: %s", lock_is_open() ? "OPEN" : "CLOSED");
        
        LOG_INF("Closing servo");
        lock_close();
        k_sleep(K_SECONDS(2));
        
        LOG_INF("Current state: %s", lock_is_closed() ? "CLOSED" : "OPEN");
    }
    return 0;
}