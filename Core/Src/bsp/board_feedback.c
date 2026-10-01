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
static uint8_t pixels[12U * 16U * 2U];

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
static int32_t send_data(const uint8_t *data, uint32_t length) {
    HAL_GPIO_WritePin(BSP_LCD_CS_PORT, BSP_LCD_CS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BSP_LCD_DC_PORT, BSP_LCD_DC_PIN, GPIO_PIN_SET);
    int32_t result = transmit(data, length);
    HAL_GPIO_WritePin(BSP_LCD_CS_PORT, BSP_LCD_CS_PIN, GPIO_PIN_SET);
    return result;
}
static int32_t write_reg(uint8_t reg, const uint8_t *data, uint32_t length) {
    HAL_GPIO_WritePin(BSP_LCD_CS_PORT, BSP_LCD_CS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BSP_LCD_DC_PORT, BSP_LCD_DC_PIN, GPIO_PIN_RESET);
    int32_t result = transmit(&reg, 1U);
    HAL_GPIO_WritePin(BSP_LCD_DC_PORT, BSP_LCD_DC_PIN, GPIO_PIN_SET);
    if (length != 0U && result == ST7735_OK) { result = transmit(data, length); }
    HAL_GPIO_WritePin(BSP_LCD_CS_PORT, BSP_LCD_CS_PIN, GPIO_PIN_SET);
    return result;
}
/* Register values match the existing BSD-3-Clause ST7735 driver, HannStar
 * 0.96-inch panel. A table avoids linking its unused orientations/graphics.
 * Reset/sleep delays occur once in privileged boot thread, never in SVC. */
static const uint8_t setup[] = {
    0xB1,3,1,0x2C,0x2D, 0xB2,3,1,0x2C,0x2D,
    0xB3,6,1,0x2C,0x2D,1,0x2C,0x2D, 0xB4,1,7,
    0xC0,3,0xA2,2,0x84, 0xC1,1,0xC5, 0xC2,2,0x0A,0,
    0xC3,2,0x8A,0x2A, 0xC4,2,0x8A,0xEE, 0xC5,1,0x0E,
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

/* Small original 5x7 uppercase/digit bitmap. Rows are five low bits. */
static const uint8_t font[36][7] = {
    {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14},{14,17,17,15,1,1,14},
    {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14},{30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14},{7,2,2,2,2,18,12},
    {17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
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
    for (unsigned remaining = 160U * 80U * 2U; remaining != 0U;) {
        unsigned length = remaining > sizeof(pixels) ? sizeof(pixels) : remaining;
        if (send_data(pixels, length) != ST7735_OK) { return false; }
        remaining -= length;
    }
    (void)HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
    __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_2, 100U);
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
                      (font[index][y / 2U] & (1U << (4U - x / 2U))) != 0U;
            uint32_t at = (y * 12U + x) * 2U;
            pixels[at] = on ? 0x07U : 0U;
            pixels[at + 1U] = on ? 0xFFU : 0U;
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
