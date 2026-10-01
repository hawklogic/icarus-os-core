/* Focused mock-only state-machine executable; no scheduler or hardware run.
 * Parent compiles/runs on allocated Icarus DATA, never on the laptop. */
#include "bsp/board_feedback.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t tick;
static bool led;
typedef struct { uint32_t kind, value; } trace_event_t;
static trace_event_t trace_events[32];
static uint32_t trace_count;
#define TRACE_LED_OFF 3U
static void trace(uint32_t kind, uint32_t value) {
    assert(trace_count < 32U);
    trace_events[trace_count++] = (trace_event_t){kind, value};
}
uint32_t __os_get_tick_count(void) { return tick; }
void LED_On(void) { led = true; }
void LED_Off(void) { led = false; trace(TRACE_LED_OFF, 0U); }

static uint32_t status(void) {
    uint32_t result = __board_feedback_control(BOARD_FEEDBACK_STATUS, 0U);
    assert((result & BOARD_FEEDBACK_RESERVED_MASK) == 0U);
    return result;
}
static uint32_t rc(uint32_t result) { return result & BOARD_FEEDBACK_RC_MASK; }
static uint32_t percent(uint32_t result) { return (result >> 8U) & 0xFFU; }

static void cancel_as_first_service_after_wake_deadline(uint32_t op) {
    board_feedback_host_trace(NULL);
    trace_count = 0U;
    tick = 0U;
    assert(board_feedback_init());
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_SLEEP, 0U)) == BOARD_FEEDBACK_PENDING);
    tick += 120U;
    assert(rc(status()) == BOARD_FEEDBACK_OK);
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_ON, 50U)) == BOARD_FEEDBACK_PENDING);
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_LED, 20U)) == BOARD_FEEDBACK_OK);
    tick += 121U; /* No intervening status/pulse: cancel is the first service. */
    trace_count = 0U;
    board_feedback_host_trace(trace);
    uint32_t result = __board_feedback_control(op, 0U);
    board_feedback_host_trace(NULL);
    assert(percent(result) == 0U && (result & BOARD_FEEDBACK_ON_FLAG) == 0U);
    assert((result & BOARD_FEEDBACK_LED_ACTIVE) == 0U && !led);
    assert(trace_count != 0U);
    assert(trace_events[0].kind == BOARD_FEEDBACK_TRACE_BACKLIGHT && trace_events[0].value == 0U);
    for (uint32_t i = 0U; i < trace_count; i++) {
        assert(!(trace_events[i].kind == BOARD_FEEDBACK_TRACE_PANEL && trace_events[i].value == 0x29U));
        assert(!(trace_events[i].kind == BOARD_FEEDBACK_TRACE_BACKLIGHT && trace_events[i].value != 0U));
    }
    if (op == BOARD_FEEDBACK_SLEEP) {
        assert(rc(result) == BOARD_FEEDBACK_PENDING && trace_count == 4U);
        assert(trace_events[1].kind == BOARD_FEEDBACK_TRACE_PANEL && trace_events[1].value == 0x28U);
        assert(trace_events[2].kind == BOARD_FEEDBACK_TRACE_PANEL && trace_events[2].value == 0x10U);
        assert(trace_events[3].kind == TRACE_LED_OFF);
    } else {
        assert(rc(result) == BOARD_FEEDBACK_OK);
        assert(trace_events[1].kind == TRACE_LED_OFF);
        assert(trace_count == (op == BOARD_FEEDBACK_OFF ? 3U : 2U));
        if (op == BOARD_FEEDBACK_OFF) {
            assert(trace_events[2].kind == BOARD_FEEDBACK_TRACE_PANEL && trace_events[2].value == 0x28U);
        }
    }
}

int main(void) {
    assert(rc(status()) == BOARD_FEEDBACK_NOT_READY);
    assert(board_feedback_init());
    assert(status() == BOARD_FEEDBACK_READY);
    assert(!led);
    assert(__board_feedback_cell(0U, 'A')); /* scan-off retains updated GRAM */
    assert(!__board_feedback_cell(65U, 'A'));
    assert(!__board_feedback_cell(0U, '\n'));
    const uint32_t before = status();
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_ON, 0U)) == BOARD_FEEDBACK_INVALID);
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_ON, 101U)) == BOARD_FEEDBACK_INVALID);
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_STATUS, 1U)) == BOARD_FEEDBACK_INVALID);
    assert(rc(__board_feedback_control(UINT32_MAX, 0U)) == BOARD_FEEDBACK_INVALID);
    assert(status() == before);
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_BACKLIGHT, 5U)) == BOARD_FEEDBACK_NOT_READY);
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_ON, 10U)) == BOARD_FEEDBACK_OK);
    assert(percent(status()) == 10U && (status() & BOARD_FEEDBACK_ON_FLAG) != 0U);
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_BACKLIGHT, 100U)) == BOARD_FEEDBACK_OK);
    assert(percent(status()) == 100U);
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_OFF, 0U)) == BOARD_FEEDBACK_OK);
    assert(status() == BOARD_FEEDBACK_READY);

    tick = UINT32_MAX - 50U;
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_SLEEP, 0U)) == BOARD_FEEDBACK_PENDING);
    assert(!__board_feedback_cell(0U, 'A'));
    assert((status() & BOARD_FEEDBACK_ASLEEP) != 0U && percent(status()) == 0U);
    tick += 119U;
    assert(rc(status()) == BOARD_FEEDBACK_PENDING);
    tick++;
    assert(rc(status()) == BOARD_FEEDBACK_OK);
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_ON, 25U)) == BOARD_FEEDBACK_PENDING);
    assert((status() & BOARD_FEEDBACK_WAKE_PENDING) != 0U && percent(status()) == 0U);
    tick += 119U;
    assert(rc(status()) == BOARD_FEEDBACK_PENDING);
    tick++;
    assert(rc(status()) == BOARD_FEEDBACK_OK && percent(status()) == 25U);
    assert(__board_feedback_cell(0U, 'A'));

    assert(rc(__board_feedback_control(BOARD_FEEDBACK_SLEEP, 0U)) == BOARD_FEEDBACK_PENDING);
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_ON, 50U)) == BOARD_FEEDBACK_PENDING);
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_OFF, 0U)) == BOARD_FEEDBACK_OK);
    tick += 240U;
    assert((status() & BOARD_FEEDBACK_ON_FLAG) == 0U && percent(status()) == 0U);
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_ON, 50U)) == BOARD_FEEDBACK_PENDING);
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_SLEEP, 0U)) == BOARD_FEEDBACK_PENDING);
    tick += 120U;
    assert(rc(status()) == BOARD_FEEDBACK_PENDING);
    tick += 120U;
    assert(rc(status()) == BOARD_FEEDBACK_OK && (status() & BOARD_FEEDBACK_ASLEEP) != 0U);
    assert(percent(status()) == 0U);

    assert(rc(__board_feedback_control(BOARD_FEEDBACK_LED, 1001U)) == BOARD_FEEDBACK_INVALID);
    tick = UINT32_MAX - 10U;
    assert(rc(__board_feedback_control(BOARD_FEEDBACK_LED, 20U)) == BOARD_FEEDBACK_OK && led);
    tick += 19U;
    assert((status() & BOARD_FEEDBACK_LED_ACTIVE) != 0U && led);
    tick++;
    assert((status() & BOARD_FEEDBACK_LED_ACTIVE) == 0U && !led);
    led = true; /* an unrelated fault LED owner is not overwritten when idle */
    (void)status();
    assert(led);
    cancel_as_first_service_after_wake_deadline(BOARD_FEEDBACK_OFF);
    cancel_as_first_service_after_wake_deadline(BOARD_FEEDBACK_SLEEP);
    cancel_as_first_service_after_wake_deadline(BOARD_FEEDBACK_BACKLIGHT);
    puts("board_debug_control: bounded mock state controls passed");
    return 0;
}
