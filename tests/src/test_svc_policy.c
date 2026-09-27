/**
 * @file    test_svc_policy.c
 * @brief   Host-side tests for the SVC caller-buffer allowlist.
 *
 * @details SVC implementations copy with privilege, so the MPU does not
 *          stop a task from passing a pointer into kernel data, another
 *          task's data-pool slot, flash (for a write), or peripheral/system
 *          space.  svc_buffer_allowed() is a pure function of the target
 *          memory map and the caller's slot, so it is exercised here with the
 *          real target addresses even though host pointers differ.
 *
 * @author  Souham Biswas
 * @date    2026
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            Licensed under the Apache License, Version 2.0
 */

#include "unity.h"
#include "icarus/svc.h"
#include "bsp/mpu.h"

/* The calling task's slot: the second 2 KB slot of the RAM_D2 data pool. */
#define SLOT_SIZE   2048u
#define SLOT_BASE   ((uintptr_t)BSP_RAM_D2_BASE + SLOT_SIZE)

static bool rd(uintptr_t addr, uint32_t len) {
    return svc_buffer_allowed(addr, len, SVC_ACCESS_READ, SLOT_BASE, SLOT_SIZE);
}

static bool wr(uintptr_t addr, uint32_t len) {
    return svc_buffer_allowed(addr, len, SVC_ACCESS_WRITE, SLOT_BASE, SLOT_SIZE);
}

static void test_policy_flash_readable_not_writable(void) {
    uintptr_t flash = (uintptr_t)BSP_FLASH_BASE + 0x1000u;
    TEST_ASSERT_TRUE(rd(flash, 16u));
    TEST_ASSERT_FALSE(wr(flash, 16u));
    /* Last byte of flash is readable; one past it is not. */
    uintptr_t last = (uintptr_t)BSP_FLASH_BASE + (uintptr_t)BSP_FLASH_SIZE - 1u;
    TEST_ASSERT_TRUE(rd(last, 1u));
    TEST_ASSERT_FALSE(rd(last, 2u));
}

static void test_policy_itcm_readable_not_writable_not_null(void) {
    TEST_ASSERT_TRUE(rd((uintptr_t)BSP_ITCM_BASE + 0x100u, 64u));
    TEST_ASSERT_FALSE(wr((uintptr_t)BSP_ITCM_BASE + 0x100u, 64u));
    TEST_ASSERT_FALSE(rd((uintptr_t)BSP_ITCM_BASE, 4u));   /* address 0 */
}

static void test_policy_ram_d1_read_write_and_straddle(void) {
    uintptr_t base = (uintptr_t)BSP_RAM_D1_BASE;
    uintptr_t end  = base + (uintptr_t)BSP_RAM_D1_SIZE;     /* exclusive */
    TEST_ASSERT_TRUE(wr(base, 64u));
    TEST_ASSERT_TRUE(rd(base, (uint32_t)BSP_RAM_D1_SIZE));
    TEST_ASSERT_TRUE(wr(end - 4u, 4u));
    TEST_ASSERT_FALSE(wr(end - 4u, 8u));                     /* straddles */
    TEST_ASSERT_FALSE(rd(base, (uint32_t)BSP_RAM_D1_SIZE + 1u));
    TEST_ASSERT_FALSE(wr(base - 4u, 8u));                    /* starts before */
}

static void test_policy_dtcm_app_half_allowed_kernel_half_rejected(void) {
    uintptr_t app = (uintptr_t)BSP_DTCM_APP_BASE;
    TEST_ASSERT_TRUE(wr(app, 256u));
    TEST_ASSERT_TRUE(wr(app + (uintptr_t)BSP_DTCM_APP_SIZE - 1u, 1u));
    TEST_ASSERT_FALSE(wr((uintptr_t)BSP_DTCM_BASE, 4u));     /* kernel data */
    TEST_ASSERT_FALSE(rd((uintptr_t)BSP_DTCM_BASE + 0x100u, 4u));
    TEST_ASSERT_FALSE(wr(app - 2u, 4u));       /* straddles the boundary */
    TEST_ASSERT_FALSE(rd(app + (uintptr_t)BSP_DTCM_APP_SIZE - 2u, 4u));
}

static void test_policy_own_pool_slot_only(void) {
    TEST_ASSERT_TRUE(wr(SLOT_BASE, SLOT_SIZE));
    TEST_ASSERT_TRUE(rd(SLOT_BASE + 100u, 16u));
    /* Another task's slot (the one before and the one after). */
    TEST_ASSERT_FALSE(wr((uintptr_t)BSP_RAM_D2_BASE, 16u));
    TEST_ASSERT_FALSE(rd((uintptr_t)BSP_RAM_D2_BASE, 16u));
    TEST_ASSERT_FALSE(wr(SLOT_BASE + SLOT_SIZE, 16u));
    /* Straddling the end of the own slot. */
    TEST_ASSERT_FALSE(wr(SLOT_BASE + SLOT_SIZE - 2u, 4u));
    TEST_ASSERT_FALSE(wr(SLOT_BASE - 2u, 4u));
    /* No slot (size 0): the pool is not reachable at all. */
    TEST_ASSERT_FALSE(svc_buffer_allowed(SLOT_BASE, 4u, SVC_ACCESS_WRITE,
                                         SLOT_BASE, 0u));
}

static void test_policy_privileged_and_device_space_rejected(void) {
    TEST_ASSERT_FALSE(rd((uintptr_t)BSP_RAM_D3_BASE, 4u));       /* SRAM4   */
    TEST_ASSERT_FALSE(wr((uintptr_t)BSP_RAM_D3_BASE, 4u));
    TEST_ASSERT_FALSE(rd((uintptr_t)BSP_BKPSRAM_BASE, 4u));      /* backup  */
    TEST_ASSERT_FALSE(wr((uintptr_t)BSP_BKPSRAM_BASE, 4u));
    TEST_ASSERT_FALSE(rd((uintptr_t)BSP_PERIPH_BASE, 4u));       /* periph  */
    TEST_ASSERT_FALSE(wr((uintptr_t)0xE000ED00u, 4u));           /* SCB     */
    TEST_ASSERT_FALSE(rd((uintptr_t)BSP_QSPI_BASE, 4u));         /* QSPI    */
    TEST_ASSERT_FALSE(rd((uintptr_t)0x1FF00000u, 4u));           /* unmapped */
}

static void test_policy_null_and_wrap_rejected_zero_len_allowed(void) {
    TEST_ASSERT_FALSE(rd(0u, 0u));
    TEST_ASSERT_FALSE(wr(0u, 4u));
    TEST_ASSERT_FALSE(rd(UINTPTR_MAX - 3u, 8u));                 /* wraps */
    TEST_ASSERT_FALSE(wr(UINTPTR_MAX, 2u));
    /* A zero-length buffer touches nothing, wherever it points. */
    TEST_ASSERT_TRUE(wr((uintptr_t)BSP_BKPSRAM_BASE, 0u));
    TEST_ASSERT_TRUE(rd((uintptr_t)BSP_RAM_D1_BASE, 0u));
}

static void test_policy_host_caller_is_privileged(void) {
    /* Host tests call privileged implementations directly. */
    TEST_ASSERT_TRUE(svc_caller_is_privileged());
}

void run_svc_policy_tests(void) {
    RUN_TEST(test_policy_flash_readable_not_writable);
    RUN_TEST(test_policy_itcm_readable_not_writable_not_null);
    RUN_TEST(test_policy_ram_d1_read_write_and_straddle);
    RUN_TEST(test_policy_dtcm_app_half_allowed_kernel_half_rejected);
    RUN_TEST(test_policy_own_pool_slot_only);
    RUN_TEST(test_policy_privileged_and_device_space_rejected);
    RUN_TEST(test_policy_null_and_wrap_rejected_zero_len_allowed);
    RUN_TEST(test_policy_host_caller_is_privileged);
}
