# Deactivated Code Analysis

**Document ID:** ICARUS-VER-002
**Version:** 0.4
**Date:** 2026-09-27
**Status:** Draft
**Classification:** Public (Open Source)

## 1. Purpose

This document identifies and justifies deactivated code (code that cannot be executed during normal operation or testing) in accordance with DO-178C objectives. Deactivated code must be analyzed to ensure it does not adversely affect safety.

## 2. Definitions

Per DO-178C:
- **Deactivated Code:** Executable code that is not intended to be executed in any configuration of the target computer environment.
- **Dead Code:** Executable code that cannot be reached during any execution path.

## 3. Deactivated Code Inventory

### 3.1 Fault Handlers (Dead Code)

These handlers execute only when the processor encounters a fault condition. They are intentionally designed as infinite loops to halt the system safely.

#### 3.1.1 Error_Handler

| Attribute | Value |
|-----------|-------|
| **File** | `Core/Src/bsp/error.c` |
| **Function** | `Error_Handler(void)` |
| **Lines** | 4 |
| **Category** | Dead Code |
| **Trigger** | Called on unrecoverable HAL errors |

**Code:**
```c
void Error_Handler(void) {
    __disable_irq();
    while (1) {
    }
}
```

**Justification:** This function is called only when the HAL detects an unrecoverable error. The infinite loop is intentional to halt the system and prevent undefined behavior. In a certified system, this would trigger a watchdog reset.

**Verification Method:** Code review, static analysis

---

#### 3.1.2 NMI_Handler

| Attribute | Value |
|-----------|-------|
| **File** | `Core/Src/bsp/stm32h7xx_it.c` |
| **Function** | `NMI_Handler(void)` |
| **Lines** | ~3 |
| **Category** | Dead Code |
| **Trigger** | Non-maskable interrupt (hardware fault) |

**Justification:** NMI is triggered by hardware faults (e.g., clock security system). Cannot be triggered in unit tests.

**Verification Method:** Code review

---

#### 3.1.3 HardFault_Handler

| Attribute | Value |
|-----------|-------|
| **File** | `Core/Src/bsp/stm32h7xx_it.c` |
| **Function** | `HardFault_Handler(void)` |
| **Lines** | ~3 |
| **Category** | Dead Code |
| **Trigger** | Processor hard fault (invalid memory access, etc.) |

**Code Pattern:**
```c
void HardFault_Handler(void) {
    while (1) {
    }
}
```

**Justification:** Hard faults occur on invalid memory access, undefined instructions, or other processor exceptions. The infinite loop prevents further corruption. Cannot be safely triggered in unit tests.

**Verification Method:** Code review, static analysis

---

#### 3.1.4 MemManage_Handler

| Attribute | Value |
|-----------|-------|
| **File** | `Core/Src/bsp/stm32h7xx_it.c` |
| **Function** | `MemManage_Handler(void)` |
| **Lines** | ~3 |
| **Category** | Dead Code |
| **Trigger** | Memory protection unit (MPU) violation |

**Justification:** Triggered by MPU violations. Cannot be triggered without hardware MPU.

**Verification Method:** Code review

---

#### 3.1.5 BusFault_Handler

| Attribute | Value |
|-----------|-------|
| **File** | `Core/Src/bsp/stm32h7xx_it.c` |
| **Function** | `BusFault_Handler(void)` |
| **Lines** | ~3 |
| **Category** | Dead Code |
| **Trigger** | Bus error during memory access |

**Justification:** Triggered by bus errors (e.g., accessing invalid peripheral address). Cannot be triggered in host tests.

**Verification Method:** Code review

---

#### 3.1.6 UsageFault_Handler

| Attribute | Value |
|-----------|-------|
| **File** | `Core/Src/bsp/stm32h7xx_it.c` |
| **Function** | `UsageFault_Handler(void)` |
| **Lines** | ~3 |
| **Category** | Dead Code |
| **Trigger** | Usage fault (undefined instruction, unaligned access) |

**Justification:** Triggered by usage faults. Cannot be triggered in host tests.

**Verification Method:** Code review

---

### 3.2 Context Switch Handler (Target-Only Code)

#### 3.2.1 PendSV_Handler

| Attribute | Value |
|-----------|-------|
| **File** | `Core/Src/bsp/stm32h7xx_it.c` |
| **Function** | `PendSV_Handler(void)` |
| **Lines** | ~5 |
| **Category** | Target-Only Code |
| **Trigger** | PendSV interrupt for context switching |

**Code:**
```c
ITCM_FUNC __attribute__ ((naked)) void PendSV_Handler(void) {
    __asm__ volatile ("b os_yield_pendsv");  /* ARM assembly, context_switch.s */
}
```

The host build replaces it with a plain C function that calls the
`os_yield_pendsv()` mock in `tests/mocks/mock_asm.c`.

**Justification:** This handler branches to `os_yield_pendsv`, which is implemented in ARM assembly (`context_switch.s`). The assembly code manipulates ARM-specific registers (PSP, LR/EXC_RETURN, the FP registers S16–S31, etc.) that don't exist on the host platform. Since v0.5.0 it saves S16–S31 for tasks whose EXC_RETURN shows an extended (FP) frame, keeps EXC_RETURN per task and starts cold tasks with 0xFFFFFFFD (HLR-KRN-016, SDD §3.3.4); none of this runs on the host.

**Verification Method:** Target integration testing (for HLR-KRN-016, the target FPU-context probe with two FP-using tasks, see `test_traceability.md` §5), code review

---

### 3.3 Infinite Loop Tasks (Target-Only Code)

These are RTOS tasks that run continuously. They cannot be unit tested because they never return.

#### 3.3.1 os_idle_task

| Attribute | Value |
|-----------|-------|
| **File** | `Core/Src/icarus/task.c` |
| **Function** | `os_idle_task(void)` |
| **Lines** | ~8 |
| **Category** | Target-Only Code |
| **Purpose** | System idle task, initializes display |

**Code:**
```c
static void os_idle_task(void) {
    display_init();
    while (1) {
        os_yield();
    }
}
```

**Justification:** This task runs when no other tasks are ready. The `while(1)` loop is intentional RTOS design. Individual functions called within (`display_init`, `os_yield`) are tested separately.

**Verification Method:** Target integration testing, component unit tests

---

#### 3.3.2 os_heartbeart_task

| Attribute | Value |
|-----------|-------|
| **File** | `Core/Src/icarus/task.c` |
| **Function** | `os_heartbeart_task(void)` |
| **Lines** | ~25 |
| **Category** | Target-Only Code |
| **Purpose** | System heartbeat LED and display |

**Justification:** Heartbeat task blinks LED and updates display continuously. Individual functions (`LED_Blink`, `display_render_banner`, `task_active_sleep`) are tested separately.

**Verification Method:** Target integration testing, component unit tests

---

#### 3.3.3 os_transmit_printf_task (Removed)

**Removed from the source.** The printf transmit task and its print
buffer no longer exist; console output is copied straight into the USB
CDC transmit ring (`Core/Src/bsp/cdc.c`, `Core/Src/bsp/retarget_stdio.c`)
and sent from the USB transfer-complete interrupt, so no infinite-loop
task is involved. The ring logic is host-tested (`test_cdc.c`,
`test_stdio.c`); its target-only parts are listed in §3.5. The entry is
kept so that section numbers stay stable.

---

### 3.4 Static Helper Functions

#### 3.4.1 dequeue_print_buffer (Removed)

**Removed from the source** together with `os_transmit_printf_task`
(§3.3.3). No replacement helper is deactivated.

---

### 3.5 Target-Only Branches (v0.5.0)

These branches run on target in every configuration, so they are not
deactivated code; they are compiled out under `HOST_TEST` (or replaced by
a host stand-in) and are therefore absent from host coverage. Each has a
host-tested counterpart or is verified by target integration test and
review.

| File | Target-only code | Host stand-in / counterpart | Verification |
|------|------------------|-----------------------------|--------------|
| `Core/Src/icarus/svc.c` | `SVC_Handler_C()` dispatch and its per-arm caller-buffer checks; `svc_user_string_ok()`; the target branch of `svc_user_buffer_ok()` (data-pool slot lookup + allowlist) | Host wrappers call the `__` implementations directly; host `svc_user_buffer_ok()` rejects only NULL and wrap-around. The allowlist policy `svc_buffer_allowed()` is pure and host-tested with the target memory map (`test_svc_policy.c`) | Target integration test, review, static checks (`tools/check_svc_clobbers.py`, `tools/check_svc_pointer_checks.py`) |
| `Core/Src/icarus/svc.c` | `svc_caller_is_privileged()` reading IPSR and CONTROL | Always true on the host | Target integration test, review |
| `Core/Src/icarus/svc.c` | `svc_current_caller()`: data-pool slot lookup, main-stack window from `_estack` / `_Min_Stack_Size`, task detection by comparing the SVC frame with PSP | `svc_buffer_allowed()` is host-tested with task and main-stack callers (`test_svc_policy.c`) | Target integration test, review (including the assumption that the main stack stays within `_Min_Stack_Size`, SDD §4.2.8) |
| `USB_DEVICE/App/usbd_cdc_if.c`, `USB_DEVICE/Target/usbd_conf.c` | DTR reports in `CDC_Control_FS()` and link loss in `CDC_DeInit_FS()` calling `__cdc_tx_on_dtr()`; `HAL_PCD_ResumeCallback()` restarting the PHY clock, calling `USBD_LL_Resume()` and `__cdc_tx_kick()` | USB device-stack callbacks are not built on the host; the ring functions they call are host-tested (`test_cdc.c`) | Review. Close and reopen: exercised on hardware (the device keeps working across a 30 s close and reopen); discard behaviour host-tested only. Bus resume: host-tested only; not exercised on hardware |
| `Core/Src/bsp/cdc.c` | `tx_lock()` / `tx_unlock()` PRIMASK save, mask and restore; the SVC 93 asm path of `cdc_tx_write()` | Same ring logic runs unmasked against the mocked `CDC_Transmit_FS` (`test_cdc.c`) | Target integration test, review |
| `Core/Src/bsp/retarget_stdio.c` | Strong `_write()` override | `stdio_write()` and `__io_putchar()`, which `_write()` calls, are host-tested (`test_stdio.c`) | Target integration test, review |
| `Core/Src/icarus/crc.c` | SVC 89 asm path of `crc16_ccitt()` for unprivileged callers | Host calls `__crc16_ccitt()` directly (`test_crc.c`, `test_tables_load.c`) | Target integration test |
| `Core/Src/bsp/bootloader.c` | `bsp_bootloader_check()` ROM jump; `__sys_enter_bootloader()` D-cache clean and system reset | Host build records the request only | Target integration test, review |
| `Core/Src/bsp/mpu.c` | Region 8 (backup SRAM, non-cacheable) | Host backup-SRAM store survives simulated resets (`test_bkpram.c`) | Target integration test (retention across a real reset), review |

---

### 3.6 Deactivated SVC Dispatch Case (v0.5.0)

#### 3.6.1 SVC 58 (`SVC_SEMAPHORE_CONSUME_TIMEOUT`)

| Attribute | Value |
|-----------|-------|
| **File** | `Core/Src/icarus/svc.c` |
| **Function** | `SVC_Handler_C()`, case `SVC_SEMAPHORE_CONSUME_TIMEOUT` |
| **Lines** | 2 (`stack_frame[0] = 0u; break;`) |
| **Category** | Deactivated Code |
| **Trigger** | An `svc 58` instruction issued by hand; no kernel wrapper issues it |

**Code:**
```c
case SVC_SEMAPHORE_CONSUME_TIMEOUT:
    /* The timed wait sleeps and reads the tick through SVC
     * wrappers, so it runs in thread mode only (the wrapper calls
     * it directly).  Running it here would nest SVCs (HardFault);
     * a caller that issues this number directly just fails. */
    stack_frame[0] = 0u;
    break;
```

**Justification:** Until v0.5.0 this case ran `__semaphore_consume_timeout()`
inside the SVC handler. The wait sleeps and reads the tick through SVC
wrappers, so that would issue a nested SVC, which escalates to HardFault
on target. No caller used it: `semaphore_consume_timeout()` calls
`__semaphore_consume_timeout()` directly in thread mode (SDD §3.5.3).
The case now leaves false in R0 and does nothing else; the number stays
defined in `svc.h`. The case touches no memory and no kernel state, so reaching it cannot affect
another task. `SVC_Handler_C()` is compiled only for the target, so the
case is also absent from host coverage.

**Verification Method:** Code review (the wrapper does not issue SVC 58;
the case only writes the return value)

## 4. Safety Analysis

### 4.1 Impact Assessment

| Code Category | Safety Impact | Mitigation |
|---------------|---------------|------------|
| Fault Handlers | Low - only execute on faults | Watchdog timer resets system |
| Context Switch | Medium - critical for scheduling | Target integration tests |
| Infinite Loop Tasks | Low - non-safety-critical features | Component tests cover internals |
| Static Helpers | None remaining (`dequeue_print_buffer` was removed from the source before v0.2.0; its entry is retired in v0.5.0) | — |
| Target-Only Branches (v0.5.0) | Medium - privilege and buffer checks | Pure policy host-tested; static checks; target integration tests |
| Deactivated SVC 58 case (v0.5.0) | None - returns false, touches no memory or kernel state | Wrapper runs the timed wait in thread mode without SVC 58; code review |

### 4.2 Conclusion

All identified deactivated code has been analyzed and justified. The deactivated code:
- Does not contain safety-critical logic that could cause hazards
- Is either fault-handling code (intentionally unreachable), requires target hardware, or is a dispatch case kept only to return a failure value (SVC 58, §3.6)
- Has been verified through code review and/or component testing

## 5. Approval

| Role | Name | Signature | Date |
|------|------|-----------|------|
| Author | | | |
| Reviewer | | | |
| Safety Engineer | | | |

## 6. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2025-01-26 | Souham Biswas | Initial draft |
| 0.2 | 2026-04-01 | Souham Biswas | Reviewed against v0.2.0 MPU additions; no new deactivated paths introduced (red-team attack tasks are exercised at runtime, not deactivated) |
| 0.3 | 2026-04-11 | Souham Biswas | Reviewed against v0.3.0 shared service modules. The HW CRC peripheral path in `crc.c` is `#ifndef HOST_TEST` — it is *not* deactivated code; it is target-only and exercised by on-target smoke tests with the corresponding HOST_TEST fallback covered by host unit tests. The `cdc_rx_push` ISR-direct path in the public wrapper is also target-active and exercised by the USB CDC class driver on hardware. No newly deactivated branches in the v0.3.0 modules. |
| 0.4 | 2026-09-27 | Souham Biswas | Reviewed against v0.5.0. `os_transmit_printf_task` and `dequeue_print_buffer` were removed from the source (§3.3.3, §3.4.1 kept as stubs for numbering); console output now uses the USB CDC transmit ring. Added §3.5 listing the target-only branches introduced in v0.5.0 (SVC dispatch and caller-buffer checks, caller classification, privilege detection, PRIMASK lock, `_write()`, CRC SVC path, DTR close/reopen discard and bus-resume hooks, bootloader jump, MPU region 8). One newly deactivated path: the SVC 58 (`SVC_SEMAPHORE_CONSUME_TIMEOUT`) dispatch case, which no longer runs the timed wait inside the handler and returns false (§3.6). §3.2.1 corrected to the naked `PendSV_Handler` that branches to `os_yield_pendsv`, which now also saves the floating-point context. |
