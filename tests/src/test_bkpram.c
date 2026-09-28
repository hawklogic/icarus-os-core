/**
 * @file    test_bkpram.c
 * @brief   Host-side tests for the backup SRAM read/write call gates.
 *
 * @details The host build applies exactly the same range checks as the
 *          target SVC handler, so these tests pin down the contract:
 *          4 KB window, no zero-length or wrapping ranges, and data that
 *          survives until the store is explicitly cleared.
 *
 * @author  Souham Biswas
 * @date    2026
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            Licensed under the Apache License, Version 2.0
 */

#include "unity.h"
#include "icarus/kernel.h"
#include "bsp/mpu.h"
#include <string.h>

static void test_bkpram_round_trip(void) {
    __bkpram_host_clear();
    const uint8_t in[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    uint8_t out[8] = { 0 };
    TEST_ASSERT_TRUE(bkpram_write(in, 16u, sizeof(in)));
    TEST_ASSERT_TRUE(bkpram_read(out, 16u, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(in, out, sizeof(in));
}

static void test_bkpram_last_byte_ok_one_past_rejected(void) {
    __bkpram_host_clear();
    uint8_t b = 0x5A;
    TEST_ASSERT_TRUE(bkpram_write(&b, BSP_BKPSRAM_SIZE - 1u, 1u));
    TEST_ASSERT_FALSE(bkpram_write(&b, BSP_BKPSRAM_SIZE, 1u));
    uint8_t two[2] = { 0 };
    TEST_ASSERT_FALSE(bkpram_write(two, BSP_BKPSRAM_SIZE - 1u, 2u));
}

static void test_bkpram_rejects_zero_len_and_null(void) {
    uint8_t b = 0;
    TEST_ASSERT_FALSE(bkpram_write(&b, 0u, 0u));
    TEST_ASSERT_FALSE(bkpram_read(&b, 0u, 0u));
    TEST_ASSERT_FALSE(bkpram_write(NULL, 0u, 1u));
    TEST_ASSERT_FALSE(bkpram_read(NULL, 0u, 1u));
}

static void test_bkpram_rejects_offset_wrap(void) {
    uint8_t b = 0;
    TEST_ASSERT_FALSE(bkpram_write(&b, 0xFFFFFFFFu, 2u));
    TEST_ASSERT_FALSE(bkpram_read(&b, 0xFFFFFFF0u, 0x20u));
}

static void test_bkpram_survives_until_cleared(void) {
    __bkpram_host_clear();
    const uint32_t magic = 0xCAFEF00Du;
    TEST_ASSERT_TRUE(bkpram_write(&magic, 0u, sizeof(magic)));
    uint32_t got = 0u;
    TEST_ASSERT_TRUE(bkpram_read(&got, 0u, sizeof(got)));
    TEST_ASSERT_EQUAL_HEX32(magic, got);
    __bkpram_host_clear();
    TEST_ASSERT_TRUE(bkpram_read(&got, 0u, sizeof(got)));
    TEST_ASSERT_EQUAL_HEX32(0u, got);
}

void run_bkpram_tests(void) {
    RUN_TEST(test_bkpram_round_trip);
    RUN_TEST(test_bkpram_last_byte_ok_one_past_rejected);
    RUN_TEST(test_bkpram_rejects_zero_len_and_null);
    RUN_TEST(test_bkpram_rejects_offset_wrap);
    RUN_TEST(test_bkpram_survives_until_cleared);
}
