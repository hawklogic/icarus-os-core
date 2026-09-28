/**
 * @file    svc.h
 * @brief   ICARUS Supervisor Call (SVC) Definitions
 * @version 0.1.0
 *
 * @details Defines SVC numbers for MPU-protected kernel functions.
 *          These functions execute in privileged mode with controlled
 *          access to protected kernel data structures.
 *
 *          Organization:
 *          - This header defines SVC numbers
 *          - svc.c contains all SVC wrapper implementations
 *          - Implementation files contain __prefixed privileged functions
 *
 * @author  Souham Biswas
 * @date    2025
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            https://github.com/ironhide23586/icarus-os-core
 *            Licensed under the Apache License, Version 2.0
 */

#ifndef ICARUS_SVC_H
#define ICARUS_SVC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define SVC_TASK_ACTIVE_SLEEP           0
#define SVC_TASK_BLOCKING_SLEEP         1

#define SVC_ENTER_CRITICAL              2
#define SVC_EXIT_CRITICAL               3

#define SVC_PIPE_INIT                   4
#define SVC_PIPE_ENQUEUE                5
#define SVC_PIPE_DEQUEUE                6

#define SVC_OS_INIT                     7
#define SVC_OS_START                    8
#define SVC_OS_YIELD                    9
#define SVC_OS_REGISTER_TASK            10
#define SVC_OS_GET_CURRENT_TASK_NAME    11
#define SVC_OS_GET_TICK_COUNT           12
#define SVC_OS_EXIT_TASK                13
#define SVC_OS_KILL_PROCESS             14
#define SVC_OS_TASK_SUICIDE             15

#define SVC_SEMAPHORE_INIT              16
#define SVC_SEMAPHORE_CONSUME           17
#define SVC_SEMAPHORE_FEED              18
#define SVC_SEMAPHORE_GET_COUNT         19
#define SVC_SEMAPHORE_GET_MAX_COUNT     20

#define SVC_PIPE_GET_COUNT              21
#define SVC_PIPE_GET_MAX_COUNT          22

#define SVC_TASK_BUSY_WAIT              23
#define SVC_OS_GET_RUNNING_TASK_COUNT   24
#define SVC_OS_GET_TASK_TICKS_REMAINING 25

#define SVC_KERNEL_GET_STACK            26
#define SVC_KERNEL_GET_DATA             27
#define SVC_KERNEL_PROTECTED_DATA       28

/* Call gates for spinning functions — read kernel state from priv mode      */
/* so DTCM can be made priv-only without faulting the spin loops             */
#define SVC_SEM_CAN_FEED                29  /* bool: count < max && engaged  */
#define SVC_SEM_CAN_CONSUME             30  /* bool: count > 0  && engaged   */
#define SVC_PIPE_CAN_ENQUEUE            31  /* bool: free >= bytes && engaged */
#define SVC_PIPE_CAN_DEQUEUE            32  /* bool: count >= bytes && engaged*/

/* Write gates for spinning functions — modify kernel state from priv mode   */
#define SVC_SEM_INCREMENT               33  /* ++count, update tick          */
#define SVC_SEM_DECREMENT               34  /* --count, update tick          */
#define SVC_PIPE_WRITE_BYTES            35  /* write to buffer, update tick  */
#define SVC_PIPE_READ_BYTES             36  /* read from buffer, update tick */

/* Read gates for display/diagnostics — read task metadata from priv mode    */
#define SVC_GET_TASK_NAME               37  /* const char*: task_list[i]->name */
#define SVC_GET_NUM_TASKS               38  /* uint8_t: num_created_tasks    */
#define SVC_OS_IS_RUNNING               39  /* uint8_t: os_running flag      */

/* CDC RX ring buffer (data in DTCM_PRIV)                                    */
#define SVC_CDC_RX_INIT                 40  /* clear head/tail               */
#define SVC_CDC_RX_READ_BYTE            41  /* bool: pop one byte if any     */
#define SVC_CDC_RX_AVAILABLE            42  /* uint32_t: bytes available     */

/* Event ring buffer + per-module squelch (data in DTCM_PRIV)                */
#define SVC_EVENT_INIT                  43  /* clear ring + squelch          */
#define SVC_OS_EVENT                    44  /* emit one entry                */
#define SVC_EVENT_SET_SQUELCH           45  /* set per-module threshold      */
#define SVC_EVENT_GET_SQUELCH           46  /* read per-module threshold     */
#define SVC_EVENT_DRAIN                 47  /* copy entries into caller buf  */
#define SVC_EVENT_GET_COUNT             48  /* uint32_t: ring fill           */

/* Ground-loadable table engine (data in DTCM_PRIV)                          */
#define SVC_TBL_INIT                    49  /* clear registry                */
#define SVC_TBL_REGISTER                50  /* add a descriptor              */
#define SVC_TBL_LOAD                    51  /* append bytes to staging       */
#define SVC_TBL_ACTIVATE_PREPARE        52  /* validate + copy staging→tmp   */
#define SVC_TBL_ACTIVATE_COMMIT         53  /* copy tmp→active               */
#define SVC_TBL_DUMP                    54  /* copy active→caller buf        */
#define SVC_TBL_GET_DESCRIPTOR          55  /* return descriptor pointer     */
#define SVC_TBL_COUNT                   56  /* uint8_t: registered count     */

/* Task lifecycle extensions                                                 */
#define SVC_OS_RESTART_TASK             57  /* restart killed/finished task   */

/* Timed semaphore wait                                                      */
#define SVC_SEMAPHORE_CONSUME_TIMEOUT   58  /* bool: acquire with timeout    */

/* Task diagnostics                                                          */
#define SVC_GET_TASK_STATE              59  /* icarus_task_state_t           */
#define SVC_GET_TASK_DISPATCH_COUNT     60  /* uint32_t: dispatch counter    */
#define SVC_GET_STACK_WATERMARK         61  /* uint32_t: min free words      */
#define SVC_UPDATE_STACK_WATERMARK      62  /* void: scan + update           */

/* Checksum integrity monitor (data in DTCM_PRIV)                            */
#define SVC_CS_INIT                     63  /* void: clear regions           */
#define SVC_CS_SET_CALLBACK             64  /* void: set mismatch callback   */
#define SVC_CS_ADD_REGION               65  /* bool: register + baseline CRC */
#define SVC_CS_ENABLE                   66  /* bool: enable/disable region   */
#define SVC_CS_REBASELINE               67  /* bool: recompute baseline      */
#define SVC_CS_CHECK_ALL                68  /* uint8_t: scan all regions     */
#define SVC_CS_GET_REGION               69  /* bool: read region descriptor  */
#define SVC_CS_REGION_COUNT             70

/* Generic BKPRAM write gate (data in RAM_D3, priv-only by default MPU)   */
#define SVC_BKPRAM_WRITE                71  /* bool: memcpy src→BKPRAM       */

/* Software Bus (data in DTCM_PRIV)                                        */
#define SVC_SB_INIT                     72  /* void: clear routes            */
#define SVC_SB_SUBSCRIBE                73  /* bool: add (msg,pipe) binding  */
#define SVC_SB_UNSUBSCRIBE              74  /* bool: remove binding          */
#define SVC_SB_PUBLISH                  75  /* uint8_t: enqueue to all subs  */
#define SVC_SB_SUBSCRIBER_COUNT         76  /* uint8_t: count for msg_id     */
#define SVC_SB_ROUTE_COUNT              77  /* uint8_t: total route entries  */

/* Filesystem (data in .bss, accessed via critical sections)               */
#define SVC_FS_INIT                     78  /* void: clear table + data      */
#define SVC_FS_CREATE                   79  /* bool: allocate new file       */
#define SVC_FS_OPEN                     80  /* bool: open existing file      */
#define SVC_FS_WRITE                    81  /* bool: append to file          */
#define SVC_FS_READ                     82  /* uint16_t: read from offset    */
#define SVC_FS_DELETE                   83  /* bool: remove by name          */
#define SVC_FS_LIST                     84  /* uint8_t: enumerate files      */
#define SVC_FS_STATS                    85  /* void: fill stats struct       */

/* Backup SRAM read gate (data in BKPSRAM, priv-only)                      */
#define SVC_BKPRAM_READ                 86  /* bool: memcpy BKPRAM→dst       */

/* Table engine extensions                                                 */
#define SVC_TBL_GET_INFO                87  /* bool: copy descriptor out     */
#define SVC_TBL_ABORT                   88  /* bool: discard staging         */

/* CRC engine for unprivileged callers                                     */
#define SVC_CRC16_CCITT                 89  /* uint16_t: CRC over caller buf */

/* System control                                                          */
#define SVC_SYS_ENTER_BOOTLOADER        90  /* noreturn: jump to ROM loader  */

/* CDC RX diagnostics                                                      */
#define SVC_CDC_RX_DROPPED              91  /* uint32_t: bytes dropped (full)*/

/* Offset-addressed table load                                             */
#define SVC_TBL_LOAD_AT                 92  /* bool: write chunk at offset   */

/* USB CDC transmit ring (data in DTCM_PRIV)                               */
#define SVC_CDC_TX_WRITE                93  /* uint16_t: bytes queued        */

/** @brief Highest SVC number in use.  Update when adding a new SVC. */
#define SVC_MAX_NUMBER                  SVC_CDC_TX_WRITE

/* ============================================================================
 * COMPILE-TIME SVC VALIDATION
 * ========================================================================= */

/* SVC instruction encodes number in 1 byte (0-255) */
_Static_assert(SVC_MAX_NUMBER <= 255,
               "Highest SVC number must fit in 8-bit immediate");

_Static_assert((SVC_MAX_NUMBER >= SVC_FS_STATS) &&
               (SVC_MAX_NUMBER >= SVC_CDC_RX_DROPPED) &&
               (SVC_MAX_NUMBER >= SVC_TBL_LOAD_AT) &&
               (SVC_MAX_NUMBER >= SVC_CDC_TX_WRITE),
               "SVC_MAX_NUMBER must be >= all other SVC numbers");

/* ============================================================================
 * CALLER BUFFER POLICY
 * ========================================================================= */

/**
 * @brief  How a privileged SVC implementation touches a caller buffer.
 */
typedef enum {
    SVC_ACCESS_READ  = 0,   /**< Kernel reads the buffer (caller → kernel).  */
    SVC_ACCESS_WRITE = 1    /**< Kernel writes the buffer (kernel → caller). */
} svc_access_t;

/**
 * @brief  The caller of an SVC, as the buffer policy sees it.
 *
 * @details Built by the dispatcher's buffer checks from the exception frame
 *          it records on entry.  @c from_task is true when the exception
 *          frame was stacked on the process stack (a task); it is false for
 *          privileged thread code that still runs on the main stack (boot
 *          code before the scheduler starts).
 */
typedef struct {
    uintptr_t slot_base;        /**< Caller's data-pool slot start.           */
    uint32_t  slot_size;        /**< Slot size in bytes (0 = no slot).        */
    uintptr_t main_stack_base;  /**< Lowest address of the main-stack window. */
    uint32_t  main_stack_size;  /**< Window size in bytes (0 = no window).    */
    bool      from_task;        /**< Frame is on the process stack.           */
} svc_caller_t;

/**
 * @brief  Pure allowlist check for a buffer an SVC handler will touch.
 *
 * @details SVC implementations copy with privilege, so the MPU does not
 *          stop them.  Every caller-supplied pointer is therefore checked
 *          against the memory an unprivileged task may legitimately own:
 *          - write (and read): RAM_D1, the upper (application) DTCM half,
 *            and the calling task's own data-pool slot;
 *          - read only: internal flash and ITCM (constants and code).
 *          Everything else — privileged DTCM, other tasks' data-pool slots,
 *          SRAM4, backup SRAM, peripherals, the system control space and
 *          unmapped addresses — is rejected.  The whole range must lie
 *          inside one region.
 *
 *          The main stack sits at the top of RAM_D1 and holds the SVC
 *          handler's own frame (saved registers and return address) while
 *          the call runs.  For a task caller, a range that overlaps the
 *          main-stack window is rejected for reads and writes, so a task
 *          can neither redirect the privileged return nor read handler
 *          state.  Privileged code that still runs on the main stack
 *          (@c from_task false) may pass buffers there, such as locals in
 *          boot code.
 *
 *          The window is the reserved main stack, [_estack - _Min_Stack_Size,
 *          _estack).  It covers the handler's frame only while everything on
 *          the main stack stays within that reservation: the frames left
 *          by main() and os_start() (never unwound, since the first task is
 *          launched from them), the SVC handler and any interrupts nested on
 *          top of it.  An application whose main() keeps large locals must
 *          raise _Min_Stack_Size to match.  Resetting MSP to _estack at the
 *          first-task launch would remove this dependency; it is not done
 *          yet.
 *
 *          The function has no state: the caller is a parameter, so the
 *          policy can be unit-tested on the host with the target memory map.
 *
 * @param[in] addr    Buffer start address.
 * @param[in] len     Buffer length in bytes (0 is allowed: nothing is
 *                    touched).
 * @param[in] access  @ref SVC_ACCESS_READ or @ref SVC_ACCESS_WRITE.
 * @param[in] caller  The calling context (data-pool slot, main-stack window,
 *                    stack the frame is on).  Must not be NULL.
 *
 * @retval true   @p addr is non-NULL, the range does not wrap, it does not
 *                overlap the main-stack window when @p caller is a task, and
 *                it lies wholly inside one region that permits @p access.
 * @retval false  Otherwise, or @p caller is NULL.
 */
bool svc_buffer_allowed(uintptr_t addr, uint32_t len, svc_access_t access,
                        const svc_caller_t *caller);

/**
 * @brief  Whether the caller may call a privileged implementation directly.
 *
 * @details True in handler mode (exceptions, interrupts, SVC implementations)
 *          and in privileged thread mode (boot code before the scheduler
 *          drops privilege).  Wrappers that must work from any context use
 *          it to skip the SVC, which would fault from handler mode.
 *          Always true under HOST_TEST.
 *
 * @retval true   Handler mode or privileged thread mode.
 * @retval false  Unprivileged thread mode (a task): use the SVC gate.
 */
bool svc_caller_is_privileged(void);

/* ============================================================================
 * HOST-TEST NESTED-SVC GUARD
 * ========================================================================= */

#ifdef HOST_TEST
/**
 * @brief  Host-only guard that detects a nested supervisor call.
 *
 * @details On target, issuing an SVC while already executing inside the
 *          SVC handler escalates to a HardFault.  Host builds call the
 *          privileged implementations directly, so that class of bug is
 *          invisible to unit tests.  Every SVC-gated wrapper therefore
 *          opens a host "gate" on entry and closes it on return; opening a
 *          gate while another is open (for example from a callback invoked
 *          by a privileged implementation) is reported through the
 *          nesting handler.  Wrappers that run thread-mode code on target
 *          hold no gate while that code runs: spin loops gate only their
 *          individual SVC calls, and table activation gates its prepare and
 *          commit steps but not the callback between them.
 *
 * @par Usage (inside a wrapper's HOST_TEST branch):
 * @code
 *     SVC_HOST_GATE();
 *     return __impl(args);
 * @endcode
 */
typedef void (*svc_host_nesting_fn)(const char *outer, const char *inner);

/** @brief Open a gate; returns a token for the matching close. */
int  svc_host_gate_enter(const char *fn_name);
/** @brief Close a gate (called automatically via the cleanup attribute). */
void svc_host_gate_exit(int *token);
/**
 * @brief  Install a nesting handler.  NULL restores the default, which
 *         prints both wrapper names and aborts the test process.
 */
void svc_host_set_nesting_handler(svc_host_nesting_fn fn);
/** @brief Number of nesting violations observed since the last reset. */
uint32_t svc_host_nesting_count(void);
/** @brief Reset the gate depth and the violation counter. */
void svc_host_gate_reset(void);

#define SVC_HOST_GATE() \
    int svc_host_gate_token_ \
        __attribute__((cleanup(svc_host_gate_exit), unused)) = \
        svc_host_gate_enter(__func__)
#endif /* HOST_TEST */

/* ============================================================================
 * SVC HANDLER (called from assembly - target only)
 * ========================================================================= */

#ifndef HOST_TEST
/**
 * @brief C-level SVC dispatcher called from SVC_Handler assembly
 * @param stack_frame Pointer to exception stack frame
 */
void SVC_Handler_C(uint32_t *stack_frame);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ICARUS_SVC_H */
