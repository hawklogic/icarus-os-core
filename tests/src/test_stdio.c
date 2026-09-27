/**
 * @file    test_stdio.c
 * @brief   Host-side tests for non-blocking console output (__io_putchar).
 *
 * @details Regression guard for the console stall: when no host was
 *          reading the USB port, every flushed line retried up to 1000
 *          times with the scheduler locked, which starved the watchdog
 *          feeder and reset the board every few seconds.  Output must now
 *          drop quickly and count what it dropped.
 *
 * @author  Souham Biswas
 * @date    2026
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            Licensed under the Apache License, Version 2.0
 */

#include "unity.h"
#include "mock_usb.h"
#include "bsp/retarget_stdio.h"

static void put_line(const char *s) {
    while (*s != '\0') {
        (void)__io_putchar((int)*s);
        s++;
    }
    (void)__io_putchar('\n');
}

static void stdio_setup(void) {
    mock_usb_reset();
    __stdio_host_reset();
}

static void test_stdio_ok_sends_once_and_counts_nothing(void) {
    stdio_setup();
    put_line("hello");
    TEST_ASSERT_EQUAL_UINT32(1u, mock_usb_transmit_calls());
    TEST_ASSERT_EQUAL_UINT32(0u, stdio_get_tx_dropped());
}

static void test_stdio_not_configured_drops_without_retry(void) {
    stdio_setup();
    mock_usb_set_transmit_result(USBD_FAIL);
    put_line("abc");                           /* 3 chars + '\n' */
    TEST_ASSERT_EQUAL_UINT32(1u, mock_usb_transmit_calls());
    TEST_ASSERT_EQUAL_UINT32(4u, stdio_get_tx_dropped());
}

static void test_stdio_busy_with_listener_retries_bounded(void) {
    stdio_setup();
    mock_usb_set_transmit_result(USBD_BUSY);
    mock_usb_set_dtr(1u);
    put_line("x");
    TEST_ASSERT_EQUAL_UINT32(1u + STDIO_TX_RETRY_MAX, mock_usb_transmit_calls());
    TEST_ASSERT_EQUAL_UINT32(2u, stdio_get_tx_dropped());
}

static void test_stdio_busy_without_listener_does_not_wait(void) {
    stdio_setup();
    mock_usb_set_transmit_result(USBD_BUSY);
    mock_usb_set_dtr(0u);
    put_line("x");
    TEST_ASSERT_EQUAL_UINT32(1u, mock_usb_transmit_calls());
    TEST_ASSERT_EQUAL_UINT32(2u, stdio_get_tx_dropped());
}

static void test_stdio_full_buffer_flushes(void) {
    stdio_setup();
    for (uint32_t k = 0u; k < STDIO_LINE_BUF_SIZE; k++) {
        (void)__io_putchar('B');
    }
    TEST_ASSERT_EQUAL_UINT32(1u, mock_usb_transmit_calls());
}

static void test_stdio_recovers_after_drops(void) {
    stdio_setup();
    mock_usb_set_transmit_result(USBD_FAIL);
    put_line("lost");
    mock_usb_set_transmit_result(USBD_OK);
    put_line("kept");
    TEST_ASSERT_EQUAL_UINT32(2u, mock_usb_transmit_calls());
    TEST_ASSERT_EQUAL_UINT32(5u, stdio_get_tx_dropped());
}

void run_stdio_tests(void) {
    RUN_TEST(test_stdio_ok_sends_once_and_counts_nothing);
    RUN_TEST(test_stdio_not_configured_drops_without_retry);
    RUN_TEST(test_stdio_busy_with_listener_retries_bounded);
    RUN_TEST(test_stdio_busy_without_listener_does_not_wait);
    RUN_TEST(test_stdio_full_buffer_flushes);
    RUN_TEST(test_stdio_recovers_after_drops);
    mock_usb_reset();
    __stdio_host_reset();
}
