/**
 * @file    test_cdc.c
 * @brief   Host-side unit tests for bsp/cdc.h (USB CDC transmit ring)
 *
 * @details Under HOST_TEST the transmit ring runs unchanged against the
 *          mocked `CDC_Transmit_FS`; the host hooks (`__cdc_host_*`)
 *          capture every transmitted run, simulate a configured or
 *          unconfigured device, and fire the transfer-complete interrupt
 *          (automatically by default, or one at a time on request).
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

/** @brief Clean ring, configured device, manual completion. */
static void ring_setup_manual(void) {
    mock_usb_reset();
    __cdc_host_reset_state();
    __stdio_host_reset();
    __cdc_host_set_auto_complete(false);
}

/** @brief Complete transfers until the ring is idle; returns how many. */
static uint32_t drain_all(void) {
    uint32_t n = 0u;
    while (__cdc_host_complete()) {
        n++;
    }
    return n;
}

static void ring_teardown(void) {
    mock_usb_reset();
    __cdc_host_reset_state();
    __stdio_host_reset();
}

static void test_cdc_write_basic(void) {
    __cdc_host_reset_state();
    const uint8_t payload[] = { 'h', 'e', 'l', 'l', 'o' };
    TEST_ASSERT_TRUE(CDC_Write(payload, sizeof(payload)));
    TEST_ASSERT_EQUAL_UINT16(sizeof(payload), __cdc_host_sink_len());
    TEST_ASSERT_EQUAL_MEMORY(payload, __cdc_host_sink(), sizeof(payload));
    TEST_ASSERT_EQUAL_UINT32(1, __cdc_host_call_count());
}

static void test_cdc_write_zero_len_is_noop_success(void) {
    __cdc_host_reset_state();
    TEST_ASSERT_TRUE(CDC_Write((const uint8_t *)"x", 0));
    TEST_ASSERT_EQUAL_UINT16(0, __cdc_host_sink_len());
    TEST_ASSERT_EQUAL_UINT32(1, __cdc_host_call_count());
}

static void test_cdc_write_null_data_with_nonzero_len_fails(void) {
    __cdc_host_reset_state();
    TEST_ASSERT_FALSE(CDC_Write(NULL, 4));
}

static void test_cdc_write_failure_propagates(void) {
    __cdc_host_reset_state();
    __cdc_host_set_fail(true);
    const uint8_t payload[] = { 1, 2, 3 };
    TEST_ASSERT_FALSE(CDC_Write(payload, sizeof(payload)));
    /* Forced failure should not have populated the sink. */
    TEST_ASSERT_EQUAL_UINT16(0, __cdc_host_sink_len());
}

static void test_cdc_write_string_basic(void) {
    __cdc_host_reset_state();
    TEST_ASSERT_TRUE(CDC_WriteString("hello, world"));
    TEST_ASSERT_EQUAL_UINT16(12, __cdc_host_sink_len());
    TEST_ASSERT_EQUAL_MEMORY("hello, world", __cdc_host_sink(), 12);
}

static void test_cdc_write_string_null_is_success_noop(void) {
    __cdc_host_reset_state();
    TEST_ASSERT_TRUE(CDC_WriteString(NULL));
    /* No call into the underlying CDC_Write expected. */
    TEST_ASSERT_EQUAL_UINT32(0, __cdc_host_call_count());
}

static void test_cdc_write_string_empty_is_success(void) {
    __cdc_host_reset_state();
    TEST_ASSERT_TRUE(CDC_WriteString(""));
    /* Empty string still calls through with len=0. */
    TEST_ASSERT_EQUAL_UINT32(1, __cdc_host_call_count());
    TEST_ASSERT_EQUAL_UINT16(0, __cdc_host_sink_len());
}

/* ---- Transmit ring ------------------------------------------------------- */

static void test_ring_keeps_order_across_text_and_binary(void) {
    ring_setup_manual();
    const uint8_t frame[] = { 0x1Au, 0xCFu, 0xFCu, 0x1Du };
    TEST_ASSERT_EQUAL_UINT32(2u, stdio_write((const uint8_t *)"AB", 2u));
    TEST_ASSERT_TRUE(CDC_Write(frame, sizeof(frame)));
    TEST_ASSERT_EQUAL_INT('C', __io_putchar('C'));
    TEST_ASSERT_TRUE(CDC_WriteString("de"));

    /* "AB" went out at once; the rest queued behind it. */
    TEST_ASSERT_EQUAL_UINT16(2u, __cdc_host_inflight());
    TEST_ASSERT_EQUAL_UINT32(2u, drain_all());
    TEST_ASSERT_EQUAL_UINT32(2u, __cdc_host_chunk_count());

    const uint8_t expect[] = { 'A', 'B', 0x1Au, 0xCFu, 0xFCu, 0x1Du,
                               'C', 'd', 'e' };
    TEST_ASSERT_EQUAL_UINT16(sizeof(expect), __cdc_host_sink_len());
    TEST_ASSERT_EQUAL_MEMORY(expect, __cdc_host_sink(), sizeof(expect));
    ring_teardown();
}

static void test_ring_whole_is_all_or_nothing_partial_takes_what_fits(void) {
    static uint8_t fill[CDC_TX_RING_SIZE - 3u];
    ring_setup_manual();
    __cdc_host_set_configured(false);          /* nothing leaves the ring */
    (void)memset(fill, 0x55, sizeof(fill));
    TEST_ASSERT_EQUAL_UINT16(sizeof(fill),
                             cdc_tx_write(fill, sizeof(fill), true));

    const uint8_t five[] = { 1u, 2u, 3u, 4u, 5u };
    TEST_ASSERT_FALSE(CDC_Write(five, sizeof(five)));          /* refused */
    TEST_ASSERT_EQUAL_UINT32(CDC_TX_RING_SIZE - 3u, __cdc_tx_queued());
    TEST_ASSERT_EQUAL_UINT16(0u, cdc_tx_write(five, sizeof(five), true));

    TEST_ASSERT_EQUAL_UINT16(3u, cdc_tx_write(five, sizeof(five), false));
    TEST_ASSERT_EQUAL_UINT32(CDC_TX_RING_SIZE, __cdc_tx_queued());
    /* 5 + 5 refused whole, 2 dropped from the partial write. */
    TEST_ASSERT_EQUAL_UINT32(12u, __cdc_tx_dropped());

    /* The partial write kept the first bytes, in order. */
    __cdc_host_set_configured(true);
    __cdc_tx_kick();
    TEST_ASSERT_EQUAL_UINT32(2u, drain_all());
    uint16_t n = __cdc_host_sink_len();
    TEST_ASSERT_EQUAL_UINT16(CDC_TX_RING_SIZE, n);
    TEST_ASSERT_EQUAL_MEMORY(five, &__cdc_host_sink()[n - 3u], 3u);
    ring_teardown();
}

static void test_ring_chunks_are_capped_and_wrap_around(void) {
    static uint8_t a[3000];
    static uint8_t b[2000];
    ring_setup_manual();
    for (uint32_t i = 0u; i < sizeof(a); i++) {
        a[i] = (uint8_t)i;
    }
    for (uint32_t i = 0u; i < sizeof(b); i++) {
        b[i] = (uint8_t)(0xA5u ^ i);
    }

    TEST_ASSERT_TRUE(CDC_Write(a, sizeof(a)));
    TEST_ASSERT_EQUAL_UINT16(CDC_TX_MAX_CHUNK, __cdc_host_last_chunk_len());
    TEST_ASSERT_TRUE(__cdc_host_complete());
    TEST_ASSERT_EQUAL_UINT16(sizeof(a) - CDC_TX_MAX_CHUNK,
                             __cdc_host_last_chunk_len());
    TEST_ASSERT_TRUE(__cdc_host_complete());
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_tx_queued());

    /* Head sits 3000 bytes in: b wraps past the end of the ring. */
    TEST_ASSERT_TRUE(CDC_Write(b, sizeof(b)));
    TEST_ASSERT_EQUAL_UINT16(CDC_TX_RING_SIZE - sizeof(a),
                             __cdc_host_last_chunk_len());   /* to ring end */
    TEST_ASSERT_TRUE(__cdc_host_complete());
    TEST_ASSERT_EQUAL_UINT16(sizeof(b) - (CDC_TX_RING_SIZE - sizeof(a)),
                             __cdc_host_last_chunk_len());   /* from index 0 */
    TEST_ASSERT_TRUE(__cdc_host_complete());
    TEST_ASSERT_FALSE(__cdc_host_complete());

    TEST_ASSERT_EQUAL_UINT16(sizeof(a) + sizeof(b), __cdc_host_sink_len());
    TEST_ASSERT_EQUAL_MEMORY(a, __cdc_host_sink(), sizeof(a));
    TEST_ASSERT_EQUAL_MEMORY(b, &__cdc_host_sink()[sizeof(a)], sizeof(b));
    ring_teardown();
}

static void test_ring_completion_advances_tail(void) {
    ring_setup_manual();
    TEST_ASSERT_TRUE(CDC_WriteString("abc"));
    TEST_ASSERT_TRUE(CDC_WriteString("de"));
    TEST_ASSERT_EQUAL_UINT16(3u, __cdc_host_inflight());
    TEST_ASSERT_EQUAL_UINT32(5u, __cdc_tx_queued());

    TEST_ASSERT_TRUE(__cdc_host_complete());   /* "abc" done, "de" starts */
    TEST_ASSERT_EQUAL_UINT32(2u, __cdc_tx_queued());
    TEST_ASSERT_EQUAL_UINT16(2u, __cdc_host_inflight());

    TEST_ASSERT_TRUE(__cdc_host_complete());
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_tx_queued());
    TEST_ASSERT_EQUAL_UINT16(0u, __cdc_host_inflight());
    TEST_ASSERT_EQUAL_MEMORY("abcde", __cdc_host_sink(), 5u);
    ring_teardown();
}

static void test_ring_not_configured_then_configured_retries(void) {
    ring_setup_manual();
    __cdc_host_set_configured(false);
    TEST_ASSERT_TRUE(CDC_WriteString("hi"));   /* queued, not sent */
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_host_chunk_count());
    TEST_ASSERT_EQUAL_UINT16(0u, __cdc_host_inflight());
    TEST_ASSERT_EQUAL_UINT32(2u, __cdc_tx_queued());

    __cdc_host_set_configured(true);
    TEST_ASSERT_TRUE(CDC_WriteString("!"));    /* next write retries */
    TEST_ASSERT_EQUAL_UINT32(1u, __cdc_host_chunk_count());
    TEST_ASSERT_EQUAL_UINT16(3u, __cdc_host_inflight());
    TEST_ASSERT_EQUAL_MEMORY("hi!", __cdc_host_sink(), 3u);
    ring_teardown();
}

static void test_ring_kick_sends_output_queued_before_host_attached(void) {
    ring_setup_manual();
    __cdc_host_set_configured(false);
    TEST_ASSERT_TRUE(CDC_WriteString("boot"));
    __cdc_host_set_configured(true);
    __cdc_tx_kick();                           /* host opened the port */
    TEST_ASSERT_EQUAL_UINT16(4u, __cdc_host_inflight());
    TEST_ASSERT_EQUAL_MEMORY("boot", __cdc_host_sink(), 4u);
    ring_teardown();
}

static void test_ring_link_reset_clears_wedged_transfer(void) {
    ring_setup_manual();
    TEST_ASSERT_TRUE(CDC_WriteString("abc"));
    TEST_ASSERT_EQUAL_UINT16(3u, __cdc_host_inflight());

    /* The transfer is lost (USB reset): no completion ever arrives, so
     * everything else just queues behind it. */
    TEST_ASSERT_TRUE(CDC_WriteString("d"));
    TEST_ASSERT_EQUAL_UINT32(1u, __cdc_host_chunk_count());

    /* Re-configuration resets the in-flight state; the next write sends
     * the lost bytes again, followed by the rest. */
    __cdc_tx_on_link_reset();
    TEST_ASSERT_EQUAL_UINT16(0u, __cdc_host_inflight());
    TEST_ASSERT_EQUAL_UINT32(4u, __cdc_tx_queued());
    TEST_ASSERT_TRUE(CDC_WriteString("e"));
    TEST_ASSERT_EQUAL_UINT32(2u, __cdc_host_chunk_count());
    TEST_ASSERT_EQUAL_UINT16(5u, __cdc_host_inflight());
    TEST_ASSERT_EQUAL_UINT16(8u, __cdc_host_sink_len());
    TEST_ASSERT_EQUAL_MEMORY("abcabcde", __cdc_host_sink(), 8u);
    ring_teardown();
}

static void test_ring_foreign_busy_endpoint_leaves_data_queued(void) {
    ring_setup_manual();
    mock_usb_set_transmit_result(USBD_BUSY);
    TEST_ASSERT_TRUE(CDC_WriteString("xy"));
    TEST_ASSERT_EQUAL_UINT16(0u, __cdc_host_inflight());
    TEST_ASSERT_EQUAL_UINT32(2u, __cdc_tx_queued());

    /* The foreign transfer's completion interrupt restarts the ring. */
    mock_usb_set_transmit_result(USBD_OK);
    __cdc_tx_on_complete();
    TEST_ASSERT_EQUAL_UINT16(2u, __cdc_host_inflight());
    TEST_ASSERT_EQUAL_MEMORY("xy", __cdc_host_sink(), 2u);
    ring_teardown();
}

static void test_ring_port_close_discards_queued_keeps_in_flight(void) {
    ring_setup_manual();
    TEST_ASSERT_TRUE(CDC_WriteString("abc"));  /* in flight */
    TEST_ASSERT_TRUE(CDC_WriteString("de"));   /* queued behind it */
    TEST_ASSERT_EQUAL_UINT16(3u, __cdc_host_inflight());

    /* Host closes the port: only the queued "de" goes. */
    TEST_ASSERT_EQUAL_UINT32(2u, __cdc_tx_discard_queued());
    TEST_ASSERT_EQUAL_UINT32(3u, __cdc_tx_queued());
    TEST_ASSERT_EQUAL_UINT16(3u, __cdc_host_inflight());
    TEST_ASSERT_EQUAL_UINT32(2u, __cdc_tx_dropped());

    /* The transfer in flight completes normally and nothing follows it. */
    TEST_ASSERT_TRUE(__cdc_host_complete());
    TEST_ASSERT_FALSE(__cdc_host_complete());
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_tx_queued());
    TEST_ASSERT_EQUAL_UINT32(1u, __cdc_host_chunk_count());

    /* New output after the reopen goes out right behind it. */
    TEST_ASSERT_TRUE(CDC_WriteString("xy"));
    TEST_ASSERT_EQUAL_UINT32(1u, drain_all());
    TEST_ASSERT_EQUAL_UINT16(5u, __cdc_host_sink_len());
    TEST_ASSERT_EQUAL_MEMORY("abcxy", __cdc_host_sink(), 5u);
    TEST_ASSERT_EQUAL_UINT32(2u, __cdc_tx_dropped());
    ring_teardown();
}

static void test_ring_port_close_rewinds_wrapped_head(void) {
    static uint8_t a[3000];
    static uint8_t b[2000];
    static uint8_t c[100];
    ring_setup_manual();
    (void)memset(a, 0x11, sizeof(a));
    (void)memset(b, 0x22, sizeof(b));
    (void)memset(c, 0x33, sizeof(c));

    /* Leave the tail 3000 bytes in, then queue b: it wraps, so the transfer
     * in flight runs exactly to the ring end and the head sits past index
     * 0. */
    TEST_ASSERT_TRUE(CDC_Write(a, sizeof(a)));
    TEST_ASSERT_EQUAL_UINT32(2u, drain_all());
    TEST_ASSERT_TRUE(CDC_Write(b, sizeof(b)));
    TEST_ASSERT_TRUE(CDC_Write(c, sizeof(c)));
    uint16_t inflight = __cdc_host_inflight();
    TEST_ASSERT_EQUAL_UINT16(CDC_TX_RING_SIZE - sizeof(a), inflight);

    TEST_ASSERT_EQUAL_UINT32(sizeof(b) + sizeof(c) - inflight,
                             __cdc_tx_discard_queued());
    TEST_ASSERT_EQUAL_UINT32(inflight, __cdc_tx_queued());

    /* The head was rewound to the end of the in-flight run (index 0 after
     * the wrap): the next write follows it with no stale bytes between. */
    TEST_ASSERT_TRUE(__cdc_host_complete());
    TEST_ASSERT_TRUE(CDC_WriteString("Z"));
    TEST_ASSERT_EQUAL_UINT32(1u, drain_all());
    uint16_t n = __cdc_host_sink_len();
    TEST_ASSERT_EQUAL_UINT16(sizeof(a) + inflight + 1u, n);
    TEST_ASSERT_EQUAL_MEMORY(b, &__cdc_host_sink()[sizeof(a)], inflight);
    TEST_ASSERT_EQUAL_UINT8('Z', __cdc_host_sink()[n - 1u]);
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_tx_queued());
    ring_teardown();
}

static void test_ring_port_close_idle_and_empty(void) {
    ring_setup_manual();
    /* Nothing queued: nothing to drop. */
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_tx_discard_queued());
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_tx_dropped());

    /* Nothing in flight (device not configured): every queued byte goes. */
    __cdc_host_set_configured(false);
    TEST_ASSERT_TRUE(CDC_WriteString("stale"));
    TEST_ASSERT_EQUAL_UINT32(5u, __cdc_tx_discard_queued());
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_tx_queued());
    TEST_ASSERT_EQUAL_UINT32(5u, __cdc_tx_dropped());

    __cdc_host_set_configured(true);
    __cdc_tx_kick();
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_host_chunk_count());
    TEST_ASSERT_TRUE(CDC_WriteString("new"));
    TEST_ASSERT_EQUAL_MEMORY("new", __cdc_host_sink(), 3u);
    ring_teardown();
}

static void test_ring_reopen_drops_output_written_while_closed(void) {
    ring_setup_manual();
    (void)__cdc_tx_on_dtr(true);                   /* first open */
    TEST_ASSERT_TRUE(CDC_WriteString("live1"));
    TEST_ASSERT_TRUE(__cdc_host_complete());
    TEST_ASSERT_TRUE(CDC_WriteString("live2"));    /* in flight at close */

    /* The host closes the port and stops reading: the transfer stays in
     * flight and later output queues behind it. */
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_tx_on_dtr(false));
    TEST_ASSERT_TRUE(CDC_WriteString("stale-A"));
    TEST_ASSERT_TRUE(CDC_WriteString("stale-B"));

    /* Reopen: what was written while closed goes; the in-flight "live2"
     * cannot be recalled and still arrives. */
    TEST_ASSERT_EQUAL_UINT32(14u, __cdc_tx_on_dtr(true));
    TEST_ASSERT_EQUAL_UINT32(5u, __cdc_tx_queued());
    TEST_ASSERT_TRUE(CDC_WriteString("new"));
    (void)drain_all();
    TEST_ASSERT_EQUAL_UINT16(13u, __cdc_host_sink_len());
    TEST_ASSERT_EQUAL_MEMORY("live1live2new", __cdc_host_sink(), 13u);
    TEST_ASSERT_EQUAL_UINT32(14u, __cdc_tx_dropped());

    /* A report that does not change DTR drops nothing. */
    TEST_ASSERT_TRUE(CDC_WriteString("x"));
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_tx_on_dtr(true));
    ring_teardown();
}

static void test_ring_first_open_keeps_output_from_before_it(void) {
    ring_setup_manual();
    __cdc_host_set_configured(false);
    TEST_ASSERT_TRUE(CDC_WriteString("boot"));
    __cdc_host_set_configured(true);

    /* The first open since boot keeps the boot output. */
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_tx_on_dtr(true));
    __cdc_tx_kick();
    TEST_ASSERT_EQUAL_UINT16(4u, __cdc_host_inflight());
    TEST_ASSERT_EQUAL_MEMORY("boot", __cdc_host_sink(), 4u);

    /* Link lost while open (reported as a close), then the port opens
     * again: output written in between is dropped. */
    TEST_ASSERT_TRUE(__cdc_host_complete());
    __cdc_tx_on_link_reset();
    (void)__cdc_tx_on_dtr(false);
    __cdc_host_set_configured(false);
    TEST_ASSERT_TRUE(CDC_WriteString("gone"));
    __cdc_host_set_configured(true);
    TEST_ASSERT_EQUAL_UINT32(4u, __cdc_tx_on_dtr(true));
    __cdc_tx_kick();
    TEST_ASSERT_EQUAL_UINT32(1u, __cdc_host_chunk_count());
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_tx_queued());
    ring_teardown();
}

static void test_ring_rejects_null_and_empty(void) {
    ring_setup_manual();
    TEST_ASSERT_EQUAL_UINT16(0u, cdc_tx_write(NULL, 4u, false));
    TEST_ASSERT_EQUAL_UINT16(0u, cdc_tx_write((const uint8_t *)"x", 0u,
                                              true));
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_tx_queued());
    TEST_ASSERT_EQUAL_UINT32(0u, __cdc_tx_dropped());
    ring_teardown();
}

void run_cdc_tests(void) {
    RUN_TEST(test_cdc_write_basic);
    RUN_TEST(test_cdc_write_zero_len_is_noop_success);
    RUN_TEST(test_cdc_write_null_data_with_nonzero_len_fails);
    RUN_TEST(test_cdc_write_failure_propagates);
    RUN_TEST(test_cdc_write_string_basic);
    RUN_TEST(test_cdc_write_string_null_is_success_noop);
    RUN_TEST(test_cdc_write_string_empty_is_success);
    RUN_TEST(test_ring_keeps_order_across_text_and_binary);
    RUN_TEST(test_ring_whole_is_all_or_nothing_partial_takes_what_fits);
    RUN_TEST(test_ring_chunks_are_capped_and_wrap_around);
    RUN_TEST(test_ring_completion_advances_tail);
    RUN_TEST(test_ring_not_configured_then_configured_retries);
    RUN_TEST(test_ring_kick_sends_output_queued_before_host_attached);
    RUN_TEST(test_ring_link_reset_clears_wedged_transfer);
    RUN_TEST(test_ring_foreign_busy_endpoint_leaves_data_queued);
    RUN_TEST(test_ring_port_close_discards_queued_keeps_in_flight);
    RUN_TEST(test_ring_port_close_rewinds_wrapped_head);
    RUN_TEST(test_ring_port_close_idle_and_empty);
    RUN_TEST(test_ring_reopen_drops_output_written_while_closed);
    RUN_TEST(test_ring_first_open_keeps_output_from_before_it);
    RUN_TEST(test_ring_rejects_null_and_empty);
}
