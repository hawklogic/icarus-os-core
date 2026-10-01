/** Bounded board text cells and event LED. No application semantics. */
#include "bsp/board_feedback.h"
#include "icarus/scheduler.h"
#include "bsp/led.h"
#ifndef HOST_TEST
#include "bsp/spi.h"
#include "bsp/timer.h"
#include "bsp/config.h"
#include "st7735.h"

static bool io_failed;
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
    0x36,1,0xA8, 0x13,0, 0x29,0
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

bool board_feedback_init(void) {
    ready = false;
    pulse_ticks = 0U;
    LED_Off();
#ifndef HOST_TEST
    io_failed = false;
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
    /* MX_TIM1_Init already configured channel 2 in PWM mode, non-slave.
     * This interface exclusively owns its complementary backlight output. */
    TIM1->CCR2 = 100U;
    TIM1->CCER |= TIM_CCER_CC2NE;
    TIM1->BDTR |= TIM_BDTR_MOE;
    TIM1->CR1 |= TIM_CR1_CEN;
#endif
    ready = true;
    return true;
}

bool __board_feedback_cell(uint32_t cell, uint32_t character) {
    bool digit = character >= '0' && character <= '9';
    bool letter = character >= 'A' && character <= 'Z';
    if (!ready || cell >= BOARD_FEEDBACK_CELLS || (!digit && !letter && character != ' ')) { return false; }
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
        return false;
    }
#endif
    return true;
}

bool __board_feedback_pulse(uint32_t ticks) {
    if (ticks > 1000U) { return false; }
    uint32_t now = __os_get_tick_count();
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
