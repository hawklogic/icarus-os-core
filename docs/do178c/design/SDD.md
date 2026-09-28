# Software Design Description (SDD)

**Document ID:** ICARUS-SDD-001
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
| 0.2 | 2026-04-01 | Souham Biswas | Added MPU protection architecture (§4.2), DTCM/ITCM placement, SVC call-gate dispatch model |
| 0.3 | 2026-04-11 | Souham Biswas | Added shared service modules (CDC RX, event ring, CRC16, internal filesystem, table engine) to the component summary; SVC count grew 40 → 57; HW CRC peripheral integration noted; tbl_activate priv↔thread split documented |
| 0.4 | 2026-06-15 | Souham Biswas | Added Software Bus and Background Checksum modules (§3.10.8, §3.10.9); SVC count grew 57 → 63; new RTOS primitives (restart, timed sem, wider pipes, task diagnostics); new BSP modules (IWDG, button, CDC write) |
| 0.5 | 2026-09-27 | Souham Biswas | v0.5.0: USB CDC transmit ring and console retarget (§3.11) replace the CDC busy-retry write and the putchar line buffers, with discard on port close and reopen and restart on bus resume; §3.7 print buffer marked superseded; ROM bootloader entry (§3.12); SVC caller-buffer validation with main-stack exclusion and its static check (§4.2.8) and SVC wrapper rules (§4.2.9); MPU table corrected and region 8 (backup SRAM, non-cacheable) added; SVC count grew 63 → 94 (IDs 0–93, new 86–93); §4.2.5 SVC table corrected to match `svc.h`; SVC ranges for Software Bus (72–77) and Checksum (63–70) corrected; checksum callbacks delivered in thread mode; table engine `tbl_load_at`/`tbl_abort`/`tbl_get_info` and commit gating; context switch saves S16–S31 for tasks with an FP frame and keeps EXC_RETURN per task, cold tasks start with 0xFFFFFFFD (§3.2.4, §3.3, HLR-KRN-016); timed semaphore timeout measured on the system tick (§3.5); SVC 58 dispatch case returns false (§4.2.5) |

---

## 1. Introduction

### 1.1 Purpose

This Software Design Description (SDD) defines the architecture, components, interfaces, and data structures of ICARUS OS. It provides the bridge between requirements (SRS) and implementation, enabling traceability and design verification.

### 1.2 Scope

This document covers:
- System architecture and decomposition
- Component design and interfaces
- Data structures and algorithms
- Memory architecture
- Timing design

### 1.3 Design Principles

| Principle | Description |
|-----------|-------------|
| **Determinism** | All operations have bounded, predictable timing |
| **Static Allocation** | No dynamic memory after initialization |
| **Minimal Footprint** | Kernel <32KB code, <8KB RAM |
| **Portability** | Hardware abstraction enables multi-platform |
| **Certifiability** | Design supports DO-178C verification |

---

## 2. System Architecture

### 2.1 Architectural Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         APPLICATION LAYER                                │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐ │
│  │ User Task 1 │  │ User Task 2 │  │ User Task N │  │   AI Tasks      │ │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └────────┬────────┘ │
├─────────┴────────────────┴────────────────┴──────────────────┴──────────┤
│                          ICARUS KERNEL                                   │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                      KERNEL API                                    │  │
│  │  os_init() os_start() os_yield() task_sleep() ipc_send() ai_run() │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐ │
│  │  Scheduler  │  │Task Manager │  │     IPC     │  │   AI Runtime    │ │
│  │             │  │             │  │  (Planned)  │  │   (Planned)     │ │
│  │• Round-robin│  │• Create     │  │• Queues     │  │• Model loader   │ │
│  │• Preemptive │  │• Kill       │  │• Semaphores │  │• Inference      │ │
│  │• Priority   │  │• Sleep      │  │• Mutexes    │  │• Operators      │ │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └────────┬────────┘ │
├─────────┴────────────────┴────────────────┴──────────────────┴──────────┤
│                    PLATFORM ABSTRACTION LAYER                            │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │  icarus_hal_init()  icarus_hal_context_switch()  icarus_hal_tick()│  │
│  └───────────────────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────────────┤
│                     BOARD SUPPORT PACKAGE (BSP)                          │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐ │
│  │   GPIO    │ │    I2C    │ │    SPI    │ │    USB    │ │  Display  │ │
│  └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ │
├────────┴─────────────┴─────────────┴─────────────┴─────────────┴────────┤
│                        HARDWARE LAYER                                    │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │  STM32H750 (Cortex-M7) │ STM32F4 │ RISC-V │ Zynq │ x86 (Sim)     │  │
│  └───────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Component Summary

| Component | Status | Source Location | Description |
|-----------|--------|-----------------|-------------|
| Scheduler | ✅ Implemented | `Core/Src/icarus/scheduler.c` | Preemptive round-robin |
| Task Manager | ✅ Implemented | `Core/Src/icarus/task.c` | Task lifecycle management |
| Context Switch | ✅ Implemented | `Core/Src/icarus/context_switch.s` | ARM assembly context save/restore |
| Semaphores | ✅ Implemented | `Core/Src/icarus/semaphore.c` | Bounded counting semaphores |
| Message pipes (IPC) | ✅ Implemented | `Core/Src/icarus/pipe.c` | FIFO byte-stream queues (see `pipe.h`) |
| SVC dispatcher | ✅ Implemented | `Core/Src/icarus/svc.c` | 94 numbered call gates (IDs 0–93, see `svc.h`) between unprivileged tasks and privileged kernel state. Every caller buffer an implementation touches is checked against an allowlist first (§4.2.8) |
| **CDC RX ring buffer** | ✅ Implemented (v0.3) | `Core/Src/icarus/cdc_rx.c` | 512 B SPSC USB CDC receive ring; producer is the privileged USB ISR, consumer is any RTOS task. Backing data in `DTCM_DATA_PRIV`, hot path in `ITCM_FUNC`, thread-mode reads through SVC gates 40–42. |
| **Event ring + squelch** | ✅ Implemented (v0.3) | `Core/Src/icarus/event.c` | 32-slot ring of compact 16-byte event entries with a 16-entry per-module severity squelch. Transport-agnostic (drains into a caller-provided buffer). SVC gates 43–48. |
| **CRC16-CCITT helper** | ✅ Implemented (v0.3) | `Core/Src/icarus/crc.c` | `crc16_ccitt(data, len)` with poly 0x1021, init 0xFFFF. **Hardware-accelerated** on STM32H7 via the on-chip CRC peripheral on the AHB4 bus, lazy-initialised on first call. Unprivileged callers go through SVC 89 (v0.5). Portable bytewise fallback under HOST_TEST. |
| **Internal flat-file filesystem** | ✅ Implemented (v0.3) | `Core/Src/icarus/fs.c` | 16 files × 2 KB = 32 KB RAM-backed store with create/open/write/read/delete/list/stats. Functions placed in ITCM; the 32 KB store stays in regular SRAM (won't fit DTCM). |
| **Ground-loadable table engine** | ✅ Implemented (v0.3) | `Core/Src/icarus/tables.c` | Registry of up to 8 tables with double-buffered staging/active swap. CRC-validated load / activate / dump with a registered activate callback that runs in unprivileged thread mode against a stack scratch copy. SVC gates 49–56. |
| **Software Bus** | ✅ Implemented (v0.4) | `Core/Src/icarus/sb.c` | Lightweight pub/sub message router. 32 routes × 4 subscribers per message ID. Best-effort delivery via kernel pipes — if a subscriber's pipe is full the message is silently dropped. Route table in `DTCM_DATA_PRIV`, hot path in `ITCM_FUNC`. SVC gates 72–77; inside the handler the bus calls the `__pipe_*` implementations directly. |
| **Background Checksum** | ✅ Implemented (v0.4) | `Core/Src/icarus/cs.c` | Periodic CRC16-CCITT integrity scanner over up to 8 memory regions. Baselines captured at registration; mismatches reported through a user-supplied callback. HW CRC self-test (0x29B1). Region table in `DTCM_DATA_PRIV`, functions in `ITCM_FUNC`. |
| BSP - IWDG watchdog | ✅ Implemented (v0.4) | `bsp/iwdg.c` | Independent Watchdog abstraction: init, refresh, reset-reason query, flag clear |
| BSP - K1 button | ✅ Implemented (v0.4) | `bsp/button.c` | Raw read of K1 user button (PC13, pressed at `BSP_KEY_PRESSED_LEVEL`; active high on the reference board) |
| BSP - USB CDC transmit ring | ✅ Implemented (v0.5) | `bsp/cdc.c` | One 4 KB transmit ring in privileged DTCM for all CDC output. `cdc_tx_write` (SVC 93 for tasks, direct for privileged code and handlers) queues all-or-nothing or partial; `CDC_Write` / `CDC_WriteString` are all-or-nothing and return false when the ring is full. Nothing waits, sleeps or retries (§3.11) |
| BSP - Console retarget | ✅ Implemented (v0.5) | `bsp/retarget_stdio.c` | Strong `_write()` (and `stdio_write()`, legacy `__io_putchar`) copy console output into the CDC transmit ring, one masked copy per call, keep what fits and count drops (`stdio_get_tx_dropped()`); no line buffer; `printf()` itself is not locked (§3.11.3) |
| BSP - ROM bootloader entry | ✅ Implemented (v0.5) | `bsp/bootloader.c` | `sys_enter_bootloader()` (SVC 90): reset into the ROM USB DFU bootloader without BOOT0/RESET (§3.12) |
| Backup SRAM gates | ✅ Implemented (v0.5) | `Core/Src/icarus/svc.c` | `bkpram_write` (SVC 71) / `bkpram_read` (SVC 86) over the 4 KB backup SRAM, mapped by MPU region 8 as privileged, non-cacheable (§4.2.3) |
| AI Runtime | 🔲 Planned | TBD | Deterministic inference |
| BSP - GPIO | ✅ Implemented | `bsp/gpio.c` | Digital I/O |
| BSP - I2C | ✅ Implemented | `bsp/i2c.c` | I2C communication |
| BSP - SPI | ✅ Implemented | `bsp/spi.c` | SPI communication |
| BSP - Display | ✅ Implemented | `bsp/display.c` | Terminal display with vertical bar |
| BSP - USB | ✅ Implemented | `USB_DEVICE/` | USB CDC serial |


---

## 3. Component Design

### 3.1 Scheduler Component

#### 3.1.1 Design Overview

The scheduler implements preemptive round-robin scheduling with configurable time quantum.

```
┌─────────────────────────────────────────────────────────────┐
│                    SCHEDULER STATE MACHINE                   │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│                        ┌──────────┐                         │
│            os_start()  │   COLD   │  Initial state          │
│                ┌───────┤          ├───────┐                 │
│                │       └──────────┘       │                 │
│                ▼                          │                 │
│         ┌──────────┐              ┌───────┴────┐            │
│         │ RUNNING  │◄────────────│   READY    │            │
│         │          │  schedule() │            │            │
│         └────┬─────┘             └──────▲─────┘            │
│              │                          │                   │
│    yield()   │    ┌──────────┐         │ wake()            │
│    preempt() │    │ BLOCKED  │─────────┘                   │
│              └───►│ (sleep)  │                             │
│                   └──────────┘                             │
│                        │                                    │
│         kill()         │         exit()                     │
│              ┌─────────┴─────────┐                         │
│              ▼                   ▼                          │
│         ┌──────────┐       ┌──────────┐                    │
│         │  KILLED  │       │ FINISHED │                    │
│         └──────────┘       └──────────┘                    │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

#### 3.1.2 Scheduling Algorithm

```
ALGORITHM: Round-Robin Preemptive Scheduler

INPUT: task_list[], current_task_index, os_tick_count
OUTPUT: next task to run

ON SysTick_Handler:
    os_tick_count++
    IF os_running AND scheduler_enabled:
        current_task_ticks_remaining--
        IF current_task_ticks_remaining == 0:
            current_task_ticks_remaining = TICKS_PER_TASK
            trigger_context_switch()  // Set PendSV pending

ON PendSV_Handler (lowest priority):
    save_context(current_task)
    
    // Find next runnable task (round-robin)
    FOR i = 1 TO num_created_tasks:
        next_idx = (current_task_index + i) % num_created_tasks
        task = task_list[next_idx]
        
        IF task.state == BLOCKED:
            IF os_tick_count - task.global_tick_paused >= task.ticks_to_pause:
                task.state = READY  // Wake up
        
        IF task.state == READY OR task.state == RUNNING:
            current_task_index = next_idx
            BREAK
    
    task_list[current_task_index].state = RUNNING
    restore_context(task_list[current_task_index])
```

#### 3.1.3 Timing Characteristics

| Parameter | Value | Configurable |
|-----------|-------|--------------|
| Tick period | 1 ms | Yes (SysTick) |
| Time quantum | 50 ticks (50 ms) | Yes (TICKS_PER_TASK) |
| Context switch time | <10 μs | No |
| Interrupt latency | <5 μs | No |

#### 3.1.4 Requirements Traceability

| Design Element | Implements Requirement |
|----------------|----------------------|
| Round-robin selection | HLR-KRN-011 |
| Preemptive switching | HLR-KRN-010 |
| Configurable quantum | HLR-KRN-012 |
| Voluntary yield | HLR-KRN-013 |
| Tick counter | HLR-KRN-020 |

---

### 3.2 Task Manager Component

#### 3.2.1 Design Overview

The Task Manager handles task lifecycle: creation, execution, sleep, and termination.

#### 3.2.2 Task Control Block (TCB)

```c
typedef struct task {
    void (*function)(void);      // Task entry point
    uint32_t *stack_base;        // Stack base address
    uint32_t stack_size;         // Stack size in words
    uint32_t *stack_pointer;     // Current stack pointer (offset: 12)
    uint8_t task_state;          // Current state (offset: 16)
    uint32_t global_tick_paused; // Tick when sleep started (offset: 20)
    uint32_t ticks_to_pause;     // Sleep duration (offset: 24)
    uint8_t task_priority;       // Priority level
    char name[MAX_TASK_NAME_LENGTH]; // Task name for debug
} task_t;

// Static assertions ensure offsets match assembly expectations
_Static_assert(offsetof(task_t, stack_pointer) == 12);
_Static_assert(offsetof(task_t, task_state) == 16);
```

#### 3.2.3 Task States

| State | Value | Description |
|-------|-------|-------------|
| TASK_COLD | 0 | Created but never run |
| TASK_RUNNING | 1 | Currently executing |
| TASK_READY | 2 | Ready to run |
| TASK_BLOCKED | 3 | Waiting (sleep) |
| TASK_KILLED | 4 | Terminated by another task |
| TASK_FINISHED | 5 | Exited normally |

#### 3.2.4 Stack Frame Layout

```
High Address (stack_base + stack_size)
┌─────────────────────────────────────┐
│           xPSR (0x01000000)         │ ← Initial: Thumb bit set
├─────────────────────────────────────┤
│           PC (function)             │ ← Task entry point
├─────────────────────────────────────┤
│           LR (os_exit_task)         │ ← Return address
├─────────────────────────────────────┤
│           R12                       │
├─────────────────────────────────────┤
│           R3                        │
├─────────────────────────────────────┤
│           R2                        │
├─────────────────────────────────────┤
│           R1                        │
├─────────────────────────────────────┤
│           R0                        │ ← stack_pointer points here
├─────────────────────────────────────┤
│           ...                       │
│       (Task local variables)        │
│           ...                       │
├─────────────────────────────────────┤
Low Address (stack_base)
```

This is the frame of a task that has never run (built by `os_create_task()`
and rebuilt by `__os_restart_task()`): the 8-word basic exception frame and
nothing else. It holds no software-saved registers, so the first switch to
the task pops no R4–R11 and returns with EXC_RETURN 0xFFFFFFFD (thread
mode, process stack, basic frame). Once the task has run, each switch away
from it leaves the hardware frame (basic or extended) and the
software-saved context below its current stack top, and `stack_pointer`
then points at the saved R4 (§3.3.4).

#### 3.2.5 Requirements Traceability

| Design Element | Implements Requirement |
|----------------|----------------------|
| task_t structure | HLR-KRN-001, HLR-KRN-002 |
| Stack allocation | HLR-KRN-003 |
| os_exit_task() | HLR-KRN-004 |
| os_kill_process() | HLR-KRN-005 |
| Task 0 protection | HLR-KRN-006 |
| task_state field | HLR-KRN-007 |


---

### 3.3 Context Switch Component

#### 3.3.1 Design Overview

Context switching is implemented in ARM assembly for Cortex-M7
(`os_yield_pendsv` in `Core/Src/icarus/context_switch.s`, placed in ITCM).
It uses the PendSV exception (lowest priority) to ensure all other
interrupts complete before switching. `PendSV_Handler` is a naked
handler that branches to `os_yield_pendsv`, so LR holds the outgoing
task's EXC_RETURN on entry.

The switch preserves each task's integer and floating-point register
context (HLR-KRN-016). The FPU is enabled for all code (CP10/CP11 full
access in `SystemInit()`), and the kernel does not write FPCCR, so
automatic FP state preservation and lazy stacking stay at the
processor's reset settings (both enabled). Once a task has executed an FP
instruction (CONTROL.FPCA set), every exception taken from it stacks an
extended frame, and its EXC_RETURN has bit 4 clear.

#### 3.3.2 Context Switch Flow

```
┌─────────────────────────────────────────────────────────────┐
│                  CONTEXT SWITCH SEQUENCE                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. TRIGGER (SysTick or os_yield)                           │
│     └── SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk                 │
│                                                             │
│  2. PENDSV ENTRY (hardware automatic)                       │
│     ├── Push xPSR, PC, LR, R12, R3-R0 to PSP                │
│     ├── FP task (CONTROL.FPCA = 1): also reserve S0-S15,    │
│     │   FPSCR (extended frame, written lazily)              │
│     └── LR = EXC_RETURN (bit 4 = 0 for an extended frame)   │
│                                                             │
│  3. SAVE CONTEXT (os_yield_pendsv)                          │
│     ├── MRS R0, PSP                                         │
│     ├── EXC_RETURN bit 4 = 0: VSTMDB R0!, {S16-S31}         │
│     │   (also completes the pending lazy save of S0-S15)    │
│     ├── STMDB R0!, {R4-R11, LR}   // LR = EXC_RETURN        │
│     └── Store R0 to current_task->stack_pointer             │
│                                                             │
│  4. SELECT NEXT TASK (assembly, round-robin)                │
│     ├── Skip KILLED/FINISHED, wake expired BLOCKED tasks    │
│     └── MPU_ConfigureTaskData(next task's data region)      │
│                                                             │
│  5. RESTORE CONTEXT (os_yield_pendsv)                       │
│     ├── Load R1 from next_task->stack_pointer               │
│     ├── Warm task: LDMIA R1!, {R4-R11, LR}                  │
│     │   EXC_RETURN bit 4 = 0: VLDMIA R1!, {S16-S31}         │
│     ├── Cold task: LR = 0xFFFFFFFD (basic frame)            │
│     └── MSR PSP, R1; BX LR                                  │
│                                                             │
│  6. PENDSV EXIT (hardware automatic)                        │
│     └── Pop R0-R3, R12, LR, PC, xPSR (and S0-S15, FPSCR     │
│         for an extended frame) from PSP                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

#### 3.3.3 Assembly Implementation

```asm
/* context_switch.s, os_yield_pendsv (excerpt; task selection omitted) */
.syntax unified
.cpu cortex-m7
.fpu fpv5-d16
.thumb

os_yield_pendsv:
    /* Save the outgoing task's context */
    mrs      r0, psp
    tst      lr, #0x10               /* EXC_RETURN bit 4 clear: FP frame */
    it       eq
    vstmdbeq r0!, {s16-s31}          /* S16-S31, FP tasks only */
    stmdb    r0!, {r4-r11, lr}       /* R4-R11 and EXC_RETURN */
    /* ... store r0 to the current TCB's stack_pointer (offset 12),
     *     choose the next task (r11 = its TCB) ... */

yield_postprocess:
    push     {r0-r3, r12, lr}
    ldr      r0, [r11, #TCB_DATA_PTR]
    bl       MPU_ConfigureTaskData   /* MPU region for the next task */
    pop      {r0-r3, r12, lr}
    ldr      r1, [r11, #TCB_STACK_PTR]
    /* ... mark it RUNNING; a COLD task branches to
     *     increment_running_count ... */

    /* Warm task: restore its context */
    ldmia    r1!, {r4-r11, lr}       /* R4-R11 and its own EXC_RETURN */
    tst      lr, #0x10
    it       eq
    vldmiaeq r1!, {s16-s31}          /* S16-S31, FP tasks only */

branch_to_next_task:
    msr      psp, r1
    bx       lr                      /* hardware unstacks the frame type
                                        recorded in EXC_RETURN */

increment_running_count:
    /* ... running_task_count++ ... */
    ldr      lr, =0xFFFFFFFD         /* cold frame is basic: thread mode,
                                        PSP, no FP state */
    b        branch_to_next_task
```

#### 3.3.4 Saved Context and Floating-Point State (v0.5.0)

A task that has been switched out holds its context on its own stack,
lowest address first:

| Order | Contents | Words | Saved by | Present |
|-------|----------|------:|----------|---------|
| 1 (at `stack_pointer`) | R4–R11, EXC_RETURN | 9 | `stmdb` in `os_yield_pendsv` | Every task switched out |
| 2 | S16–S31 | 16 | `vstmdb` in `os_yield_pendsv` | Only when EXC_RETURN bit 4 = 0 |
| 3 (PSP at PendSV entry) | R0–R3, R12, LR, PC, xPSR (basic frame) | 8 | Hardware, on exception entry | Always |
| 4 | S0–S15, FPSCR, reserved word | 18 | Hardware, on exception entry (lazy stacking: space reserved at entry, registers written on the first FP instruction in the handler) | Only when EXC_RETURN bit 4 = 0 |

```
High address
┌─────────────────────────────────────┐
│  reserved, FPSCR, S15 ... S0        │ ← extended frame only (bit 4 = 0)
├─────────────────────────────────────┤
│  xPSR, PC, LR, R12, R3, R2, R1, R0  │ ← basic frame, always
├─────────────────────────────────────┤ ← PSP at PendSV entry (frame[0])
│  S31 ... S16                        │ ← software, FP frame only
├─────────────────────────────────────┤
│  EXC_RETURN, R11 ... R4             │ ← software, every warm task
└─────────────────────────────────────┘ ← task->stack_pointer (R4)
Low address
```

- **EXC_RETURN per task.** EXC_RETURN is saved with R4–R11 and restored
  from the incoming task's own stack, so each task returns with the
  frame type it was switched out with. Before v0.5.0 the switch saved
  only R4–R11 and returned with the outgoing task's EXC_RETURN: a task
  could resume with another task's S0–S31 and FPSCR, or the hardware
  could unstack the wrong frame type.
- **S16–S31 in software.** The hardware frame holds only S0–S15 and
  FPSCR. The switch saves and restores S16–S31 only for tasks whose
  EXC_RETURN shows an extended frame; a task that has never used the FPU
  costs only the extra EXC_RETURN word.
- **Cold tasks.** A task that has never run, or was restarted by
  `__os_restart_task()`, has only the basic frame built by
  `os_create_task()` (§3.2.4). The switch pops nothing for it and loads
  LR with 0xFFFFFFFD (thread mode, PSP, basic frame), whatever frame type
  the outgoing task had. The first task is launched by `start_cold_task`
  from `os_start()` and does not take this path.
- **Lazy stacking before the MPU reprogram.** With lazy stacking the
  hardware only reserves space for S0–S15 and FPSCR at exception entry.
  `vstmdb` is an FP instruction, so when the outgoing task has an FP
  frame it first completes that pending save into the outgoing task's
  frame. It is the first thing the switch does, before
  `MPU_ConfigureTaskData()` reprograms the MPU for the next task.
- **Frame offsets unchanged.** The basic frame is at the bottom of either
  frame type, so frame[0..7] (R0 … PC, xPSR) keep their offsets whether
  or not the task uses the FPU. The SVC dispatcher reads the caller's PC
  at frame[6] to find the SVC number and compares the frame address with
  PSP to classify the caller (§4.2.8), and the MemManage handler reads
  and advances the stacked PC at frame[6] (§4.2.6); both work unchanged
  with an extended frame.
- **Stack cost.** A switched-out task that has used the FPU holds
  9 + 16 + 26 = 51 words (204 bytes) of context on its stack, against
  9 + 8 = 17 words (68 bytes) for one that has not, plus any alignment
  word the hardware adds to the frame.
- **Verification.** Host tests cannot exercise the switch:
  `context_switch.s` is replaced on the host by `tests/mocks/mock_asm.c`.
  It is verified on target by the target FPU-context probe (two FP-using
  tasks), see SVP §4.7 and `test_traceability.md` §5.

#### 3.3.5 Requirements Traceability

| Design Element | Implements Requirement |
|----------------|----------------------|
| PendSV mechanism | HLR-BSP-004 |
| PSP usage | HLR-KRN-003 (stack isolation) |
| Register save/restore | HLR-KRN-010 (preemption) |
| S16–S31 saved when EXC_RETURN bit 4 = 0; EXC_RETURN kept per task; cold tasks start with 0xFFFFFFFD | HLR-KRN-016 |

---

### 3.4 Critical Section Component

#### 3.4.1 Design Overview

Critical sections protect shared data from concurrent access by disabling the scheduler (not interrupts).

#### 3.4.2 Implementation

```c
static volatile uint8_t critical_stack_depth = 0;
volatile bool scheduler_enabled = true;

static inline void enter_critical(void) {
    scheduler_enabled = false;
    critical_stack_depth++;
}

static inline void exit_critical(void) {
    if (--critical_stack_depth == 0)
        scheduler_enabled = true;
}
```

#### 3.4.3 Design Rationale

| Approach | Pros | Cons | Decision |
|----------|------|------|----------|
| Disable interrupts | Simple, absolute protection | Increases interrupt latency | Not used |
| Disable scheduler | Interrupts still serviced | Only protects from task preemption | **Selected** |
| Spinlock | Fine-grained | Complex, potential deadlock | Future (multi-core) |

#### 3.4.4 Requirements Traceability

| Design Element | Implements Requirement |
|----------------|----------------------|
| scheduler_enabled flag | HLR-KRN-030 |
| critical_stack_depth | HLR-KRN-031 |
| Inline implementation | HLR-KRN-032 (minimal duration) |

---

### 3.5 Semaphore Component

#### 3.5.1 Design Overview

The semaphore component implements bounded counting semaphores for producer-consumer synchronization. Semaphores use non-blocking waits (task_active_sleep) to allow other tasks to run while waiting.

#### 3.5.2 Data Structure

```c
#define MAX_SEMAPHORES 32

typedef struct {
    uint32_t count;                          // Current count
    uint32_t init_count;                     // Maximum capacity (bounded)
    uint8_t consumer_task_idx_list[MAX_TASKS]; // Tasks waiting to consume
    uint8_t feeder_task_idx_list[MAX_TASKS];   // Tasks waiting to feed
    uint8_t num_consumers_queued;            // Number of waiting consumers
    uint8_t num_feeders_queued;              // Number of waiting feeders
    uint32_t tick_updated_at;                // Last update timestamp
    bool engaged;                            // Semaphore is initialized
} semaphore_t;

semaphore_t* semaphore_list[MAX_SEMAPHORES];
```

#### 3.5.3 Semaphore Operations

```
ALGORITHM: Bounded Semaphore Feed (Producer)

INPUT: semaphore_idx
OUTPUT: true on success, false on invalid index

semaphore_feed(semaphore_idx):
    IF semaphore_idx >= MAX_SEMAPHORES OR NOT engaged:
        RETURN false
    
    WHILE count >= init_count:      // Buffer full
        task_active_sleep(1)        // Yield, let consumer run
    
    ENTER_CRITICAL
    count++
    EXIT_CRITICAL
    RETURN true


ALGORITHM: Bounded Semaphore Consume (Consumer)

INPUT: semaphore_idx
OUTPUT: true on success, false on invalid index

semaphore_consume(semaphore_idx):
    IF semaphore_idx >= MAX_SEMAPHORES OR NOT engaged:
        RETURN false
    
    WHILE count == 0:               // Buffer empty
        task_active_sleep(1)        // Yield, let producer run
    
    ENTER_CRITICAL
    count--
    EXIT_CRITICAL
    RETURN true


ALGORITHM: Timed Semaphore Consume (v0.4; timeout on the tick since v0.5.0)

INPUT: semaphore_idx, max_ticks
OUTPUT: true when acquired, false on timeout or invalid index

semaphore_consume_timeout(semaphore_idx, max_ticks):
    IF semaphore_idx >= MAX_SEMAPHORES:
        RETURN false
    
    start = os_get_tick_count()
    WHILE NOT sem_can_consume(semaphore_idx):         // SVC 30
        IF (os_get_tick_count() - start) >= max_ticks:  // unsigned: wrap-safe
            RETURN false                               // max_ticks == 0: try once
        task_active_sleep(1)
    
    sem_decrement(semaphore_idx)                       // SVC 34
    RETURN true
```

The timed consume runs in the calling task's thread mode:
`semaphore_consume_timeout()` calls `__semaphore_consume_timeout()`
directly, and the loop reaches kernel state only through the SVC
wrappers for the tick count, the sleep, `sem_can_consume()` and
`sem_decrement()`. SVC 58 is not used (§4.2.5). The host build also
rejects a semaphore that is not engaged before the loop.

The timeout is measured on the system tick, not by counting sleeps. A
`task_active_sleep(1)` lasts until the scheduler next runs the waiter,
which is a whole round of the other ready tasks' time slices when they
are busy. Up to v0.4 the loop counted iterations, which stretched the
timeout by that factor: with two CPU-bound tasks using their 50-tick
slices, a 1000-tick timeout took about 100 s. The unsigned difference
`now - start` stays correct across a tick-counter wrap. Elapsed time is
checked each time the waiter runs, so the call returns false once at
least `max_ticks` have elapsed, up to one scheduling round later.

On the host, where no other task runs, `__sched_host_set_ticks_per_sleep()`
(`scheduler.h`, HOST_TEST only) makes each `task_active_sleep()` advance
the tick by a set amount, standing in for the other tasks' slices; the
default 0 keeps the tick still.

#### 3.5.4 Blocking Behavior

| Condition | Behavior |
|-----------|----------|
| Feed when count < init_count | Immediate increment |
| Feed when count >= init_count | Block until consumer decrements |
| Consume when count > 0 | Immediate decrement |
| Consume when count == 0 | Block until producer increments |
| Timed consume when count > 0 | Immediate decrement, no sleep |
| Timed consume when count == 0 | Block until producer increments, or return false once at least `max_ticks` system ticks have elapsed since the call |
| Timed consume with `max_ticks` == 0 | Check once; decrement or return false without sleeping |

The blocking uses `task_active_sleep(1)` which yields to the scheduler, allowing other tasks to run. This is cooperative blocking, not busy-waiting.

#### 3.5.5 Requirements Traceability

| Design Element | Implements Requirement |
|----------------|----------------------|
| semaphore_t structure | HLR-KRN-041, HLR-KRN-042, HLR-KRN-043 |
| count field | HLR-KRN-042 (counting) |
| init_count (bounded) | HLR-KRN-043 |
| task_active_sleep blocking | HLR-KRN-049 (bounded WCET) |
| engaged flag | Initialization safety |
| MAX_SEMAPHORES=32 | HLR-KRN-051 |
| `semaphore_consume_timeout()`; `max_ticks` == 0 is a try | HLR-KRN-098 |
| Timeout measured on the system tick (wrap-safe unsigned difference) | HLR-KRN-098.1 |

---

### 3.6 Message Pipe Component

#### 3.6.1 Design Overview

The message pipe component implements FIFO byte-stream communication channels between tasks. Pipes support variable-length messages with bounded capacity and blocking semantics.

#### 3.6.2 Data Structure

```c
#define MAX_MESSAGE_QUEUES 32
#define MAX_PIPE_CAPACITY 512

typedef struct {
    uint8_t buffer[MAX_PIPE_CAPACITY];  // Circular buffer storage
    uint16_t head;                      // Read index
    uint16_t tail;                      // Write index
    uint16_t count;                     // Current bytes in pipe
    uint16_t capacity;                  // Maximum bytes (≤512)
    bool engaged;                       // Pipe is initialized
} message_pipe_t;

message_pipe_t* message_pipe_list[MAX_MESSAGE_QUEUES];
```

#### 3.6.3 Pipe Operations

```
ALGORITHM: Pipe Enqueue (Producer)

INPUT: pipe_idx, data (byte)
OUTPUT: true on success, false on full/invalid

pipe_enqueue(pipe_idx, data):
    IF pipe_idx >= MAX_MESSAGE_QUEUES OR NOT engaged:
        RETURN false
    
    WHILE count >= capacity:        // Pipe full
        task_active_sleep(1)        // Yield, let consumer run
    
    ENTER_CRITICAL
    buffer[tail] = data
    tail = (tail + 1) % capacity
    count++
    EXIT_CRITICAL
    RETURN true


ALGORITHM: Pipe Dequeue (Consumer)

INPUT: pipe_idx
OUTPUT: byte value, or -1 on empty/invalid

pipe_dequeue(pipe_idx):
    IF pipe_idx >= MAX_MESSAGE_QUEUES OR NOT engaged:
        RETURN -1
    
    WHILE count == 0:               // Pipe empty
        task_active_sleep(1)        // Yield, let producer run
    
    ENTER_CRITICAL
    data = buffer[head]
    head = (head + 1) % capacity
    count--
    EXIT_CRITICAL
    RETURN data
```

#### 3.6.4 Circular Buffer Layout

```
┌─────────────────────────────────────────────────────────────┐
│                    CIRCULAR PIPE BUFFER                      │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Index:  0   1   2   3   4   5   6   7   ...  126  127      │
│        ┌───┬───┬───┬───┬───┬───┬───┬───┬─────┬───┬───┐     │
│  Data: │ A │ B │ C │ D │   │   │   │   │ ... │   │   │     │
│        └───┴───┴───┴───┴───┴───┴───┴───┴─────┴───┴───┘     │
│          ▲               ▲                                   │
│          │               │                                   │
│       head=0          tail=4                                │
│                                                              │
│  Empty: count == 0                                          │
│  Full:  count == capacity                                   │
│  FIFO:  head follows tail, wraps at capacity                │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

#### 3.6.5 Pipe Characteristics

| Property | Value | Rationale |
|----------|-------|-----------|
| Ordering | FIFO | HLR-KRN-047 |
| Capacity | 1-512 bytes | HLR-KRN-053 |
| Message size | Variable (1-N bytes) | HLR-KRN-048 |
| Blocking | Cooperative (task_active_sleep) | HLR-KRN-050 |
| Atomicity | Critical sections | Data integrity |
| Max pipes | 32 | HLR-KRN-052 |

#### 3.6.6 Requirements Traceability

| Design Element | Implements Requirement |
|----------------|----------------------|
| message_pipe_t structure | HLR-KRN-040 |
| Circular buffer | HLR-KRN-047 (FIFO) |
| Variable count | HLR-KRN-048 (variable-length) |
| task_active_sleep blocking | HLR-KRN-050 (blocking) |
| MAX_MESSAGE_QUEUES=32 | HLR-KRN-052 |
| MAX_PIPE_CAPACITY=128 | HLR-KRN-053 |
| Bounded operations | HLR-KRN-046 (bounded WCET) |

---

### 3.7 Print Buffer Component (Superseded in v0.5.0)

#### 3.7.1 Design Overview

**Superseded.** The 256-byte print buffer in `task.c`, its transmit task
and the later per-task stdio line buffers have been removed. All console
and raw USB CDC output now goes through the single USB CDC transmit ring
described in §3.11 (HLR-BSP-027, HLR-BSP-028). The section number is kept
so that existing references stay valid.


---

### 3.8 Mutex Component (Planned)

#### 3.8.1 Design Overview

Inter-Process Communication provides synchronization and data exchange between tasks.

#### 3.8.2 Message Queue Design

```c
typedef struct {
    uint8_t *buffer;           // Queue storage
    size_t item_size;          // Size of each item
    size_t capacity;           // Maximum items
    volatile size_t head;      // Dequeue index
    volatile size_t tail;      // Enqueue index
    volatile size_t count;     // Current item count
    task_t *waiting_send;      // Tasks blocked on send
    task_t *waiting_recv;      // Tasks blocked on receive
} ipc_queue_t;
```

#### 3.8.3 Queue Operations

| Operation | Blocking | Timeout | WCET |
|-----------|----------|---------|------|
| `ipc_queue_send()` | Optional | Yes | O(1) + wake |
| `ipc_queue_receive()` | Optional | Yes | O(1) + wake |
| `ipc_queue_peek()` | No | N/A | O(1) |
| `ipc_queue_count()` | No | N/A | O(1) |

#### 3.8.4 Mutex Design

```c
typedef struct {
    volatile int32_t count;    // Current count (signed for mutex)
    int32_t max_count;         // Maximum count (1 for binary)
    task_t *waiting_list;      // Tasks blocked on acquire
    task_t *owner;             // For mutex: current owner
    uint8_t original_priority; // For priority inheritance
} ipc_sem_t;
```

#### 3.8.5 Requirements Traceability

| Design Element | Implements Requirement |
|----------------|----------------------|
| ipc_queue_t | HLR-KRN-044 (planned) |
| ipc_sem_t (count=1) | HLR-KRN-044 (planned) |
| ipc_sem_t (count>1) | HLR-KRN-044 (planned) |
| owner + priority fields | HLR-KRN-044 (planned) |
| Bounded operations | HLR-KRN-046 (planned) |

---

### 3.9 AI Runtime Component (Planned)

#### 3.9.1 Design Overview

The AI Runtime provides deterministic neural network inference with certifiable properties.

#### 3.9.2 Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    AI RUNTIME ARCHITECTURE                   │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │                    AI API Layer                      │    │
│  │  ai_model_load()  ai_inference_run()  ai_validate() │    │
│  └─────────────────────────┬───────────────────────────┘    │
│                            │                                 │
│  ┌─────────────────────────▼───────────────────────────┐    │
│  │                  Model Manager                       │    │
│  │  • Model parsing (flatbuffer/custom)                │    │
│  │  • Memory allocation (static)                       │    │
│  │  • Integrity verification (CRC32)                   │    │
│  │  • Operator validation (whitelist)                  │    │
│  └─────────────────────────┬───────────────────────────┘    │
│                            │                                 │
│  ┌─────────────────────────▼───────────────────────────┐    │
│  │                 Inference Engine                     │    │
│  │  • Layer-by-layer execution                         │    │
│  │  • Tensor memory management                         │    │
│  │  • Intermediate buffer reuse                        │    │
│  │  • WCET tracking per layer                          │    │
│  └─────────────────────────┬───────────────────────────┘    │
│                            │                                 │
│  ┌─────────────────────────▼───────────────────────────┐    │
│  │              Certified Operator Library              │    │
│  │  ┌───────┐ ┌───────┐ ┌───────┐ ┌───────┐ ┌───────┐ │    │
│  │  │Conv2D │ │ Dense │ │ ReLU  │ │Softmax│ │MaxPool│ │    │
│  │  │ int8  │ │ int8  │ │ int8  │ │ int16 │ │ int8  │ │    │
│  │  └───────┘ └───────┘ └───────┘ └───────┘ └───────┘ │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

#### 3.9.3 Model Structure

```c
typedef struct {
    uint32_t magic;            // "ICAR" (0x52414349)
    uint32_t version;          // Model format version
    uint32_t crc32;            // Integrity check
    uint32_t num_layers;       // Layer count
    uint32_t input_size;       // Input tensor size (bytes)
    uint32_t output_size;      // Output tensor size (bytes)
    uint32_t scratch_size;     // Intermediate buffer size
    uint32_t wcet_us;          // Worst-case execution time
    ai_layer_t layers[];       // Layer definitions
} ai_model_header_t;

typedef struct {
    uint8_t op_type;           // Operator type (enum)
    uint8_t activation;        // Activation function
    uint16_t flags;            // Layer flags
    uint32_t input_offset;     // Input tensor offset
    uint32_t output_offset;    // Output tensor offset
    uint32_t weights_offset;   // Weights offset
    uint32_t bias_offset;      // Bias offset
    uint16_t params[8];        // Operator-specific params
} ai_layer_t;
```

#### 3.9.4 Operator Specifications

| Operator | Input Type | Output Type | WCET Formula |
|----------|------------|-------------|--------------|
| Conv2D | int8 | int8 | O(H×W×C_in×C_out×K²) |
| Dense | int8 | int8 | O(N_in×N_out) |
| ReLU | int8 | int8 | O(N) |
| Softmax | int8 | int16 | O(N) |
| MaxPool2D | int8 | int8 | O(H×W×C×K²) |
| Add | int8 | int8 | O(N) |
| Concat | int8 | int8 | O(N) |

#### 3.9.5 Determinism Guarantees

| Property | Mechanism |
|----------|-----------|
| No dynamic allocation | Static tensor buffers |
| Bounded execution | WCET per layer, total budget |
| Reproducible results | Fixed-point only, no FP |
| Verifiable structure | Operator whitelist |
| Integrity | CRC32 on model load |

#### 3.9.6 Requirements Traceability

| Design Element | Implements Requirement |
|----------------|----------------------|
| ai_model_load() | HLR-AI-001 |
| CRC32 verification | HLR-AI-002 |
| Operator whitelist | HLR-AI-003, HLR-AI-004 |
| Layer-by-layer execution | HLR-AI-010 |
| int8/int16 types | HLR-AI-011, HLR-AI-014, HLR-AI-015 |
| wcet_us field | HLR-AI-012 |
| Static buffers | HLR-AI-013 |

### 3.10 Shared Service Modules (v0.3.0)

The shared service modules provide reusable kernel infrastructure that
sits at the same architectural level as the IPC primitives but addresses
domains common to most embedded applications: serial input
buffering, structured event logging, integrity checks, scratch storage,
and ground-loadable configuration tables.

All five modules follow the kernel's existing privilege-separation
pattern: backing data lives in `DTCM_DATA_PRIV`, hot functions are
placed in `ITCM_FUNC`, and every public entry point that an unprivileged
task may call is split into a `__`-prefixed privileged implementation
plus an `svc.c` wrapper that issues a numbered SVC instruction. Under
`HOST_TEST` the wrappers short-circuit straight to the `__`
implementation so the host suite needs no special-casing.

#### 3.10.1 CDC RX Ring Buffer (`Core/Src/icarus/cdc_rx.c`)

| Property | Value |
|---|---|
| Backing storage | 512 B ring + head/tail cursors in `.dtcm_priv` |
| Hot path placement | `.itcm` |
| SVC numbers | 40 (`SVC_CDC_RX_INIT`), 41 (`SVC_CDC_RX_READ_BYTE`), 42 (`SVC_CDC_RX_AVAILABLE`), 91 (`SVC_CDC_RX_DROPPED`, v0.5) |
| Overflow accounting | Bytes that do not fit are dropped and counted since the last `cdc_rx_init()`; `cdc_rx_dropped()` returns the count (HLR-KRN-090.3) |
| ISR producer path | `cdc_rx_push` calls `__cdc_rx_push` directly (no SVC) — the USB CDC ISR is already in privileged handler context |

Single-producer / single-consumer lock-free ring buffer for incoming
serial data over the kernel's USB CDC class. The producer is the
`CDC_Receive_FS` callback which executes in the privileged USB ISR; the
consumer is any RTOS task draining bytes via `cdc_rx_read_byte`. No
locking is needed for the ring head/tail because aligned 32-bit writes
are atomic on Cortex-M7 and the producer/consumer roles are fixed.

#### 3.10.2 Event Ring + Per-Module Squelch (`Core/Src/icarus/event.c`)

| Property | Value |
|---|---|
| Backing storage | 32 × 16-byte entries + 16 squelch bytes + cursors in `.dtcm_priv` |
| Hot path placement | `.itcm` |
| SVC numbers | 43–48 (init, os_event, set/get_squelch, drain, get_count) |
| Drain semantics | FIFO; oldest entries first; partial drains supported |

A transport-agnostic structured event ring. Producers call
`os_event(module_id, severity, event_id, payload, payload_len)` which
packs the event into a fixed 16-byte slot. Per-module severity squelch
filters at emission time so dropped events never consume ring slots.
Downstream consumers drain entries into a caller-provided buffer via
`event_drain` and decide independently how to ship them (CCSDS
telemetry, file logging, etc); the kernel module itself has no
transport coupling.

The five user-level args of `os_event` pack into three SVC registers
as `R0 = (event_id << 16) | (severity << 8) | module_id`,
`R1 = payload pointer`, `R2 = payload_len`, staying inside the AAPCS
R0–R3 budget without spilling to the caller's stack.

#### 3.10.3 CRC16-CCITT Helper (`Core/Src/icarus/crc.c`)

| Property | Value |
|---|---|
| Polynomial | 0x1021 |
| Initial value | 0xFFFF |
| Hardware backend | STM32H7 on-chip CRC peripheral on the AHB4 bus, lazy-initialised on first call |
| Host fallback | Portable bytewise loop under `HOST_TEST` |
| Thread safety | Each call wraps a critical section because the CRC peripheral has internal state shared across tasks |
| Placement | `ITCM_FUNC` |
| SVC number | 89 (`SVC_CRC16_CCITT`, v0.5). Each computation runs inside a critical section that touches privileged scheduler state, so an unprivileged caller's `crc16_ccitt()` goes through the SVC, which validates the input buffer for reading (§4.2.8). Privileged code and handlers call `__crc16_ccitt()` directly (`svc_caller_is_privileged()`, §4.2.9) |

Common dependency for CCSDS Space Packet integrity checks and small
flash record protection. The hardware path is approximately 4× faster
than the bytewise software loop on the buffer sizes used by downstream
consumers (kvstore record validation, table-services data CRC).

#### 3.10.4 Internal Flat-File Filesystem (`Core/Src/icarus/fs.c`)

| Property | Value |
|---|---|
| Capacity | 16 files × 2 KB = 32 KB total |
| Backing storage | Static byte array in regular SRAM (32 KB will not fit DTCM) |
| Hot path placement | `.itcm` |
| Concurrency | `enter_critical()` / `exit_critical()` around table mutations |

A minimal flat-file storage layer with `create / open / write / read /
delete / list / stats`. The on-disk format is opaque so a real flash
backend can be wired in later without changing the public API.

The 32 KB store is the only shared module that does **not** live in
`DTCM_DATA_PRIV` — it would consume a quarter of the entire DTCM
budget, which is uneconomic given the kernel control state already
allocated there. The deliberate trade-off is documented in the source
header. If MPU lockdown becomes a hard requirement for the filesystem
data, the alternatives are (a) shrink to ≤ 16 KB and move into DTCM,
or (b) place the store in RAM_D2 with an MPU region carved out per
caller.

#### 3.10.5 Ground-Loadable Table Engine (`Core/Src/icarus/tables.c`)

| Property | Value |
|---|---|
| Capacity | Up to 8 registered tables, each ≤ 512 B |
| Backing storage | 8 × `tbl_slot_t` (≈ 1056 B each) in `.dtcm_priv`, total ≈ 8.3 KB |
| Hot path placement | `.itcm` |
| SVC numbers | 49 (init), 50 (register), 51 (load), 52 (activate_prepare), 53 (activate_commit), 54 (dump), 55 (get_descriptor), 56 (count); v0.5: 87 (get_info), 88 (abort), 92 (load_at) |
| Activate callback | Runs in unprivileged thread mode against a stack scratch copy — never sees `DTCM_PRIV` |

Generic ground-loadable table engine. Tables are validated by schema
CRC and CRC16 data checksum, then committed via a registered activate
callback. Application-specific table identifiers (e.g. FDIR rules,
scheduler slots, limit-checker watchpoints) live in downstream
consumers; the engine itself is generic.

`tbl_activate` is the only call in any of the new modules that must
straddle privileged and unprivileged execution: the registered activate
callback is user code that may issue further SVCs, so it cannot run
inside the SVC handler. The wrapper splits the operation across two
SVCs:

```
__tbl_activate_prepare()  ── SVC 52: validate + copy staging into a
                                     thread-mode scratch buffer
user activate(scratch)    ── runs in thread mode, no SVC
__tbl_activate_commit()   ── SVC 53: copy scratch into the active buffer
```

The active buffer never moves until the callback succeeds, so a
rejecting callback leaves the previous configuration in place
atomically. Since v0.5.0 the commit SVC is accepted only right after a
successful prepare for the same table (the slot's `prepared` flag),
with the descriptor-size length and data identical to what was staged,
so a stray commit cannot install arbitrary bytes (HLR-KRN-094.7).

**v0.5.0 additions:**

- `tbl_load_at(id, offset, data, len, schema_crc)` (SVC 92) writes a
  chunk at an explicit offset for chunked loads over a lossy link.
  Offset 0 starts a new load (discarding any earlier staging), except
  that an identical copy of the first chunk sent while that load is
  still incomplete is a retransmit and changes nothing. For chunks at
  offset > 0, each next chunk must start where the staged data ends; a
  chunk wholly inside the bytes already staged is a retransmit and is
  accepted only if identical; gaps, conflicting retransmits and a schema
  CRC that differs from the first chunk's are rejected without touching
  staging. A chunk that overruns the descriptor size is rejected at any
  offset. When
  the staged length reaches the descriptor size the data CRC is computed
  and staging is marked valid. `tbl_load` remains as the append form
  (HLR-KRN-094.4).
- `tbl_abort(id)` (SVC 88) discards staged bytes; the active buffer is
  unchanged (HLR-KRN-094.5).
- `tbl_get_info(id, out)` (SVC 87) copies the descriptor fields and the
  staging/active state into a caller `tbl_info_t`. The pointer returned
  by `tbl_get_descriptor` refers to privileged memory and must not be
  dereferenced by unprivileged code (HLR-KRN-094.6).

#### 3.10.6 Section Footprint Summary

```
cdc_rx.o    .itcm  =   156 B   .dtcm_priv =    520 B
event.o     .itcm  =   336 B   .dtcm_priv =    536 B
tables.o    .itcm  =   796 B   .dtcm_priv =  8 484 B
crc.o       .itcm  ≈    96 B   .dtcm_priv =      0 B   (pure function)
fs.o        .itcm  ≈ 1 100 B   regular SRAM = 32 KB    (32 KB store)
```

The DTCM total grows by ≈ 9.6 KB on top of the existing kernel
control state — well within the 128 KB budget.

#### 3.10.7 Traceability

| Design Element | Implements Requirement |
|---|---|
| `cdc_rx_*` | HLR-KRN-090 |
| `os_event` / `event_drain` / squelch | HLR-KRN-091 |
| `crc16_ccitt` (HW peripheral) | HLR-KRN-092 |
| `fs_*` (16-file flat store) | HLR-KRN-093 |
| `tbl_load` / `tbl_activate` / `tbl_dump` | HLR-KRN-094 |

#### 3.10.8 Software Bus (`Core/Src/icarus/sb.c`)

| Property | Value |
|---|---|
| Route table | 32 entries × 4 subscriber slots each in `.dtcm_priv` |
| Hot path placement | `.itcm` |
| SVC numbers | 72–77 (`SVC_SB_INIT`, `SUBSCRIBE`, `UNSUBSCRIBE`, `PUBLISH`, `SUBSCRIBER_COUNT`, `ROUTE_COUNT`); inside the handler the bus calls the `__pipe_*` implementations directly (no nested SVC). `sb_publish` validates the payload buffer for reading (§4.2.8) |
| Thread safety | `enter_critical()` / `exit_critical()` around route table mutations |

Lightweight publish/subscribe message router built on top of the kernel
pipe IPC. Tasks subscribe to 16-bit message IDs; publishers push
messages by ID and the bus copies the payload into every subscriber's
pipe automatically.

Each route entry maps a `sb_msg_id_t` to up to `SB_MAX_SUBS_PER_MSG`
(4) pipe indices. Publishing iterates the subscriber list and calls
`pipe_can_enqueue` + `pipe_write_bytes` for each subscriber. If a
subscriber's pipe is full the message is silently dropped for that
subscriber — the publisher is never blocked (best-effort delivery).

Zero dynamic allocation: the route table is a static array of 32
`sb_route_t` structs in DTCM_DATA_PRIV. Subscribe/unsubscribe
operations are O(n) in the route count; publish is O(subscribers).

```
sb_subscribe(msg_id, pipe_idx)
    ├─ Find or create route entry for msg_id
    ├─ Reject if route table full or subscriber limit reached
    └─ Store (msg_id, pipe_idx) pair

sb_publish(msg_id, data, len)
    ├─ Find route entry for msg_id
    ├─ For each subscriber pipe:
    │   ├─ pipe_can_enqueue(pipe_idx, len)?
    │   │   ├─ Yes → pipe_write_bytes(pipe_idx, data, len); delivered++
    │   │   └─ No  → skip (best-effort drop)
    └─ Return delivered count
```

#### 3.10.9 Background Checksum Integrity Monitor (`Core/Src/icarus/cs.c`)

| Property | Value |
|---|---|
| Region table | 8 × `cs_region_t` in `.dtcm_priv` |
| Hot path placement | `.itcm` |
| SVC numbers | 63–70 (`SVC_CS_INIT`, `SET_CALLBACK`, `ADD_REGION`, `ENABLE`, `REBASELINE`, `CHECK_ALL`, `GET_REGION`, `REGION_COUNT`) |
| CRC backend | `crc16_ccitt()` from `icarus/crc.h` (HW-accelerated on target); inside the handler it uses the peripheral directly, without a nested SVC (§4.2.9) |
| Self-test | On `cs_init()`: CRC16 of `"123456789"` must equal 0x29B1 |
| Thread safety | All public functions are SVC call gates; the region table is only touched in privileged mode |
| Callback context (v0.5) | The mismatch callback runs in the calling task's thread mode after the SVC returns, never inside the handler (HLR-KRN-096.2) |

Periodically computes CRC16-CCITT over registered memory regions and
compares against baseline values captured at registration time. Typical
use: monitor flash code segments, critical data tables, and SRAM guard
patterns to detect bit-flips from radiation (SEU) or software
corruption.

Each region is described by a `cs_region_t` struct containing the start
address, size, baseline CRC, and an enabled flag. `cs_add_region`
computes the baseline CRC immediately from the current memory contents.
`cs_check_all` iterates all enabled regions, recomputes the CRC, and
reports failures through the user-supplied mismatch callback.

Until v0.5.0 the callback was invoked from inside the SVC handler, so a
callback that made any kernel call issued a nested SVC, which is a
HardFault on target. The scan is now split: the privileged
implementation `__cs_check_all()` records each mismatch (and the
registered callback) in a caller-provided `cs_scan_result_t`, and the
thread-mode wrapper delivers the recorded mismatches after the SVC has
returned. The callback may therefore use any kernel API.

```
cs_add_region(idx, addr, size)
    ├─ Validate idx < CS_MAX_REGIONS, addr != NULL, size > 0
    ├─ Store addr, size in region table
    ├─ Compute baseline = crc16_ccitt(addr, size)
    └─ Set enabled = true

cs_check_all()                               (thread mode)
    ├─ SVC 68 → __cs_check_all(&result)      (handler mode)
    │   ├─ Engine self-test failed → record (0xFF, ...); scan no region
    │   └─ Otherwise, for each enabled region:
    │       ├─ actual = crc16_ccitt(region.addr, region.size)
    │       ├─ actual == region.baseline?
    │       │   ├─ Yes → pass
    │       │   └─ No  → record (idx, expected, actual); failures++
    ├─ For each recorded mismatch:
    │   └─ result.callback(idx, expected, actual)   (thread mode)
    └─ Return failure count
```

#### 3.10.10 Traceability (v0.4.0 additions)

| Design Element | Implements Requirement |
|---|---|
| `sb_init` / `sb_subscribe` / `sb_publish` | HLR-KRN-095 |
| `cs_init` / `cs_add_region` / `cs_check_all` | HLR-KRN-096 |
| `os_restart_task` | HLR-KRN-097 |
| `semaphore_consume_timeout` (thread mode; timeout measured on the system tick since v0.5, §3.5.3) | HLR-KRN-098, HLR-KRN-098.1 |
| `dispatch_count` / `stack_watermark` / `os_get_task_state` | HLR-KRN-099 |
| `IWDG_Init` / `IWDG_Refresh` / `IWDG_WasReset` | HLR-BSP-025 |
| `Button_IsPressed` | HLR-BSP-026 |
| `CDC_Write` / `CDC_WriteString` (all-or-nothing on the transmit ring, v0.5) | HLR-BSP-027 |

#### 3.10.11 Traceability (v0.5.0 additions)

| Design Element | Implements Requirement |
|---|---|
| `cdc_rx_dropped` (SVC 91) | HLR-KRN-090.3 |
| `crc16_ccitt` SVC path (SVC 89) | HLR-KRN-092.3 |
| `tbl_load_at` (SVC 92) | HLR-KRN-094.4 |
| `tbl_abort` (SVC 88) | HLR-KRN-094.5 |
| `tbl_get_info` (SVC 87) | HLR-KRN-094.6 |
| `__tbl_activate_commit` `prepared` gate | HLR-KRN-094.7 |
| `__cs_check_all` + thread-mode callback delivery | HLR-KRN-096.2 |

### 3.11 USB CDC Transmit Path (v0.5.0)

#### 3.11.1 Design Overview

Every byte sent to the USB CDC IN endpoint — `printf` output, single
characters from `__io_putchar`, raw `CDC_Write` traffic and any other
producer — goes through one byte ring owned by privileged code
(`Core/Src/bsp/cdc.c`). Producers copy their bytes in and return at
once; nothing waits, sleeps or spins, so the path is safe from any task
and any configurable-priority interrupt or exception handler (not NMI or
HardFault, see Locking below), and a host that holds the port open
without reading cannot stall a caller.

This replaces two earlier designs that failed under preemption: the
stdio line buffers were shared by preemptive tasks without a lock (a
task preempted mid-flush let the next printer write past the buffer and
hand the USB stack a buffer it was still sending), and `CDC_Write`
retried up to 500 × `task_active_sleep(1)`, so callers missed their
deadlines whenever the host stopped reading.

| Property | Value |
|---|---|
| Ring size | `CDC_TX_RING_SIZE` = 4096 B (overridable), in `.dtcm_priv` |
| Largest transfer | `CDC_TX_MAX_CHUNK` = 2048 B; the class driver appends a zero-length packet when needed |
| Hot path placement | `.itcm` (`__cdc_tx_write`, `tx_start_locked`, completion, reset, kick and DTR hooks) |
| SVC number | 93 (`SVC_CDC_TX_WRITE`) for unprivileged tasks; buffer validated for reading (§4.2.8) |
| Privileged / handler callers | Call `__cdc_tx_write()` directly (`svc_caller_is_privileged()`, §4.2.9) |
| Locking | PRIMASK saved, interrupts masked, restored (nests); the masked window is one copy of at most the free ring space plus one transfer start. PRIMASK does not mask NMI or HardFault, so those handlers must not write to the ring |
| Drop accounting | Bytes refused or dropped for lack of ring space, and bytes discarded on port close, reopen or link loss, are added to a counter (`__cdc_tx_dropped()`, privileged). A NULL or empty call, and a buffer that SVC 93 rejects, are not counted |

#### 3.11.2 Ring Operation

```
         tail                              head
          | in flight |   queued   |  free  |

tx_count    = bytes queued, including the transfer in flight
tx_inflight = bytes currently owned by the USB stack (0 = idle)
```

```
__cdc_tx_write(data, len, whole)             (interrupts masked)
    ├─ space = RING_SIZE - tx_count
    ├─ n = len; if n > space: n = whole ? 0 : space
    ├─ copy n bytes at head (wrapping); head += n; tx_count += n
    ├─ tx_dropped += len - n
    ├─ tx_start_locked()        also retries data queued while unconfigured
    └─ return n

tx_start_locked()
    ├─ if tx_inflight != 0 or tx_count == 0: return
    ├─ run = min(RING_SIZE - tail, tx_count, CDC_TX_MAX_CHUNK)
    ├─ tx_inflight = run
    └─ CDC_Transmit_FS(&ring[tail], run) != OK → tx_inflight = 0
                                          (data stays queued for the next try)

__cdc_tx_on_complete()                       (USB transfer-complete ISR)
    ├─ tail += tx_inflight; tx_count -= tx_inflight; tx_inflight = 0
    └─ tx_start_locked()

__cdc_tx_on_link_reset()                     (CDC interface init / de-init)
    └─ tx_inflight = 0          bytes stay at the tail and are sent again
                                (unless the de-init's DTR report below
                                discards them)

__cdc_tx_kick()                   (host sets control lines; bus resumes)
    └─ tx_start_locked()

__cdc_tx_on_dtr(asserted)                    (interrupts masked)
    ├─ asserted == tx_dtr                     → return 0 (no change)
    ├─ !asserted (close) or tx_opened (reopen) → n = __cdc_tx_discard_queued()
    ├─ first clear → set since boot           → keep queued bytes (n = 0)
    ├─ if asserted: tx_opened = true
    ├─ tx_dtr = asserted
    └─ return n                 (starts no transfer; the caller kicks)

__cdc_tx_discard_queued()
    ├─ n = tx_count - tx_inflight            bytes behind the in-flight run
    ├─ head = tail + tx_inflight; tx_count = tx_inflight
    ├─ tx_dropped += n
    └─ return n
```

The bytes handed to the USB stack are read in place by the USB
interrupt after `CDC_Transmit_FS()` has returned (the OTG FS core runs
without DMA, so privileged DTCM is a valid transmit buffer). Producers
never write into the in-flight run, and the tail advances only in the
transfer-complete interrupt. Because the idle check and the transfer
start happen under one interrupt mask, two producers in different tasks
and the USB interrupt cannot both start a transfer.

A USB reset or re-configuration re-initialises the CDC interface, which
calls `__cdc_tx_on_link_reset()` (from `CDC_Init_FS()` and
`CDC_DeInit_FS()`); a transfer lost to the reset therefore cannot leave
the ring marked busy forever. `CDC_DeInit_FS()` (reset or disconnect:
the link is lost) then calls `__cdc_tx_on_dtr(false)`, so if the port
was open the loss is handled as a close and the queued bytes, including
the lost transfer's, are discarded (HLR-BSP-027.5). While the device is
not configured, or the bus is suspended, `CDC_Transmit_FS()` fails, so
output stays queued up to the ring capacity and goes out on the next
write, the next completion, when the host sets the control lines
(opening the port), or when the bus resumes. The suspend callback stops
the PHY clock and nothing else restarts it on a host-initiated resume,
so `HAL_PCD_ResumeCallback()` first restarts it
(`__HAL_PCD_UNGATE_PHYCLOCK`), then restores the device state
(`USBD_LL_Resume()`) and calls `__cdc_tx_kick()` (HLR-BSP-027.6). The
resume path is host-tested only (the ring functions it calls); it is not
exercised on hardware.

A host that closes the port stops reading, so output written while it
is closed would otherwise reach the next open as if it were new.
`CDC_Control_FS()` passes every `SET_CONTROL_LINE_STATE` DTR report to
`__cdc_tx_on_dtr()` and then kicks the ring. On a change of DTR from set
to clear (close), and from clear to set after an earlier open since boot
(reopen), the bytes queued behind the transfer in flight are discarded
and counted as dropped. Discarding on the reopen as well as the close
matters because the stale bytes pile up after the close. The first
clear-to-set since boot keeps them, so output queued before the first
open (such as a boot banner) is sent. A report that does not change DTR
does nothing. The transfer in flight is never discarded — the USB stack
is still reading it from the ring, and its completion releases it as
usual — so up to `CDC_TX_MAX_CHUNK` (2048) bytes written before a close
can still arrive after the reopen (HLR-BSP-027.7). Close and reopen are
exercised on hardware (the device keeps working across a 30 s close and
reopen); the discard behaviour is host-tested only.

#### 3.11.3 Producers

| Producer | Mode | Result when the ring is full | Requirement |
|---|---|---|---|
| `cdc_tx_write(data, len, whole)` | All-or-nothing (`whole = true`) or partial | Returns the bytes queued (0, or what fitted) | HLR-BSP-027.2 |
| `CDC_Write(data, len)` / `CDC_WriteString(s)` | All-or-nothing | Returns false; nothing queued. `len == 0` and `s == NULL` are no-op successes | HLR-BSP-027 |
| `_write()` (strong override of the weak newlib stub, which looped over `__io_putchar`; `printf`, `puts`, `fwrite(stdout)`) | Partial via `stdio_write()` | Keeps what fits, adds the rest to `stdio_get_tx_dropped()`, returns `len` so newlib never retries or marks stdout failed | HLR-BSP-028, HLR-BSP-028.1 |
| `stdio_write(data, len)` | Partial, in chunks of at most 65535 bytes | Returns the bytes queued; the rest (or all of them, for a buffer an unprivileged caller may not pass) are counted as dropped | HLR-BSP-028, HLR-BSP-028.1 |
| `__io_putchar(ch)` (legacy entry for direct callers; newlib no longer uses it) | Partial (1 byte) via `stdio_write()` | Byte dropped and counted; returns `ch` | HLR-BSP-028 |

The console retarget (`Core/Src/bsp/retarget_stdio.c`) keeps no buffer
of its own. Its drop counter lives in ordinary RAM so unprivileged code
can read it, and is updated with an atomic add because printing tasks
can preempt each other. Line buffering is left to the C library's stdout
buffering (HLR-BSP-028.2).

**Concurrency (HLR-BSP-028.3).** Each `_write()` / `stdio_write()` call
is copied into the ring in one masked step, so no other producer's bytes
land inside it. `printf()` and the other stdio functions are not made
thread-safe: newlib-nano formats into the stdout `FILE` buffer that all
callers share, and its lock hooks (`__retarget_lock_*`) are the library's
no-op stubs in this build. Concurrent `printf()` from preemptive tasks,
or from a task and an interrupt handler, can interleave or duplicate
characters. Callers that need whole lines serialise their `printf()`
calls (inside a critical section or from a single task) or format into
their own buffer and call `stdio_write()`.

#### 3.11.4 Requirements Traceability

| Design Element | Implements Requirement |
|---|---|
| `CDC_Write` / `CDC_WriteString` → `cdc_tx_write(..., true)` | HLR-BSP-027 |
| Single ring `tx_ring[CDC_TX_RING_SIZE]` in `.dtcm_priv` | HLR-BSP-027.1 |
| `cdc_tx_write` whole/partial, `tx_dropped`, SVC 93 | HLR-BSP-027.2 |
| No wait, sleep or retry in any producer | HLR-BSP-027.3 |
| `tx_lock` / `tx_start_locked`, `CDC_TX_MAX_CHUNK`, `__cdc_tx_on_complete` | HLR-BSP-027.4 |
| `__cdc_tx_on_link_reset`; `__cdc_tx_on_dtr(false)` in `CDC_DeInit_FS` | HLR-BSP-027.5 |
| `__cdc_tx_kick` on port open and bus resume (after the PHY clock restart in `HAL_PCD_ResumeCallback`), retry on next write / completion | HLR-BSP-027.6 |
| `__cdc_tx_on_dtr` → `__cdc_tx_discard_queued` on close and reopen, from `CDC_Control_FS` and `CDC_DeInit_FS` | HLR-BSP-027.7 |
| `stdio_write`, `_write`, `__io_putchar` | HLR-BSP-028 |
| `stdio_get_tx_dropped` (atomic counter) | HLR-BSP-028.1 |
| No retarget-layer buffer | HLR-BSP-028.2 |
| One masked copy per `_write()` / `stdio_write()` call; `printf()` serialisation left to callers | HLR-BSP-028.3 |

### 3.12 ROM Bootloader Entry (v0.5.0)

`sys_enter_bootloader()` (SVC 90, `Core/Src/bsp/bootloader.c`) hands the
chip to the built-in ROM bootloader (USB DFU on the reference board) so
a new image can be loaded without touching BOOT0/RESET.

Jumping straight from a running application is unreliable: clocks,
caches, MPU, interrupts and an already-started watchdog would leak into
the bootloader. The request is therefore recorded and the chip reset:

```
sys_enter_bootloader()                       (task → SVC 90)
    ├─ __disable_irq()
    ├─ bootloader_request = BSP_BOOTLOADER_MAGIC   (.ram_d3: NOLOAD, not
    │                                                zeroed by startup)
    ├─ SCB_DisableDCache()      cleans, so the marker reaches SRAM4
    └─ NVIC_SystemReset()

Reset_Handler → bsp_bootloader_check()       (after RAM init, before
    │                                          main(), clocks, watchdog)
    ├─ bootloader_request != MAGIC → return (normal boot)
    ├─ clear the request
    └─ jump to the ROM bootloader (BSP_SYSTEM_BOOTLOADER_ADDR)
```

Under HOST_TEST the call only records the request. Implements
HLR-BSP-029.


---

## 4. Memory Architecture

### 4.1 Memory Map (STM32H750)

```
┌─────────────────────────────────────────────────────────────┐
│                    MEMORY MAP (STM32H750)                    │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  0x00000000 ┌─────────────────────────────────────┐         │
│             │           ITCM (64 KB)              │         │
│             │  • Interrupt vectors                │         │
│             │  • Critical kernel code             │         │
│             │  • Context switch routine           │         │
│  0x00010000 └─────────────────────────────────────┘         │
│                                                              │
│  0x08000000 ┌─────────────────────────────────────┐         │
│             │         FLASH (128 KB)              │         │
│             │  • Application code                 │         │
│             │  • Constant data                    │         │
│             │  • AI model storage (if fits)       │         │
│  0x08020000 └─────────────────────────────────────┘         │
│                                                              │
│  0x20000000 ┌─────────────────────────────────────┐         │
│             │          DTCM (128 KB)              │         │
│             │  • Task stacks (fast access)        │         │
│             │  • Kernel data structures           │         │
│             │  • Critical variables               │         │
│  0x20020000 └─────────────────────────────────────┘         │
│                                                              │
│  0x24000000 ┌─────────────────────────────────────┐         │
│             │        AXI SRAM (512 KB)            │         │
│             │  • AI model data                    │         │
│             │  • AI tensor buffers                │         │
│             │  • Application heap (if needed)     │         │
│  0x24080000 └─────────────────────────────────────┘         │
│                                                              │
│  0x30000000 ┌─────────────────────────────────────┐         │
│             │       SRAM1-4 (288 KB)              │         │
│             │  • DMA buffers                      │         │
│             │  • Display frame buffer             │         │
│             │  • USB buffers                      │         │
│  0x30048000 └─────────────────────────────────────┘         │
│                                                              │
│  0x38800000 ┌─────────────────────────────────────┐         │
│             │        Backup SRAM (4 KB)           │         │
│             │  • Reset-surviving data (bkpram_*)  │         │
│             │  • MPU region 8: priv, uncached     │         │
│  0x38801000 └─────────────────────────────────────┘         │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 MPU Protection Architecture

#### 4.2.1 Overview

ICARUS OS implements hardware-enforced memory protection using the ARM Cortex-M7 Memory Protection Unit (MPU). The protection model provides:
- **Privilege separation**: Kernel runs privileged, tasks run unprivileged
- **Code protection**: ITCM is read-only to prevent code modification
- **Data isolation**: DTCM is privileged-only to protect kernel data
- **Task isolation**: Each task's data region is isolated from other tasks
- **SVC call gates**: Controlled kernel service access via Supervisor Calls

#### 4.2.2 MPU Region Configuration

| Region | Base Address | Size | Access | Purpose | Implements |
|--------|--------------|------|--------|---------|------------|
| 0 | 0x00000000 | 64 KB | PRIV_RO_URO | ITCM code (read-only for all) | HLR-KRN-063, HLR-KRN-066 |
| 1 | 0x90000000 | 8 MB | PRIV_RO_URO | QSPI Flash (read-only) | HLR-KRN-066 |
| 2 | 0x08000000 | 128 KB | PRIV_RO_URO | Internal flash (code and constants, read-only) | HLR-KRN-066 |
| 3 | 0x20010000 | 64 KB | FULL_ACCESS, XN | Application (upper) half of DTCM | HLR-KRN-074.1 |
| 4 | Dynamic | 2 KB | PRIV_RW_URW | Task data (reconfigured on switch) | HLR-KRN-065, HLR-KRN-071 |
| 5 | 0x20000000 | 128 KB | PRIV_RW, XN | DTCM kernel data (privileged-only); subregions 4–7 disabled so region 3 governs the upper 64 KB | HLR-KRN-064, HLR-KRN-070 |
| 6 | 0x24000000 | 512 KB | FULL_ACCESS, XN | RAM_D1 (stacks, `.data`, `.bss`, shared buffers) | - |
| 7 | 0x40000000 | 512 MB | FULL_ACCESS, device, XN | Peripherals | - |
| 8 | 0x38800000 | 4 KB | PRIV_RW, non-cacheable, XN | Backup SRAM (reset-surviving data) *(v0.5.0)* | HLR-KRN-078.2 |

The MPU is enabled with the privileged default background map, so
unprivileged access to anything not covered above (for example the
system control space and SRAM4) faults. Note that the SVC caller-buffer
allowlist (§4.2.8) is narrower than this map: the kernel copies with
privilege, so it rejects peripherals, SRAM4 and backup SRAM even though
the MPU would let privileged code reach them.

#### 4.2.3 Memory Region Details

**Region 0: ITCM Code Protection**
```c
r.BaseAddress      = 0x00000000;
r.Size             = MPU_REGION_SIZE_64KB;
r.AccessPermission = MPU_REGION_PRIV_RO_URO;  // Read-only for all
r.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
```
- Contains kernel functions marked with `ITCM_FUNC` macro
- Read-only prevents code modification attacks
- Executable by all (ARM MPU limitation: cannot restrict execution by privilege level)
- Protection enforced by DTCM isolation (kernel functions fault when accessing kernel data from unprivileged mode)

**Region 4: Task Data Isolation**
```c
// Reconfigured on every context switch
r.BaseAddress      = task->data_base;  // 2KB-aligned
r.Size             = MPU_REGION_SIZE_2KB;
r.AccessPermission = MPU_REGION_PRIV_RW_URW;  // Full access for current task
```
- Each task has 2KB data region in `data_pool` (RAM_D2)
- MPU reconfigured on context switch to grant access only to current task
- Prevents cross-task memory corruption
- Verified by MPU_REDTEAM stress test

**Region 5: DTCM Kernel Data Protection**
```c
r.BaseAddress      = 0x20000000;
r.Size             = MPU_REGION_SIZE_128KB;
r.AccessPermission = MPU_REGION_PRIV_RW;  // Privileged-only
r.SubRegionDisable = 0xF0;                // upper 64 KB governed by region 3
```
- Contains kernel data structures: `task_list`, `semaphore_list`, `pipe_list`, `os_tick_count`
- Unprivileged tasks cannot read or write
- Forces use of SVC call gates for kernel services
- Prevents direct kernel function calls from working

**Region 8: Backup SRAM** *(v0.5.0)*
```c
r.BaseAddress      = BSP_BKPSRAM_BASE;         // 0x38800000
r.Size             = MPU_REGION_SIZE_4KB;
r.AccessPermission = MPU_REGION_PRIV_RW;       // Privileged-only
r.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE; // TEX=1 C=0 B=0
r.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
r.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
```
- Holds the `.bkpsram` section (`BKPRAM_DATA`), which survives a system
  or watchdog reset (and power loss while VBAT is held)
- Non-cacheable: with the write-back D-cache a write could sit in a dirty
  cache line that the reset discards, so the data would never reach the
  SRAM. Before v0.5.0 the backup data lived in SRAM4 behind the cache
  and was lost on reset
- Unprivileged tasks use `bkpram_write` (SVC 71) and `bkpram_read`
  (SVC 86). Each call checks `offset`/`len` against the 4 KB size
  (non-zero length, no overflow) and the caller buffer against the
  allowlist (§4.2.8), then copies with privilege
- Implements HLR-KRN-078, HLR-KRN-078.1, HLR-KRN-078.2

#### 4.2.4 Privilege Separation Model

```
┌─────────────────────────────────────────────────────────────┐
│                    PRIVILEGE ARCHITECTURE                    │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  PRIVILEGED MODE (Handler/SVC)                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  • Kernel functions (__os_* prefix)                    │ │
│  │  • Exception handlers (SysTick, PendSV, MemManage)    │ │
│  │  • SVC dispatcher                                      │ │
│  │  • MPU configuration                                   │ │
│  │  • Access to DTCM (kernel data)                       │ │
│  │  • Access to ITCM (kernel code)                       │ │
│  └────────────────────────────────────────────────────────┘ │
│                           ▲                                  │
│                           │ SVC instruction                  │
│                           │ (controlled transition)          │
│  ┌────────────────────────┴───────────────────────────────┐ │
│  │  SVC CALL GATES (os_* wrappers)                       │ │
│  │  • sem_can_feed() → SVC 29                             │ │
│  │  • sem_can_consume() → SVC 30                          │ │
│  │  • pipe_can_enqueue() → SVC 31                         │ │
│  │  • bkpram_read() → SVC 86                              │ │
│  │  • cdc_tx_write() → SVC 93                             │ │
│  └────────────────────────────────────────────────────────┘ │
│                           ▲                                  │
│                           │ Function call                    │
│                           │                                  │
│  UNPRIVILEGED MODE (Thread)                                 │
│  ┌────────────────────────┴───────────────────────────────┐ │
│  │  • User task code                                      │ │
│  │  • Task local variables (stack)                       │ │
│  │  • Task data region (2KB, MPU Region 4)               │ │
│  │  • Shared buffers (RAM_D1, RAM_D2)                    │ │
│  │  • NO access to DTCM (kernel data)                    │ │
│  │  • Read-only access to ITCM (kernel code)             │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

#### 4.2.5 SVC Call Gates

All kernel services are accessed via SVC (Supervisor Call) instructions.
`Core/Inc/icarus/svc.h` is the authoritative list: 94 numbers, IDs 0–93
(`SVC_MAX_NUMBER` = `SVC_CDC_TX_WRITE`). A compile-time assertion keeps
`SVC_MAX_NUMBER` ≤ 255 (8-bit immediate) and ≥ every other number.
Module ranges: 40–42 CDC RX, 43–48 event ring, 49–56 tables, 57 task
restart, 58 timed semaphore, 59–62 task diagnostics, 63–70 checksum monitor, 71 backup SRAM write,
72–77 Software Bus, 78–85 filesystem. The spin-loop helpers and the
v0.5.0 additions are listed below.

SVC 58 (`SVC_SEMAPHORE_CONSUME_TIMEOUT`) keeps its number in `svc.h`, but
since v0.5.0 its dispatch case does not run the timed wait: it returns
false. The wait sleeps and reads the tick through SVC wrappers, so run
inside the handler it would issue nested SVCs (a HardFault on target).
`semaphore_consume_timeout()` never issues SVC 58; it calls
`__semaphore_consume_timeout()` directly in thread mode (§3.5.3). Only a
caller that issues SVC 58 by hand reaches the case, and it gets false.

| SVC # | Function | Purpose | Implements |
|-------|----------|---------|------------|
| 29 | `SVC_SEM_CAN_FEED` | Check if semaphore can accept a feed | HLR-KRN-067 |
| 30 | `SVC_SEM_CAN_CONSUME` | Check if semaphore can be consumed | HLR-KRN-067 |
| 31 | `SVC_PIPE_CAN_ENQUEUE` | Check if pipe has room for N bytes | HLR-KRN-067 |
| 32 | `SVC_PIPE_CAN_DEQUEUE` | Check if pipe holds N bytes | HLR-KRN-067 |
| 33 | `SVC_SEM_INCREMENT` | Increment semaphore count | HLR-KRN-067 |
| 34 | `SVC_SEM_DECREMENT` | Decrement semaphore count | HLR-KRN-067 |
| 35 | `SVC_PIPE_WRITE_BYTES` | Copy caller bytes into a pipe (buffer validated for reading; returns `bool` since v0.5.0) | HLR-KRN-067, HLR-KRN-074 |
| 36 | `SVC_PIPE_READ_BYTES` | Copy pipe bytes to the caller (buffer validated for writing; returns `bool` since v0.5.0) | HLR-KRN-067, HLR-KRN-074 |
| 37 | `SVC_GET_TASK_NAME` | Get a task's name | HLR-KRN-067 |
| 38 | `SVC_GET_NUM_TASKS` | Get created-task count | HLR-KRN-067 |
| 39 | `SVC_OS_IS_RUNNING` | Check if the OS is running | HLR-KRN-067 |
| 58 | `SVC_SEMAPHORE_CONSUME_TIMEOUT` | Returns false when issued directly; the wrapper does not use it and runs the timed wait in thread mode *(changed v0.5.0)* | HLR-KRN-098 |
| 86 | `SVC_BKPRAM_READ` | Copy backup SRAM to the caller *(v0.5.0)* | HLR-KRN-078 |
| 87 | `SVC_TBL_GET_INFO` | Copy a table descriptor and state out *(v0.5.0)* | HLR-KRN-094.6 |
| 88 | `SVC_TBL_ABORT` | Discard staged table bytes *(v0.5.0)* | HLR-KRN-094.5 |
| 89 | `SVC_CRC16_CCITT` | CRC16 over a caller buffer for unprivileged tasks *(v0.5.0)* | HLR-KRN-092.3 |
| 90 | `SVC_SYS_ENTER_BOOTLOADER` | Reset into the ROM bootloader (does not return) *(v0.5.0)* | HLR-BSP-029 |
| 91 | `SVC_CDC_RX_DROPPED` | CDC RX bytes dropped on overflow *(v0.5.0)* | HLR-KRN-090.3 |
| 92 | `SVC_TBL_LOAD_AT` | Write a table chunk at an offset *(v0.5.0)* | HLR-KRN-094.4 |
| 93 | `SVC_CDC_TX_WRITE` | Queue bytes on the USB CDC transmit ring *(v0.5.0)* | HLR-BSP-027.2 |

#### 4.2.6 Fault Handling

The MemManage fault handler provides graceful recovery from memory protection violations:

```c
void MemManage_Handler(void) {
    // Check fault type
    bool is_daccviol = (CFSR & SCB_CFSR_DACCVIOL_Msk) != 0;  // Data access
    bool is_iaccviol = (CFSR & SCB_CFSR_IACCVIOL_Msk) != 0;  // Instruction fetch
    
    // Check privilege level
    bool from_psp = (LR & 0x4) != 0;  // From unprivileged task
    
    if (is_daccviol && !is_iaccviol && from_psp) {
        // Recoverable: skip faulting instruction
        advance_pc_by_instruction_length();
        clear_fault_status();
        return;  // Task continues
    }
    
    // Non-recoverable: halt with LED blink code
    halt_with_4_blinks();
}
```

**Fault Recovery Policy:**
- **Data access violations (DACCVIOL)** from unprivileged tasks: Recoverable, skip instruction
- **Instruction fetch violations (IACCVIOL)**: Non-recoverable, system halt
- **Faults from privileged code**: Non-recoverable, system halt

Implements: HLR-KRN-081, HLR-KRN-082, HLR-KRN-083

#### 4.2.7 Protection Verification

MPU protection is verified by stress tests:

| Test | Purpose | Expected Result | Implements |
|------|---------|-----------------|------------|
| `mpu_redteam_task` | Cross-task memory attack | Faults, shows [PROTECTED] | HLR-KRN-071 |
| `mpu_itcm_write_test` | ITCM write attempt | Faults, shows [WRITE PROTECTED] | HLR-KRN-066 |
| `mpu_dtcm_attack_task` | DTCM read attempt | Faults, shows [DTCM PROTECTED] | HLR-KRN-070 |
| `mpu_kernel_bypass_test` | Direct kernel function call | Faults, shows [DTCM PROTECTED] | HLR-KRN-070 |

All tests run continuously and display real-time status via USB CDC terminal.

#### 4.2.8 SVC Caller-Buffer Validation (v0.5.0)

SVC implementations run privileged, so the MPU does not stop them from
touching any address a task passes in. Before v0.5.0 the dispatcher only
rejected privileged DTCM and backup SRAM, and several gates (checksum
scan and region read, table load, the pipe byte gates, filesystem, event
drain, bus publish) did no check at all: a task could make the kernel
copy, with privilege, into another task's data-pool slot, SRAM4 or the
system control space, or fault the handler on read-only or unmapped
memory.

Every dispatch arm whose implementation dereferences a caller pointer now
checks it first, for the access the implementation makes:

| Access | Allowed regions | Requirement |
|--------|-----------------|-------------|
| Write (kernel → caller) | RAM_D1 (`BSP_RAM_D1_BASE`, 512 KB) less the main-stack window for task callers; the application (upper) DTCM half (`BSP_DTCM_APP_BASE`, 64 KB, MPU region 3); the calling task's own data-pool slot | HLR-KRN-074.1 |
| Read (caller → kernel) | All write regions, plus internal flash (`BSP_FLASH_BASE`, 128 KB) and ITCM (`BSP_ITCM_BASE`, 64 KB) | HLR-KRN-074.2 |
| Rejected | NULL / address 0 (so ITCM's first byte), the privileged DTCM half, other tasks' data-pool slots, SRAM4, backup SRAM, peripherals, the system control space, unmapped addresses; any range that wraps or is not wholly inside one region; for a task caller, any range that overlaps the main-stack window (read or write) | HLR-KRN-074.3, HLR-KRN-074.7 |

The main stack sits at the top of RAM_D1 (`_estack`, with
`_Min_Stack_Size` reserved below it) and holds the SVC handler's own
frame — saved registers and the exception return — while the call runs.
If a task could aim a kernel copy there it could redirect the privileged
return or read handler state, so for a task caller the whole window is
rejected for reads and writes. Privileged code that still runs on the
main stack (boot code before the scheduler starts) may pass buffers
there, such as its locals.

**Assumption and limitation (HLR-KRN-074.7).** The window is the
reserved main stack, [`_estack` − `_Min_Stack_Size`, `_estack`). It
covers the handler's frame only while everything on the main stack stays
within that reservation: the frames left by `main()` and `os_start()`
(never unwound, since the first task is launched from them), the SVC
handler, and any interrupts nested on top of it. An application whose
`main()` keeps large locals must raise `_Min_Stack_Size` to match.
Resetting MSP to `_estack` at the first-task launch would remove this
dependency; it is not done yet. That the main stack stays within
`_Min_Stack_Size` is checked by review, not by test.

```
svc_caller_t { slot_base, slot_size,             (filled per call)
               main_stack_base, main_stack_size,
               from_task }

svc_buffer_allowed(addr, len, access, caller)                  (pure)
    ├─ caller == NULL or addr == 0             → false
    ├─ len == 0                                → true  (nothing touched)
    ├─ addr + len wraps                        → false
    ├─ caller.from_task and range overlaps
    │  [main_stack_base, +main_stack_size)      → false
    ├─ inside [slot_base, +slot_size)           → true
    ├─ for each fixed region (RAM_D1, DTCM app, flash, ITCM):
    │   ├─ access == WRITE and region read-only → skip
    │   └─ range wholly inside region           → true
    └─ false

svc_current_caller(&caller)                  (target, inside the handler)
    ├─ __kernel_current_data_slot(&slot_base, &slot_size)
    ├─ main_stack_base = _estack - _Min_Stack_Size; size = _Min_Stack_Size
    └─ from_task = (SVC frame address == PSP)
         a task's frame is always at PSP, so a task cannot pass itself
         off as main-stack code; a misclassified main-stack caller is
         only treated more strictly

svc_user_buffer_ok(addr, len, access)        (target, inside the handler)
    ├─ svc_current_caller(&caller)
    └─ svc_buffer_allowed(addr, len, access, &caller)

svc_user_string_ok(addr, max_len)            (NUL-terminated names)
    └─ check each byte with svc_buffer_allowed(p, 1, READ, &caller)
       before reading it; stop at NUL or max_len  → HLR-KRN-074.5
```

The SVC frame pointer used for `from_task` is kept in privileged DTCM
(`svc_frame`, set on entry to `SVC_Handler_C()`): in RAM_D1 a task could
rewrite it between calls. SVCs do not nest, so one instance serves every
call.

`svc_user_opt_buffer_ok()` is used where the implementation accepts NULL
to mean "no buffer": NULL passes, anything else is checked. When a check
fails the dispatch arm skips the implementation and leaves the call's
documented failure value in R0 (false, 0, -1 or NULL), so no memory is
touched through the rejected pointer (HLR-KRN-074.4). Two arms differ:
`SVC_CS_CHECK_ALL` with a rejected result buffer still runs the scan and
returns the failure count, but writes no result and so delivers no
callbacks; `SVC_CRC16_CCITT` returns 0xFFFF.

The policy is kept a pure function of its arguments (the caller is a
parameter), so the host suite runs it against the target memory map with
both a task caller and a main-stack caller
(`tests/src/test_svc_policy.c`, HLR-KRN-074.6). The host build of
`svc_user_buffer_ok()` only rejects NULL and wrap-around, because host
addresses are not target addresses.

Because the dispatcher is target-only, the per-case wiring is guarded by
a static check, `tools/check_svc_pointer_checks.py` (HLR-KRN-074.8),
which the `check-svc-ptr` target runs before the host suite in
`make -C tests`. For
every `switch` with `case SVC_...:` labels it treats each
`stack_frame[N]` (and prologue copies such as `arg0`) as caller register
N, follows locals assigned from them within the case, and finds every
cast of such a value, or of an expression built from it, to a pointer
type. Each data-pointer use needs a validator call
(`svc_user_buffer_ok()`, `svc_user_opt_buffer_ok()`,
`svc_user_string_ok()`, `svc_buffer_allowed()`, or `bkpram_range_ok()`
for an offset into backup SRAM) on the same register earlier in the same
case. Function-pointer casts are accepted only for the two cases that
store a pointer the kernel never calls privileged (`SVC_OS_REGISTER_TASK`
task entry, `SVC_CS_SET_CALLBACK` callback); a stale allowlist entry or
an unclassifiable cast type also fails. The check is textual: it does
not prove that the validator's result gates the use or that its length
argument is right — the host policy tests cover the policy itself.

#### 4.2.9 SVC Wrapper Rules (v0.5.0)

| Rule | Mechanism | Requirement |
|------|-----------|-------------|
| Every inline-asm block that issues `svc` lists `"memory"` in its clobbers. Without it the compiler may keep memory values in registers across the SVC, or assume a local whose address was passed is unchanged: at -O2 `tbl_activate()` read its out-parameters before the handler's writes were visible and every activation failed on target, while host tests (which never run the asm) passed | All SVC asm blocks in `svc.c`, `fs.c`, `sb.c`, `crc.c`, `cdc.c`, `bootloader.c`, including `enter_critical()` / `exit_critical()` | HLR-KRN-075 |
| The clobber rule is checked statically | `tools/check_svc_clobbers.py` scans `Core/Src/**/*.c`; the `check-svc-asm` target runs it before the Unity runner in `make -C tests` and fails the run on any offender | HLR-KRN-075.1 |
| No SVC is issued from inside the SVC handler (a nested SVC escalates to HardFault on target). Host builds, which call the privileged implementations directly, detect the pattern | `SVC_HOST_GATE()` in the HOST_TEST branch of each wrapper whose implementation runs inside the handler on target opens a gate for the call's duration; opening a second gate calls the nesting handler, which by default prints both wrapper names and aborts the test run. Wrappers that run thread-mode code on target hold no gate while that code runs: spin loops only gate their individual SVC calls, `tbl_activate` gates its prepare and commit steps but not the callback between them, and `cdc_tx_write` (legal from any context) opens no gate | HLR-KRN-076 |
| Services callable from handler mode do not issue an SVC | `svc_caller_is_privileged()` is true in handler mode (IPSR ≠ 0) and in privileged thread mode (CONTROL.nPRIV = 0); `cdc_tx_write` and `crc16_ccitt` then call `__cdc_tx_write` / `__crc16_ccitt` directly. Always true on the host | HLR-KRN-077 |

### 4.3 Stack Allocation

```c
#define MAX_TASKS 16
#define STACK_WORDS 512  // 2KB per task

// Static allocation - no malloc
static uint32_t stack_pool[MAX_TASKS][STACK_WORDS];
static task_t task_pool[MAX_TASKS];
task_t* task_list[MAX_TASKS];
```

| Task Type | Stack Size | Location |
|-----------|------------|----------|
| System tasks | 2 KB | DTCM |
| User tasks | 2 KB (configurable) | DTCM |
| AI inference | 4 KB (planned) | DTCM |

### 4.4 AI Memory Budget

| Component | Size | Location |
|-----------|------|----------|
| Model header | 64 B | AXI SRAM |
| Model weights | Up to 256 KB | AXI SRAM |
| Input tensor | Model-dependent | AXI SRAM |
| Output tensor | Model-dependent | AXI SRAM |
| Scratch buffer | Model-dependent | AXI SRAM |

---

## 5. Interface Design

### 5.1 Kernel API Summary

```c
// ═══════════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════════
void os_init(void);
    // Initialize kernel state, register system tasks
    // Must be called before os_start()
    // Implements: HLR-KRN-001

void os_start(void);
    // Start scheduler, never returns
    // Implements: HLR-KRN-010

// ═══════════════════════════════════════════════════════════
// TASK MANAGEMENT
// ═══════════════════════════════════════════════════════════
void os_register_task(void (*function)(void), const char *name);
    // Register task using internal pool
    // Implements: HLR-KRN-001, HLR-KRN-002, HLR-KRN-003

void os_create_task(task_t *task, void (*function)(void),
                    uint32_t *stack, uint32_t stack_size,
                    const char *name);
    // Create task with external storage
    // Implements: HLR-KRN-001, HLR-KRN-002, HLR-KRN-003

void os_exit_task(void);
    // Exit current task (called automatically on return)
    // Implements: HLR-KRN-004

void os_kill_process(uint8_t task_index);
    // Terminate another task
    // Implements: HLR-KRN-005, HLR-KRN-006

// ═══════════════════════════════════════════════════════════
// SCHEDULING
// ═══════════════════════════════════════════════════════════
void os_yield(void);
    // Voluntarily give up CPU
    // Implements: HLR-KRN-013

uint32_t task_active_sleep(uint32_t ticks);
    // Non-blocking sleep (task yields)
    // Implements: HLR-KRN-021

uint32_t task_blocking_sleep(uint32_t ticks);
    // Blocking sleep (busy wait)
    // Implements: HLR-KRN-022

// ═══════════════════════════════════════════════════════════
// INFORMATION
// ═══════════════════════════════════════════════════════════
uint32_t os_get_tick_count(void);
    // Get current tick count
    // Implements: HLR-KRN-023

uint8_t os_get_running_task_count(void);
const char* os_get_current_task_name(void);
uint32_t os_get_task_ticks_remaining(void);
```

### 5.2 BSP Interface Summary

```c
// ═══════════════════════════════════════════════════════════
// PLATFORM ABSTRACTION (Current)
// ═══════════════════════════════════════════════════════════
void hal_init(void);
    // Initialize all hardware
    // Implements: HLR-BSP-001, HLR-BSP-002, HLR-BSP-003

// ═══════════════════════════════════════════════════════════
// GPIO
// ═══════════════════════════════════════════════════════════
void LED_On(void);
void LED_Off(void);
void LED_Blink(uint32_t on_ticks, uint32_t off_ticks);
    // Implements: HLR-BSP-010

// ═══════════════════════════════════════════════════════════
// COMMUNICATION
// ═══════════════════════════════════════════════════════════
int32_t platform_write(void *handle, uint8_t reg,
                       uint8_t *data, uint16_t len);
int32_t platform_read(void *handle, uint8_t reg,
                      uint8_t *data, uint16_t len);
    // Implements: HLR-BSP-011

// ═══════════════════════════════════════════════════════════
// DISPLAY
// ═══════════════════════════════════════════════════════════
void display_init(void);
void display_render_bar(uint8_t row, const char *task_name,
                        uint32_t elapsed_ticks, uint32_t period_ticks);
void display_render_banner(uint8_t row, const char *task_name,
                           bool is_on);
    // Implements: HLR-BSP-014
```


---

## 6. Design Traceability Matrix

### 6.1 Requirements → Design Traceability

This matrix traces each requirement to its design element(s).

#### 6.1.1 Kernel Requirements

| Requirement | Design Element | Section | Status |
|-------------|----------------|---------|--------|
| HLR-KRN-001 | task_pool[], MAX_TASKS | 3.2.2 | ✅ |
| HLR-KRN-002 | task_t.name, current_task_index | 3.2.2 | ✅ |
| HLR-KRN-003 | stack_pool[], task_t.stack_base | 3.2.4 | ✅ |
| HLR-KRN-004 | os_exit_task() | 5.1 | ✅ |
| HLR-KRN-005 | os_kill_process() | 5.1 | ✅ |
| HLR-KRN-006 | task_index > 0 check | 3.2 | ✅ |
| HLR-KRN-007 | task_t.task_state, TASK_* enums | 3.2.3 | ✅ |
| HLR-KRN-010 | SysTick + PendSV mechanism | 3.1, 3.3 | ✅ |
| HLR-KRN-011 | Round-robin in schedule_next_task | 3.1.2 | ✅ |
| HLR-KRN-012 | TICKS_PER_TASK constant | 3.1.3 | ✅ |
| HLR-KRN-013 | os_yield() | 5.1 | ✅ |
| HLR-KRN-014 | task_t.task_priority | 3.2.2 | 🔲 Planned |
| HLR-KRN-015 | ipc_sem_t.original_priority | 3.6.4 | 🔲 Planned |
| HLR-KRN-016 | `os_yield_pendsv`: S16–S31 and EXC_RETURN saved per task, cold-task EXC_RETURN 0xFFFFFFFD | 3.3.4 | ✅ |
| HLR-KRN-020 | os_tick_count | 3.1 | ✅ |
| HLR-KRN-021 | task_active_sleep() | 5.1 | ✅ |
| HLR-KRN-022 | task_blocking_sleep() | 5.1 | ✅ |
| HLR-KRN-023 | os_get_tick_count() | 5.1 | ✅ |
| HLR-KRN-024 | Tick-based wake check | 3.1.2 | ✅ |
| HLR-KRN-030 | scheduler_enabled flag | 3.4.2 | ✅ |
| HLR-KRN-031 | critical_stack_depth | 3.4.2 | ✅ |
| HLR-KRN-032 | Inline enter/exit_critical | 3.4.2 | ✅ |
| HLR-KRN-040 | ipc_queue_t | 3.8.2 | 🔲 Planned |
| HLR-KRN-041 | semaphore_t (binary) | 3.5.2 | ✅ |
| HLR-KRN-042 | semaphore_t (counting) | 3.5.2 | ✅ |
| HLR-KRN-043 | semaphore_t (bounded) | 3.5.2 | ✅ |
| HLR-KRN-044 | owner, original_priority | 3.8.4 | 🔲 Planned |
| HLR-KRN-045 | Event flags | - | 🔲 Planned |
| HLR-KRN-046 | Bounded operations | 3.5.3, 3.6.3 | ✅ |
| HLR-KRN-047 | FIFO ordering | 3.6.4 | ✅ |
| HLR-KRN-048 | Variable-length messages | 3.6.5 | ✅ |
| HLR-KRN-049 | Semaphore blocking | 3.5.4 | ✅ |
| HLR-KRN-050 | Pipe blocking | 3.6.3 | ✅ |
| HLR-KRN-051 | MAX_SEMAPHORES=32 | 3.5.2 | ✅ |
| HLR-KRN-052 | MAX_MESSAGE_QUEUES=32 | 3.6.2 | ✅ |
| HLR-KRN-053 | Pipe capacity 128 bytes | 3.6.2 | ✅ |
| HLR-KRN-060 | Static allocation only | 4.2 | ✅ |
| HLR-KRN-051 | Stack canary (planned) | - | 🔲 Planned |
| HLR-KRN-052 | MPU configuration | - | 🔲 Planned |
| HLR-KRN-060 | Fault handlers | 3.3 | ✅ |
| HLR-KRN-061 | Watchdog integration | - | 🔲 Planned |
| HLR-KRN-074 | Per-arm caller-buffer checks in `SVC_Handler_C` | 4.2.8 | ✅ |
| HLR-KRN-074.1 | Write allowlist: RAM_D1, application DTCM half, own data-pool slot | 4.2.8 | ✅ |
| HLR-KRN-074.2 | Read allowlist adds internal flash and ITCM; NULL rejected | 4.2.8 | ✅ |
| HLR-KRN-074.3 | Everything else rejected; wrap and cross-region rejected | 4.2.8 | ✅ |
| HLR-KRN-074.4 | Failure value left in R0, implementation skipped | 4.2.8 | ✅ |
| HLR-KRN-074.5 | `svc_user_string_ok` byte-by-byte check | 4.2.8 | ✅ |
| HLR-KRN-074.6 | Pure `svc_buffer_allowed()` | 4.2.8 | ✅ |
| HLR-KRN-074.7 | Main-stack window excluded for task callers (`svc_caller_t.from_task`) | 4.2.8 | ✅ |
| HLR-KRN-074.8 | `tools/check_svc_pointer_checks.py` | 4.2.8 | ✅ |
| HLR-KRN-075 | `"memory"` clobber on every SVC asm block | 4.2.9 | ✅ |
| HLR-KRN-075.1 | `tools/check_svc_clobbers.py` in `make -C tests` | 4.2.9 | ✅ |
| HLR-KRN-076 | `SVC_HOST_GATE` nested-SVC guard (host) | 4.2.9 | ✅ |
| HLR-KRN-077 | `svc_caller_is_privileged()` bypass | 4.2.9 | ✅ |
| HLR-KRN-078 | `bkpram_write` / `bkpram_read` (SVC 71, 86) | 4.2.3 | ✅ |
| HLR-KRN-078.1 | `bkpram_range_ok()` + buffer check | 4.2.3 | ✅ |
| HLR-KRN-078.2 | MPU region 8, non-cacheable | 4.2.2, 4.2.3 | ✅ |
| HLR-KRN-090.3 | `cdc_rx_dropped` (SVC 91) | 3.10.1 | ✅ |
| HLR-KRN-092.3 | `crc16_ccitt` SVC 89 path | 3.10.3 | ✅ |
| HLR-KRN-094.4 | `tbl_load_at` (SVC 92) | 3.10.5 | ✅ |
| HLR-KRN-094.5 | `tbl_abort` (SVC 88) | 3.10.5 | ✅ |
| HLR-KRN-094.6 | `tbl_get_info` (SVC 87) | 3.10.5 | ✅ |
| HLR-KRN-094.7 | Commit gated on `prepared` | 3.10.5 | ✅ |
| HLR-KRN-096.2 | Thread-mode callback delivery | 3.10.9 | ✅ |
| HLR-KRN-098 | `semaphore_consume_timeout()` in thread mode; SVC 58 case returns false | 3.5.3, 4.2.5 | ✅ |
| HLR-KRN-098.1 | Timeout measured on the system tick (wrap-safe) | 3.5.3 | ✅ |

#### 6.1.2 BSP Requirements

| Requirement | Design Element | Section | Status |
|-------------|----------------|---------|--------|
| HLR-BSP-001 | hal_init() | 5.2 | ✅ |
| HLR-BSP-002 | SystemClock_Config() | 5.2 | ✅ |
| HLR-BSP-003 | NVIC configuration | 5.2 | ✅ |
| HLR-BSP-004 | context_switch.s | 3.3 | ✅ |
| HLR-BSP-010 | LED_On/Off/Blink | 5.2 | ✅ |
| HLR-BSP-011 | platform_read/write | 5.2 | ✅ |
| HLR-BSP-012 | SPI HAL wrapper | 5.2 | ✅ |
| HLR-BSP-013 | USB CDC | 5.2 | ✅ |
| HLR-BSP-014 | display_* functions | 5.2 | ✅ |
| HLR-BSP-020 | SysTick_Config() | 3.1 | ✅ |
| HLR-BSP-025 | IWDG_Init / IWDG_Refresh / IWDG_WasReset / IWDG_ClearResetFlag | 3.10.10 | ✅ |
| HLR-BSP-026 | Button_IsPressed (`BSP_KEY_PRESSED_LEVEL`) | 3.10.10 | ✅ |
| HLR-BSP-027 | CDC_Write / CDC_WriteString (all-or-nothing, non-blocking) | 3.11.3 | ✅ |
| HLR-BSP-027.1 | Single transmit ring in `.dtcm_priv` | 3.11.1 | ✅ |
| HLR-BSP-027.2 | `cdc_tx_write` whole/partial, drop counter, SVC 93 | 3.11.2, 3.11.3 | ✅ |
| HLR-BSP-027.3 | No wait, sleep or retry | 3.11.1 | ✅ |
| HLR-BSP-027.4 | Masked idle-check-and-start, chunk cap, release on completion | 3.11.2 | ✅ |
| HLR-BSP-027.5 | `__cdc_tx_on_link_reset`; link loss reported as a close | 3.11.2 | ✅ |
| HLR-BSP-027.6 | `__cdc_tx_kick` on port open and bus resume (PHY clock restarted first) | 3.11.2 | ✅ |
| HLR-BSP-027.7 | `__cdc_tx_on_dtr` discards on close and reopen, keeps on first open | 3.11.2 | ✅ |
| HLR-BSP-028 | `_write` / `__io_putchar` → `stdio_write` (partial) | 3.11.3 | ✅ |
| HLR-BSP-028.1 | `stdio_get_tx_dropped` (atomic) | 3.11.3 | ✅ |
| HLR-BSP-028.2 | No retarget-layer buffer | 3.11.3 | ✅ |
| HLR-BSP-028.3 | One masked copy per call; `printf()` not locked | 3.11.3 | ✅ |
| HLR-BSP-029 | `sys_enter_bootloader` (SVC 90) + `bsp_bootloader_check` | 3.12 | ✅ |

#### 6.1.3 AI Runtime Requirements

| Requirement | Design Element | Section | Status |
|-------------|----------------|---------|--------|
| HLR-AI-001 | ai_model_load() | 3.7.2 | 🔲 Planned |
| HLR-AI-002 | CRC32 in header | 3.7.3 | 🔲 Planned |
| HLR-AI-003 | Model validation | 3.7.2 | 🔲 Planned |
| HLR-AI-004 | Operator whitelist | 3.7.4 | 🔲 Planned |
| HLR-AI-010 | Layer-by-layer exec | 3.7.2 | 🔲 Planned |
| HLR-AI-011 | int8/int16 types | 3.7.4 | 🔲 Planned |
| HLR-AI-012 | wcet_us field | 3.7.3 | 🔲 Planned |
| HLR-AI-013 | Static buffers | 3.7.5 | 🔲 Planned |
| HLR-AI-014 | int8 operators | 3.7.4 | 🔲 Planned |
| HLR-AI-015 | int16 operators | 3.7.4 | 🔲 Planned |
| HLR-AI-020-027 | Operator library | 3.7.4 | 🔲 Planned |
| HLR-AI-030 | Output bounds check | 3.7.5 | 🔲 Planned |
| HLR-AI-031 | Confidence scores | 3.7.2 | 🔲 Planned |

### 6.2 Design → Code Traceability

| Design Element | Source File | Function/Symbol |
|----------------|-------------|-----------------|
| Scheduler | `icarus/scheduler.c` | SysTick-driven scheduling, `schedule_next_task` |
| Task Manager | `icarus/task.c` | os_create_task, os_kill_process |
| Context Switch | `icarus/context_switch.s` | os_yield_pendsv, start_cold_task |
| Critical Section | `icarus/kernel.c` | `__enter_critical` / `__exit_critical` (via SVC) |
| Semaphores | `icarus/semaphore.c` | semaphore_init, semaphore_feed, semaphore_consume, `__semaphore_consume_timeout` |
| Message Pipes | `icarus/pipe.c` | pipe_init, pipe_enqueue, pipe_dequeue |
| Print Buffer | — | Removed from the source before v0.2.0; its documentation entry is retired in v0.5.0 (see §3.7, §3.11) |
| USB CDC transmit ring | `bsp/cdc.c`, `USB_DEVICE/App/usbd_cdc_if.c`, `USB_DEVICE/Target/usbd_conf.c` | cdc_tx_write, CDC_Write, CDC_WriteString, `__cdc_tx_write`, `__cdc_tx_on_complete`, `__cdc_tx_on_link_reset`, `__cdc_tx_kick`, `__cdc_tx_on_dtr`, `__cdc_tx_discard_queued`; callers `CDC_Init_FS`, `CDC_DeInit_FS`, `CDC_Control_FS`, `CDC_TransmitCplt_FS`, `HAL_PCD_ResumeCallback` |
| Console retarget | `bsp/retarget_stdio.c` | stdio_write, `_write`, `__io_putchar`, stdio_get_tx_dropped |
| ROM bootloader entry | `bsp/bootloader.c` | sys_enter_bootloader, bsp_bootloader_check |
| SVC caller-buffer policy | `icarus/svc.c` | svc_buffer_allowed, svc_current_caller, svc_user_buffer_ok, svc_user_opt_buffer_ok, svc_user_string_ok, svc_caller_is_privileged |
| Backup SRAM gates | `icarus/svc.c`, `bsp/mpu.c` | bkpram_write, bkpram_read, MPU_Config (region 8) |
| Nested-SVC guard (host) | `icarus/svc.c` | svc_host_gate_enter, svc_host_gate_exit, SVC_HOST_GATE |
| BSP Init | `bsp/retarget_hal.c` | hal_init |
| GPIO | `bsp/gpio.c` | LED_On, LED_Off |
| Display | `bsp/display.c` | display_init, display_render_*, display_render_vbar |
| Interrupts | `bsp/stm32h7xx_it.c` | SysTick_Handler, PendSV_Handler |

### 6.3 Traceability Summary

This matrix covers High-Level Requirements (HLR) only. Performance (PRF) and Safety (SAF) requirements are traced separately in the SRS.

| Category | Total HLR | Designed | Implemented | Coverage |
|----------|-----------|----------|-------------|----------|
| Kernel | 111 | 111 | 106 | 95% |
| BSP | 30 | 30 | 26 | 87% |
| AI Runtime | 24 | 24 | 0 | 0% |
| **Total** | **165** | **165** | **132** | **80%** |

> v0.5: counted with the SRS §7.2 convention (every HLR row, each
> sub-requirement separately). "Coverage" is Implemented ÷ Total HLR.
> "Designed" equals "Total HLR" by convention: every row is counted as
> designed. Not every row is traced by ID in this document: 34 of the
> 141 kernel and BSP rows appear nowhere in it. Of these, 20 are
> sub-requirements of the shared service modules (§3.10) and the v0.4
> primitives whose parent requirement is traced; the other 14 are the
> kernel rows KRN-062, KRN-068, KRN-069, KRN-072, KRN-073, KRN-080,
> KRN-084, KRN-085 and KRN-086 and the BSP rows BSP-015, BSP-016,
> BSP-017, BSP-021 and BSP-022. Five of them (KRN-073, BSP-015, BSP-016,
> BSP-017, BSP-021) are Planned and have no design yet. The §6.1 tables
> list the original set plus the rows added or changed since v0.3.

---

## 7. Design Verification

### 7.1 Design Review Checklist

- [ ] All requirements have corresponding design elements
- [ ] Design elements are traceable to requirements
- [ ] Data structures are adequately defined
- [ ] Algorithms are specified with complexity bounds
- [ ] Interfaces are fully documented
- [ ] Memory usage is bounded and documented
- [ ] Timing characteristics are specified
- [ ] Error handling is defined

### 7.2 Design Metrics

| Metric | Target | Current |
|--------|--------|---------|
| Requirements coverage | 100% | 100% (designed) |
| Implementation coverage | 100% | 45% |
| Interface documentation | 100% | 100% |
| Algorithm specification | 100% | 85% |

---

*End of Document*
