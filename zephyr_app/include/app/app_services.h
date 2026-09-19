/**
 * @file app_services.h
 * @brief Start of each service thread, in the order main() calls them
 *
 * The threads are defined statically and start stopped; main() starts them
 * once the settings and the watchdog are ready.
 */

#ifndef APP_SERVICES_H
#define APP_SERVICES_H

/** Model modules (attitude, segments, route, zones), before any thread starts */
void model_svc_init(void);

void storage_svc_start(void);
void power_svc_start(void);
void sensors_svc_start(void);
void gnss_svc_start(void);
void radio_svc_start(void);
void model_svc_start(void);
void ui_svc_start(void);

/** Tell the system machine that every service started (Partida -> Ligado) */
void power_svc_ready(void);

#endif /* APP_SERVICES_H */
