/**
 * @file model_lock.h
 * @brief Lock that serializes the model between main_loop and the display
 *
 * main_loop owns the model: it is the only thread that writes it, and it
 * holds this lock while it runs one step (buttons, GPS, sensors, boucle).
 * The display thread holds it while it composes a frame from the model and
 * releases it before the SPI transfer. Interrupts and Bluetooth callbacks
 * never take it: they hand their data to main_loop instead.
 */

#ifndef MODEL_LOCK_H
#define MODEL_LOCK_H

/**
 * @brief Take the model lock, waiting as long as needed
 *
 * Recursive for the owning thread. Never call it from an ISR.
 */
void model_lock(void);

/**
 * @brief Release the model lock taken by model_lock()
 */
void model_unlock(void);

#endif /* MODEL_LOCK_H */
