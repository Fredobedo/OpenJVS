#include "hardware/rotary.h"
#include "hardware/device.h"
#include "console/debug.h"

gpio_handler_t *gpio18, *gpio19, *gpio20, *gpio21;
/**
 * Init Rotary on Raspberry Pi HAT
 *
 * Inits the rotary controller on the Raspberry Pi HAT
 * to select which map we will use
 *
 * @returns JVS_ROTARY_STATUS_SUCCESS if it inited correctly.
 *
 *
 * !!! WARNING METHODPORTED TO LIBGPIOD BUT NOT TESTED YET !!!
 *
 */
JVSRotaryStatus initRotary()
{
    if ((gpio18 = gpio_setup_input(18)) == NULL)
    {
        debug(1, "Warning: Failed to set Raspberry Pi GPIO Pin 18\n");
        return JVS_ROTARY_STATUS_ERROR;
    }

    if ((gpio19 = gpio_setup_input(19)) == NULL)
    {
        debug(1, "Warning: Failed to set Raspberry Pi GPIO Pin 19\n");
        return JVS_ROTARY_STATUS_ERROR;
    }

    if ((gpio20 = gpio_setup_input(20)) == NULL)
    {
        debug(1, "Warning: Failed to set Raspberry Pi GPIO Pin 20\n");
        return JVS_ROTARY_STATUS_ERROR;
    }

    if ((gpio21 = gpio_setup_input(21)) == NULL)
    {
        debug(1, "Warning: Failed to set Raspberry Pi GPIO Pin 21\n");
        return JVS_ROTARY_STATUS_ERROR;
    }

    return JVS_ROTARY_STATUS_SUCCESS;
}

/**
 * Get rotary value
 *
 * Returns the value from 0 to 15 for
 * which map to use.
 *
 * @returns The value from 0 to 15 on the rotary encoder
 *
 * !!! WARNING METHODPORTED TO LIBGPIOD BUT NOT TESTED YET !!!
 *
 */
int getRotaryValue()
{
    int value = 0;
    value = value | gpio_read(gpio18) << 0;
    value = value | gpio_read(gpio19) << 1;
    value = value | gpio_read(gpio20) << 2;
    value = value | gpio_read(gpio21) << 3;

    value = ~value & 0x0F;

    return value;
}
