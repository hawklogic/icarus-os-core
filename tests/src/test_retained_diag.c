/** @file test_retained_diag.c @brief Host controls for retained diagnostic format. */
#include "unity.h"
#include "bsp/retained_diag.h"
#include "icarus/kernel.h"

#include <stddef.h>
#include <string.h>

static void reset_store(void)
{
    __bkpram_host_clear();
    retained_diag_host_reset_context();
}

static retained_diag_snapshot_t read_snapshot(void)
{
    retained_diag_snapshot_t s = {0};
    (void)retained_diag_read(&s);
    return s;
}

static void test_raw_empty_corrupt_and_explicit_initialization(void)
{
    reset_store();
    retained_diag_snapshot_t empty = read_snapshot();
    TEST_ASSERT_EQUAL_INT(RETAINED_DIAG_EMPTY, empty.status);
    TEST_ASSERT_FALSE(empty.phase_valid);
    TEST_ASSERT_FALSE(empty.fault_valid);
    uint8_t bad = 0x7au;
    TEST_ASSERT_TRUE(bkpram_write(&bad, RETAINED_DIAG_PHASE_A + 3u, 1u));
    retained_diag_snapshot_t corrupt = read_snapshot();
    TEST_ASSERT_EQUAL_INT(RETAINED_DIAG_CORRUPT, corrupt.status);
    TEST_ASSERT_EQUAL_HEX8(bad, corrupt.raw[3]);
    uint32_t seq = 0u;
    TEST_ASSERT_TRUE(retained_diag_arm_after_ack(&corrupt, 7u, 0x12340000u,
                                                  0xaabbccddu, &seq));
    TEST_ASSERT_EQUAL_UINT32(1u, seq);
    retained_diag_snapshot_t armed = read_snapshot();
    TEST_ASSERT_TRUE(armed.phase_valid);
    TEST_ASSERT_EQUAL_UINT32(RETAINED_DIAG_PHASE_ARMED, armed.phase.phase);
    TEST_ASSERT_EQUAL_UINT32(7u, armed.phase.boot);
    TEST_ASSERT_EQUAL_UINT32(0x12340000u, armed.phase.attempt);
    TEST_ASSERT_EQUAL_UINT32(0xaabbccddu, armed.phase.source_tag);
}

static void test_arm_requires_exact_128_byte_snapshot(void)
{
    reset_store();
    retained_diag_snapshot_t shown = read_snapshot();
    uint8_t one = 1u;
    TEST_ASSERT_TRUE(bkpram_write(&one, RETAINED_DIAG_FAULT + 9u, 1u));
    uint32_t seq = 99u;
    TEST_ASSERT_FALSE(retained_diag_arm_after_ack(&shown, 1u, 2u, 3u, &seq));
    TEST_ASSERT_EQUAL_UINT32(99u, seq);
    retained_diag_snapshot_t now = read_snapshot();
    TEST_ASSERT_EQUAL_HEX8(one, now.raw[64u + 9u]);
    TEST_ASSERT_EQUAL_INT(RETAINED_DIAG_CORRUPT, now.status);
}

static void test_phase_alternation_and_context_check(void)
{
    reset_store();
    retained_diag_snapshot_t shown = read_snapshot();
    TEST_ASSERT_TRUE(retained_diag_arm_after_ack(&shown, 2u, 0x10001u,
                                                  0x12345678u, NULL));
    TEST_ASSERT_FALSE(retained_diag_mark(3u, 0x10001u, 0x12345678u, 1u));
    TEST_ASSERT_FALSE(retained_diag_mark(2u, 0x10001u, 0x12345678u, 0u));
    TEST_ASSERT_TRUE(retained_diag_mark(2u, 0x10001u, 0x12345678u, 1u));
    retained_diag_snapshot_t first = read_snapshot();
    TEST_ASSERT_EQUAL_UINT32(2u, first.phase.sequence);
    TEST_ASSERT_EQUAL_UINT32(1u, first.phase.phase);
    TEST_ASSERT_TRUE(retained_diag_mark(2u, 0x10001u, 0x12345678u, 2u));
    retained_diag_snapshot_t second = read_snapshot();
    TEST_ASSERT_EQUAL_UINT32(3u, second.phase.sequence);
    TEST_ASSERT_EQUAL_UINT32(2u, second.phase.phase);
    /* The other slot still holds the previously committed phase. */
    retained_diag_phase_t older;
    (void)memcpy(&older, &second.raw[32], sizeof(older));
    TEST_ASSERT_EQUAL_UINT32(2u, older.sequence);
    TEST_ASSERT_EQUAL_HEX32(RETAINED_DIAG_COMMIT, older.commit);
}

static void test_interrupted_arm_has_no_stale_armed_context(void)
{
    for (uint32_t cut = 0u; cut < 4u; ++cut) {
        reset_store();
        retained_diag_snapshot_t shown = read_snapshot();
        retained_diag_host_fail_after_writes(cut);
        TEST_ASSERT_FALSE(retained_diag_arm_after_ack(&shown, 8u, 9u, 10u, NULL));
        /* An interrupted task/boot cannot mark from the partial arm. */
        retained_diag_host_fail_after_writes(UINT32_MAX);
        TEST_ASSERT_FALSE(retained_diag_mark(8u, 9u, 10u, 1u));
        retained_diag_host_reset_context();
        retained_diag_snapshot_t partial = read_snapshot();
        TEST_ASSERT_FALSE(partial.fault_bound);
        TEST_ASSERT_FALSE(retained_diag_mark(8u, 9u, 10u, 1u));
        /* Explicit acknowledgment of the exact new raw state repairs it. */
        TEST_ASSERT_TRUE(retained_diag_arm_after_ack(&partial, 8u, 9u, 10u, NULL));
    }
}

static void test_interrupted_phase_preserves_prior_committed_slot(void)
{
    for (uint32_t cut = 0u; cut < 3u; ++cut) {
        reset_store();
        retained_diag_snapshot_t shown = read_snapshot();
        TEST_ASSERT_TRUE(retained_diag_arm_after_ack(&shown, 1u, 2u, 3u, NULL));
        retained_diag_host_fail_after_writes(cut);
        TEST_ASSERT_FALSE(retained_diag_mark(1u, 2u, 3u, 4u));
        retained_diag_host_fail_after_writes(UINT32_MAX);
        retained_diag_snapshot_t s = read_snapshot();
        TEST_ASSERT_TRUE(s.phase_valid);
        TEST_ASSERT_EQUAL_UINT32(1u, s.phase.sequence);
        TEST_ASSERT_EQUAL_UINT32(RETAINED_DIAG_PHASE_ARMED, s.phase.phase);
    }
}

static void write_fault(uint32_t phase_sequence, uint32_t boot,
                        uint32_t attempt, uint32_t phase)
{
    retained_diag_fault_t f = {0};
    f.format_kind_flags = RETAINED_DIAG_FORMAT |
                          (RETAINED_DIAG_KIND_HARD << 8);
    f.phase_sequence = phase_sequence;
    f.boot = boot;
    f.attempt = attempt;
    f.phase = phase;
    f.raw_msp = 0x2407fff0u;
    f.raw_psp = 0x24010000u;
    f.exc_return = 0xfffffffdu;
    f.cfsr = 0x82u;
    f.checksum = retained_diag_checksum(&f,
                  (uint32_t)offsetof(retained_diag_fault_t, checksum));
    f.commit = RETAINED_DIAG_COMMIT;
    TEST_ASSERT_TRUE(bkpram_write(&f, RETAINED_DIAG_FAULT, sizeof(f)));
}

static void test_fault_during_phase_commit_is_separate_and_latched(void)
{
    reset_store();
    retained_diag_snapshot_t shown = read_snapshot();
    TEST_ASSERT_TRUE(retained_diag_arm_after_ack(&shown, 10u, 11u, 12u, NULL));
    retained_diag_host_fail_after_writes(2u); /* destination commit and body */
    TEST_ASSERT_FALSE(retained_diag_mark(10u, 11u, 12u, 44u));
    retained_diag_host_fail_after_writes(UINT32_MAX);
    retained_diag_snapshot_t partial = read_snapshot();
    TEST_ASSERT_TRUE(partial.phase_valid);
    TEST_ASSERT_EQUAL_UINT32(1u, partial.phase.sequence);
    write_fault(partial.phase.sequence, 10u, 11u,
                RETAINED_DIAG_PHASE_ARMED);
    TEST_ASSERT_FALSE(retained_diag_mark(10u, 11u, 12u, 45u));
    retained_diag_snapshot_t captured = read_snapshot();
    TEST_ASSERT_TRUE(captured.fault_valid);
    TEST_ASSERT_TRUE(captured.fault_bound);
    TEST_ASSERT_EQUAL_UINT32(12u, captured.fault_source_tag);
}

static void test_fault_requires_exact_phase_join_and_latches_until_arm(void)
{
    reset_store();
    retained_diag_snapshot_t shown = read_snapshot();
    TEST_ASSERT_TRUE(retained_diag_arm_after_ack(&shown, 5u, 6u, 7u, NULL));
    TEST_ASSERT_TRUE(retained_diag_mark(5u, 6u, 7u, 42u));
    retained_diag_snapshot_t before = read_snapshot();
    write_fault(before.phase.sequence, 5u, 6u, 42u);
    retained_diag_snapshot_t bound = read_snapshot();
    TEST_ASSERT_TRUE(bound.fault_valid);
    TEST_ASSERT_TRUE(bound.fault_bound);
    TEST_ASSERT_EQUAL_UINT32(7u, bound.fault_source_tag);
    TEST_ASSERT_FALSE(retained_diag_mark(5u, 6u, 7u, 43u));
    write_fault(before.phase.sequence + 9u, 5u, 6u, 42u);
    retained_diag_snapshot_t unbound = read_snapshot();
    TEST_ASSERT_TRUE(unbound.fault_valid);
    TEST_ASSERT_FALSE(unbound.fault_bound);
    TEST_ASSERT_TRUE(retained_diag_arm_after_ack(&unbound, 6u, 8u, 9u, NULL));
    retained_diag_snapshot_t rearmed = read_snapshot();
    TEST_ASSERT_FALSE(rearmed.fault_valid);
    TEST_ASSERT_TRUE(rearmed.phase_valid);
    TEST_ASSERT_EQUAL_UINT32(6u, rearmed.phase.boot);
}

static void test_frame_guard_basic_extended_and_fault_flags(void)
{
    uintptr_t frame = 0u;
    uint32_t bytes = 0u;
    TEST_ASSERT_TRUE(retained_diag_frame_range(0x24001000u, 0x24002000u,
                     0xfffffffdu, 0u, 0x24001f00u, 0x24002100u,
                     &frame, &bytes));
    TEST_ASSERT_EQUAL_HEX32(0x24002000u, (uint32_t)frame);
    TEST_ASSERT_EQUAL_UINT32(32u, bytes);
    TEST_ASSERT_TRUE(retained_diag_frame_range(0x24001000u, 0x24002000u,
                     0xffffffedu, 0u, 0x24001f00u, 0x24002100u,
                     &frame, &bytes));
    TEST_ASSERT_EQUAL_UINT32(104u, bytes);
    TEST_ASSERT_FALSE(retained_diag_frame_range(0x24001000u, 0x24002000u,
                      0xffffffedu, 1u << 12, 0x24001f00u, 0x24002100u,
                      &frame, &bytes));
    TEST_ASSERT_FALSE(retained_diag_frame_range(0x24001000u, 0x24002004u,
                      0xfffffffdu, 0u, 0x24001f00u, 0x24002100u,
                      &frame, &bytes));
    TEST_ASSERT_FALSE(retained_diag_frame_range(0x24001000u, 0x24002000u,
                      0xfffffffdu, 0u, 0x24001f00u, 0x2400201cu,
                      &frame, &bytes));
    TEST_ASSERT_FALSE(retained_diag_frame_range(0x24001000u, 0x24002000u,
                      0xffffffbdu, 0u, 0x24001f00u, 0x24002100u,
                      &frame, &bytes));
}

static void test_memmanage_terminal_threshold_before_recovery(void)
{
    TEST_ASSERT_FALSE(retained_diag_memmanage_should_terminate(0u));
    TEST_ASSERT_FALSE(retained_diag_memmanage_should_terminate(29u));
    TEST_ASSERT_TRUE(retained_diag_memmanage_should_terminate(30u));
    TEST_ASSERT_TRUE(retained_diag_memmanage_should_terminate(31u));
}

void run_retained_diag_tests(void)
{
    RUN_TEST(test_raw_empty_corrupt_and_explicit_initialization);
    RUN_TEST(test_arm_requires_exact_128_byte_snapshot);
    RUN_TEST(test_phase_alternation_and_context_check);
    RUN_TEST(test_interrupted_arm_has_no_stale_armed_context);
    RUN_TEST(test_interrupted_phase_preserves_prior_committed_slot);
    RUN_TEST(test_fault_during_phase_commit_is_separate_and_latched);
    RUN_TEST(test_fault_requires_exact_phase_join_and_latches_until_arm);
    RUN_TEST(test_frame_guard_basic_extended_and_fault_flags);
    RUN_TEST(test_memmanage_terminal_threshold_before_recovery);
}
