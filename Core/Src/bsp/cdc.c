/**
 * @file    cdc.c
 * @brief   USB CDC transmit ring and raw write helper implementation
 *
 * @details See bsp/cdc.h.  One byte ring in privileged DTCM carries every
 *          byte sent to the CDC IN endpoint:
 *
 *              tail                              head
 *               | in flight |   queued   |  free  |
 *
 *          - `tx_count` bytes starting at `tx_tail` are queued; the first
 *            `tx_inflight` of them belong to the USB stack (0 = idle).
 *          - Producers copy at `tx_head` (wrapping) and never touch the
 *            in-flight bytes, which the USB interrupt reads after
 *            CDC_Transmit_FS() has returned.
 *          - Every access runs with PRIMASK set (saved and restored, so it
 *            nests), which makes the idle check and the transfer start one
 *            step with respect to the USB interrupt and to other producers.
 *            The masked window is one copy of at most the free ring space
 *            plus one transfer start.  Nothing in this file waits.
 *
 *          The USB stack reads the ring from its interrupt with the CPU
 *          (the OTG FS core runs without DMA), so privileged DTCM is a valid
 *          transmit buffer.
 *
 *          Under HOST_TEST the ring code is unchanged; transfers go to the
 *          mocked CDC_Transmit_FS and the transmitted bytes are captured for
 *          the `__cdc_host_*` hooks.
 *
 * @author  Souham Biswas
 * @date    2026
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            https://github.com/ironhide23586/icarus-os-core
 *            Licensed under the Apache License, Version 2.0
 */

#include "bsp/cdc.h"

#include <stddef.h>
#include <string.h>

#include "icarus/config.h"   /* ITCM_FUNC, DTCM_DATA_PRIV */
#include "icarus/svc.h"      /* SVC_CDC_TX_WRITE, svc_caller_is_privileged */
#include "usbd_cdc_if.h"     /* CDC_Transmit_FS (mocked under HOST_TEST) */

#ifndef HOST_TEST
#include "stm32h7xx.h"       /* PRIMASK intrinsics */
#endif

#ifndef SKIP_STATIC_ASSERTS
_Static_assert((CDC_TX_MAX_CHUNK > 0u) && (CDC_TX_MAX_CHUNK <= 0xFFFFu),
               "CDC_TX_MAX_CHUNK must fit a USB transfer length (uint16_t)");
_Static_assert(CDC_TX_RING_SIZE >= CDC_TX_MAX_CHUNK,
               "CDC_TX_RING_SIZE must hold at least one full transfer");
#endif

/* ============================================================================
 * RING STATE (DTCM_PRIV — privileged code only)
 * ========================================================================= */

DTCM_DATA_PRIV static uint8_t  tx_ring[CDC_TX_RING_SIZE];
DTCM_DATA_PRIV static uint32_t tx_head;      /**< Next free byte.                */
DTCM_DATA_PRIV static uint32_t tx_tail;      /**< Oldest queued byte.            */
DTCM_DATA_PRIV static uint32_t tx_count;     /**< Queued bytes, incl. in flight. */
DTCM_DATA_PRIV static uint32_t tx_inflight;  /**< Bytes owned by USB (0 = idle). */
DTCM_DATA_PRIV static uint32_t tx_dropped;   /**< Bytes refused or dropped.      */

#ifdef HOST_TEST
/* ---- Host capture and simulation state ---------------------------------- */

#define CDC_HOST_SINK_SIZE  8192u

static uint8_t  g_cdc_sink[CDC_HOST_SINK_SIZE];
static uint16_t g_cdc_sink_len;
static uint32_t g_cdc_call_count;
static bool     g_cdc_force_fail;
static bool     g_cdc_configured    = true;
static bool     g_cdc_auto_complete = true;
static uint32_t g_cdc_chunks;
static uint16_t g_cdc_last_chunk;
#endif

/* ============================================================================
 * INTERNAL HELPERS
 * ========================================================================= */

/**
 * @brief  Mask interrupts and return the previous PRIMASK.
 * @return Value to pass to tx_unlock().
 */
static inline uint32_t tx_lock(void) {
#ifndef HOST_TEST
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
#else
    return 0u;
#endif
}

/**
 * @brief  Restore the PRIMASK saved by tx_lock().
 * @param[in] primask  Value returned by tx_lock().
 */
static inline void tx_unlock(uint32_t primask) {
#ifndef HOST_TEST
    __set_PRIMASK(primask);
#else
    (void)primask;
#endif
}

/**
 * @brief  Hand one contiguous run of the ring to the USB stack.
 * @param[in] buf  First byte (inside the ring; must stay put until the
 *                 transfer completes).
 * @param[in] len  Byte count.
 * @return USBD_OK if the transfer started, otherwise USBD_BUSY/USBD_FAIL.
 */
static uint8_t tx_hw_start(uint8_t *buf, uint16_t len) {
#ifndef HOST_TEST
    return CDC_Transmit_FS(buf, len);
#else
    if (!g_cdc_configured) {
        return (uint8_t)USBD_FAIL;
    }
    uint8_t result = (uint8_t)CDC_Transmit_FS(buf, len);
    if (result == (uint8_t)USBD_OK) {
        uint16_t room = (uint16_t)(CDC_HOST_SINK_SIZE - g_cdc_sink_len);
        uint16_t n    = (len < room) ? len : room;
        (void)memcpy(&g_cdc_sink[g_cdc_sink_len], buf, n);
        g_cdc_sink_len   = (uint16_t)(g_cdc_sink_len + n);
        g_cdc_chunks++;
        g_cdc_last_chunk = len;
    }
    return result;
#endif
}

/**
 * @brief  Start a transfer when the ring is idle and holds data.
 * @details Sends the largest contiguous run at the tail, capped at
 *          @ref CDC_TX_MAX_CHUNK.  If the stack refuses (device not
 *          configured, or the endpoint was busy with a foreign transfer)
 *          the ring stays idle and the data stays queued for the next try.
 * @note   Caller holds tx_lock().
 */
ITCM_FUNC static void tx_start_locked(void) {
    if ((tx_inflight != 0u) || (tx_count == 0u)) {
        return;
    }
    uint32_t run = (uint32_t)CDC_TX_RING_SIZE - tx_tail;   /* to ring end */
    if (run > tx_count) {
        run = tx_count;
    }
    if (run > (uint32_t)CDC_TX_MAX_CHUNK) {
        run = (uint32_t)CDC_TX_MAX_CHUNK;
    }
    tx_inflight = run;
    if (tx_hw_start(&tx_ring[tx_tail], (uint16_t)run) != (uint8_t)USBD_OK) {
        tx_inflight = 0u;
    }
}

#ifdef HOST_TEST
/**
 * @brief  Host stand-in for the transfer-complete interrupt: with automatic
 *         completion, finish every transfer the last call started.
 */
static void tx_host_auto_drain(void) {
    if (!g_cdc_auto_complete) {
        return;
    }
    while (tx_inflight != 0u) {
        __cdc_tx_on_complete();
    }
}
#endif

/* ============================================================================
 * PRIVILEGED IMPLEMENTATIONS
 * ========================================================================= */

/** @copydoc __cdc_tx_write */
ITCM_FUNC uint16_t __cdc_tx_write(const uint8_t *data, uint16_t len,
                                  bool whole) {
    if ((data == NULL) || (len == 0u)) {
        return 0u;
    }

    uint32_t key   = tx_lock();
    uint32_t space = (uint32_t)CDC_TX_RING_SIZE - tx_count;
    uint32_t n     = (uint32_t)len;
    if (n > space) {
        n = whole ? 0u : space;
    }
    if (n > 0u) {
        uint32_t first = (uint32_t)CDC_TX_RING_SIZE - tx_head;
        if (first > n) {
            first = n;
        }
        (void)memcpy(&tx_ring[tx_head], data, first);
        (void)memcpy(&tx_ring[0], &data[first], n - first);
        tx_head   = (tx_head + n) % (uint32_t)CDC_TX_RING_SIZE;
        tx_count += n;
    }
    tx_dropped += (uint32_t)len - n;
    /* Also retries data left queued while the device was unconfigured. */
    tx_start_locked();
    tx_unlock(key);

#ifdef HOST_TEST
    tx_host_auto_drain();
#endif
    return (uint16_t)n;
}

/** @copydoc __cdc_tx_on_complete */
ITCM_FUNC void __cdc_tx_on_complete(void) {
    uint32_t key = tx_lock();
    if (tx_inflight != 0u) {
        tx_tail      = (tx_tail + tx_inflight) % (uint32_t)CDC_TX_RING_SIZE;
        tx_count    -= tx_inflight;
        tx_inflight  = 0u;
    }
    tx_start_locked();
    tx_unlock(key);
}

/** @copydoc __cdc_tx_on_link_reset */
ITCM_FUNC void __cdc_tx_on_link_reset(void) {
    uint32_t key = tx_lock();
    /* The reset discarded the transfer; its bytes are still at the tail
     * and go out again with the next transfer. */
    tx_inflight = 0u;
    tx_unlock(key);
}

/** @copydoc __cdc_tx_kick */
ITCM_FUNC void __cdc_tx_kick(void) {
    uint32_t key = tx_lock();
    tx_start_locked();
    tx_unlock(key);

#ifdef HOST_TEST
    tx_host_auto_drain();
#endif
}

/** @copydoc __cdc_tx_queued */
uint32_t __cdc_tx_queued(void) {
    return tx_count;
}

/** @copydoc __cdc_tx_dropped */
uint32_t __cdc_tx_dropped(void) {
    return tx_dropped;
}

/* ============================================================================
 * PUBLIC API
 * ========================================================================= */

/** @copydoc cdc_tx_write */
uint16_t cdc_tx_write(const uint8_t *data, uint16_t len, bool whole) {
#ifndef HOST_TEST
    /* Handler mode must not issue an SVC; privileged code need not. */
    if (svc_caller_is_privileged()) {
        return __cdc_tx_write(data, len, whole);
    }
    uint32_t result;
    __asm__ volatile (
        "mov r0, %1\n"
        "mov r1, %2\n"
        "mov r2, %3\n"
        "svc %4\n"
        "mov %0, r0\n"
        : "=r" (result)
        : "r" ((uint32_t)(uintptr_t)data), "r" ((uint32_t)len),
          "r" (whole ? 1u : 0u), "I" (SVC_CDC_TX_WRITE)
        : "r0", "r1", "r2", "memory"
    );
    return (uint16_t)result;
#else
    /* Legal from any context on target (no SVC from handler mode), so no
     * nested-SVC host gate here. */
    return __cdc_tx_write(data, len, whole);
#endif
}

/** @copydoc CDC_Write */
bool CDC_Write(const uint8_t *data, uint16_t len) {
#ifdef HOST_TEST
    g_cdc_call_count++;
    if (g_cdc_force_fail) {
        return false;
    }
#endif
    if (len == 0u) {
        return true;
    }
    if (data == NULL) {
        return false;
    }
    return cdc_tx_write(data, len, true) == len;
}

/** @copydoc CDC_WriteString */
bool CDC_WriteString(const char *s) {
    if (s == NULL) {
        return true;
    }
    return CDC_Write((const uint8_t *)s, (uint16_t)strlen(s));
}

#ifdef HOST_TEST
/* ============================================================================
 * TEST-ONLY HOST HOOKS
 * ========================================================================= */

const uint8_t *__cdc_host_sink(void) {
    return g_cdc_sink;
}

uint16_t __cdc_host_sink_len(void) {
    return g_cdc_sink_len;
}

uint32_t __cdc_host_call_count(void) {
    return g_cdc_call_count;
}

void __cdc_host_set_fail(bool fail) {
    g_cdc_force_fail = fail;
}

void __cdc_host_reset_state(void) {
    (void)memset(tx_ring, 0, sizeof(tx_ring));
    tx_head     = 0u;
    tx_tail     = 0u;
    tx_count    = 0u;
    tx_inflight = 0u;
    tx_dropped  = 0u;

    (void)memset(g_cdc_sink, 0, sizeof(g_cdc_sink));
    g_cdc_sink_len      = 0u;
    g_cdc_call_count    = 0u;
    g_cdc_force_fail    = false;
    g_cdc_configured    = true;
    g_cdc_auto_complete = true;
    g_cdc_chunks        = 0u;
    g_cdc_last_chunk    = 0u;
}

void __cdc_host_set_configured(bool configured) {
    g_cdc_configured = configured;
}

void __cdc_host_set_auto_complete(bool automatic) {
    g_cdc_auto_complete = automatic;
}

bool __cdc_host_complete(void) {
    if (tx_inflight == 0u) {
        return false;
    }
    __cdc_tx_on_complete();
    return true;
}

uint32_t __cdc_host_chunk_count(void) {
    return g_cdc_chunks;
}

uint16_t __cdc_host_last_chunk_len(void) {
    return g_cdc_last_chunk;
}

uint16_t __cdc_host_inflight(void) {
    return (uint16_t)tx_inflight;
}
#endif /* HOST_TEST */
