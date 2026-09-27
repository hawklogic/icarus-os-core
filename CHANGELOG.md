# Changelog

All notable changes to ICARUS OS are documented in this file.
Format follows [Keep a Changelog](https://keepachangelog.com/).

## [0.5.0] - 2026-09-27

Robustness release.  Most fixes below were first seen failing on an
STM32H750 board.  Not every fix has a host test: the SVC caller-buffer
policy and the USB CDC transmit ring are host-tested (the policy against
the target memory map); the per-case pointer checks in the SVC dispatcher,
which is target-only code, are guarded by a static check
(`tools/check_svc_pointer_checks.py`), as is the SVC asm `"memory"` clobber
(`tools/check_svc_clobbers.py`); the rest (USB callbacks, startup and
linker placement, backup SRAM, MPU behaviour) is verified on hardware,
with two gaps: the bus-resume path is host-tested only (the hardware run
never suspends the bus), and closing and reopening the port is exercised
on hardware (the board keeps working across a 30 s close) but the discard
of the output written while closed is checked only on the host.

### Fixed

- **Console output could stall a task until the watchdog reset.**
  `CDC_Transmit_FS` read a NULL class handle before the host configured
  the device and reported BUSY forever, and `__io_putchar` retried with the
  scheduler locked.  The transmit call now fails while unconfigured.
- **USB CDC output could corrupt itself or block its caller.**  The stdio
  line buffers were shared by preemptive tasks without a lock: a task
  preempted mid-flush let the next printer write past the buffer and hand
  USB a buffer it was still sending.  `CDC_Write` and `printf` also raced
  on `CDC_Transmit_FS`'s busy check, and `CDC_Write` retried up to 500 ×
  `task_active_sleep(1)`, so callers missed deadlines whenever a host held
  the port open without reading.  All CDC output (`printf` via a strong
  `_write()`, `__io_putchar`, `CDC_Write`) now goes through one 4 KB
  transmit ring in privileged DTCM (`bsp/cdc.h`, `CDC_TX_RING_SIZE`).
  Producers copy in and return — nothing waits, spins or sleeps, from
  tasks or configurable-priority handlers (not NMI or HardFault, which the
  ring's PRIMASK lock does not mask).  `CDC_Write` is all-or-nothing and
  returns false when the ring is full; `printf` keeps what fits and counts
  the rest (`stdio_get_tx_dropped()`).  The idle-check-and-start of a
  transfer runs with interrupts masked, bytes stay in the ring until the
  transfer-complete interrupt releases them, and a USB reset or
  re-configuration clears the in-flight state so a lost transfer cannot
  wedge the ring.  Output queued before a host attaches is sent when it
  opens the port.  Each `_write()` / `stdio_write()` call is atomic, but
  `printf` itself formats into newlib-nano's shared stdout `FILE` buffer,
  which nothing locks (the `__retarget_lock_*` hooks are newlib's no-op
  stubs in this build): concurrent `printf` from preemptive tasks can
  still interleave or duplicate characters unless the callers serialise,
  for example inside a critical section.
- **Output queued while the USB bus was suspended waited for the next
  write.**  Transfers fail while the device is suspended; the resume
  callback now restarts the PHY clock that the suspend callback stops,
  restores the device state and restarts the transmit ring.
- **Reopening the port replayed old output as if it were live.**  A host
  that closes the port stops reading, so bytes written while it was closed
  went out on the next open, possibly minutes later.  On a DTR change the
  ring now discards the bytes queued behind the transfer in flight, and
  counts them as dropped: when the host closes the port, and again when it
  reopens it after an earlier open (`__cdc_tx_on_dtr()`; a lost link counts
  as a close).  Output queued before the first open since boot is kept and
  sent when the port opens.  A transfer already in flight at the close
  cannot be recalled, so up to `CDC_TX_MAX_CHUNK` bytes from before the
  close can still arrive after the reopen.
- **SVC handlers dereferenced caller pointers almost unchecked.**  The
  buffer check only rejected privileged DTCM and backup SRAM, so a task
  could make the kernel copy, with privilege, into another task's
  data-pool slot, SRAM4 or the system control space, or fault the handler
  on read-only or unmapped addresses; `cs_check_all`, `cs_get_region`,
  `tbl_load`, the pipe byte gates, `fs_*`, `event_drain`, `sb_publish` and
  others did no check at all.  Every SVC that touches caller memory now
  validates it against an allowlist for the access it makes
  (`svc_buffer_allowed()`): writes only to RAM_D1, the application DTCM
  half, or the caller's own data-pool slot; reads also from internal flash
  and ITCM.  Names are checked byte by byte up to their maximum length.
  A rejected pointer returns the call's failure value and touches no
  memory.  `tools/check_svc_pointer_checks.py` now fails `make -C tests`
  if a dispatch case uses a caller pointer without a preceding check.
- **A task could aim an SVC copy at the handler's own return address.**
  The RAM_D1 allowance covered the main stack at its top: MSP starts at
  `_estack` and is never moved, so while an SVC runs the handler's frame
  (saved LR / EXC_RETURN) sits just below it, and a task could
  `bkpram_write` chosen bytes and `bkpram_read` them over that frame to
  redirect the privileged return.  For a task caller (exception frame on
  the process stack) any read or write range that overlaps the main-stack
  window `[_estack - _Min_Stack_Size, _estack)` is now rejected.  Boot
  code still running on the main stack may pass its own locals.  The
  window covers the handler's frame only while the main stack stays within
  `_Min_Stack_Size` (the never-unwound `main()`/`os_start()` frames plus
  the handler and nested interrupts); an application whose `main()` keeps
  large locals must raise it.  The
  caller is an input of the pure policy (`svc_caller_t`), so the rule is
  host-tested with the target constants.
- **SVC inline asm had no `"memory"` clobber** (80 blocks, including
  `enter_critical`/`exit_critical`).  At -O2 `tbl_activate()` read its
  out-parameters before the handler's writes were visible, so every table
  activation failed on target.  `tools/check_svc_clobbers.py` now fails
  `make -C tests` if any SVC asm block lacks the clobber.
- **Checksum-monitor callbacks ran inside the SVC handler**; a callback
  that made a kernel call issued a nested SVC (HardFault).  `cs_check_all()`
  now collects mismatches in the handler and invokes the callback in thread
  mode.
- **Backup data did not survive reset.**  `BKPRAM_DATA` sat in SRAM4
  behind the write-back D-cache.  It now lives in the 4 KB backup SRAM
  (0x38800000, `.bkpsram`) mapped by MPU region 8 as privileged,
  non-cacheable, execute-never; `bkpram_write` is bounds-checked and
  `bkpram_read()` is new.
- **Uninitialised RAM at boot:** startup zero-fills `.dtcm_obc`, and the
  orphan `.ram_d1` input section is placed in `.data`.
- **Event ring drain** computed the oldest entry wrongly after a wrap.
- **Tables:** `tbl_load_at()` honours the chunk offset (identical
  retransmits accepted; gaps, overruns and mixed schema CRCs rejected);
  `tbl_abort()`; `tbl_get_info()` copy-out for unprivileged callers;
  commit is accepted only right after a matching prepare.
- **`crc16_ccitt()` from unprivileged tasks** faulted on the CRC
  peripheral; it now goes through an SVC with buffer validation.
- `SVC_PIPE_INIT` truncated the capacity to 8 bits; the SysTick time-slice
  counter decremented while the scheduler was stopped; K1 was read
  active-low although PC13 is pulled down (`BSP_KEY_PRESSED_LEVEL`).

### Added

- `sys_enter_bootloader()` (SVC 90): reboot into the ROM USB DFU
  bootloader, so boards can be re-flashed without BOOT0/RESET.
- `cdc_rx_dropped()` (SVC 91), `CDC_IsDtrAsserted()`,
  `os_get_memmanage_fault_count()`.
- Host-test nested-SVC guard (`SVC_HOST_GATE`): a wrapper called while
  another wrapper's handler runs aborts the test run.
- SVC numbers 86–93: `SVC_BKPRAM_READ`, `SVC_TBL_GET_INFO`,
  `SVC_TBL_ABORT`, `SVC_CRC16_CCITT`, `SVC_SYS_ENTER_BOOTLOADER`,
  `SVC_CDC_RX_DROPPED`, `SVC_TBL_LOAD_AT`, `SVC_CDC_TX_WRITE`.
- `cdc_tx_write()` (SVC 93): queue bytes on the USB CDC transmit ring,
  all-or-nothing or partial; privileged code and handlers call the ring
  directly.  `stdio_write()`, `svc_caller_is_privileged()`, and the pure
  `svc_buffer_allowed()` policy (host-tested with the target memory map).
- Host hooks to simulate a configured/unconfigured device, capture
  transmitted runs and fire USB transfer completion
  (`__cdc_host_set_configured`, `__cdc_host_set_auto_complete`,
  `__cdc_host_complete`, ...).
- `__cdc_tx_on_dtr()`: port open/close hook that drops output written
  while the port was closed; `__cdc_tx_discard_queued()` drops the queued
  bytes that are not in flight.
- `tools/check_svc_pointer_checks.py` (`make -C tests`, target
  `check-svc-ptr`): for every `case SVC_...:` of the dispatcher, a caller
  argument (or a local copied from one) that is cast to a pointer must be
  checked first by `svc_user_*_ok()`, `svc_buffer_allowed()` or
  `bkpram_range_ok()` on the same register.  Function pointers are
  accepted only for the cases on an explicit allowlist (task entry,
  checksum callback), and stale allowlist entries fail.  Prologue copies
  of a caller register may be cast or comma-separated; any other prologue
  read of one fails the check.

### Changed

- The host test suite links and runs again (`cs.c`, `sb.c`,
  `bootloader.c` were missing and a failing run was masked by `|| true`):
  272 tests, run after the two static SVC checks.  CI runs it on every
  push.
- `svc_buffer_allowed()` takes the caller as a `const svc_caller_t *`
  (data-pool slot, main-stack window, whether the frame is on the process
  stack) instead of the slot base and size.
- `pipe_write_bytes()` / `pipe_read_bytes()` return `bool` (false when the
  pipe is invalid or the buffer is rejected) and `pipe_enqueue()` /
  `pipe_dequeue()` pass it on instead of always returning true.
- Removed `CDC_WRITE_MAX_RETRIES`, `STDIO_LINE_BUF_SIZE`,
  `STDIO_TX_RETRY_MAX` and `STDIO_TX_RETRY_SPIN` (no retry loops remain).
- `ICARUS_VERSION_STRING` was stale at 0.2.0; now 0.5.0.

## [0.4.0] - 2026-06-15

### Added

- **Software Bus** (`icarus/sb.h` / `sb.c`) — lightweight pub/sub message
  router built on top of kernel pipes. 32 routes × 4 subscribers per message
  ID. Best-effort delivery: if a subscriber's pipe is full the message is
  silently dropped for that subscriber. Route table in `DTCM_DATA_PRIV`,
  hot-path functions in `ITCM_FUNC`. API: `sb_init`, `sb_subscribe`,
  `sb_unsubscribe`, `sb_publish`, `sb_subscriber_count`, `sb_route_count`.
- **Background Checksum integrity monitor** (`icarus/cs.h` / `cs.c`) —
  periodic CRC16-CCITT scanner over up to 8 registered memory regions.
  Baselines captured at registration time; mismatches reported through a
  user-supplied callback. Hardware CRC self-test on init (expected 0x29B1).
  Region table in `DTCM_DATA_PRIV`, functions in `ITCM_FUNC`. API:
  `cs_init`, `cs_set_callback`, `cs_add_region`, `cs_enable`,
  `cs_rebaseline`, `cs_check_all`, `cs_get_region`, `cs_region_count`.
- **`os_restart_task(task_index)`** — cold-restart a killed or finished task
  in-place from its original entry point. No new stack slot is allocated;
  the task re-enters the scheduler as `TASK_STATE_COLD`. SVC 57.
- **`semaphore_consume_timeout(idx, max_ticks)`** — timed semaphore wait.
  Returns `false` if the semaphore is not acquired within `max_ticks`
  (0 = non-blocking try). SVC 58.
- **`os_get_task_state(task_index)`** — SVC-gated query (SVC 59) returning
  the current `icarus_task_state_t` for any task index.
- **`os_get_task_dispatch_count(task_index)`** — per-task scheduling counter
  (SVC 60). Incremented each time the scheduler selects the task.
- **`os_get_stack_watermark(task_index)`** / **`os_update_stack_watermark(task_index)`**
  — stack high-water mark tracking using a `0xDEADC0DE` sentinel pattern.
  `os_update_stack_watermark` scans the stack and updates the TCB field;
  `os_get_stack_watermark` returns the cached minimum free words. SVC 61–62.
- **BSP: IWDG watchdog** (`bsp/iwdg.h` / `iwdg.c`) — thin wrapper around
  the STM32H7 Independent Watchdog (IWDG1). `IWDG_Init`, `IWDG_Refresh`,
  `IWDG_WasReset`, `IWDG_ClearResetFlag`.
- **BSP: K1 user button** (`bsp/button.h` / `button.c`) — `Button_IsPressed()`
  raw read of the K1 button on PC13 (active low).
- **BSP: CDC raw write helper** (`bsp/cdc.h` / `cdc.c`) — `CDC_Write` and
  `CDC_WriteString` for pushing raw bytes to the USB CDC IN endpoint with
  busy-retry via `task_active_sleep`.
- **6 new SVC numbers (57–62)** — `SVC_OS_RESTART_TASK` (57),
  `SVC_SEMAPHORE_CONSUME_TIMEOUT` (58), `SVC_GET_TASK_STATE` (59),
  `SVC_GET_TASK_DISPATCH_COUNT` (60), `SVC_GET_STACK_WATERMARK` (61),
  `SVC_UPDATE_STACK_WATERMARK` (62). All routed through `SVC_Handler_C`.

### Changed

- **Pipe capacity widened** — `ICARUS_MAX_MESSAGE_BYTES` increased from 128
  to 512 bytes; pipe internal counters widened from `uint8_t` to `uint16_t`
  to support the larger buffers. Enables Software Bus and CFDP PDU routing.
- **`dispatch_count` field added to TCB** — tracks how many times each task
  has been scheduled. Useful for load-balance diagnostics.
- **`stack_watermark` field added to TCB** — minimum free stack words
  observed, updated lazily via `os_update_stack_watermark()`. Stack is
  painted with `0xDEADC0DE` sentinel at creation time.
- **SVC count grew 57 → 63** (IDs 0–62).
- **`icarus/icarus.h` umbrella header** now also exports `sb.h` and `cs.h`.

## [0.3.0] - 2026-04-11

### Added

- **CDC RX ring buffer** (`icarus/cdc_rx.h` / `cdc_rx.c`) — generic SPSC USB
  CDC receive ring buffer (512 B). Producer is the USB CDC ISR; consumer is
  any RTOS task. Backing store in `DTCM_DATA_PRIV`, hot path in `ITCM_FUNC`,
  thread-mode reads through SVC gates.
- **Generic event ring buffer + per-module severity squelch** (`icarus/event.h`
  / `event.c`) — 32-slot ring of compact 16-byte event entries with a 16-entry
  squelch table. Transport-agnostic: drains into a caller-provided buffer
  via `event_drain()`. Emission via `os_event(module_id, severity, event_id,
  payload, len)`. Silent (no logging dependency).
- **CRC16-CCITT helper** (`icarus/crc.h` / `crc.c`) — `crc16_ccitt(data, len)`
  with poly 0x1021, init 0xFFFF. **Hardware-accelerated** on STM32H7 via the
  on-chip CRC peripheral (AHB4 bus, configurable polynomial), lazy-initialised
  on first call. Approximately 4× faster than the bytewise software loop on
  the buffer sizes used by downstream consumers. Portable bytewise fallback
  under `HOST_TEST`.
- **Internal RAM-backed flat-file filesystem** (`icarus/fs.h` / `fs.c`) — 16
  files × 2 KB = 32 KB total, with create/open/write/read/delete/list/stats.
  On-disk format opaque so a real flash backend can be substituted later
  without changing the public API.
- **Generic ground-loadable table engine** (`icarus/tables.h` / `tables.c`)
  — registry of up to 8 tables, each with a staging buffer and an active
  buffer (double-buffered swap). Tables validated by schema CRC and CRC16
  data checksum, then committed via a registered activate callback.
  `tbl_activate` is split across two SVCs (`prepare` + `commit`) so the
  user activate callback runs in unprivileged thread mode against a stack
  scratch copy — never sees DTCM_PRIV.
- **17 new SVC numbers (40–56)** — `SVC_CDC_RX_*` (40–42), `SVC_EVENT_*`
  (43–48), `SVC_TBL_*` (49–56). All routed through `SVC_Handler_C`. Each
  module's public API in `svc.c` follows the existing wrapper pattern (asm
  `svc %imm` on target, direct `__` call under `HOST_TEST`).
- **Showcase demo** (`Core/Src/showcase_tasks.c`, `Core/Inc/showcase_tasks.h`)
  — three sleep-and-poll tasks (`sensor_task`, `ground_task`, `shell_task`)
  that drive every public entry point of the new modules together. Gated
  behind `ENABLE_SHOWCASE` in `main.c` (default off so the existing game
  demo stays primary). When enabled the firmware grows ~2.5 KB text + 33 KB
  bss for the three task stacks.

### Changed

- **`Core/Inc/icarus/icarus.h` umbrella header** now also exports
  `cdc_rx.h`, `event.h`, `crc.h`, `fs.h`, and `tables.h`, so a single
  `#include "icarus/icarus.h"` makes the new modules available alongside
  kernel/scheduler/task/semaphore/pipe.
- **Kernel host test suite** (`tests/Makefile`) gained the new module
  sources (`cdc_rx.c`, `event.c`, `crc.c`, `fs.c`, `tables.c`). The host
  build now links the entire kernel surface — previously the new modules
  were not in `KERNEL_SRC` so `make -C tests` would fail with undefined
  `__cdc_rx_*` / `__os_event` / `__tbl_*` references when the application consumer
  reached for them.
- **`__os_init`**: only one system task is registered now (`ICARUS_KEEPALIVE_TASK`).
  The heartbeat task line is commented out at `Core/Src/icarus/kernel.c:244`
  for the interactive demo. Test `test_os_init` was updated to match.

### Fixed

- **`test_os_init` host test** was asserting `num_created_tasks == 2` and
  probing `task_list[1] / ">ICARUS_HEARTBEAT<"`, causing `make -C tests` to
  report 140/141 passing on every dev rebuild. Heartbeat task has been
  disabled since 0.2.0; test now expects 1 system task.
- **`-Werror=bad-function-cast` warnings in `svc.c`** — eight new SVC
  dispatch arms (cdc_rx_read_byte, event_get_squelch, event_drain, all
  five tbl_*) cast function-call results directly to `uint32_t`, which the
  standalone kernel build's stricter warnings caught. Each cast now goes
  via an intermediate variable.

### Hardening

- **MPU compatibility for new modules** — `cdc_rx`, `event`, and `tables`
  follow the kernel's existing `semaphore` / `pipe` pattern: backing data
  in `DTCM_DATA_PRIV`, hot functions in `ITCM_FUNC`, public entry points
  split into `__`-prefixed privileged implementations + thread-mode
  wrappers that issue SVC instructions. Once the MPU is locked priv-only,
  unprivileged tasks reach these modules through the SVC gates exactly as
  they do for semaphores and pipes.
  - `cdc_rx`: 156 B ITCM / 520 B DTCM_PRIV
  - `event`:  336 B ITCM / 536 B DTCM_PRIV
  - `tables`: 796 B ITCM / 8.3 KB DTCM_PRIV
  - `crc`:    pure function, ITCM only
  - `fs`:     32 KB store stays in regular SRAM (won't fit DTCM); functions
    use the standard `enter_critical()` / `exit_critical()` pattern. The
    deliberate trade-off is documented in `fs.c`.
- **`cdc_rx_push` ISR fast path** — the public wrapper calls
  `__cdc_rx_push` directly without an SVC because the USB CDC ISR is
  already in privileged handler context, and issuing an SVC from a
  high-priority interrupt would either fault or cause a priority
  inversion. Mirrors the pattern used by `pipe_enqueue`.
- **`tbl_activate` priv↔thread split** — the registered activate callback
  is user code that may issue further SVCs, so it cannot run inside the
  SVC handler. The wrapper splits the operation across two SVCs:
  `__tbl_activate_prepare` (validate + copy staging into a thread-mode
  scratch buffer), the user callback runs in thread mode against the
  scratch copy, then `__tbl_activate_commit` (copy scratch into the active
  buffer). The active buffer never moves until the callback succeeds.
- **`os_event` 5-arg packing** — `module_id`, `severity`, `event_id`,
  `payload`, `payload_len` packs into three SVC registers as
  `R0 = (event_id << 16) | (severity << 8) | module_id`, `R1 = payload`,
  `R2 = payload_len`, staying inside the AAPCS R0–R3 budget without
  spilling to the caller's stack.

## [0.2.0] - 2026-04-01

### Added

- **MPU-based memory protection** — DTCM privileged-only region for kernel data
  structures, ITCM read-only + execute protection, per-task data isolation via
  dynamic MPU region reconfiguration on context switch.
- **SVC call-gate architecture** — 40 SVC IDs (0-39) providing unprivileged
  tasks safe access to privileged kernel services, including atomic DTCM
  read/write helpers (IDs 29-39).
- **ICARUS Runner game** — 4-task real-time obstacle-dodge demo showcasing
  preemptive scheduling, IPC pipes, and terminal GUI rendering.
- **Interactive button-LED demo** — hardware button input with LED feedback,
  selectable via compile-time flag.
- **Message pipes** — FIFO byte-stream IPC primitive (`pipe.c`) with blocking
  read/write and configurable buffer sizes.
- **Memory map visualizer** — runtime tool that prints the active linker-section
  layout over USB CDC.
- **Stress test suite** — MPU attack tests, multi-alloc verification, and
  red-team memory probes.
- **DO-178C documentation suite** — PSAC, SDP, SVP, SDD, SRS, SQAP, SCMP,
  coverage analysis, deactivated code analysis, and requirement traceability
  matrix.

### Changed

- Kernel sources moved from `kernel/` to `Core/Src/icarus/` (headers under
  `Core/Inc/icarus/`).
- Unit test suite expanded from 76 to 140 Unity tests.
- Host-based code coverage improved to ~91% line, ~89.5% function.
- Default terminal GUI changed from IPC dashboard to ICARUS Runner game
  (`ENABLE_GAME=1` in `main.c`).
- Copyright headers updated to 2025-2026.

### Fixed

- USB enumeration timing for reliable CDC connection on boot.
- Float handling in unprivileged mode for game physics.
- Task name buffer placement for DTCM protection compatibility.
- MemManage handler made recoverable for MPU fault testing.

## [0.1.0] - 2025-06-15

Initial release — preemptive RTOS kernel with round-robin scheduler,
semaphores, context switching, and basic terminal output on STM32H750VBT6.
