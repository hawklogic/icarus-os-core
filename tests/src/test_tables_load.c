/**
 * @file    test_tables_load.c
 * @brief   Host-side tests for offset-addressed table loads, abort,
 *          commit validation, info copy-out, the CRC entry point and the
 *          CDC RX drop counter.
 *
 * @details Regression guards for chunked uploads over a lossy link:
 *          a retransmitted chunk used to be appended a second time (the
 *          table then activated with shifted contents), and an overlong
 *          chunk wedged staging until reboot.
 *
 * @author  Souham Biswas
 * @date    2026
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            Licensed under the Apache License, Version 2.0
 */

#include "unity.h"
#include "icarus/icarus.h"
#include <string.h>

#define T_ID    0x31u
#define T_SIZE  16u
#define T_SCRC  0xABCDu

static uint8_t last_activated[T_SIZE];
static uint16_t last_len;

static bool record_activate(const void *data, uint16_t len) {
    (void)memcpy(last_activated, data, len);
    last_len = len;
    return true;
}

static void setup_table(void) {
    tbl_init();
    tbl_descriptor_t d;
    (void)memset(&d, 0, sizeof(d));
    d.id = T_ID;
    (void)strcpy(d.name, "test");
    d.size = T_SIZE;
    d.schema_crc = T_SCRC;
    d.activate = record_activate;
    TEST_ASSERT_TRUE(tbl_register(&d));
    last_len = 0u;
    (void)memset(last_activated, 0, sizeof(last_activated));
}

static void fill(uint8_t *b, uint8_t start, uint16_t n) {
    for (uint16_t i = 0u; i < n; i++) {
        b[i] = (uint8_t)(start + i);
    }
}

static void test_retransmitted_chunk_is_idempotent(void) {
    setup_table();
    uint8_t all[T_SIZE];
    fill(all, 0x10u, T_SIZE);
    TEST_ASSERT_TRUE(tbl_load_at(T_ID, 0u, all, 8u, T_SCRC));
    TEST_ASSERT_TRUE(tbl_load_at(T_ID, 0u, all, 8u, T_SCRC));   /* resend */
    TEST_ASSERT_TRUE(tbl_load_at(T_ID, 8u, &all[8], 8u, T_SCRC));
    TEST_ASSERT_TRUE(tbl_load_at(T_ID, 8u, &all[8], 8u, T_SCRC)); /* resend */
    TEST_ASSERT_TRUE(tbl_activate(T_ID));
    TEST_ASSERT_EQUAL_UINT16(T_SIZE, last_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(all, last_activated, T_SIZE);
}

static void test_conflicting_retransmit_rejected(void) {
    setup_table();
    uint8_t a[8], b[8];
    fill(a, 0x00u, 8u);
    fill(b, 0x80u, 8u);
    TEST_ASSERT_TRUE(tbl_load_at(T_ID, 0u, a, 8u, T_SCRC));
    TEST_ASSERT_TRUE(tbl_load_at(T_ID, 8u, a, 8u, T_SCRC));
    TEST_ASSERT_FALSE(tbl_load_at(T_ID, 8u, b, 8u, T_SCRC));
}

static void test_gap_rejected(void) {
    setup_table();
    uint8_t a[4];
    fill(a, 1u, 4u);
    TEST_ASSERT_TRUE(tbl_load_at(T_ID, 0u, a, 4u, T_SCRC));
    TEST_ASSERT_FALSE(tbl_load_at(T_ID, 8u, a, 4u, T_SCRC));
    tbl_info_t info;
    TEST_ASSERT_TRUE(tbl_get_info(T_ID, &info));
    TEST_ASSERT_EQUAL_UINT16(4u, info.staged_len);
}

static void test_overrun_rejected_and_not_wedged(void) {
    setup_table();
    uint8_t big[T_SIZE + 4u];
    fill(big, 0x20u, (uint16_t)sizeof(big));
    TEST_ASSERT_FALSE(tbl_load_at(T_ID, 0u, big, (uint16_t)sizeof(big), T_SCRC));
    TEST_ASSERT_FALSE(tbl_load(T_ID, big, (uint16_t)sizeof(big), T_SCRC));
    /* A correct load afterwards still works. */
    TEST_ASSERT_TRUE(tbl_load_at(T_ID, 0u, big, T_SIZE, T_SCRC));
    TEST_ASSERT_TRUE(tbl_activate(T_ID));
}

static void test_mixed_schema_rejected(void) {
    setup_table();
    uint8_t a[8];
    fill(a, 3u, 8u);
    TEST_ASSERT_TRUE(tbl_load_at(T_ID, 0u, a, 8u, T_SCRC));
    TEST_ASSERT_FALSE(tbl_load_at(T_ID, 8u, a, 8u, (uint16_t)(T_SCRC ^ 1u)));
}

static void test_abort_discards_partial_load(void) {
    setup_table();
    uint8_t a[T_SIZE];
    fill(a, 7u, T_SIZE);
    TEST_ASSERT_TRUE(tbl_load_at(T_ID, 0u, a, 6u, T_SCRC));
    TEST_ASSERT_TRUE(tbl_abort(T_ID));
    tbl_info_t info;
    TEST_ASSERT_TRUE(tbl_get_info(T_ID, &info));
    TEST_ASSERT_EQUAL_UINT16(0u, info.staged_len);
    TEST_ASSERT_FALSE(tbl_activate(T_ID));
    /* Legacy tbl_load after abort starts cleanly. */
    TEST_ASSERT_TRUE(tbl_load(T_ID, a, T_SIZE, T_SCRC));
    TEST_ASSERT_TRUE(tbl_activate(T_ID));
}

static void test_commit_without_prepare_rejected(void) {
    setup_table();
    uint8_t junk[T_SIZE];
    fill(junk, 0x55u, T_SIZE);
    TEST_ASSERT_FALSE(__tbl_activate_commit(T_ID, junk, T_SIZE));
    uint8_t out[T_SIZE];
    TEST_ASSERT_EQUAL_INT16(-1, tbl_dump(T_ID, out, T_SIZE));
}

static void test_get_info_copies_descriptor(void) {
    setup_table();
    tbl_info_t info;
    TEST_ASSERT_TRUE(tbl_get_info(T_ID, &info));
    TEST_ASSERT_EQUAL_UINT8(T_ID, info.id);
    TEST_ASSERT_EQUAL_STRING("test", info.name);
    TEST_ASSERT_EQUAL_UINT16(T_SIZE, info.size);
    TEST_ASSERT_EQUAL_HEX16(T_SCRC, info.schema_crc);
    TEST_ASSERT_EQUAL_UINT16(0u, info.active_len);
    TEST_ASSERT_FALSE(tbl_get_info(0x7Fu, &info));
    TEST_ASSERT_FALSE(tbl_get_info(T_ID, NULL));
}

static void test_crc16_known_vector(void) {
    static const uint8_t v[] = { '1','2','3','4','5','6','7','8','9' };
    TEST_ASSERT_EQUAL_HEX16(0x29B1u, crc16_ccitt(v, 9u));
    TEST_ASSERT_EQUAL_HEX16(0x29B1u, __crc16_ccitt(v, 9u));
}

static void test_cdc_rx_counts_dropped_bytes(void) {
    cdc_rx_init();
    uint8_t chunk[64];
    (void)memset(chunk, 'x', sizeof(chunk));
    for (uint32_t i = 0u; i < (CDC_RX_BUF_SIZE / 64u) + 1u; i++) {
        cdc_rx_push(chunk, sizeof(chunk));
    }
    /* Capacity is size-1; everything beyond that is counted. */
    TEST_ASSERT_EQUAL_UINT32(CDC_RX_BUF_SIZE - 1u, cdc_rx_available());
    TEST_ASSERT_EQUAL_UINT32(64u + 1u, cdc_rx_dropped());
    cdc_rx_init();
    TEST_ASSERT_EQUAL_UINT32(0u, cdc_rx_dropped());
}

void run_tables_load_tests(void) {
    RUN_TEST(test_retransmitted_chunk_is_idempotent);
    RUN_TEST(test_conflicting_retransmit_rejected);
    RUN_TEST(test_gap_rejected);
    RUN_TEST(test_overrun_rejected_and_not_wedged);
    RUN_TEST(test_mixed_schema_rejected);
    RUN_TEST(test_abort_discards_partial_load);
    RUN_TEST(test_commit_without_prepare_rejected);
    RUN_TEST(test_get_info_copies_descriptor);
    RUN_TEST(test_crc16_known_vector);
    RUN_TEST(test_cdc_rx_counts_dropped_bytes);
}
