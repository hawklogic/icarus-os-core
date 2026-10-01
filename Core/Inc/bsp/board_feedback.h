#ifndef BSP_BOARD_FEEDBACK_H
#define BSP_BOARD_FEEDBACK_H
#include <stdbool.h>
#include <stdint.h>

#define BOARD_FEEDBACK_COLUMNS 13U
#define BOARD_FEEDBACK_CELLS 65U
/** Boot-only privileged initialization. Never call from a task or handler. */
bool board_feedback_init(void);
/** Bounded scalar-only gate: one 12x16 cell, uppercase ASCII/digits/space. */
bool board_feedback_cell(uint32_t cell, uint32_t character);
/** Start a 1..1000 tick pulse; zero services expiry without starting a pulse.
 * Caller must service expiry periodically. Fault handlers retain LED ownership. */
bool board_feedback_pulse(uint32_t ticks);
bool __board_feedback_cell(uint32_t cell, uint32_t character);
bool __board_feedback_pulse(uint32_t ticks);
#endif
