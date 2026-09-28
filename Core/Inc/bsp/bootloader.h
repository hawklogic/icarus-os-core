/**
 * @file    bootloader.h
 * @brief   Board Support Package — enter the STM32 system (ROM) bootloader
 *
 * @details Lets firmware hand the chip to the built-in ROM bootloader (USB
 *          DFU on this board) without touching BOOT0/RESET, so a new image
 *          can be flashed remotely.
 *
 *          Jumping straight from a running application is unreliable: the
 *          clocks, caches, MPU, interrupts and an already-started watchdog
 *          would all leak into the bootloader.  Instead the request is
 *          recorded in reset-surviving RAM and the chip is reset; the
 *          startup code calls bsp_bootloader_check() right after RAM
 *          initialisation — before main(), clock setup or the watchdog —
 *          and jumps to the ROM bootloader from that clean state.
 *
 * @author  Souham Biswas
 * @date    2026
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            https://github.com/ironhide23586/icarus-os-core
 *            Licensed under the Apache License, Version 2.0
 */

#ifndef BSP_BOOTLOADER_H
#define BSP_BOOTLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/** @brief ROM bootloader base (STM32H74x/H75x, AN2606). */
#define BSP_SYSTEM_BOOTLOADER_ADDR  0x1FF09800UL

/** @brief Request marker kept in reset-surviving RAM. */
#define BSP_BOOTLOADER_MAGIC        0xB0070ADEUL

/**
 * @brief  Called from the startup code before main().  If a bootloader
 *         request is pending, clears it and jumps to the ROM bootloader
 *         (does not return in that case).
 */
void bsp_bootloader_check(void);

/**
 * @brief  Request the ROM bootloader and reset the chip (privileged).
 * @note   Does not return on target.  Use sys_enter_bootloader() from
 *         unprivileged code.
 */
void __sys_enter_bootloader(void);

/**
 * @brief  Request the ROM bootloader from any task (SVC gate).
 * @note   Does not return on target.  Under HOST_TEST it only records the
 *         request (see __bootloader_host_requested()).
 */
void sys_enter_bootloader(void);

#ifdef HOST_TEST
/** @brief Test hook: true once sys_enter_bootloader() was called. */
bool __bootloader_host_requested(void);
/** @brief Test hook: clear the recorded request. */
void __bootloader_host_reset(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* BSP_BOOTLOADER_H */
