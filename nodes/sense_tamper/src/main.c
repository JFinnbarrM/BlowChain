#include "new_sensor_lib.h"
#include "bluetooth_disco.h"

#define MY_STACK_SIZE 1024
#define COMMON_PRIORITY 4

K_THREAD_DEFINE(sensor_tid, MY_STACK_SIZE * 4,  sender_thread_disco, NULL, NULL, NULL, COMMON_PRIORITY, 0, 0);

int main(void) {
    bluetooth_init_disco();
    printf("done!\n");
    return 0;
}