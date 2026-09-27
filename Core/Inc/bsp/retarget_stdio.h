/**
 * @file    retarget_stdio.h
 * @brief   printf retarget to USB CDC — non-blocking console output
 *
 * @details __io_putchar() buffers characters and flushes a line (or a full
 *          buffer) to the USB CDC endpoint.  Output never blocks for long:
 *          - device not configured by a host → the line is dropped at once;
 *          - endpoint busy and the host has the port open (DTR asserted) →
 *            a short bounded retry, then the line is dropped;
 *          - endpoint busy and nobody listening (DTR clear) → dropped at once.
 *          Dropped bytes are counted so the application can report them.
 *
 *          The retry loop is a plain bounded spin: it issues no supervisor
 *          calls and never disables the scheduler, so console output cannot
 *          starve other tasks or the watchdog when no host is reading.
 *
 * @author  Souham Biswas
 * @date    2026
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            https://github.com/ironhide23586/icarus-os-core
 *            Licensed under the Apache License, Version 2.0
 */

#ifndef BSP_RETARGET_STDIO_H
#define BSP_RETARGET_STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** @brief Console line buffer size (bytes). */
#define STDIO_LINE_BUF_SIZE      64u

/** @brief Transmit attempts after the first, while DTR is asserted. */
#define STDIO_TX_RETRY_MAX       10u

/** @brief Spin iterations between attempts (~0.2–0.3 ms at 480 MHz). */
#define STDIO_TX_RETRY_SPIN      20000u

/**
 * @brief  Retarget hook used by newlib's printf.
 * @param  ch  Character to output.
 * @return The character written.
 */
int __io_putchar(int ch);

/**
 * @brief  Total console bytes dropped because the host was not reading.
 * @return Byte count since boot (wraps at 2^32).
 */
uint32_t stdio_get_tx_dropped(void);

#ifdef HOST_TEST
/** @brief Test hook: discard any partially buffered line and the counter. */
void __stdio_host_reset(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* BSP_RETARGET_STDIO_H */
