#ifndef BSP_BOARD_FEEDBACK_H
#define BSP_BOARD_FEEDBACK_H
#include <stdbool.h>
#include <stdint.h>

#define BOARD_FEEDBACK_COLUMNS 13U
#define BOARD_FEEDBACK_CELLS 65U
/* Single command definition consumed by the C enum and the host debug catalogue.
 * Keep entries literal and one per line so hosts need no C preprocessor. */
#define BOARD_FEEDBACK_COMMANDS(X) \
    X(STATUS, 0, "display-status", 0, 0, "Read display and LED software state") \
    X(OFF, 1, "display-off", 0, 0, "Disable panel scan and backlight") \
    X(ON, 2, "display-on", 1, 100, "Explicitly enable display at brightness percent") \
    X(SLEEP, 3, "display-sleep", 0, 0, "Sleep the panel with backlight off") \
    X(BACKLIGHT, 4, "backlight", 0, 100, "Set awake display backlight percent") \
    X(LED, 5, "led", 1, 1000, "Request a bounded LED pulse in ticks")
#define BOARD_FEEDBACK_ENUM(symbol, opcode, name, minimum, maximum, description) \
    BOARD_FEEDBACK_##symbol = opcode,
enum { BOARD_FEEDBACK_COMMANDS(BOARD_FEEDBACK_ENUM) };
#undef BOARD_FEEDBACK_ENUM

enum {
    BOARD_FEEDBACK_OK = 0U,
    BOARD_FEEDBACK_INVALID = 1U,
    BOARD_FEEDBACK_NOT_READY = 2U,
    BOARD_FEEDBACK_PENDING = 3U,
    BOARD_FEEDBACK_IO = 4U,
    BOARD_FEEDBACK_UNAVAILABLE = 5U
};
#define BOARD_FEEDBACK_RC_MASK 0xFFU
#define BOARD_FEEDBACK_READY (1UL << 16U)
#define BOARD_FEEDBACK_ON_FLAG (1UL << 17U)
#define BOARD_FEEDBACK_ASLEEP (1UL << 18U)
#define BOARD_FEEDBACK_WAKE_PENDING (1UL << 19U)
#define BOARD_FEEDBACK_IO_FAILED (1UL << 20U)
#define BOARD_FEEDBACK_LED_ACTIVE (1UL << 21U)
#define BOARD_FEEDBACK_RESERVED_MASK 0xFFC00000UL
#define BOARD_FEEDBACK_CAPABILITIES 0x3FU

/** Bounded scalar-only runtime control; low8 result, bits8..15 percent, above flags.
 * PENDING is not completed visual state. Poll status after controller delay;
 * pulse(0) also services pending work. No delay or pointers cross the SVC. */
uint32_t board_feedback_control(uint32_t op, uint32_t value);
uint32_t __board_feedback_control(uint32_t op, uint32_t value);
#ifdef HOST_TEST
/* Ordered intent trace for narrow no-transient-enable controls; no target code. */
#define BOARD_FEEDBACK_TRACE_PANEL 1U
#define BOARD_FEEDBACK_TRACE_BACKLIGHT 2U
typedef void (*board_feedback_trace_fn)(uint32_t kind, uint32_t value);
void board_feedback_host_trace(board_feedback_trace_fn trace);
#endif
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
