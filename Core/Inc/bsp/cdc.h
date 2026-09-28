/**
 * @file    bsp_cdc.h
 * @brief   Board Support Package — USB CDC transmit ring and raw write helper
 *
 * @details Every byte sent to the USB CDC IN endpoint — `printf()` output
 *          (`bsp/retarget_stdio.c`), raw writes (`CDC_Write`) and any other
 *          producer — goes through one byte ring owned by privileged code.
 *          Producers copy their bytes into the ring and return at once: the
 *          ring never blocks, sleeps or spins, so it is safe from any task
 *          and from any configurable-priority interrupt or exception handler
 *          (not NMI or HardFault, which the ring's PRIMASK lock does not
 *          mask), and a host that holds the port open without reading
 *          cannot stall the caller.
 *
 *          Transmission is driven from the ring: when the endpoint is idle
 *          the largest contiguous run at the tail (at most
 *          @ref CDC_TX_MAX_CHUNK bytes) is handed to the USB stack in place.
 *          Those bytes stay in the ring until the transfer-complete
 *          interrupt advances the tail and starts the next run.  The
 *          idle-check-and-start runs with interrupts masked, so producers in
 *          different tasks and the USB interrupt cannot both start a
 *          transfer or overwrite bytes the USB stack is still reading.
 *
 *          While the device is not configured or the bus is suspended
 *          nothing can be sent: bytes stay queued (until the ring is full)
 *          and the next write, the host opening the port, the bus resuming,
 *          or a transfer completion retries.  A USB reset or
 *          re-configuration clears the in-flight state, so a transfer lost
 *          to the reset cannot wedge the ring; its bytes are sent again.
 *
 *          A host that closes the port stops reading, so output written
 *          while it is closed would otherwise reach the next open as if it
 *          were new.  When DTR goes from set to clear (close), and when it is
 *          set again after an earlier open (reopen), the bytes queued behind
 *          the transfer in flight are discarded and counted as dropped.  A
 *          transfer already in flight at the close cannot be recalled and may
 *          still arrive after the reopen (at most CDC_TX_MAX_CHUNK bytes).
 *          Output queued before the first open since boot is kept and sent
 *          when the port opens.
 *
 *          Access paths:
 *          - unprivileged tasks: `cdc_tx_write()` / `CDC_Write()` issue
 *            `SVC_CDC_TX_WRITE`, which validates the buffer before copying;
 *          - privileged thread code, SVC implementations and interrupt
 *            handlers: the same functions call `__cdc_tx_write()` directly;
 *          - the USB CDC interface callbacks call `__cdc_tx_on_complete()`,
 *            `__cdc_tx_on_link_reset()`, `__cdc_tx_on_dtr()` and
 *            `__cdc_tx_kick()`; the USB resume callback restarts the PHY
 *            clock stopped at suspend and calls `__cdc_tx_kick()`.
 *
 *          Under HOST_TEST the same ring logic runs against the mocked
 *          `CDC_Transmit_FS`.  Transfers complete immediately unless a test
 *          selects manual completion (see the `__cdc_host_*` hooks).
 *
 * @author  Souham Biswas
 * @date    2026
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            https://github.com/ironhide23586/icarus-os-core
 *            Licensed under the Apache License, Version 2.0
 */

#ifndef BSP_CDC_H
#define BSP_CDC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Transmit ring size in bytes (shared by all USB CDC output).
 * @details The ring lives in privileged DTCM.  Bytes that do not fit are
 *          dropped (partial writes) or refused (all-or-nothing writes).
 */
#ifndef CDC_TX_RING_SIZE
#define CDC_TX_RING_SIZE        4096u
#endif

/**
 * @brief  Largest single USB transfer started from the ring (bytes).
 * @details Bounds how long one transfer holds ring space.  The CDC class
 *          driver appends a zero-length packet when a transfer is a
 *          multiple of the endpoint packet size, so any length is valid.
 */
#ifndef CDC_TX_MAX_CHUNK
#define CDC_TX_MAX_CHUNK        2048u
#endif

/**
 * @brief  Queue bytes for the USB CDC IN endpoint.
 *
 * @param  data   Bytes to send (copied into the ring before returning).
 * @param  len    Number of bytes.
 * @param  whole  true: all-or-nothing — nothing is queued unless all
 *                @p len bytes fit.  false: queue as many bytes as fit and
 *                drop the rest.
 *
 * @return Number of bytes queued: @p len on success, 0 when @p whole is
 *         set and the bytes do not fit, otherwise the bytes that fitted.
 *         0 if @p data is NULL or, from an unprivileged task, not a buffer
 *         the task may pass to the kernel (see svc_buffer_allowed()).
 *
 * @note   Never blocks.  Callable from tasks and from configurable-priority
 *         interrupt and exception handlers (not NMI or HardFault):
 *         unprivileged tasks go through `SVC_CDC_TX_WRITE`; privileged code
 *         and interrupt handlers call the ring directly.
 */
uint16_t cdc_tx_write(const uint8_t *data, uint16_t len, bool whole);

/**
 * @brief  Queue @p len bytes for the USB CDC IN endpoint, all or nothing.
 *
 * @param  data  Pointer to the bytes to send (must be non-NULL when
 *               @p len > 0).
 * @param  len   Number of bytes to send.
 *
 * @return `true` if all bytes were queued (or @p len is 0); `false` if the
 *         transmit ring does not have room for all of them, or, when called
 *         from an unprivileged task, @p data is not a buffer the task may
 *         pass to the kernel (see svc_buffer_allowed()).  On `false` none
 *         were queued.
 *
 * @note   Never blocks or sleeps, so a host that stops reading cannot hold
 *         up the caller; callable from any task and from configurable-
 *         priority interrupt and exception handlers (not NMI or HardFault,
 *         which the ring's PRIMASK lock does not mask).
 *
 * @note   The bytes are copied: @p data may be reused as soon as the
 *         function returns.
 */
bool CDC_Write(const uint8_t *data, uint16_t len);

/**
 * @brief  Convenience wrapper that pushes a NUL-terminated C string.
 *
 * @param  s  NUL-terminated string. NULL is treated as a no-op success.
 *
 * @return `true` on success, `false` if the underlying `CDC_Write`
 *         could not queue the whole string.
 */
bool CDC_WriteString(const char *s);

/* ============================================================================
 * PRIVILEGED IMPLEMENTATIONS (Internal - Do Not Call Directly)
 * ========================================================================= */

/**
 * @brief  Privileged implementation of cdc_tx_write().
 * @param[in] data   Bytes to queue.
 * @param[in] len    Byte count.
 * @param[in] whole  All-or-nothing when true.
 * @return Bytes queued.
 * @note   Masks interrupts briefly; starts a transfer if the endpoint is
 *         idle.  Privileged callers only.
 */
uint16_t __cdc_tx_write(const uint8_t *data, uint16_t len, bool whole);

/**
 * @brief  Transfer-complete hook: release the sent bytes, send the next run.
 * @note   Called from the USB CDC transmit-complete callback (USB ISR).
 */
void __cdc_tx_on_complete(void);

/**
 * @brief  Link reset hook: forget the transfer in flight (bytes are kept).
 * @note   Called when the CDC interface is initialised or de-initialised
 *         (USB reset, re-configuration, disconnect), so a transfer that the
 *         reset discarded cannot leave the ring marked busy forever.
 */
void __cdc_tx_on_link_reset(void);

/**
 * @brief  Start a transfer from the ring if data is queued and the ring is
 *         idle.  Safe to call at any time from privileged code.
 * @note   Called when the host changes the control lines (opening the port)
 *         and when the bus resumes from suspend, so output queued while no
 *         host was attached, or while the bus was suspended, is sent without
 *         waiting for the next write.
 */
void __cdc_tx_kick(void);

/**
 * @brief  Discard the queued bytes that are not in flight.
 * @details The transfer in flight (if any) is left alone: the USB stack is
 *          still reading it from the ring and its completion releases it as
 *          usual.  Everything queued behind it is dropped and added to the
 *          count returned by __cdc_tx_dropped().
 * @return Number of bytes discarded.
 * @note   Used by __cdc_tx_on_dtr().  Privileged callers only.
 */
uint32_t __cdc_tx_discard_queued(void);

/**
 * @brief  Port open/close hook: track DTR and drop output nobody read.
 * @param[in] asserted  DTR as just reported by the host (true = open).
 * @details On a change of DTR:
 *          - set to clear (the host closed the port): discard the queued
 *            bytes that are not in flight;
 *          - clear to set after an earlier open since boot (reopen): discard
 *            them again, since they were written while the port was closed;
 *          - clear to set for the first time: keep them (output from before
 *            the first open, such as a boot banner, is still sent).
 *          A report that does not change DTR does nothing.  A transfer in
 *          flight is never discarded, so up to CDC_TX_MAX_CHUNK bytes written
 *          before a close can still arrive after the reopen.
 * @return Number of bytes discarded.
 * @note   Does not start a transfer: the caller kicks the ring afterwards.
 *         Called from the CDC control request handler (DTR changes) and the
 *         CDC de-initialisation callback (link lost, reported as clear), both
 *         in the USB interrupt.  Privileged callers only.
 */
uint32_t __cdc_tx_on_dtr(bool asserted);

/**
 * @brief  Bytes currently queued (including a transfer in flight).
 * @note   Privileged callers only (reads privileged DTCM).
 */
uint32_t __cdc_tx_queued(void);

/**
 * @brief  Total bytes refused or dropped by the ring since boot, including
 *         bytes discarded when the port closes or reopens.
 * @note   Privileged callers only (reads privileged DTCM).
 */
uint32_t __cdc_tx_dropped(void);

#ifdef HOST_TEST
/* ============================================================================
 * HOST TEST HOOKS
 * ========================================================================= */

/** @brief Bytes transmitted since the last reset, in transfer order. */
const uint8_t *__cdc_host_sink(void);
/** @brief Number of bytes in __cdc_host_sink() (capped at its capacity). */
uint16_t __cdc_host_sink_len(void);
/** @brief Number of CDC_Write() calls since the last reset. */
uint32_t __cdc_host_call_count(void);
/** @brief Force CDC_Write() to fail without queueing (as if full). */
void __cdc_host_set_fail(bool fail);
/**
 * @brief  Reset the ring, the capture and all hooks: configured, automatic
 *         completion, no forced failure.
 */
void __cdc_host_reset_state(void);
/** @brief Simulate a configured (true) or unconfigured (false) device. */
void __cdc_host_set_configured(bool configured);
/**
 * @brief  Select automatic completion (default: every started transfer
 *         completes at once) or manual completion via __cdc_host_complete().
 */
void __cdc_host_set_auto_complete(bool automatic);
/**
 * @brief  Fire the transfer-complete interrupt once.
 * @retval true   A transfer was in flight and has completed.
 * @retval false  Nothing was in flight.
 */
bool __cdc_host_complete(void);
/** @brief Number of transfers started since the last reset. */
uint32_t __cdc_host_chunk_count(void);
/** @brief Length of the most recently started transfer. */
uint16_t __cdc_host_last_chunk_len(void);
/** @brief Bytes in the transfer currently in flight (0 = idle). */
uint16_t __cdc_host_inflight(void);
#endif /* HOST_TEST */

#ifdef __cplusplus
}
#endif

#endif /* BSP_CDC_H */
