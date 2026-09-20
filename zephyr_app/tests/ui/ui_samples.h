/**
 * @file ui_samples.h
 * @brief Sample snapshots for the host renderer
 */

#ifndef UI_SAMPLES_H
#define UI_SAMPLES_H

#include <stdbool.h>

#include "ui/ui_model.h"

/** A ride in CRS with every field filled, no segment on screen */
void ui_sample_ride(ui_model_t *m);

/** One segment on screen, running (on) or approaching */
void ui_sample_one_segment(ui_model_t *m, bool on);

/** Two segments on screen, each running or approaching */
void ui_sample_two_segments(ui_model_t *m, bool on0, bool on1);

/** GNSS searching: old position and a sparse sky */
void ui_sample_searching(ui_model_t *m);

/** Trainer: GNSS in backup, no sun */
void ui_sample_trainer(ui_model_t *m);

#endif /* UI_SAMPLES_H */
