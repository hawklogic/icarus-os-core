/**
 * @file    retarget_stdio.h
 * @brief   printf retarget to USB CDC — non-blocking console output
 *
 * @details The strong `_write()` below copies newlib's console output into
 *          the USB CDC transmit ring (`bsp/cdc.h`), the same ring that
 *          carries raw `CDC_Write()` traffic, so text and binary output keep
 *          their order.  Output never blocks, sleeps or spins: bytes that do
 *          not fit in the ring (host not reading, or device not configured
 *          for long enough to fill it) are dropped and counted so the
 *          application can report them.
 *
 *          Concurrency:
 *          - Each `_write()` / `stdio_write()` call is atomic: its bytes
 *            are copied into the ring in one step with interrupts masked,
 *            so no other producer lands in the middle of them.  Both are
 *            safe from tasks and from interrupt handlers that PRIMASK masks
 *            (every configurable-priority handler; not NMI or HardFault).
 *          - `printf()` and the other stdio functions are not.  newlib-nano
 *            formats into the stdout `FILE` buffer, which all callers share,
 *            and its lock hooks (`__retarget_lock_*`) are the library's
 *            no-op stubs in this build, so nothing locks that buffer.
 *            Concurrent `printf()` from preemptive tasks (or from a task
 *            and an interrupt handler) can interleave or duplicate
 *            characters.  Callers that need whole lines must serialise
 *            their `printf()` calls, for example inside a critical section
 *            or from a single task, or format into their own buffer and
 *            call `stdio_write()`.
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
 * @note   Atomic with respect to other producers (one masked copy into the
 *         ring for up to 65535 bytes); callable from tasks and
 *         configurable-priority handlers.
 *         From an unprivileged task, @p data must be a buffer the task may
 *         pass to the kernel (see svc_buffer_allowed()); otherwise nothing
 *         is queued and all @p len bytes count as dropped.
 */
uint32_t stdio_write(const uint8_t *data, uint32_t len);

/**
 * @brief  Legacy single-character entry for code that calls it directly.
 * @details newlib does not use it: its output reaches the ring only
 *          through the strong `_write()` (which replaces the weak
 *          syscalls.c version that looped over this function).
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
 * @note   Atomic per call, but see the file description: the stdout `FILE`
 *         buffer in front of it is not locked.
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
