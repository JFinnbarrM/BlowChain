#include "new_sensor_lib.h"
#include "bluetooth_thingy.h"

#define MY_STACK_SIZE 1024
#define COMMON_PRIORITY 4

K_THREAD_DEFINE(voc_tid, MY_STACK_SIZE * 4,  sender_thread_thingy, NULL, NULL, NULL, COMMON_PRIORITY, 0, 0);

int main(void) {
    bluetooth_init_thingy();
    printf("done!\n");
    return 0;
}