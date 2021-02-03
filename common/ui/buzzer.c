/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <drivers/pwm.h>


/* from assect tracker kconfig:
 * config UI_BUZZER_PWM_DEV_NAME
	string "PWM device name for buzzer"
	default "PWM_1" if BOARD_THINGY91_NRF9160NS

config UI_BUZZER_PIN
	int "Buzzer pin number"
	default 28 if BOARD_THINGY91_NRF9160NS

config UI_BUZZER_MIN_FREQUENCY
	int "Minimum buzzer frequency"
	default 100

config UI_BUZZER_MAX_FREQUENCY
	int "Maximum buzzer frequency"
	default 10000

 */

#define DEV_NAME "PWM_1"

#define BUZZER_PWM_PIN 28

#define BUZZER_MIN_FREQUENCY		100
#define BUZZER_MAX_FREQUENCY		10000
#define BUZZER_MIN_INTENSITY		0
#define BUZZER_MAX_INTENSITY		100
#define BUZZER_MIN_DUTY_CYCLE_DIV	100
#define BUZZER_MAX_DUTY_CYCLE_DIV	2

static const struct device *pwm_dev;
static atomic_t buzzer_enabled;

static uint32_t intensity_to_duty_cycle_divisor(uint8_t intensity)
{
    return MIN(
            MAX(((intensity - BUZZER_MIN_INTENSITY) *
                 (BUZZER_MAX_DUTY_CYCLE_DIV - BUZZER_MIN_DUTY_CYCLE_DIV) /
                 (BUZZER_MAX_INTENSITY - BUZZER_MIN_INTENSITY) +
                 BUZZER_MIN_DUTY_CYCLE_DIV),
                BUZZER_MAX_DUTY_CYCLE_DIV),
            BUZZER_MIN_DUTY_CYCLE_DIV);
}

static int pwm_out(uint32_t frequency, uint8_t intensity)
{
    static uint32_t prev_period;
    uint32_t period = (frequency > 0) ? USEC_PER_SEC / frequency : 0;
    uint32_t duty_cycle = (intensity == 0) ? 0 :
                          period / intensity_to_duty_cycle_divisor(intensity);

    /* Applying workaround due to limitations in PWM driver that doesn't
     * allow changing period while PWM is running. Setting pulse to 0
     * disables the PWM, but not before the current period is finished.
     */
    if (prev_period) {
        pwm_pin_set_usec(pwm_dev, BUZZER_PWM_PIN,
                         prev_period, 0, 0);
        k_sleep(K_MSEC(MAX((prev_period / USEC_PER_MSEC), 1)));
    }

    prev_period = period;

    return pwm_pin_set_usec(pwm_dev, BUZZER_PWM_PIN,
                            period, duty_cycle, 0);
}

static void buzzer_disable(void)
{
    atomic_set(&buzzer_enabled, 0);

    pwm_out(0, 0);
}

static int buzzer_enable(void)
{
    int err = 0;

    atomic_set(&buzzer_enabled, 1);

    return err;
}

int ui_buzzer_init(void)
{
    const char *dev_name = DEV_NAME;
    int err = 0;

    pwm_dev = device_get_binding(dev_name);
    if (!pwm_dev) {
        printk("Could not bind to device %s\n", dev_name);
        err = -ENODEV;
    }

    buzzer_enable();

    return err;
}

int ui_buzzer_set_frequency(uint32_t frequency, uint8_t intensity)
{
    if (frequency == 0 || intensity == 0) {
        printk("Frequency set to 0, disabling PWM\n");
        buzzer_disable();
        return 0;
    }

    if ((frequency < BUZZER_MIN_FREQUENCY) ||
        (frequency > BUZZER_MAX_FREQUENCY)) {
        return -EINVAL;
    }

    if ((intensity < BUZZER_MIN_INTENSITY) ||
        (intensity > BUZZER_MAX_INTENSITY)) {
        return -EINVAL;
    }

    if (!atomic_get(&buzzer_enabled)) {
        buzzer_enable();
    }

    return pwm_out(frequency, intensity);
}
