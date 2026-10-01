/** Bounded board text cells and event LED. No application semantics. */
#include "bsp/board_feedback.h"
#include "icarus/scheduler.h"
#include "bsp/led.h"
#include <stddef.h>
static bool io_failed;
#ifndef HOST_TEST
#include "bsp/spi.h"
#include "bsp/timer.h"
#include "bsp/config.h"
#include "st7735.h"

/* One glyph, never a screen framebuffer or a task-stack allocation. */
static uint16_t pixels[12U * 16U];

/* Each transfer has a finite polling budget independent of SysTick (which
 * cannot preempt SVC). SPI4 is owned exclusively by this optional interface.
 * On failure the display is disabled; inference and USB remain usable. */
static int32_t transmit(const uint8_t *data, uint32_t length) {
    if (io_failed) { return ST7735_ERROR; }
    uint32_t budget = 100000U;
    SPI_1LINE_TX(&hspi4);
    MODIFY_REG(SPI4->CR2, SPI_CR2_TSIZE, length);
    __HAL_SPI_ENABLE(&hspi4);
    SET_BIT(SPI4->CR1, SPI_CR1_CSTART);
    while (length != 0U && budget != 0U) {
        budget--;
        if ((SPI4->SR & SPI_SR_TXP) != 0U) {
            *(__IO uint8_t *)&SPI4->TXDR = *data++;
            length--;
        }
    }
    while ((SPI4->SR & SPI_SR_EOT) == 0U && budget != 0U) { budget--; }
    __HAL_SPI_DISABLE(&hspi4);
    SPI4->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC;
    io_failed = budget == 0U;
    return io_failed ? ST7735_ERROR : ST7735_OK;
}
static int32_t write_reg(uint32_t reg, const uint8_t *data, uint32_t length) {
    BSP_LCD_CS_PORT->BSRR = (uint32_t)BSP_LCD_CS_PIN << 16U;
    int32_t result = ST7735_OK;
    if (reg < 256U) {
        uint8_t command = (uint8_t)reg;
        BSP_LCD_DC_PORT->BSRR = (uint32_t)BSP_LCD_DC_PIN << 16U;
        result = transmit(&command, 1U);
    }
    BSP_LCD_DC_PORT->BSRR = BSP_LCD_DC_PIN;
    if (length != 0U && result == ST7735_OK) { result = transmit(data, length); }
    BSP_LCD_CS_PORT->BSRR = BSP_LCD_CS_PIN;
    return result;
}
#define send_data(data, length) write_reg(256U, (const uint8_t *)(data), (length))
/* Register values match the existing BSD-3-Clause ST7735 driver, HannStar
 * 0.96-inch panel. A table avoids linking its unused orientations/graphics.
 * Reset/sleep delays occur once in privileged boot thread, never in SVC. */
static const uint8_t setup[] = {
    0xB1,3,1,0x2C,0x2D, 0xB4,1,7,
    0xC0,3,0xA2,2,0x84, 0xC1,1,0xC5, 0xC2,2,0x0A,0,
    0xC5,1,0x0E,
    0x21,0, 0x3A,1,5,
    0xE0,16,2,0x1C,7,0x12,0x37,0x32,0x29,0x2D,0x29,0x25,0x2B,0x39,0,1,3,0x10,
    0xE1,16,3,0x1D,7,6,0x2E,0x2C,0x29,0x2D,0x2E,0x2E,0x37,0x3F,0,0,2,0x10,
    0x36,1,0xA8, 0x13,0, 0x28,0
};
static bool rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    /* Landscape rotated 180, HannStar controller RAM offsets. */
    const uint8_t column[4] = {0U, (uint8_t)(x + 1U), 0U, (uint8_t)(x + width)};
    const uint8_t row[4] = {0U, (uint8_t)(y + 26U), 0U, (uint8_t)(y + height + 25U)};
    return write_reg(ST7735_CASET, column, 4U) == ST7735_OK &&
           write_reg(ST7735_RASET, row, 4U) == ST7735_OK &&
           write_reg(ST7735_WRITE_RAM, NULL, 0U) == ST7735_OK;
}

/* Small original 5x7 uppercase/digit bitmap. Columns are seven low bits. */
static const uint8_t font[36][5] = {
    {62,81,73,69,62},
    {0,66,127,64,0},
    {66,97,81,73,70},
    {65,73,73,73,54},
    {24,20,18,127,16},
    {79,73,73,73,49},
    {62,73,73,73,48},
    {1,113,9,5,3},
    {54,73,73,73,54},
    {6,73,73,73,62},
    {126,9,9,9,126},
    {127,73,73,73,54},
    {62,65,65,65,34},
    {127,65,65,65,62},
    {127,73,73,73,65},
    {127,9,9,9,1},
    {62,65,73,73,122},
    {127,8,8,8,127},
    {0,65,127,65,0},
    {32,64,65,63,1},
    {127,8,20,34,65},
    {127,64,64,64,64},
    {127,2,12,2,127},
    {127,2,4,8,127},
    {62,65,65,65,62},
    {127,9,9,9,6},
    {62,65,81,33,94},
    {127,9,25,41,70},
    {70,73,73,73,49},
    {1,1,127,1,1},
    {63,64,64,64,63},
    {31,32,64,32,31},
    {63,64,56,64,63},
    {99,20,8,20,99},
    {3,4,120,4,3},
    {97,81,73,69,67}
};
#endif

static bool ready;
static uint32_t pulse_start, pulse_ticks;
static bool display_on, want_on, want_sleep;
static uint32_t brightness, requested_brightness, phase_start;
enum { PANEL_AWAKE, PANEL_SLEEP_WAIT, PANEL_ASLEEP, PANEL_WAKE_WAIT };
static uint32_t phase;
#ifdef HOST_TEST
static board_feedback_trace_fn trace_io;
void board_feedback_host_trace(board_feedback_trace_fn trace) { trace_io = trace; }
#endif

static void backlight(uint32_t percent) {
    brightness = percent;
#ifdef HOST_TEST
    if (trace_io != NULL) { trace_io(BOARD_FEEDBACK_TRACE_BACKLIGHT, percent); }
#endif
#ifndef HOST_TEST
    TIM1->CCR2 = percent * 10U;
    if (percent == 0U) {
        TIM1->CCER &= ~TIM_CCER_CC2NE;
        TIM1->BDTR &= ~TIM_BDTR_MOE;
        TIM1->CR1 &= ~TIM_CR1_CEN;
    } else {
        TIM1->CCER |= TIM_CCER_CC2NE;
        TIM1->BDTR |= TIM_BDTR_MOE;
        TIM1->CR1 |= TIM_CR1_CEN;
    }
#endif
}

static bool panel_command(uint32_t command) {
#ifndef HOST_TEST
    if (write_reg(command, NULL, 0U) != ST7735_OK) {
        ready = false;
        backlight(0U);
        return false;
    }
#else
    if (trace_io != NULL) { trace_io(BOARD_FEEDBACK_TRACE_PANEL, command); }
#endif
    return true;
}

static bool sleep_panel(uint32_t now) {
    if (!panel_command(0x28U)) { return false; }
    display_on = false;
    if (!panel_command(0x10U)) { return false; }
    phase = PANEL_SLEEP_WAIT;
    phase_start = now;
    want_sleep = false;
    return true;
}

static void service_display(uint32_t now) {
    if (!ready) { return; }
    if (phase == PANEL_SLEEP_WAIT && now - phase_start >= 120U) {
        phase = PANEL_ASLEEP;
    }
    if (phase == PANEL_WAKE_WAIT && now - phase_start >= 120U) {
        phase = PANEL_AWAKE;
        if (want_sleep) { (void)sleep_panel(now); }
        else if (want_on && panel_command(0x29U)) {
            display_on = true;
            backlight(requested_brightness);
        }
    }
    if (phase == PANEL_ASLEEP && want_on && panel_command(0x11U)) {
        phase = PANEL_WAKE_WAIT;
        phase_start = now;
    }
}

static uint32_t state_word(uint32_t result) {
    return result | (brightness << 8U) |
           (ready ? BOARD_FEEDBACK_READY : 0U) |
           (display_on ? BOARD_FEEDBACK_ON_FLAG : 0U) |
           ((phase == PANEL_ASLEEP || phase == PANEL_SLEEP_WAIT) ? BOARD_FEEDBACK_ASLEEP : 0U) |
           ((phase == PANEL_WAKE_WAIT || (phase == PANEL_SLEEP_WAIT && want_on)) ? BOARD_FEEDBACK_WAKE_PENDING : 0U) |
           (io_failed ? BOARD_FEEDBACK_IO_FAILED : 0U) |
           (pulse_ticks != 0U ? BOARD_FEEDBACK_LED_ACTIVE : 0U);
}

bool board_feedback_init(void) {
    ready = false;
    pulse_ticks = 0U;
    io_failed = false;
    phase = PANEL_AWAKE;
    display_on = want_on = want_sleep = false;
    requested_brightness = 0U;
    backlight(0U);
    LED_Off();
#ifndef HOST_TEST
    if (write_reg(ST7735_SW_RESET, NULL, 0U) != ST7735_OK) { return false; }
    HAL_Delay(120U);
    if (write_reg(ST7735_SLEEP_OUT, NULL, 0U) != ST7735_OK) { return false; }
    HAL_Delay(120U);
    for (unsigned at = 0U; at < sizeof(setup);) {
        uint8_t reg = setup[at++], length = setup[at++];
        if (write_reg(reg, &setup[at], length) != ST7735_OK) { return false; }
        at += length;
    }
    /* Clear the full 160x80 panel, including the four-pixel right margin. */
    if (!rectangle(0U, 0U, 160U, 80U)) { return false; }
    for (unsigned row = 0U; row < 80U; row++) {
        if (send_data(pixels, 320U) != ST7735_OK) { return false; }
    }
    /* Panel scan and PWM stay off until explicit runtime ON. */
#endif
    ready = true;
    return true;
}

bool __board_feedback_cell(uint32_t cell, uint32_t character) {
    bool digit = character >= '0' && character <= '9';
    bool letter = character >= 'A' && character <= 'Z';
    if (!ready || phase != PANEL_AWAKE || cell >= BOARD_FEEDBACK_CELLS ||
        (!digit && !letter && character != ' ')) { return false; }
#ifndef HOST_TEST
    uint32_t index = digit ? character - '0' : character - 'A' + 10U;
    for (uint32_t y = 0U; y < 16U; y++) {
        for (uint32_t x = 0U; x < 12U; x++) {
            bool on = character != ' ' && x < 10U && y < 14U &&
                      (font[index][x / 2U] & (1U << (y / 2U))) != 0U;
            pixels[y * 12U + x] = on ? 0xFFFFU : 0U;
        }
    }
    if (!rectangle((cell % BOARD_FEEDBACK_COLUMNS) * 12U,
                   (cell / BOARD_FEEDBACK_COLUMNS) * 16U, 12U, 16U) ||
        send_data(pixels, sizeof(pixels)) != ST7735_OK) {
        ready = false;
        backlight(0U);
        return false;
    }
#endif
    return true;
}

bool __board_feedback_pulse(uint32_t ticks) {
    if (ticks > 1000U) { return false; }
    uint32_t now = __os_get_tick_count();
    service_display(now);
    if (ticks != 0U) {
        pulse_start = now;
        pulse_ticks = ticks;
        LED_On();
    } else if (pulse_ticks != 0U && now - pulse_start >= pulse_ticks) {
        pulse_ticks = 0U;
        LED_Off();
    }
    return true;
}

uint32_t __board_feedback_control(uint32_t op, uint32_t value) {
    bool valid = false;
#define BOARD_FEEDBACK_VALIDATE(symbol, opcode, name, minimum, maximum, description) \
    case opcode: valid = value - (uint32_t)(minimum) <= (uint32_t)((maximum) - (minimum)); break;
    switch (op) { BOARD_FEEDBACK_COMMANDS(BOARD_FEEDBACK_VALIDATE) default: break; }
#undef BOARD_FEEDBACK_VALIDATE
    if (!valid) { return state_word(BOARD_FEEDBACK_INVALID); }
    /* Capture darkness/cancellation intent before the first deadline service.
     * Otherwise an expired pending ON could enable PWM and only then be undone
     * by OFF, SLEEP or BACKLIGHT(0), despite an apparently correct final state. */
    if (op == BOARD_FEEDBACK_OFF || op == BOARD_FEEDBACK_SLEEP ||
        (op == BOARD_FEEDBACK_BACKLIGHT && value == 0U)) {
        want_on = false;
        requested_brightness = 0U;
        if (op == BOARD_FEEDBACK_OFF) { want_sleep = false; }
        if (op == BOARD_FEEDBACK_SLEEP && phase == PANEL_WAKE_WAIT) { want_sleep = true; }
        backlight(0U);
    }
    (void)__board_feedback_pulse(0U);
    if (!ready) { return state_word(io_failed ? BOARD_FEEDBACK_IO : BOARD_FEEDBACK_NOT_READY); }
    uint32_t result = BOARD_FEEDBACK_OK;
    switch (op) {
        case BOARD_FEEDBACK_STATUS:
            if (phase == PANEL_SLEEP_WAIT || phase == PANEL_WAKE_WAIT) { result = BOARD_FEEDBACK_PENDING; }
            break;
        case BOARD_FEEDBACK_OFF:
            want_on = want_sleep = false;
            if (phase == PANEL_AWAKE && !panel_command(0x28U)) { result = BOARD_FEEDBACK_IO; }
            else { display_on = false; }
            break;
        case BOARD_FEEDBACK_ON:
            requested_brightness = value;
            want_on = true;
            want_sleep = false;
            if (phase == PANEL_AWAKE) {
                if (panel_command(0x29U)) { display_on = true; backlight(value); }
                else { result = BOARD_FEEDBACK_IO; }
            } else {
                service_display(__os_get_tick_count());
                result = ready ? BOARD_FEEDBACK_PENDING : BOARD_FEEDBACK_IO;
            }
            break;
        case BOARD_FEEDBACK_SLEEP:
            want_on = false;
            if (phase == PANEL_WAKE_WAIT) { want_sleep = true; result = BOARD_FEEDBACK_PENDING; break; }
            if (phase == PANEL_AWAKE) {
                if (!sleep_panel(__os_get_tick_count())) { result = BOARD_FEEDBACK_IO; break; }
            }
            if (phase == PANEL_SLEEP_WAIT) { result = BOARD_FEEDBACK_PENDING; }
            break;
        case BOARD_FEEDBACK_BACKLIGHT:
            if (value != 0U && (phase != PANEL_AWAKE || !display_on)) { result = BOARD_FEEDBACK_NOT_READY; }
            else if (value != 0U) { requested_brightness = value; backlight(value); }
            break;
        case BOARD_FEEDBACK_LED:
            (void)__board_feedback_pulse(value);
            break;
        default: result = BOARD_FEEDBACK_INVALID; break;
    }
    return state_word(result);
}
