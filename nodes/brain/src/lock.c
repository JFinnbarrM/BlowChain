#include "lock.h"

#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(lock);

// Updated pulse widths for standard servos
#define LOCK_OPEN_PULSE PWM_MSEC(2.0)   // 1.0ms pulse (0°)
#define LOCK_CLOSE_PULSE PWM_MSEC(1.0)  // 2.0ms pulse (90°)
#define SERVO_PERIOD PWM_MSEC(20)      // 20ms period (50Hz)

static const struct device *lock_device = DEVICE_DT_GET(DT_NODELABEL(pwm0));
static atomic_t lock_state = ATOMIC_INIT(LOCK_CLOSED);

static void set_lock_position(lock_state_t state)
{
    if (state == LOCK_OPEN) {
        pwm_set(lock_device, 0, SERVO_PERIOD, LOCK_OPEN_PULSE, PWM_POLARITY_NORMAL);
        atomic_set(&lock_state, LOCK_OPEN);
        LOG_INF("Servo moved to OPEN position (1.0ms pulse)");
    } else {
        pwm_set(lock_device, 0, SERVO_PERIOD, LOCK_CLOSE_PULSE, PWM_POLARITY_NORMAL);
        atomic_set(&lock_state, LOCK_CLOSED);
        LOG_INF("Servo moved to CLOSED position (2.0ms pulse)");
    }
}

void lock_init(void)
{
    if (!device_is_ready(lock_device)) {
        LOG_ERR("PWM device not ready");
        return;
    }
    
    // Initialize to closed position
    set_lock_position(LOCK_CLOSED);
    LOG_INF("Servo initialized to CLOSED position");
}

void lock_open(void)
{
    set_lock_position(LOCK_OPEN);
}

void lock_close(void)
{
    set_lock_position(LOCK_CLOSED);
}

bool lock_is_open(void)
{
    return atomic_get(&lock_state) == LOCK_OPEN;
}

bool lock_is_closed(void)
{
    return atomic_get(&lock_state) == LOCK_CLOSED;
}