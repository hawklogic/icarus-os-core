/**
 * @file    kernel.h
 * @brief   ICARUS Kernel Core - State and Initialization
 * @version 0.1.0
 *
 * @details Core kernel state variables, data structures, and initialization.
 *          This is the foundation layer that other kernel modules depend on.
 *
 * @author  Souham Biswas
 * @date    2025
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            https://github.com/ironhide23586/icarus-os-core
 *            Licensed under the Apache License, Version 2.0
 */

#ifndef ICARUS_KERNEL_H
#define ICARUS_KERNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "bsp/retarget_hal.h"
#include "icarus/config.h"
#include "icarus/types.h"

/* ============================================================================
 * KERNEL STATE (extern declarations)
 * ========================================================================= */

/** @brief Array of pointers to all task control blocks */
extern icarus_task_t* task_list[ICARUS_MAX_TASKS];

/** @brief Array of pointers to all semaphores */
extern icarus_semaphore_t* semaphore_list[ICARUS_MAX_SEMAPHORES];

/** @brief Array of pointers to all message pipes */
extern icarus_pipe_t* message_pipe_list[ICARUS_MAX_MESSAGE_QUEUES];

/** @brief Index of currently executing task */
extern uint8_t current_task_index;

/** @brief Count of tasks in active states */
extern uint8_t running_task_count;

/** @brief Total number of created tasks */
extern uint8_t num_created_tasks;

/** @brief Ticks remaining in current task's time slice */
extern volatile uint32_t current_task_ticks_remaining;

/** @brief Configured ticks per time slice */
extern volatile uint32_t ticks_per_task;

/** @brief System tick counter */
extern volatile uint32_t os_tick_count;

/** @brief Flag indicating OS is running */
extern volatile uint8_t os_running;

/** @brief Scheduler enable flag */
extern volatile bool scheduler_enabled;

/** @brief Cleanup task index queue */
extern int8_t cleanup_task_idx[ICARUS_MAX_TASKS];

/** @brief Current cleanup queue write index */
extern int8_t current_cleanup_task_idx;

/* ============================================================================
 * KERNEL INITIALIZATION
 * ========================================================================= */

/**
 * @brief Initialize the ICARUS kernel
 *
 * @details Must be called before any other kernel function. Initializes:
 *          - Task pool and task list
 *          - Semaphore pool
 *          - Message pipe pool
 *          - System tasks (idle, heartbeat)
 *
 * @pre     HAL must be initialized (hal_init() called)
 * @post    Kernel is ready for task registration
 */
void os_init(void);

/**
 * @brief Start the ICARUS scheduler
 *
 * @details Begins task execution. This function does not return.
 *
 * @pre     os_init() must have been called
 * @pre     At least one task must be registered
 *
 * @warning This function never returns!
 */
void os_start(void);

/* Forward declarations for scheduler functions used by kernel */
void os_yield(void);
uint32_t task_active_sleep(uint32_t ticks);
const char* os_get_current_task_name(void);

/* ============================================================================
 * CRITICAL SECTION API
 * ========================================================================= */

/**
 * @brief Enter critical section (disable scheduler)
 * @note  Supports nesting
 */
void enter_critical(void);

/**
 * @brief Exit critical section (re-enable scheduler if outermost)
 */
void exit_critical(void);

/**
 * @brief Get stack pointer for task index (internal use)
 * @param task_idx Task index
 * @return Pointer to stack memory
 */
uint32_t* kernel_get_stack(uint8_t task_idx);

/**
 * @brief Get data pointer for task index (internal use)
 * @param task_idx Task index
 * @return Pointer to data memory
 */
uint32_t* kernel_get_data(uint8_t task_idx);


void* kernel_protected_data(uint16_t num_words);

/* ============================================================================
 * BACKUP SRAM GATES
 * ========================================================================= */

/**
 * @brief  Copy data into the backup SRAM (4 KB at 0x38800000) via SVC.
 *
 * @details On target this issues an SVC that runs a validated memcpy in
 *          privileged mode, allowing unprivileged tasks to persist data
 *          without an MPU grant.  The region is mapped non-cacheable, so
 *          the data is in the SRAM when the call returns and survives a
 *          system or watchdog reset (and power loss while VBAT is held).
 *          Under HOST_TEST the offset/length check is the same, but host
 *          addresses are not target addresses, so the source buffer is only
 *          checked for NULL and wrap-around; the data goes to a host buffer
 *          that survives simulated resets.
 *
 * @param[in] src     Source buffer.  Must lie in memory the caller may pass
 *                    to the kernel for reading (see svc_buffer_allowed():
 *                    RAM_D1, the application DTCM half, the caller's own
 *                    data-pool slot, internal flash or ITCM).  From a task
 *                    it must not overlap the main stack at the top of
 *                    RAM_D1.
 * @param[in] offset  Byte offset into backup SRAM (0 .. BSP_BKPSRAM_SIZE-1).
 * @param[in] len     Number of bytes to copy (must be > 0).
 *
 * @retval true   Write completed successfully.
 * @retval false  Validation failed (range outside backup SRAM, len == 0,
 *                or a disallowed source buffer).
 */
bool bkpram_write(const void *src, uint32_t offset, uint32_t len);

/**
 * @brief  Copy data out of the backup SRAM via SVC.
 *
 * @param[out] dst     Destination buffer.  Must be writable by the caller
 *                     (RAM_D1, the application DTCM half or the caller's
 *                     own data-pool slot; see svc_buffer_allowed()).  From
 *                     a task it must not overlap the main stack at the top
 *                     of RAM_D1; boot code on the main stack may read into
 *                     its own locals.
 * @param[in]  offset  Byte offset into backup SRAM.
 * @param[in]  len     Number of bytes to copy (must be > 0).
 *
 * @retval true   Read completed successfully.
 * @retval false  Validation failed (range outside backup SRAM, len == 0,
 *                or a disallowed destination buffer); @p dst is untouched.
 */
bool bkpram_read(void *dst, uint32_t offset, uint32_t len);

#ifdef HOST_TEST
/** @brief Test hook: zero the host backup-SRAM store (simulated power loss). */
void __bkpram_host_clear(void);
#endif

/**
 * @brief  Number of MemManage faults recovered since boot (unprivileged
 *         accesses the MPU rejected; the faulting instruction was skipped).
 */
uint32_t os_get_memmanage_fault_count(void);

/* ============================================================================
 * PRIVILEGED IMPLEMENTATIONS (Internal - Do Not Call Directly)
 * ========================================================================= */

void __enter_critical(void);
void __exit_critical(void);
void __os_init(void);
void __os_start(void);
void* __kernel_protected_data(uint16_t num_words);
uint32_t* __kernel_get_stack(uint8_t task_idx);
uint32_t* __kernel_get_data(uint8_t task_idx);
void __kernel_current_data_slot(uintptr_t *base, uint32_t *size);

#ifdef __cplusplus
}
#endif

#endif /* ICARUS_KERNEL_H */
