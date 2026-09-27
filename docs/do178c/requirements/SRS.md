# Software Requirements Specification (SRS)

**Document ID:** ICARUS-SRS-001
**Version:** 0.5
**Date:** 2026-09-27
**Status:** Draft
**Classification:** Public (Open Source)

---

## Document Control

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Author | | | |
| Reviewer | | | |
| Approver | | | |

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | 2025-01-26 | Souham Biswas | Initial draft |
| 0.2 | 2026-04-01 | Souham Biswas | Added 14 memory protection requirements (HLR-KRN-063 to HLR-KRN-086) |
| 0.3 | 2026-04-11 | Souham Biswas | Added 5 shared service module requirements (HLR-KRN-090 to HLR-KRN-094): CDC RX ring buffer, event ring + squelch, CRC16 helper, internal filesystem, ground-loadable table engine |
| 0.4 | 2026-06-15 | Souham Biswas | Added Software Bus (HLR-KRN-095), Background Checksum (HLR-KRN-096), task restart (HLR-KRN-097), timed semaphore (HLR-KRN-098), wider pipes (HLR-KRN-053 updated), task diagnostics (HLR-KRN-099), BSP watchdog/button/CDC write (HLR-BSP-025 to HLR-BSP-027) |
| 0.5 | 2026-09-27 | Souham Biswas | v0.5.0: HLR-BSP-027 rewritten for the shared non-blocking USB CDC transmit ring (HLR-BSP-027.1 to HLR-BSP-027.7, including discard on port close and reopen); console retarget with partial writes and drop counter (HLR-BSP-028) replaces putchar line buffering; ROM bootloader entry (HLR-BSP-029); SVC caller-buffer validation, main-stack exclusion and pointer-check static check (HLR-KRN-074); SVC wrapper rules (HLR-KRN-075 to HLR-KRN-077); backup SRAM gates and MPU region (HLR-KRN-078); CDC RX drop counter (HLR-KRN-090.3); CRC via SVC (HLR-KRN-092.3); offset table load, abort, info and commit gating (HLR-KRN-094.4 to HLR-KRN-094.7); HLR-KRN-096.2 (callbacks in thread mode) and HLR-BSP-026 (K1 pressed level) corrected; §7.2 recounted |

---

## 1. Introduction

### 1.1 Purpose

This Software Requirements Specification (SRS) defines the functional and performance requirements for ICARUS OS. These requirements form the basis for design, implementation, and verification activities.

### 1.2 Scope

ICARUS OS is a real-time operating system providing:
- Preemptive multitasking
- Deterministic scheduling
- Hardware abstraction
- AI inference runtime
- Inter-process communication

### 1.3 Definitions

| Term | Definition |
|------|------------|
| Task | Independent unit of execution with own stack |
| Tick | Fundamental time unit (default: 1ms) |
| WCET | Worst-Case Execution Time |
| IPC | Inter-Process Communication |
| BSP | Board Support Package |

---

## 2. System Requirements Allocation

The following system-level requirements are allocated to ICARUS OS:

| System Req | Description | Allocated To |
|------------|-------------|--------------|
| SYS-001 | Provide deterministic task scheduling | Kernel |
| SYS-002 | Support minimum 8 concurrent tasks | Kernel |
| SYS-003 | Provide hardware abstraction | BSP |
| SYS-004 | Support AI inference <50ms | AI Runtime |
| SYS-005 | Boot to operational <500ms | Kernel + BSP |

---

## 3. High-Level Requirements (HLR)

### 3.1 Kernel Requirements

#### 3.1.1 Task Management

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-KRN-001 | The kernel shall support creation of up to MAX_TASKS concurrent tasks | Must | ✅ Implemented |
| HLR-KRN-002 | The kernel shall assign each task a unique identifier | Must | ✅ Implemented |
| HLR-KRN-003 | The kernel shall allocate a dedicated stack for each task | Must | ✅ Implemented |
| HLR-KRN-004 | The kernel shall support task termination by the task itself | Must | ✅ Implemented |
| HLR-KRN-005 | The kernel shall support task termination by other tasks | Must | ✅ Implemented |
| HLR-KRN-006 | The kernel shall protect system tasks from termination | Must | ✅ Implemented |
| HLR-KRN-007 | The kernel shall track task state (COLD, RUNNING, READY, BLOCKED, KILLED, FINISHED) | Must | ✅ Implemented |

#### 3.1.2 Scheduling

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-KRN-010 | The kernel shall implement preemptive scheduling | Must | ✅ Implemented |
| HLR-KRN-011 | The kernel shall use round-robin scheduling among equal-priority tasks | Must | ✅ Implemented |
| HLR-KRN-012 | The kernel shall support configurable time quantum (default: 50ms) | Must | ✅ Implemented |
| HLR-KRN-013 | The kernel shall support voluntary yield by tasks | Must | ✅ Implemented |
| HLR-KRN-014 | The kernel shall support priority-based scheduling | Should | Planned |
| HLR-KRN-015 | The kernel shall prevent priority inversion | Should | Planned |

#### 3.1.3 Timing Services

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-KRN-020 | The kernel shall maintain a monotonic tick counter | Must | ✅ Implemented |
| HLR-KRN-021 | The kernel shall support task sleep (non-blocking) | Must | ✅ Implemented |
| HLR-KRN-022 | The kernel shall support task delay (blocking) | Must | ✅ Implemented |
| HLR-KRN-023 | The kernel shall provide tick count query | Must | ✅ Implemented |
| HLR-KRN-024 | Sleep accuracy shall be ±1 tick | Must | ✅ Implemented |

#### 3.1.4 Critical Sections

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-KRN-030 | The kernel shall support disabling preemption for critical sections | Must | ✅ Implemented |
| HLR-KRN-031 | The kernel shall support nested critical sections | Must | ✅ Implemented |
| HLR-KRN-032 | Critical section duration shall be minimized (<100μs typical) | Should | ✅ Implemented |


#### 3.1.5 Inter-Process Communication (IPC)

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-KRN-040 | The kernel shall support message queues (pipes) | Must | ✅ Implemented |
| HLR-KRN-041 | The kernel shall support binary semaphores | Must | ✅ Implemented |
| HLR-KRN-042 | The kernel shall support counting semaphores | Should | ✅ Implemented |
| HLR-KRN-043 | The kernel shall support bounded semaphores with max capacity | Must | ✅ Implemented |
| HLR-KRN-044 | The kernel shall support mutexes with priority inheritance | Should | Planned |
| HLR-KRN-045 | The kernel shall support event flags | Should | Planned |
| HLR-KRN-046 | IPC operations shall have bounded WCET | Must | ✅ Implemented |
| HLR-KRN-047 | Message queues shall support FIFO ordering | Must | ✅ Implemented |
| HLR-KRN-048 | Message queues shall support variable-length messages | Must | ✅ Implemented |
| HLR-KRN-049 | Semaphore operations shall block when resource unavailable | Must | ✅ Implemented |
| HLR-KRN-050 | Pipe operations shall block when full/empty | Must | ✅ Implemented |
| HLR-KRN-051 | The kernel shall support up to ICARUS_MAX_SEMAPHORES (64) semaphores | Must | ✅ Implemented |
| HLR-KRN-052 | The kernel shall support up to ICARUS_MAX_MESSAGE_QUEUES (64) pipes | Must | ✅ Implemented |
| HLR-KRN-053 | Each pipe shall support up to 512 bytes capacity | Must | ✅ Implemented |

#### 3.1.6 Memory Management

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-KRN-060 | The kernel shall use static memory allocation only | Must | ✅ Implemented |
| HLR-KRN-061 | The kernel shall detect stack overflow | Should | ✅ Implemented |
| HLR-KRN-062 | The kernel shall support MPU-based memory protection | Must | ✅ Implemented |
| HLR-KRN-063 | The kernel shall isolate kernel code in privileged ITCM region | Must | ✅ Implemented |
| HLR-KRN-064 | The kernel shall isolate kernel data in privileged DTCM region | Must | ✅ Implemented |
| HLR-KRN-065 | The kernel shall isolate task data regions from each other | Must | ✅ Implemented |
| HLR-KRN-066 | The kernel shall enforce read-only protection on code regions | Must | ✅ Implemented |
| HLR-KRN-067 | The kernel shall provide SVC call gates for kernel services | Must | ✅ Implemented |
| HLR-KRN-068 | The kernel shall execute tasks in unprivileged mode | Must | ✅ Implemented |
| HLR-KRN-069 | The kernel shall execute kernel functions in privileged mode | Must | ✅ Implemented |
| HLR-KRN-070 | The kernel shall prevent unprivileged access to kernel data | Must | ✅ Implemented |
| HLR-KRN-071 | The kernel shall prevent cross-task memory access | Must | ✅ Implemented |
| HLR-KRN-072 | The kernel shall recover from recoverable memory faults | Must | ✅ Implemented |
| HLR-KRN-073 | The kernel shall support memory partitioning (ARINC 653) | Could | Planned |
| HLR-KRN-074 | Every SVC whose privileged implementation reads or writes caller-supplied memory shall validate the whole buffer, for the access it makes, before touching it | Must | ✅ Implemented |
| HLR-KRN-074.1 | A buffer the kernel writes shall lie wholly inside RAM_D1 (less the main-stack window for task callers, HLR-KRN-074.7), the application (upper) half of DTCM (MPU region 3), or the calling task's own data-pool slot | Must | ✅ Implemented |
| HLR-KRN-074.2 | A buffer the kernel only reads may in addition lie in internal flash or ITCM; address 0 (NULL) shall never be accepted as a buffer start, which also excludes the first byte of ITCM | Must | ✅ Implemented |
| HLR-KRN-074.3 | All other memory shall be rejected: the privileged DTCM half, other tasks' data-pool slots, SRAM4, backup SRAM, peripherals, the system control space and unmapped addresses. A range that wraps the address space or spans two regions shall be rejected; a zero-length buffer shall be accepted and not touched | Must | ✅ Implemented |
| HLR-KRN-074.4 | A call whose buffer is rejected shall return its documented failure value and shall not read or write memory through the rejected pointer | Must | ✅ Implemented |
| HLR-KRN-074.5 | A NUL-terminated string passed to the kernel shall be validated byte by byte, up to its maximum length, before each byte is read | Must | ✅ Implemented |
| HLR-KRN-074.6 | The allowlist policy shall be a pure function of address, length, access and a description of the caller (data-pool slot, main-stack window, stack the exception frame is on; `svc_buffer_allowed()`) so that it can be verified on the host against the target memory map; a missing caller description shall never grant access | Should | ✅ Implemented |
| HLR-KRN-074.7 | For a caller whose exception frame is on the process stack (a task), a buffer that overlaps the main-stack window at the top of RAM_D1 (`_estack` − `_Min_Stack_Size` up to `_estack`, which holds the SVC handler's own frame) shall be rejected for reads and writes. Privileged code still running on the main stack (boot code) may pass buffers there. Assumption: the window covers the handler's frame only while everything on the main stack (the `main()` / `os_start()` frames, which are never unwound, the SVC handler and nested interrupts) stays within `_Min_Stack_Size`; MSP is not yet reset at first-task launch (SDD §4.2.8) | Must | ✅ Implemented |
| HLR-KRN-074.8 | A static check (`tools/check_svc_pointer_checks.py`), run by `make -C tests` before the host suite, shall fail when an SVC dispatch case casts a caller argument (or an expression built from it) to a data pointer without a caller-buffer validator on that argument earlier in the same case, or casts one to a function pointer outside a documented allowlist | Should | ✅ Implemented |
| HLR-KRN-075 | Every inline-assembly block that issues an SVC shall declare a `"memory"` clobber, so the compiler neither keeps memory values in registers across the call nor assumes that memory whose address was passed to the handler is unchanged | Must | ✅ Implemented |
| HLR-KRN-075.1 | A static check (`tools/check_svc_clobbers.py`) shall fail the host test run (`make -C tests`) if any SVC inline-assembly block lacks the `"memory"` clobber | Must | ✅ Implemented |
| HLR-KRN-076 | Host builds shall detect a nested supervisor call: entering a wrapper whose implementation runs inside the SVC handler on target, while another such wrapper's implementation is running, shall be reported and by default abort the test run (`SVC_HOST_GATE`). Wrappers that run thread-mode code on target shall not hold a gate while that code runs: spin loops only gate their individual SVC calls, `tbl_activate` gates its prepare and commit steps but not the callback between them, and `cdc_tx_write` opens no gate | Must | ✅ Implemented |
| HLR-KRN-077 | Kernel services that may be called from handler mode or privileged thread mode (`cdc_tx_write`, `crc16_ccitt`) shall call their privileged implementation directly instead of issuing an SVC (`svc_caller_is_privileged()`) | Must | ✅ Implemented |
| HLR-KRN-078 | The kernel shall provide SVC-gated copy-in and copy-out for the 4 KB backup SRAM at 0x38800000 (`bkpram_write`, SVC 71; `bkpram_read`, SVC 86) so unprivileged tasks can keep data across resets without an MPU grant | Must | ✅ Implemented |
| HLR-KRN-078.1 | Backup SRAM accesses shall reject a zero length, an offset or range outside the backup SRAM (including offset + length overflow) and a caller buffer not allowed by HLR-KRN-074, without touching memory | Must | ✅ Implemented |
| HLR-KRN-078.2 | The backup SRAM shall be mapped by its own MPU region (region 8) as privileged read/write, non-cacheable and execute-never, so data written is in the SRAM when the call returns and survives a system or watchdog reset | Must | ✅ Implemented |

#### 3.1.7 Fault Handling

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-KRN-080 | The kernel shall handle processor faults gracefully | Must | ✅ Implemented |
| HLR-KRN-081 | The kernel shall handle MemManage faults from unprivileged tasks | Must | ✅ Implemented |
| HLR-KRN-082 | The kernel shall recover from data access violations | Must | ✅ Implemented |
| HLR-KRN-083 | The kernel shall halt on instruction fetch violations | Must | ✅ Implemented |
| HLR-KRN-084 | The kernel shall support watchdog integration | Should | ✅ Implemented |
| HLR-KRN-085 | The kernel shall log fault information for diagnostics | Should | ✅ Implemented |
| HLR-KRN-086 | The kernel shall support fault recovery (task restart) | Could | ✅ Implemented |

#### 3.1.8 Shared Service Modules

The shared service modules provide reusable kernel infrastructure for
serial input buffering, structured event logging, integrity checks,
scratch storage, and ground-loadable configuration tables. All five
follow the kernel's MPU-aware privilege-separation pattern (DTCM_PRIV
backing data, ITCM hot path, SVC-gated public API). See SDD §3.10 for
the detailed design.

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-KRN-090 | The kernel shall provide a single-producer / single-consumer USB CDC receive ring buffer with capacity ≥ 256 bytes, callable from a privileged ISR producer and an unprivileged thread-mode consumer | Must | ✅ Implemented |
| HLR-KRN-090.1 | The CDC RX ring buffer shall not block the USB ISR — overflow shall silently drop incoming bytes rather than wait | Must | ✅ Implemented |
| HLR-KRN-090.2 | Thread-mode consumer reads of the CDC RX ring shall route through SVC gates so the ring data may live in privileged-only DTCM | Must | ✅ Implemented |
| HLR-KRN-090.3 | The CDC RX ring shall count the bytes dropped on overflow since the last `cdc_rx_init()`; unprivileged tasks shall read the count through `cdc_rx_dropped()` (SVC 91) | Should | ✅ Implemented |
| HLR-KRN-091 | The kernel shall provide a generic structured event ring buffer with per-module severity squelch filtering | Must | ✅ Implemented |
| HLR-KRN-091.1 | Event entries shall be fixed at 16 bytes (header + ≤12 byte payload) for deterministic memory budget | Must | ✅ Implemented |
| HLR-KRN-091.2 | The event module shall be transport-agnostic — it shall drain into a caller-provided buffer rather than encoding any specific telemetry format | Must | ✅ Implemented |
| HLR-KRN-091.3 | Event emission shall be O(1) and non-blocking; ring overflow shall overwrite the oldest entry rather than block the producer | Must | ✅ Implemented |
| HLR-KRN-092 | The kernel shall provide a CRC16-CCITT helper (polynomial 0x1021, initial value 0xFFFF, no reflection) | Must | ✅ Implemented |
| HLR-KRN-092.1 | On STM32H7 target, the CRC16 helper shall use the on-chip CRC peripheral on the AHB4 bus rather than a software loop | Should | ✅ Implemented |
| HLR-KRN-092.2 | A portable software fallback shall be available under HOST_TEST so unit tests run unchanged off-target | Must | ✅ Implemented |
| HLR-KRN-092.3 | `crc16_ccitt()` called from unprivileged thread mode shall compute through `SVC_CRC16_CCITT` (89), which validates the input buffer (HLR-KRN-074); privileged code and handlers shall call the implementation directly | Must | ✅ Implemented |
| HLR-KRN-093 | The kernel shall provide a minimal flat-file storage layer with create/open/read/write/delete/list/stats operations | Must | ✅ Implemented |
| HLR-KRN-093.1 | The filesystem shall support at least 16 named files of at least 2 KB each, totalling at least 32 KB capacity | Must | ✅ Implemented |
| HLR-KRN-093.2 | The on-disk format shall be opaque to allow a real flash backend to be substituted later without changing the public API | Should | ✅ Implemented |
| HLR-KRN-094 | The kernel shall provide a generic ground-loadable table engine with double-buffered staging/active swap | Must | ✅ Implemented |
| HLR-KRN-094.1 | Table activation shall be gated by both a schema CRC and a data CRC16; mismatches shall reject the activation atomically | Must | ✅ Implemented |
| HLR-KRN-094.2 | The activate callback registered by a table producer shall execute in unprivileged thread mode against a stack scratch copy of the staged bytes — never against the live DTCM_PRIV staging buffer | Must | ✅ Implemented |
| HLR-KRN-094.3 | The active table buffer shall not be modified until the activate callback returns success | Must | ✅ Implemented |
| HLR-KRN-094.4 | The table engine shall accept offset-addressed chunks (`tbl_load_at`, SVC 92). Offset 0 starts a new load, except that an identical copy of the first chunk sent while that load is still incomplete is treated as a retransmit. The retransmit and schema-CRC rules apply to chunks at offset > 0: each next chunk starts where the staged data ends; a chunk wholly inside the bytes already staged shall be accepted only if identical (retransmit); gaps, conflicting retransmits and a schema CRC that differs from the first chunk's shall be rejected without modifying staging. A chunk that overruns the descriptor size shall be rejected at any offset | Must | ✅ Implemented |
| HLR-KRN-094.5 | `tbl_abort` (SVC 88) shall discard the staged, not yet activated bytes of a table and leave its active buffer unchanged | Must | ✅ Implemented |
| HLR-KRN-094.6 | `tbl_get_info` (SVC 87) shall copy a table's descriptor fields and staging/active state into caller memory, so unprivileged tasks never dereference a descriptor pointer that refers to privileged memory | Must | ✅ Implemented |
| HLR-KRN-094.7 | The activate commit shall be accepted only right after a successful activate prepare for the same table, with the descriptor-size length and data identical to what was staged | Must | ✅ Implemented |
| HLR-KRN-095 | The kernel shall provide a lightweight pub/sub Software Bus that routes messages by ID to subscriber pipes | Must | ✅ Implemented |
| HLR-KRN-095.1 | The Software Bus shall support at least 32 distinct message routes with up to 4 subscribers per route | Must | ✅ Implemented |
| HLR-KRN-095.2 | Publishing shall be best-effort: if a subscriber's pipe is full the message shall be silently dropped for that subscriber without blocking the publisher | Must | ✅ Implemented |
| HLR-KRN-095.3 | The Software Bus route table shall reside in DTCM_DATA_PRIV; hot-path functions shall be placed in ITCM_FUNC | Must | ✅ Implemented |
| HLR-KRN-096 | The kernel shall provide a periodic background checksum integrity monitor using CRC16-CCITT over registered memory regions | Must | ✅ Implemented |
| HLR-KRN-096.1 | The checksum monitor shall support at least 8 independently enabled memory regions with baselines captured at registration time | Must | ✅ Implemented |
| HLR-KRN-096.2 | CRC mismatches shall be reported through a user-supplied callback that cs_check_all() invokes in the calling task's thread mode after the privileged scan has returned — never inside the SVC handler — so the callback may call any kernel API | Must | ✅ Implemented |
| HLR-KRN-096.3 | The checksum module shall perform a hardware CRC self-test at initialization (expected value 0x29B1) | Must | ✅ Implemented |
| HLR-KRN-097 | The kernel shall provide os_restart_task(task_index) to cold-restart a killed or finished task in-place from its original entry point without allocating a new stack slot | Must | ✅ Implemented |
| HLR-KRN-097.1 | os_restart_task shall only accept tasks in TASK_STATE_KILLED or TASK_STATE_FINISHED; the restarted task shall enter the scheduler as TASK_STATE_COLD | Must | ✅ Implemented |
| HLR-KRN-098 | The kernel shall provide semaphore_consume_timeout(idx, max_ticks) for timed semaphore acquisition; 0 max_ticks shall be a non-blocking try | Must | ✅ Implemented |
| HLR-KRN-098.1 | semaphore_consume_timeout shall return false if the semaphore is not acquired within max_ticks | Must | ✅ Implemented |
| HLR-KRN-099 | The kernel shall provide per-task diagnostic fields: dispatch_count (scheduling counter) and stack_watermark (minimum free stack words using 0xDEADC0DE sentinel) | Must | ✅ Implemented |
| HLR-KRN-099.1 | os_get_task_state(task_index) shall return the current task state via an SVC-gated query | Must | ✅ Implemented |
| HLR-KRN-099.2 | os_update_stack_watermark(task_index) shall scan the task stack for the sentinel pattern and update the watermark field in the TCB | Must | ✅ Implemented |

---

### 3.2 BSP Requirements

#### 3.2.1 Platform Abstraction

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-BSP-001 | The BSP shall provide platform-independent kernel interface | Must | ✅ Implemented |
| HLR-BSP-002 | The BSP shall initialize processor and clocks | Must | ✅ Implemented |
| HLR-BSP-003 | The BSP shall configure interrupt priorities | Must | ✅ Implemented |
| HLR-BSP-004 | The BSP shall provide context switch mechanism | Must | ✅ Implemented |

#### 3.2.2 Peripheral Drivers

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-BSP-010 | The BSP shall support GPIO control | Must | ✅ Implemented |
| HLR-BSP-011 | The BSP shall support I2C communication | Must | ✅ Implemented |
| HLR-BSP-012 | The BSP shall support SPI communication | Must | ✅ Implemented |
| HLR-BSP-013 | The BSP shall support UART/USB serial output | Must | ✅ Implemented |
| HLR-BSP-014 | The BSP shall support display output | Should | ✅ Implemented |
| HLR-BSP-015 | The BSP shall support PWM output | Should | Planned |
| HLR-BSP-016 | The BSP shall support ADC input | Should | Planned |
| HLR-BSP-017 | The BSP shall support CAN bus | Could | Planned |

#### 3.2.3 Timing Hardware

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-BSP-020 | The BSP shall configure SysTick for 1ms tick | Must | ✅ Implemented |
| HLR-BSP-021 | The BSP shall support high-resolution timer | Should | Planned |
| HLR-BSP-022 | The BSP shall support RTC for wall-clock time | Should | ✅ Implemented |
| HLR-BSP-025 | The BSP shall provide an Independent Watchdog (IWDG) abstraction with init, refresh, reset-reason query, and flag clear | Must | ✅ Implemented |
| HLR-BSP-026 | The BSP shall provide a K1 user button read function that returns true while the button is held, i.e. while the pin reads `BSP_KEY_PRESSED_LEVEL` (PC13 is active high on the reference board) | Must | ✅ Implemented |

#### 3.2.4 USB CDC Output

All output to the USB CDC IN endpoint shares one transmit ring owned by
privileged code. See SDD §3.11 for the design.

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-BSP-027 | The BSP shall provide a non-blocking raw CDC write (`CDC_Write`, `CDC_WriteString`) that queues either all of the caller's bytes on the USB CDC transmit ring or none of them, and returns false when the ring lacks room; it shall never wait, sleep or retry | Must | ✅ Implemented |
| HLR-BSP-027.1 | All USB CDC IN-endpoint output (`printf` through `_write()`, `__io_putchar`, `CDC_Write`, `cdc_tx_write`) shall pass through one transmit ring of `CDC_TX_RING_SIZE` bytes (default 4096) in privileged DTCM, so bytes from different producers are sent in the order they were queued | Must | ✅ Implemented |
| HLR-BSP-027.2 | `cdc_tx_write` shall offer an all-or-nothing mode and a partial mode that queues what fits, and shall count the bytes refused or dropped for lack of ring space and the bytes discarded on port close, reopen or link loss (HLR-BSP-027.7). Unprivileged tasks shall reach the ring only through `SVC_CDC_TX_WRITE` (93), which validates the caller buffer (HLR-KRN-074) | Must | ✅ Implemented |
| HLR-BSP-027.3 | Queueing on the transmit ring shall never block, sleep or spin, and shall be callable from any task and from any configurable-priority interrupt or exception handler. NMI and HardFault handlers shall not write to the ring (the ring's PRIMASK lock does not mask them) | Must | ✅ Implemented |
| HLR-BSP-027.4 | Checking that the endpoint is idle and starting a transfer shall be one step with respect to the USB interrupt and to other producers (interrupts masked). A transfer shall be at most `CDC_TX_MAX_CHUNK` bytes, and its bytes shall stay in the ring until the transfer-complete interrupt releases them | Must | ✅ Implemented |
| HLR-BSP-027.5 | A USB reset or re-configuration shall clear the in-flight transfer state, so a transfer lost to the reset cannot wedge the ring. The lost transfer's bytes shall stay queued and be sent again, unless the port was open when the link was lost: the CDC de-initialisation then reports the port closed and the queued bytes are discarded as for a close (HLR-BSP-027.7) | Must | ✅ Implemented |
| HLR-BSP-027.6 | While the device is not configured or the bus is suspended, output shall stay queued up to the ring capacity and shall be sent when the host opens the port (unless a reopen discards it, HLR-BSP-027.7), when the bus resumes, on the next write, or on the next transfer completion | Must | ✅ Implemented |
| HLR-BSP-027.7 | On a change of the host's DTR line, output written while the port was closed shall not reach the next open: when DTR goes from set to clear (close, including link loss) and when it goes from clear to set after an earlier open since boot (reopen), the bytes queued behind the transfer in flight shall be discarded and counted as dropped (`__cdc_tx_dropped()`). The first clear-to-set since boot shall keep them, so output queued before the first open is sent. A report that does not change DTR shall discard nothing. A transfer already in flight shall never be discarded and shall complete normally, so up to `CDC_TX_MAX_CHUNK` (2048) bytes written before a close can still arrive after the reopen | Should | ✅ Implemented |
| HLR-BSP-028 | The BSP shall retarget console output to the CDC transmit ring with partial-write semantics through a strong `_write()` (the C library's path for `printf`/`puts`/`fwrite(stdout)`), `stdio_write()` and the legacy single-character entry `__io_putchar`: bytes that fit are queued and the rest are dropped. `_write()` returns `len` and `__io_putchar()` returns its argument, so the C library never retries. `stdio_write()` returns the number of bytes queued, and the rest are dropped and counted (HLR-BSP-028.1) | Must | ✅ Implemented |
| HLR-BSP-028.1 | Console bytes dropped because the transmit ring was full, or because an unprivileged caller passed a buffer it may not hand to the kernel, shall be counted since boot with an atomic update and shall be readable from unprivileged code (`stdio_get_tx_dropped()`) | Must | ✅ Implemented |
| HLR-BSP-028.2 | The console retarget layer shall keep no buffer of its own: no line buffering, no flush on newline and no flush on buffer full; stdout buffering is left to the C library | Must | ✅ Implemented |
| HLR-BSP-028.3 | Each `_write()` / `stdio_write()` call (up to 65535 bytes) shall be copied into the ring in one step with interrupts masked, so no other producer's bytes land inside it. `printf()` itself is not made thread-safe: the C library's shared stdout buffer is not locked, and callers that need whole lines from several tasks or handlers shall serialise their `printf()` calls or format into their own buffer and call `stdio_write()` | Must | ✅ Implemented |

#### 3.2.5 System Control

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-BSP-029 | The BSP shall provide `sys_enter_bootloader()` (SVC 90), which records a request in reset-surviving RAM and resets the chip; the startup code shall detect the request before `main()`, clock setup and the watchdog start, clear it, and jump to the ROM bootloader | Should | ✅ Implemented |

---

### 3.3 AI Runtime Requirements

#### 3.3.1 Model Management

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-AI-001 | The AI runtime shall load models from memory | Must | Planned |
| HLR-AI-002 | The AI runtime shall validate model integrity (CRC) | Must | Planned |
| HLR-AI-003 | The AI runtime shall validate model structure | Must | Planned |
| HLR-AI-004 | The AI runtime shall reject models with unsupported operators | Must | Planned |
| HLR-AI-005 | The AI runtime shall support multiple concurrent models | Should | Planned |

#### 3.3.2 Inference Execution

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-AI-010 | The AI runtime shall execute inference deterministically | Must | Planned |
| HLR-AI-011 | The AI runtime shall use fixed-point arithmetic only | Must | Planned |
| HLR-AI-012 | The AI runtime shall complete inference within WCET budget | Must | Planned |
| HLR-AI-013 | The AI runtime shall not allocate memory during inference | Must | Planned |
| HLR-AI-014 | The AI runtime shall support int8 quantized models | Must | Planned |
| HLR-AI-015 | The AI runtime shall support int16 quantized models | Should | Planned |

#### 3.3.3 Supported Operators

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-AI-020 | The AI runtime shall support Conv2D operator | Must | Planned |
| HLR-AI-021 | The AI runtime shall support Dense (fully connected) operator | Must | Planned |
| HLR-AI-022 | The AI runtime shall support ReLU activation | Must | Planned |
| HLR-AI-023 | The AI runtime shall support Softmax activation | Must | Planned |
| HLR-AI-024 | The AI runtime shall support MaxPool2D operator | Must | Planned |
| HLR-AI-025 | The AI runtime shall support AveragePool2D operator | Should | Planned |
| HLR-AI-026 | The AI runtime shall support Add/Concatenate operators | Should | Planned |
| HLR-AI-027 | The AI runtime shall support BatchNormalization | Should | Planned |

#### 3.3.4 Safety Features

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| HLR-AI-030 | The AI runtime shall check output bounds | Must | Planned |
| HLR-AI-031 | The AI runtime shall provide confidence scores | Must | Planned |
| HLR-AI-032 | The AI runtime shall support fallback on low confidence | Should | Planned |
| HLR-AI-033 | The AI runtime shall detect numerical overflow | Must | Planned |
| HLR-AI-034 | The AI runtime shall isolate AI execution from kernel | Should | Planned |


---

## 4. Performance Requirements

### 4.1 Timing Requirements

| ID | Requirement | Value | Status |
|----|-------------|-------|--------|
| PRF-001 | Context switch time | <10μs | ✅ Met |
| PRF-002 | Interrupt latency | <5μs | ✅ Met |
| PRF-003 | Task creation time | <100μs | ✅ Met |
| PRF-004 | Boot to first task | <100ms | ✅ Met |
| PRF-005 | AI inference (small model) | <10ms | Planned |
| PRF-006 | AI inference (medium model) | <50ms | Planned |
| PRF-007 | Pipe enqueue (uncontended) | <5μs | ✅ Met |
| PRF-008 | Semaphore acquire (uncontended) | <2μs | ✅ Met |
| PRF-009 | Pipe dequeue (uncontended) | <5μs | ✅ Met |
| PRF-010 | Semaphore release (uncontended) | <2μs | ✅ Met |

### 4.2 Resource Requirements

| ID | Requirement | Value | Status |
|----|-------------|-------|--------|
| PRF-010 | Kernel code size | <32KB | ✅ Met |
| PRF-011 | Kernel RAM usage | <8KB + stacks | ✅ Met |
| PRF-012 | Per-task stack | 2 KB (ICARUS_STACK_WORDS=512 words) | ✅ Met |
| PRF-013 | AI runtime code size | <64KB | Planned |
| PRF-014 | AI runtime RAM (base) | <16KB | Planned |

### 4.3 Scalability Requirements

| ID | Requirement | Value | Status |
|----|-------------|-------|--------|
| PRF-020 | Maximum tasks | 128 (ICARUS_MAX_TASKS, configurable) | ✅ Met |
| PRF-021 | Maximum priority levels | 8 | Planned |
| PRF-022 | Maximum message queues | 64 (ICARUS_MAX_MESSAGE_QUEUES) | ✅ Met |
| PRF-023 | Maximum semaphores | 64 (ICARUS_MAX_SEMAPHORES) | ✅ Met |
| PRF-024 | Maximum AI models loaded | 4 | Planned |

---

## 5. Interface Requirements

### 5.1 Kernel API

```c
// Task Management
void os_init(void);
void os_start(void);
void os_register_task(void (*function)(void), const char *name);
void os_create_task(task_t *task, void (*function)(void), 
                    uint32_t *stack, uint32_t stack_size, const char *name);
void os_exit_task(void);
void os_kill_process(uint8_t task_index);

// Scheduling
void os_yield(void);
uint32_t task_active_sleep(uint32_t ticks);
uint32_t task_blocking_sleep(uint32_t ticks);

// Information
uint32_t os_get_tick_count(void);
uint8_t os_get_running_task_count(void);
const char* os_get_current_task_name(void);

// Semaphores (Implemented)
bool semaphore_init(uint8_t semaphore_idx, uint32_t semaphore_count);
bool semaphore_feed(uint8_t semaphore_idx);
bool semaphore_consume(uint8_t semaphore_idx);
uint32_t semaphore_get_count(uint8_t semaphore_idx);
uint32_t semaphore_get_max_count(uint8_t semaphore_idx);

// Message Pipes (Implemented)
bool pipe_init(uint8_t pipe_idx, uint8_t capacity);
bool pipe_enqueue(uint8_t pipe_idx, uint8_t data);
int16_t pipe_dequeue(uint8_t pipe_idx);
uint8_t pipe_get_count(uint8_t pipe_idx);
uint8_t pipe_get_max_count(uint8_t pipe_idx);

// Backup SRAM (SVC 71, 86) — HLR-KRN-078
bool bkpram_write(const void *src, uint32_t offset, uint32_t len);
bool bkpram_read(void *dst, uint32_t offset, uint32_t len);

// SVC caller-buffer policy — HLR-KRN-074, HLR-KRN-077
bool svc_buffer_allowed(uintptr_t addr, uint32_t len, svc_access_t access,
                        const svc_caller_t *caller);
bool svc_caller_is_privileged(void);
```

### 5.2 AI Runtime API

```c
// Model Management (Planned)
int ai_model_load(ai_model_t *model, const void *model_data, size_t size);
int ai_model_validate(ai_model_t *model);
void ai_model_unload(ai_model_t *model);

// Inference (Planned)
int ai_inference_run(ai_model_t *model, const void *input, void *output);
int ai_inference_get_confidence(ai_model_t *model, float *confidence);
uint32_t ai_inference_get_wcet(ai_model_t *model);

// Operator Library (Planned)
void ai_op_conv2d(const ai_tensor_t *input, const ai_tensor_t *weights,
                  const ai_tensor_t *bias, ai_tensor_t *output,
                  const ai_conv_params_t *params);
void ai_op_dense(const ai_tensor_t *input, const ai_tensor_t *weights,
                 const ai_tensor_t *bias, ai_tensor_t *output);
void ai_op_relu(ai_tensor_t *tensor);
void ai_op_softmax(const ai_tensor_t *input, ai_tensor_t *output);
```

### 5.3 BSP Interface

```c
// Platform Abstraction (Planned unified interface)
void icarus_hal_init(void);
uint32_t icarus_hal_get_tick(void);
void icarus_hal_enter_critical(void);
void icarus_hal_exit_critical(void);
void icarus_hal_context_switch(void);
void icarus_hal_start_first_task(task_t *task);

// Current Implementation
void hal_init(void);
void LED_On(void);
void LED_Off(void);
void LED_Blink(uint32_t on_ticks, uint32_t off_ticks);
int32_t platform_write(void *handle, uint8_t reg, uint8_t *data, uint16_t len);
int32_t platform_read(void *handle, uint8_t reg, uint8_t *data, uint16_t len);

// USB CDC output (non-blocking) — HLR-BSP-027, HLR-BSP-028
uint16_t cdc_tx_write(const uint8_t *data, uint16_t len, bool whole);
bool CDC_Write(const uint8_t *data, uint16_t len);
bool CDC_WriteString(const char *s);
uint32_t stdio_write(const uint8_t *data, uint32_t len);
uint32_t stdio_get_tx_dropped(void);

// System control — HLR-BSP-029
void sys_enter_bootloader(void);
```

---

## 6. Safety Requirements

### 6.1 Fault Detection

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| SAF-001 | The system shall detect stack overflow | Must | Planned |
| SAF-002 | The system shall detect null pointer dereference | Should | Planned |
| SAF-003 | The system shall detect watchdog timeout | Must | Planned |
| SAF-004 | The system shall detect AI model corruption | Must | Planned |
| SAF-005 | The system shall detect AI output out of bounds | Must | Planned |

### 6.2 Fault Response

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| SAF-010 | The system shall halt on unrecoverable fault | Must | ✅ Implemented |
| SAF-011 | The system shall log fault information | Should | Planned |
| SAF-012 | The system shall support graceful degradation | Should | Planned |
| SAF-013 | The system shall support task restart on fault | Could | Planned |

---

## 7. Traceability

### 7.1 Requirements to Tests

See `ICARUS-VER-003 Test Traceability Matrix` for complete mapping.

### 7.2 Requirements Status Summary

| Category | Total | Implemented | Planned |
|----------|-------|-------------|---------|
| Kernel (KRN) | 110 | 105 | 5 |
| BSP | 30 | 26 | 4 |
| AI Runtime | 24 | 0 | 24 |
| Performance | 20 | 14 | 6 |
| Safety | 9 | 1 | 8 |
| **Total** | **193** | **146** | **47** |

> Counting convention (v0.5): every table row in §3, §4 and §6 that
> carries a requirement ID is one requirement, and each sub-requirement
> (for example HLR-KRN-074.3) counts separately. "Implemented" counts
> rows marked ✅ (Implemented or Met); "Planned" counts rows marked
> Planned. PRF-010 is used by two rows (§4.1 and §4.2) and is counted
> twice. The previous totals did not follow a stated convention and are
> superseded.
>
> Counts include the v0.5.0 additions: SVC caller-buffer validation
> (HLR-KRN-074 to HLR-KRN-074.8), SVC wrapper rules (HLR-KRN-075 to
> HLR-KRN-077), backup SRAM (HLR-KRN-078), HLR-KRN-090.3,
> HLR-KRN-092.3, HLR-KRN-094.4 to HLR-KRN-094.7, the USB CDC transmit
> ring (HLR-BSP-027 to HLR-BSP-027.7), console retarget (HLR-BSP-028 to
> HLR-BSP-028.3) and ROM bootloader entry (HLR-BSP-029).
> Recount after adding or removing requirements.

---

*End of Document*
