/* Focused mock-only state-machine executable; no scheduler or hardware run.
 * Parent compiles/runs on allocated Icarus DATA, never on the laptop. */
#include "bsp/board_feedback.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t tick;
static bool led;
uint32_t __os_get_tick_count(void) { return tick; }
void LED_On(void) { led = true; }
void LED_Off(void) { led = false; }

static uint32_t status(void) {
    uint32_t result = __board_feedback_control(BOARD_FEEDBACK_STATUS, 0U);
    assert((result & BOARD_FEEDBACK_RESERVED_MASK) == 0U);
    return result;
}
static uint32_t rc(uint32_t result) { return result & BOARD_FEEDBACK_RC_MASK; }
static uint32_t percent(uint32_t result) { return (result >> 8U) & 0xFFU; }

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
    puts("board_debug_control: bounded mock state controls passed");
    return 0;
}
