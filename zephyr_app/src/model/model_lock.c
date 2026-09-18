/**
 * @file model_lock.c
 * @brief Lock that serializes the model between main_loop and the display
 */

#include <zephyr/kernel.h>

#include "model/model_lock.h"

/*
 * k_mutex has priority inheritance: while the display (priority 7) holds the
 * lock, main_loop (priority 5) waits at most one frame composition.
 */
static K_MUTEX_DEFINE(model_mutex);

void model_lock(void)
{
    (void)k_mutex_lock(&model_mutex, K_FOREVER);
}

void model_unlock(void)
{
    (void)k_mutex_unlock(&model_mutex);
}
