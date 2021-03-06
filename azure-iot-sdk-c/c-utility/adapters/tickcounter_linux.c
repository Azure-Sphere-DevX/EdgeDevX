// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include "azure_c_shared_utility/gballoc.h"
#include <stdint.h>
#include "azure_c_shared_utility/tickcounter.h"
#include "azure_c_shared_utility/optimize_size.h"
#include "azure_c_shared_utility/xlogging.h"
#include "linux_time.h"


typedef struct TICK_COUNTER_INSTANCE_TAG
{
    int64_t init_time_value;
    tickcounter_ms_t current_ms;
} TICK_COUNTER_INSTANCE;

TICK_COUNTER_HANDLE tickcounter_create(void)
{
    TICK_COUNTER_INSTANCE* result = (TICK_COUNTER_INSTANCE*)malloc(sizeof(TICK_COUNTER_INSTANCE));
    if (result != NULL)
    {
        set_time_basis();

        result->init_time_value = get_time_ms();
        if (result->init_time_value == INVALID_TIME_VALUE)
        {
            LogError("tickcounter failed: time return INVALID_TIME.");
            free(result);
            result = NULL;
        }
        else
        {
            result->current_ms = 0;
        }
    }
    return result;
}

void tickcounter_destroy(TICK_COUNTER_HANDLE tick_counter)
{
    if (tick_counter != NULL)
    {
        free(tick_counter);
    }
}

int tickcounter_get_current_ms(TICK_COUNTER_HANDLE tick_counter, tickcounter_ms_t * current_ms)
{
    int result;

    if (tick_counter == NULL || current_ms == NULL)
    {
        LogError("tickcounter failed: Invalid Arguments.");
        result = MU_FAILURE;
    }
    else
    {
        int64_t time_value = get_time_ms();
        if (time_value == INVALID_TIME_VALUE)
        {
            result = MU_FAILURE;
        }
        else
        {
            TICK_COUNTER_INSTANCE* tick_counter_instance = (TICK_COUNTER_INSTANCE*)tick_counter;
            tick_counter_instance->current_ms = (tickcounter_ms_t) time_value - tick_counter_instance->init_time_value;
            *current_ms = tick_counter_instance->current_ms;
            result = 0;
        }
    }

    return result;
}
