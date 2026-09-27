/**
 * @file    retarget_stdio.c
 * @brief   printf retarget to USB CDC — non-blocking console output
 *
 * @details See retarget_stdio.h.  Two line buffers alternate so the USB
 *          driver can still be reading the previous line while the next
 *          one is filled.  After a successful hand-off the other buffer is
 *          filled next; a dropped buffer was never handed over and is
 *          reused, so a buffer that may still be in flight is not
 *          overwritten.
 *
 * @author  Souham Biswas
 * @date    2026
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            https://github.com/ironhide23586/icarus-os-core
 *            Licensed under the Apache License, Version 2.0
 */

#include "bsp/retarget_stdio.h"

#include <stdbool.h>

#include "usbd_cdc_if.h"
#include "usbd_def.h"

/* Plain .bss (RAM_D1, full access) so unprivileged tasks can print. */
static uint8_t  stdio_buf[2][STDIO_LINE_BUF_SIZE];
static uint8_t  stdio_cur;       /**< Buffer currently being filled.  */
static uint8_t  stdio_len;       /**< Bytes in the current buffer.    */
static uint32_t stdio_dropped;   /**< Bytes dropped since boot.       */

/**
 * @brief  Short busy wait between transmit attempts.
 * @details Deliberately not a kernel sleep: this path must be usable
 *          before the scheduler starts and must never issue an SVC or
 *          lock the scheduler.
 */
static void stdio_spin(void) {
    for (volatile uint32_t d = 0u; d < STDIO_TX_RETRY_SPIN; d++) {
    }
}

/**
 * @brief  Hand one buffer to the USB endpoint.
 * @param[in] data  Bytes to send.
 * @param[in] len   Byte count.
 * @retval true   Transfer accepted by the USB stack.
 * @retval false  Dropped (not configured, or busy past the retry budget).
 */
static bool stdio_flush(uint8_t *data, uint8_t len) {
    uint8_t result = CDC_Transmit_FS(data, len);

    /* Only worth waiting if a host application has the port open. */
    uint32_t attempts = 0u;
    while ((result == (uint8_t)USBD_BUSY) && (CDC_IsDtrAsserted() != 0U) &&
           (attempts < STDIO_TX_RETRY_MAX)) {
        stdio_spin();
        result = CDC_Transmit_FS(data, len);
        attempts++;
    }

    if (result != (uint8_t)USBD_OK) {
        stdio_dropped += len;
        return false;
    }
    return true;
}

int __io_putchar(int ch) {
    stdio_buf[stdio_cur][stdio_len] = (uint8_t)ch;
    stdio_len++;

    if ((ch == (int)'\n') || (stdio_len >= (uint8_t)STDIO_LINE_BUF_SIZE)) {
        if (stdio_flush(stdio_buf[stdio_cur], stdio_len)) {
            /* The driver now owns this buffer until the transfer ends;
             * fill the other one next. */
            stdio_cur ^= 1u;
        }
        /* On a drop the buffer was never handed over, so reuse it. */
        stdio_len = 0u;
    }
    return ch;
}

uint32_t stdio_get_tx_dropped(void) {
    return stdio_dropped;
}

#ifdef HOST_TEST
void __stdio_host_reset(void) {
    stdio_cur     = 0u;
    stdio_len     = 0u;
    stdio_dropped = 0u;
}
#endif
