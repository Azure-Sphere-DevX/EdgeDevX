/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

/*
   https://libgpiod-dlang.dpldocs.info/gpiod.html
   https://www.ics.com/blog/gpio-programming-exploring-libgpiod-library
   https://blog.lxsang.me/post/id/33
   https://installati.one/ubuntu/20.04/libgpiod-dev/

   sudo apt -y install libgpiod-dev gpiod

   Raspberry Pi Notes:
   - All GPIOs on GPIO Chip 0

   Beaglebone GPIO notes.
   - https://beagleboard.org/support/bone101
   - 4 x gpio chips with 32 lines.
   - To calculate the GPIO chip number and line number - take the GPIO Number eg 49.
   - Chip number = (49 / 32) = chip number = 1 , line number = 49 % 32 = 17
*/

#pragma once

#include <stddef.h>
#include <stdbool.h>

typedef enum
{
    DX_GPIO_DIRECTION_UNKNOWN,
    DX_GPIO_INPUT,
    DX_GPIO_OUTPUT
} DX_GPIO_DIRECTION;

typedef enum
{
    DX_GPIO_LOW  = 0,
    DX_GPIO_HIGH = 1
} DX_GPIO_STATE;

typedef enum
{
    DX_GPIO_DETECT_LOW,
    DX_GPIO_DETECT_HIGH,
    DX_GPIO_DETECT_BOTH
} DX_GPIO_INPUT_DETECT;

typedef struct
{
    void *__line_handle; // points to object of type struct gpiod_line_request
    int chip_number;
    int line_number;
    char *name;
    DX_GPIO_DIRECTION direction;
    DX_GPIO_INPUT_DETECT detect;
    DX_GPIO_STATE initial_state;
} DX_GPIO_BINDING;

bool dx_gpioClose(DX_GPIO_BINDING *gpio_binding);
bool dx_gpioOff(DX_GPIO_BINDING *gpio_binding);
bool dx_gpioOn(DX_GPIO_BINDING *gpio_binding);
bool dx_gpioOpen(DX_GPIO_BINDING *gpio_binding);
int dx_gpioStateGet(DX_GPIO_BINDING *gpio_binding);
bool dx_gpioStateSet(DX_GPIO_BINDING *gpio_binding, bool state);
void dx_gpioSetClose(DX_GPIO_BINDING **gpio_bindings, size_t gpio_bindings_count);
void dx_gpioSetOpen(DX_GPIO_BINDING **gpio_bindings, size_t gpio_bindings_count);
