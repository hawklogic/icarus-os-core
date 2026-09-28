/**
 * @file    test_stdio.c
 * @brief   Host-side tests for non-blocking console output (stdio_write /
 *          __io_putchar) on top of the USB CDC transmit ring.
 *
 * @details Regression guard for two console failures: output that retried
 *          with the scheduler locked while no host was reading (the watchdog
 *          feeder starved), and a shared line buffer that preempted printers
 *          overran and overwrote while the USB stack was still sending it.
 *          Console output now copies into the privileged transmit ring, never
 *          waits, and counts what did not fit.
 *
 * @author  Souham Biswas
 * @date    2026
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            Licensed under the Apache License, Version 2.0
 */

#include "unity.h"
#include "mock_usb.h"
#include "bsp/cdc.h"
#include "bsp/retarget_stdio.h"
#include <string.h>

static void put_line(const char *s) {
    while (*s != '\0') {
        (void)__io_putchar((int)*s);
        s++;
    }
    (void)__io_putchar('\n');
}

static void stdio_setup(void) {
    mock_usb_reset();
    __cdc_host_reset_state();
    __stdio_host_reset();
}

static void stdio_teardown(void) {
    mock_usb_reset();
    __cdc_host_reset_state();
    __stdio_host_reset();
}

static void test_stdio_write_sends_bytes_and_counts_nothing(void) {
    stdio_setup();
    TEST_ASSERT_EQUAL_UINT32(6u, stdio_write((const uint8_t *)"hello\n", 6u));
    TEST_ASSERT_EQUAL_UINT32(1u, mock_usb_transmit_calls());
    TEST_ASSERT_EQUAL_UINT16(6u, __cdc_host_sink_len());
    TEST_ASSERT_EQUAL_MEMORY("hello\n", __cdc_host_sink(), 6u);
    TEST_ASSERT_EQUAL_UINT32(0u, stdio_get_tx_dropped());
    stdio_teardown();
}

static void test_stdio_putchar_returns_char_and_keeps_order(void) {
    stdio_setup();
    put_line("ab");
    TEST_ASSERT_EQUAL_UINT16(3u, __cdc_host_sink_len());
    TEST_ASSERT_EQUAL_MEMORY("ab\n", __cdc_host_sink(), 3u);
    TEST_ASSERT_EQUAL_INT('z', __io_putchar('z'));
    stdio_teardown();
}

static void test_stdio_not_configured_queues_without_retry(void) {
    stdio_setup();
    mock_usb_set_transmit_result(USBD_FAIL);
    put_line("abc");                           /* 3 chars + '\n' */
    /* One start attempt per write, no retry loop, nothing lost. */
    TEST_ASSERT_EQUAL_UINT32(4u, mock_usb_transmit_calls());
    TEST_ASSERT_EQUAL_UINT32(0u, stdio_get_tx_dropped());
    TEST_ASSERT_EQUAL_UINT32(4u, __cdc_tx_queued());
    TEST_ASSERT_EQUAL_UINT16(0u, __cdc_host_sink_len());

    /* Host configures the device and opens the port. */
    mock_usb_set_transmit_result(USBD_OK);
    __cdc_tx_kick();
    TEST_ASSERT_EQUAL_UINT16(4u, __cdc_host_sink_len());
    TEST_ASSERT_EQUAL_MEMORY("abc\n", __cdc_host_sink(), 4u);
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_tx_queued());
    stdio_teardown();
}

static void test_stdio_busy_endpoint_does_not_wait(void) {
    stdio_setup();
    mock_usb_set_transmit_result(USBD_BUSY);
    mock_usb_set_dtr(1u);
    put_line("x");
    /* A listening host used to trigger a bounded spin per line; now each
     * write makes exactly one start attempt and returns. */
    TEST_ASSERT_EQUAL_UINT32(2u, mock_usb_transmit_calls());
    TEST_ASSERT_EQUAL_UINT32(0u, stdio_get_tx_dropped());
    TEST_ASSERT_EQUAL_UINT32(2u, __cdc_tx_queued());
    stdio_teardown();
}

static void test_stdio_full_ring_drops_and_counts(void) {
    static uint8_t big[CDC_TX_RING_SIZE + 10u];
    stdio_setup();
    (void)memset(big, 'B', sizeof(big));
    __cdc_host_set_auto_complete(false);        /* host never reads */

    TEST_ASSERT_EQUAL_UINT32(CDC_TX_RING_SIZE,
                             stdio_write(big, (uint32_t)sizeof(big)));
    TEST_ASSERT_EQUAL_UINT32(10u, stdio_get_tx_dropped());

    TEST_ASSERT_EQUAL_INT('z', __io_putchar('z'));
    TEST_ASSERT_EQUAL_UINT32(11u, stdio_get_tx_dropped());
    stdio_teardown();
}

static void test_stdio_recovers_after_drops(void) {
    static uint8_t fill[CDC_TX_RING_SIZE];
    stdio_setup();
    (void)memset(fill, 'F', sizeof(fill));
    __cdc_host_set_auto_complete(false);
    (void)stdio_write(fill, (uint32_t)sizeof(fill));
    put_line("lost");                           /* 5 bytes, ring full */
    TEST_ASSERT_EQUAL_UINT32(5u, stdio_get_tx_dropped());

    /* The host starts reading again: the ring drains. */
    while (__cdc_host_complete()) {
    }
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_tx_queued());
    put_line("kept");
    TEST_ASSERT_EQUAL_UINT32(5u, stdio_get_tx_dropped());
    while (__cdc_host_complete()) {
    }
    uint16_t n = __cdc_host_sink_len();
    TEST_ASSERT_EQUAL_UINT16(CDC_TX_RING_SIZE + 5u, n);
    TEST_ASSERT_EQUAL_MEMORY("kept\n", &__cdc_host_sink()[n - 5u], 5u);
    stdio_teardown();
}

static void test_stdio_write_rejects_null_and_empty(void) {
    stdio_setup();
    TEST_ASSERT_EQUAL_UINT32(0u, stdio_write(NULL, 4u));
    TEST_ASSERT_EQUAL_UINT32(0u, stdio_write((const uint8_t *)"x", 0u));
    TEST_ASSERT_EQUAL_UINT32(0u, stdio_get_tx_dropped());
    TEST_ASSERT_EQUAL_UINT32(0u, mock_usb_transmit_calls());
    stdio_teardown();
}

void run_stdio_tests(void) {
    RUN_TEST(test_stdio_write_sends_bytes_and_counts_nothing);
    RUN_TEST(test_stdio_putchar_returns_char_and_keeps_order);
    RUN_TEST(test_stdio_not_configured_queues_without_retry);
    RUN_TEST(test_stdio_busy_endpoint_does_not_wait);
    RUN_TEST(test_stdio_full_ring_drops_and_counts);
    RUN_TEST(test_stdio_recovers_after_drops);
    RUN_TEST(test_stdio_write_rejects_null_and_empty);
}
