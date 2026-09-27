/**
 * @file    bootloader.c
 * @brief   Board Support Package — enter the STM32 system (ROM) bootloader
 *
 * @details See bootloader.h for the request-then-reset design.
 *
 *          The request word lives in SRAM4 (.ram_d3): NOLOAD, not zeroed by
 *          startup, and retained across a system reset.  Before resetting,
 *          the D-cache is cleaned and disabled so the write reaches SRAM4
 *          (that address range is write-back cacheable under the default
 *          memory map).
 *
 * @author  Souham Biswas
 * @date    2026
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            https://github.com/ironhide23586/icarus-os-core
 *            Licensed under the Apache License, Version 2.0
 */

#include "bsp/bootloader.h"
#include "icarus/svc.h"

#ifndef HOST_TEST

#include "stm32h7xx.h"

/** @brief Pending-request marker; survives a system reset. */
static volatile uint32_t bootloader_request __attribute__((section(".ram_d3")));

void bsp_bootloader_check(void) {
    if (bootloader_request != BSP_BOOTLOADER_MAGIC) {
        return;
    }
    bootloader_request = 0u;
    __DSB();

    /* Straight after reset: clocks, caches, MPU and interrupts are still
     * at their reset state and the watchdog has not been started, which is
     * what the ROM bootloader expects. */
    const volatile uint32_t *vectors =
        (const volatile uint32_t *)BSP_SYSTEM_BOOTLOADER_ADDR;
    uint32_t sp    = vectors[0];
    uint32_t entry = vectors[1];

    __set_MSP(sp);
    __ISB();
    ((void (*)(void))(uintptr_t)entry)();

    for (;;) {
    }
}

void __sys_enter_bootloader(void) {
    __disable_irq();
    bootloader_request = BSP_BOOTLOADER_MAGIC;
    SCB_DisableDCache();          /* cleans, so the marker reaches SRAM4 */
    __DSB();
    NVIC_SystemReset();
}

void sys_enter_bootloader(void) {
    __asm__ volatile ("svc %0\n" : : "I" (SVC_SYS_ENTER_BOOTLOADER));
    for (;;) {
    }
}

#else /* HOST_TEST */

static bool g_bootloader_requested;

void bsp_bootloader_check(void) {
}

void __sys_enter_bootloader(void) {
    g_bootloader_requested = true;
}

void sys_enter_bootloader(void) {
    SVC_HOST_GATE();
    __sys_enter_bootloader();
}

bool __bootloader_host_requested(void) {
    return g_bootloader_requested;
}

void __bootloader_host_reset(void) {
    g_bootloader_requested = false;
}

#endif /* HOST_TEST */
