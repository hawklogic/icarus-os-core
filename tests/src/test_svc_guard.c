/**
 * @file    test_svc_guard.c
 * @brief   Host-side tests for the nested-SVC guard and the checksum
 *          monitor's thread-mode callback delivery.
 *
 * @details On target, a supervisor call issued while already inside the
 *          SVC handler escalates to a HardFault.  These tests prove that
 *          the host guard detects that pattern, and that cs_check_all()
 *          delivers mismatch callbacks outside the privileged scan so a
 *          callback may itself use kernel call gates.
 *
 * @author  Souham Biswas
 * @date    2026
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            Licensed under the Apache License, Version 2.0
 */

#include "unity.h"
#include "icarus/icarus.h"
#include "icarus/svc.h"
#include "icarus/cs.h"
#include <string.h>

/* ---- Counting nesting handler ------------------------------------------ */

static const char *last_outer;
static const char *last_inner;

static void counting_handler(const char *outer, const char *inner) {
    last_outer = outer;
    last_inner = inner;
}

static void guard_setup(void) {
    svc_host_gate_reset();
    svc_host_set_nesting_handler(counting_handler);
    last_outer = NULL;
    last_inner = NULL;
}

static void guard_teardown(void) {
    svc_host_set_nesting_handler(NULL);
    svc_host_gate_reset();
}

/* A stand-in for a privileged implementation that (wrongly) calls a
 * kernel call gate while the outer gate is still open. */
static uint32_t fake_gated_wrapper(void) {
    SVC_HOST_GATE();
    return os_get_tick_count();   /* nested gate */
}

/* ---- Guard behaviour ----------------------------------------------------- */

static void test_guard_detects_nested_gate(void) {
    guard_setup();
    (void)fake_gated_wrapper();
    TEST_ASSERT_EQUAL_UINT32(1u, svc_host_nesting_count());
    TEST_ASSERT_EQUAL_STRING("fake_gated_wrapper", last_outer);
    TEST_ASSERT_EQUAL_STRING("os_get_tick_count", last_inner);
    guard_teardown();
}

static void test_guard_allows_sequential_gates(void) {
    guard_setup();
    (void)os_get_tick_count();
    (void)os_get_tick_count();
    enter_critical();
    exit_critical();
    TEST_ASSERT_EQUAL_UINT32(0u, svc_host_nesting_count());
    guard_teardown();
}

static void test_guard_depth_unwinds_after_return(void) {
    guard_setup();
    (void)fake_gated_wrapper();                /* one violation */
    (void)os_get_tick_count();                 /* must not be nested now */
    TEST_ASSERT_EQUAL_UINT32(1u, svc_host_nesting_count());
    guard_teardown();
}

/* ---- Checksum monitor: callback must run outside the scan -------------- */

static uint8_t  region_buf[64];
static uint32_t cb_calls;
static uint8_t  cb_region;
static uint16_t cb_expected;
static uint16_t cb_actual;

/* Mirrors a realistic application callback: it logs (tick read), takes a
 * critical section and records the fault.  Every one of these is a call
 * gate, which is exactly what HardFaulted when the callback ran inside the
 * privileged scan. */
static void kernel_calling_callback(uint8_t region, uint16_t expected,
                                    uint16_t actual) {
    (void)os_get_tick_count();
    enter_critical();
    cb_calls++;
    cb_region   = region;
    cb_expected = expected;
    cb_actual   = actual;
    exit_critical();
}

static void cs_setup(void) {
    guard_setup();
    cb_calls = 0u;
    (void)memset(region_buf, 0xAA, sizeof(region_buf));
    cs_init();
    cs_set_callback(kernel_calling_callback);
    TEST_ASSERT_TRUE(cs_add_region(1u, region_buf, sizeof(region_buf)));
}

static void test_cs_clean_scan_no_callback(void) {
    cs_setup();
    TEST_ASSERT_EQUAL_UINT8(0u, cs_check_all());
    TEST_ASSERT_EQUAL_UINT32(0u, cb_calls);
    TEST_ASSERT_EQUAL_UINT32(0u, svc_host_nesting_count());
    guard_teardown();
}

static void test_cs_mismatch_callback_may_use_kernel_calls(void) {
    cs_setup();
    cs_region_t r;
    TEST_ASSERT_TRUE(cs_get_region(1u, &r));

    region_buf[10] ^= 0x04u;                   /* single bit flip */
    TEST_ASSERT_EQUAL_UINT8(1u, cs_check_all());

    TEST_ASSERT_EQUAL_UINT32(1u, cb_calls);
    TEST_ASSERT_EQUAL_UINT8(1u, cb_region);
    TEST_ASSERT_EQUAL_HEX16(r.baseline, cb_expected);
    TEST_ASSERT_NOT_EQUAL(cb_expected, cb_actual);
    /* The regression guard: no nested gate while delivering the callback. */
    TEST_ASSERT_EQUAL_UINT32(0u, svc_host_nesting_count());
    guard_teardown();
}

static void test_cs_mismatch_reported_every_scan_until_rebaseline(void) {
    cs_setup();
    region_buf[0] ^= 0x01u;
    TEST_ASSERT_EQUAL_UINT8(1u, cs_check_all());
    TEST_ASSERT_EQUAL_UINT8(1u, cs_check_all());
    TEST_ASSERT_EQUAL_UINT32(2u, cb_calls);
    TEST_ASSERT_TRUE(cs_rebaseline(1u));
    TEST_ASSERT_EQUAL_UINT8(0u, cs_check_all());
    TEST_ASSERT_EQUAL_UINT32(2u, cb_calls);
    guard_teardown();
}

static void test_cs_scan_without_callback_counts_failures(void) {
    cs_setup();
    cs_set_callback(NULL);
    region_buf[5] ^= 0x80u;
    TEST_ASSERT_EQUAL_UINT8(1u, cs_check_all());
    TEST_ASSERT_EQUAL_UINT32(0u, cb_calls);
    guard_teardown();
}

void run_svc_guard_tests(void) {
    RUN_TEST(test_guard_detects_nested_gate);
    RUN_TEST(test_guard_allows_sequential_gates);
    RUN_TEST(test_guard_depth_unwinds_after_return);
    RUN_TEST(test_cs_clean_scan_no_callback);
    RUN_TEST(test_cs_mismatch_callback_may_use_kernel_calls);
    RUN_TEST(test_cs_mismatch_reported_every_scan_until_rebaseline);
    RUN_TEST(test_cs_scan_without_callback_counts_failures);
}
