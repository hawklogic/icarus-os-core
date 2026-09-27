/**
 * @file    test_svc_policy.c
 * @brief   Host-side tests for the SVC caller-buffer allowlist.
 *
 * @details SVC implementations copy with privilege, so the MPU does not
 *          stop a task from passing a pointer into kernel data, another
 *          task's data-pool slot, flash (for a write), or peripheral/system
 *          space.  svc_buffer_allowed() is a pure function of the target
 *          memory map and the caller (data-pool slot, main-stack window,
 *          stack the frame is on), so it is exercised here with the real
 *          target addresses even though host pointers differ.
 *
 * @author  Souham Biswas
 * @date    2026
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            Licensed under the Apache License, Version 2.0
 */

#include <stddef.h>

#include "unity.h"
#include "icarus/svc.h"
#include "bsp/mpu.h"

/* The calling task's slot: the second 2 KB slot of the RAM_D2 data pool. */
#define SLOT_SIZE   2048u
#define SLOT_BASE   ((uintptr_t)BSP_RAM_D2_BASE + SLOT_SIZE)

/* The main stack as the target links it (STM32H750VBTX_FLASH.ld:38,41):
 * _estack = end of RAM_D1 (0x24080000), _Min_Stack_Size = 0x400. */
#define MSTACK_TOP   ((uintptr_t)BSP_RAM_D1_BASE + (uintptr_t)BSP_RAM_D1_SIZE)
#define MSTACK_SIZE  0x400u
#define MSTACK_BASE  (MSTACK_TOP - (uintptr_t)MSTACK_SIZE)

/** @brief An unprivileged task: frame on the process stack. */
static const svc_caller_t task_caller = {
    SLOT_BASE, SLOT_SIZE, MSTACK_BASE, MSTACK_SIZE, true
};

/** @brief Privileged boot code on the main stack (no task slot). */
static const svc_caller_t boot_caller = {
    0u, 0u, MSTACK_BASE, MSTACK_SIZE, false
};

static bool rd(uintptr_t addr, uint32_t len) {
    return svc_buffer_allowed(addr, len, SVC_ACCESS_READ, &task_caller);
}

static bool wr(uintptr_t addr, uint32_t len) {
    return svc_buffer_allowed(addr, len, SVC_ACCESS_WRITE, &task_caller);
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
    uintptr_t end  = MSTACK_BASE;          /* where a task's RAM_D1 ends */
    TEST_ASSERT_TRUE(wr(base, 64u));
    TEST_ASSERT_TRUE(rd(base, (uint32_t)(end - base)));
    TEST_ASSERT_TRUE(wr(end - 4u, 4u));
    TEST_ASSERT_FALSE(wr(end - 4u, 8u));   /* straddles into the main stack */
    TEST_ASSERT_FALSE(rd(base, (uint32_t)BSP_RAM_D1_SIZE + 1u));
    TEST_ASSERT_FALSE(wr(base - 4u, 8u));                    /* starts before */
    /* Privileged main-stack code may use all of RAM_D1. */
    TEST_ASSERT_TRUE(svc_buffer_allowed(base, (uint32_t)BSP_RAM_D1_SIZE,
                                        SVC_ACCESS_WRITE, &boot_caller));
    TEST_ASSERT_FALSE(svc_buffer_allowed(MSTACK_TOP - 4u, 8u,
                                         SVC_ACCESS_WRITE, &boot_caller));
}

static void test_policy_main_stack_rejected_for_task_caller(void) {
    /* The handler's saved LR/EXC_RETURN sits just below _estack while an
     * SVC runs: a task must not be able to aim a kernel copy there. */
    TEST_ASSERT_FALSE(wr(MSTACK_TOP - 0x40u, 0x40u));      /* 0x2407FFC0 */
    TEST_ASSERT_FALSE(rd(MSTACK_TOP - 0x40u, 0x40u));      /* nor read it */
    TEST_ASSERT_FALSE(wr(MSTACK_BASE, 1u));                /* first byte */
    TEST_ASSERT_FALSE(rd(MSTACK_TOP - 1u, 1u));            /* last byte */
    TEST_ASSERT_FALSE(wr(MSTACK_BASE, MSTACK_SIZE));       /* whole window */
    /* Byte-by-byte string checks see the same boundary. */
    TEST_ASSERT_TRUE(rd(MSTACK_BASE - 1u, 1u));
    TEST_ASSERT_FALSE(rd(MSTACK_BASE, 1u));
}

static void test_policy_main_stack_allowed_for_main_stack_caller(void) {
    /* Boot code running on the main stack may pass its own locals. */
    TEST_ASSERT_TRUE(svc_buffer_allowed(MSTACK_TOP - 0x40u, 0x40u,
                                        SVC_ACCESS_WRITE, &boot_caller));
    TEST_ASSERT_TRUE(svc_buffer_allowed(MSTACK_TOP - 0x40u, 0x40u,
                                        SVC_ACCESS_READ, &boot_caller));
    TEST_ASSERT_TRUE(svc_buffer_allowed(MSTACK_BASE, MSTACK_SIZE,
                                        SVC_ACCESS_WRITE, &boot_caller));
    /* The rest of the policy still applies to it. */
    TEST_ASSERT_FALSE(svc_buffer_allowed((uintptr_t)BSP_FLASH_BASE, 4u,
                                         SVC_ACCESS_WRITE, &boot_caller));
    TEST_ASSERT_FALSE(svc_buffer_allowed((uintptr_t)BSP_DTCM_BASE, 4u,
                                         SVC_ACCESS_READ, &boot_caller));
}

static void test_policy_main_stack_straddling_ranges(void) {
    /* Ends exactly at the window: allowed.  One byte in: rejected. */
    TEST_ASSERT_TRUE(wr(MSTACK_BASE - 16u, 16u));
    TEST_ASSERT_FALSE(wr(MSTACK_BASE - 16u, 17u));
    TEST_ASSERT_FALSE(rd(MSTACK_BASE - 8u, 16u));
    /* A large task buffer that covers the window from below. */
    TEST_ASSERT_FALSE(wr((uintptr_t)BSP_RAM_D1_BASE,
                         (uint32_t)BSP_RAM_D1_SIZE));
    TEST_ASSERT_FALSE(rd(MSTACK_BASE - 0x100u, 0x200u));
    /* Starting inside and running off the end of RAM_D1. */
    TEST_ASSERT_FALSE(wr(MSTACK_TOP - 4u, 8u));
}

static void test_policy_window_size_zero_and_null_caller(void) {
    /* No window configured: the top of RAM_D1 is ordinary RAM_D1. */
    const svc_caller_t no_window = { SLOT_BASE, SLOT_SIZE, MSTACK_BASE, 0u,
                                     true };
    TEST_ASSERT_TRUE(svc_buffer_allowed(MSTACK_TOP - 0x40u, 0x40u,
                                        SVC_ACCESS_WRITE, &no_window));
    /* A missing caller description never grants access. */
    TEST_ASSERT_FALSE(svc_buffer_allowed((uintptr_t)BSP_RAM_D1_BASE, 4u,
                                         SVC_ACCESS_READ, NULL));
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
    const svc_caller_t no_slot = { SLOT_BASE, 0u, MSTACK_BASE, MSTACK_SIZE,
                                   true };
    TEST_ASSERT_FALSE(svc_buffer_allowed(SLOT_BASE, 4u, SVC_ACCESS_WRITE,
                                         &no_slot));
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
    RUN_TEST(test_policy_main_stack_rejected_for_task_caller);
    RUN_TEST(test_policy_main_stack_allowed_for_main_stack_caller);
    RUN_TEST(test_policy_main_stack_straddling_ranges);
    RUN_TEST(test_policy_window_size_zero_and_null_caller);
    RUN_TEST(test_policy_dtcm_app_half_allowed_kernel_half_rejected);
    RUN_TEST(test_policy_own_pool_slot_only);
    RUN_TEST(test_policy_privileged_and_device_space_rejected);
    RUN_TEST(test_policy_null_and_wrap_rejected_zero_len_allowed);
    RUN_TEST(test_policy_host_caller_is_privileged);
}
