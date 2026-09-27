/**
 * @file    retarget_stdio.h
 * @brief   printf retarget to USB CDC — non-blocking console output
 *
 * @details newlib's `_write()` (and `__io_putchar()` for single characters)
 *          copy console output into the USB CDC transmit ring
 *          (`bsp/cdc.h`), the same ring that carries raw `CDC_Write()`
 *          traffic, so text and binary output keep their order.  Output never
 *          blocks, sleeps or spins and is safe from any task or handler:
 *          bytes that do not fit in the ring (host not reading, or device
 *          not configured for long enough to fill it) are dropped and
 *          counted so the application can report them.
 *
 *          Output is not line-buffered here: newlib's own stdout buffering
 *          decides when `_write()` is called.
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

/**
 * @brief  Queue console bytes (the path `_write()` and `__io_putchar()` use).
 * @param[in] data  Bytes to send.
 * @param[in] len   Byte count.
 * @return Bytes queued; the rest were dropped and added to
 *         stdio_get_tx_dropped().
 */
uint32_t stdio_write(const uint8_t *data, uint32_t len);

/**
 * @brief  Retarget hook used by newlib for single characters.
 * @param  ch  Character to output.
 * @return The character written (also when it was dropped).
 */
int __io_putchar(int ch);

#ifndef HOST_TEST
/**
 * @brief  newlib system call behind printf()/puts()/fwrite(stdout).
 * @param  file  File descriptor (ignored: all output goes to USB CDC).
 * @param  ptr   Bytes to write.
 * @param  len   Byte count.
 * @return @p len — dropped bytes are counted, not reported, so newlib never
 *         retries or marks stdout as failed.
 */
int _write(int file, char *ptr, int len);
#endif

/**
 * @brief  Total console bytes dropped because the transmit ring was full.
 * @return Byte count since boot (wraps at 2^32).
 */
uint32_t stdio_get_tx_dropped(void);

#ifdef HOST_TEST
/** @brief Test hook: clear the dropped-byte counter. */
void __stdio_host_reset(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* BSP_RETARGET_STDIO_H */
