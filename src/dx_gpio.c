/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "dx_gpio.h"

#include <gpiod.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define MAX_CHIP_NUMBER 6

// https://libgpiod-dlang.dpldocs.info/gpiod.html

typedef struct
{
    int count;
    struct gpiod_chip *chip;
} GPIO_CHIP_T;

GPIO_CHIP_T gpio_chips[MAX_CHIP_NUMBER];

static void close_chip(int chip_number)
{
    if (gpio_chips[chip_number].count == 0)
    {
        gpiod_chip_close(gpio_chips[chip_number].chip);
        gpio_chips[chip_number].chip = NULL;
    }
}

bool dx_gpioOpen(DX_GPIO_BINDING *gpio_binding)
{
    if (gpio_binding->__line_handle)
    {
        return true;
    }

    // clang-format off
    if (DX_GPIO_DIRECTION_UNKNOWN == gpio_binding->direction ||
        gpio_binding->chip_number < 0 ||
        gpio_binding->chip_number >= MAX_CHIP_NUMBER ||
        gpio_binding->line_number < 0)
    {
        return false;
    }
    // clang-format on

    if (!gpio_chips[gpio_binding->chip_number].chip)
    {
        char chip_path[32];
        snprintf(chip_path, sizeof(chip_path), "/dev/gpiochip%d", gpio_binding->chip_number);
        gpio_chips[gpio_binding->chip_number].chip = gpiod_chip_open(chip_path);
        if (!gpio_chips[gpio_binding->chip_number].chip)
        {
            return false;
        }
    }

    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    if (!req_cfg)
    {
        close_chip(gpio_binding->chip_number);
        return false;
    }
    
    gpiod_request_config_set_consumer(req_cfg, gpio_binding->name);

    struct gpiod_line_config *line_cfg = gpiod_line_config_new();
    if (!line_cfg)
    {
        gpiod_request_config_free(req_cfg);
        close_chip(gpio_binding->chip_number);
        return false;
    }

    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    if (!settings)
    {
        gpiod_line_config_free(line_cfg);
        gpiod_request_config_free(req_cfg);
        close_chip(gpio_binding->chip_number);
        return false;
    }

    if (DX_GPIO_INPUT == gpio_binding->direction)
    {
        gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    }
    else
    {
        gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
        gpiod_line_settings_set_output_value(settings, (int)gpio_binding->initial_state);
    }

    unsigned int offset = gpio_binding->line_number;
    gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings);
    
    struct gpiod_line_request *request = gpiod_chip_request_lines(gpio_chips[gpio_binding->chip_number].chip, req_cfg, line_cfg);
    
    gpiod_line_settings_free(settings);
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    
    if (!request)
    {
        close_chip(gpio_binding->chip_number);
        return false;
    }

    gpio_binding->__line_handle = request;
    gpio_chips[gpio_binding->chip_number].count++;

    return true;
}

bool dx_gpioClose(DX_GPIO_BINDING *gpio_binding)
{
    if (gpio_binding->__line_handle)
    {
        gpiod_line_request_release((struct gpiod_line_request *)gpio_binding->__line_handle);

        gpio_binding->__line_handle = NULL;

        gpio_chips[gpio_binding->chip_number].count--;
        close_chip(gpio_binding->chip_number);
    }

    return true;
}

bool dx_gpioOn(DX_GPIO_BINDING *gpio_binding)
{
    return dx_gpioStateSet(gpio_binding, true);
}

bool dx_gpioOff(DX_GPIO_BINDING *gpio_binding)
{
    return dx_gpioStateSet(gpio_binding, false);
}

bool dx_gpioStateSet(DX_GPIO_BINDING *gpio_binding, bool state)
{
    if (!gpio_binding->__line_handle)
    {
        return false;
    }

    unsigned int offset = gpio_binding->line_number;
    enum gpiod_line_value value = state ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
    
    return 0 == gpiod_line_request_set_value((struct gpiod_line_request *)gpio_binding->__line_handle, offset, value);
}

int dx_gpioStateGet(DX_GPIO_BINDING *gpio_binding)
{
    if (!gpio_binding->__line_handle)
    {
        return false;
    }

    unsigned int offset = gpio_binding->line_number;
    enum gpiod_line_value value = gpiod_line_request_get_value((struct gpiod_line_request *)gpio_binding->__line_handle, offset);
    
    return (value == GPIOD_LINE_VALUE_ACTIVE) ? 1 : 0;
}

void dx_gpioSetOpen(DX_GPIO_BINDING **gpio_bindings, size_t gpio_bindings_count)
{
    for (int i = 0; i < gpio_bindings_count; i++)
    {
        if (!dx_gpioOpen(gpio_bindings[i]))
        {
            break;
        }
    }
}

void dx_gpioSetClose(DX_GPIO_BINDING **gpio_bindings, size_t gpio_bindings_count)
{
    for (int i = 0; i < gpio_bindings_count; i++)
    {
        dx_gpioClose(gpio_bindings[i]);
    }
}
