/**
 * @file    retarget_stdio.c
 * @brief   printf retarget to USB CDC — non-blocking console output
 *
 * @details See retarget_stdio.h.  This file keeps no buffer of its own:
 *          every byte is copied straight into the privileged USB CDC
 *          transmit ring by cdc_tx_write(), which is safe from any context
 *          (an SVC from tasks, a direct call from privileged code and
 *          handlers).  Partial writes are accepted, and whatever did not fit
 *          is added to a drop counter.  The counter lives in ordinary RAM so
 *          unprivileged code can read it, and is updated with an atomic add
 *          because tasks that print can preempt each other.
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
#include <stddef.h>

#include "bsp/cdc.h"

/** @brief Console bytes dropped since boot (plain RAM: readable by tasks). */
static uint32_t stdio_dropped;

/** @copydoc stdio_write */
uint32_t stdio_write(const uint8_t *data, uint32_t len) {
    if ((data == NULL) || (len == 0u)) {
        return 0u;
    }
    uint32_t done = 0u;
    while (done < len) {
        uint32_t chunk = len - done;
        if (chunk > 0xFFFFu) {
            chunk = 0xFFFFu;
        }
        uint16_t n = cdc_tx_write(&data[done], (uint16_t)chunk, false);
        done += n;
        if (n < chunk) {
            break;                                  /* ring full */
        }
    }
    if (done < len) {
        (void)__atomic_fetch_add(&stdio_dropped, len - done,
                                 __ATOMIC_RELAXED);
    }
    return done;
}

int __io_putchar(int ch) {
    uint8_t byte = (uint8_t)ch;
    (void)stdio_write(&byte, 1u);
    return ch;
}

#ifndef HOST_TEST
/* Overrides the weak newlib stub in syscalls.c, which wrote one character
 * at a time. */
int _write(int file, char *ptr, int len) {
    (void)file;
    if ((ptr != NULL) && (len > 0)) {
        (void)stdio_write((const uint8_t *)ptr, (uint32_t)len);
    }
    return len;
}
#endif

uint32_t stdio_get_tx_dropped(void) {
    return __atomic_load_n(&stdio_dropped, __ATOMIC_RELAXED);
}

#ifdef HOST_TEST
void __stdio_host_reset(void) {
    stdio_dropped = 0u;
}
#endif
