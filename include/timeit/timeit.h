#pragma once

void timeit_start(void);
float timeit_stop(void);


/*
 * Usage:
 *
 *  timeit_start();
 *
 *  log_debug("Time in milliseconds: %f",timeit_stop());
 */
