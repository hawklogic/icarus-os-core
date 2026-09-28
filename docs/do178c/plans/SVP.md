# Software Verification Plan (SVP)

**Document ID:** ICARUS-SVP-001
**Version:** 0.4
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
| 0.2 | 2026-04-01 | Souham Biswas | Added §4.5 Memory Protection Tests with red-team attack vectors and fault recovery verification |
| 0.3 | 2026-04-11 | Souham Biswas | Added §4.6 Shared Service Module Tests for the v0.3.0 modules; updated §5.4 coverage baseline (91.8% line / 92.5% function across 196 host tests) |
| 0.4 | 2026-09-27 | Souham Biswas | v0.5.0: added §4.7 Robustness Tests (CDC transmit ring, console retarget, SVC caller-buffer policy, nested-SVC guard, backup SRAM, table load extensions, timed semaphore timeout on the system tick, target FPU-context probe for the context switch); added the SVC static checks to §6.1 and §6.4; host baseline 274 tests |

---

## 1. Introduction

### 1.1 Purpose

This Software Verification Plan (SVP) defines the verification strategy, methods, and procedures for ICARUS OS. It establishes how software requirements are verified and how structural coverage objectives are achieved per DO-178C DAL C.

### 1.2 Scope

This plan covers verification of:
- ICARUS OS Kernel
- Board Support Package (BSP)
- AI Runtime Environment
- Integration with target hardware

### 1.3 Verification Objectives

| DO-178C Table | Objective | Applicable |
|---------------|-----------|------------|
| A-3 | Verify HLR compliance with system requirements | Yes |
| A-4 | Verify LLR compliance with HLR | Yes |
| A-5 | Verify source code compliance with LLR | Yes |
| A-6 | Verify executable object code | Yes |
| A-7 | Verify test coverage | Yes |

---

## 2. Verification Strategy

### 2.1 Multi-Level Verification

```
┌─────────────────────────────────────────────────────────────┐
│                 ICARUS Verification Pyramid                  │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│                    ┌─────────────┐                          │
│                    │   System    │  Target hardware         │
│                    │   Tests     │  End-to-end scenarios    │
│                    └──────┬──────┘                          │
│                   ┌───────┴───────┐                         │
│                   │  Integration  │  Component interaction  │
│                   │    Tests      │  Hardware-in-loop       │
│                   └───────┬───────┘                         │
│              ┌────────────┴────────────┐                    │
│              │      Unit Tests         │  Function-level    │
│              │   (Host + Target)       │  Branch coverage   │
│              └────────────┬────────────┘                    │
│         ┌─────────────────┴─────────────────┐               │
│         │        Static Analysis            │  MISRA, lint  │
│         │     (Continuous Integration)      │  Complexity   │
│         └─────────────────┬─────────────────┘               │
│    ┌──────────────────────┴──────────────────────┐          │
│    │              Code Reviews                    │  Manual  │
│    │         (Every Pull Request)                │  Inspect │
│    └─────────────────────────────────────────────┘          │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Verification Methods

| Method | Description | When Used |
|--------|-------------|-----------|
| **Review** | Manual inspection of artifacts | Requirements, design, code |
| **Analysis** | Automated examination | Static analysis, coverage |
| **Testing** | Dynamic execution | Unit, integration, system |
| **Formal Methods** | Mathematical proof | Critical algorithms (planned) |


---

## 3. Test Environment

### 3.1 Host-Based Testing

**Purpose:** Rapid iteration, high coverage, CI integration

| Component | Tool | Version |
|-----------|------|---------|
| Compiler | GCC (native) | System |
| Test Framework | Unity | 2.5.x |
| Coverage | lcov/gcov | 2.x |
| Mocking | Custom mocks | - |
| CI Runner | GitHub Actions | - |

**Capabilities:**
- ✅ Kernel logic testing
- ✅ Algorithm verification
- ✅ Branch coverage measurement
- ❌ Hardware interaction
- ❌ Timing verification
- ❌ Interrupt behavior

### 3.2 Target-Based Testing

**Purpose:** Hardware verification, timing analysis, integration

| Component | Tool | Version |
|-----------|------|---------|
| Compiler | ARM GCC | 12.x |
| Debug Probe | ST-Link / J-Link | - |
| Trace | ITM/SWO | - |
| Logic Analyzer | Saleae / similar | - |
| Serial Monitor | USB CDC | - |

**Capabilities:**
- ✅ Real hardware execution
- ✅ Interrupt handling
- ✅ Timing measurement
- ✅ Peripheral interaction
- ✅ Context switch verification

### 3.3 Simulation Testing

**Purpose:** CI integration for target code, fault injection

| Component | Tool | Purpose |
|-----------|------|---------|
| QEMU | ARM Cortex-M | Instruction-level simulation |
| Renode | Multi-platform | Peripheral simulation |
| Custom Sim | x86 BSP | Fast iteration |

### 3.4 AI Model Testing

**Purpose:** Verify AI inference correctness and timing

| Test Type | Method | Criteria |
|-----------|--------|----------|
| Accuracy | Golden reference comparison | <0.1% deviation |
| Timing | WCET measurement | Within budget |
| Robustness | Input perturbation | Bounded output |
| Integrity | Model hash verification | Match expected |

---

## 4. Test Categories

### 4.1 Unit Tests

**Scope:** Individual functions and modules

**Location:** `tests/src/test_*.c`

**Naming Convention:**
```c
void test_<module>_<function>_<scenario>(void);

// Examples:
void test_task_create_normal(void);
void test_task_create_max_tasks(void);
void test_scheduler_yield_blocked_task(void);
void test_ai_inference_simple_model(void);
```

**Coverage Target:** 100% statement coverage (testable code)

### 4.2 Integration Tests

**Scope:** Component interactions

**Categories:**

| Category | Description | Environment |
|----------|-------------|-------------|
| Kernel Integration | Scheduler + Task + IPC | Host + Target |
| BSP Integration | Kernel + Hardware drivers | Target only |
| AI Integration | AI Runtime + Kernel | Target only |
| Full Stack | Application + Kernel + BSP | Target only |

### 4.3 System Tests

**Scope:** End-to-end scenarios

**Test Scenarios:**

| Scenario | Description | Pass Criteria |
|----------|-------------|---------------|
| Boot sequence | Cold start to running | <100ms to first task |
| Task stress | MAX_TASKS concurrent | No crashes, fair scheduling |
| Sleep accuracy | Various sleep durations | ±1 tick accuracy |
| Watchdog recovery | Induced hang | Reset and recovery |
| AI inference | Model execution | Correct output, within WCET |

### 4.4 Robustness Tests

**Scope:** Error handling and edge cases

| Test | Input | Expected Behavior |
|------|-------|-------------------|
| Invalid task pointer | NULL | Return error, no crash |
| Stack overflow | Deep recursion | Detection, controlled halt |
| Invalid priority | Out of range | Clamp or reject |
| Corrupted model | Bad CRC | Reject, fallback |

### 4.5 Memory Protection Tests

**Scope:** MPU configuration and fault handling

**Purpose:** Verify memory protection mechanisms prevent unauthorized access

| Test | Attack Vector | Expected Behavior | Verifies |
|------|---------------|-------------------|----------|
| `mpu_redteam_task` | Write to another task's data region | MemManage fault, task continues | HLR-KRN-071 |
| `mpu_itcm_write_test` | Write to ITCM (code region) | MemManage fault, task continues | HLR-KRN-066 |
| `mpu_dtcm_attack_task` | Read from DTCM (kernel data) | MemManage fault, task continues | HLR-KRN-070 |
| `mpu_kernel_bypass_test` | Direct call to `__os_*` function | MemManage fault when accessing DTCM | HLR-KRN-070 |

**Test Execution:**
- Tests run continuously as stress test tasks
- Real-time status displayed via USB CDC terminal
- Fault count monitored to verify protection active
- Pass criteria: Fault count increases on each attack attempt

**Verification Evidence:**
```
MPU_VICTIM: allocs=3 verifies=7 corruptions=0 [PASS]
MPU_REDTEAM: attacks=26 own_data=OK [PROTECTED]
MPU_ITCM_WR: attempts=4 faults=27 [WRITE PROTECTED]
MPU_DTCM: attempts=4 faults=28 [DTCM PROTECTED]
MPU_BYPASS: attempts=4 faults=30 [DTCM PROTECTED]
```

**Fault Recovery Verification:**
- MemManage handler advances PC past faulting instruction
- Task continues execution after fault
- Fault address and PC captured for analysis
- System remains stable after multiple faults

### 4.6 Shared Service Module Tests (v0.3.0)

**Scope:** Verify the five v0.3.0 shared service modules
(`cdc_rx`, `event`, `crc`, `fs`, `tables`) against their HLR-KRN-09x
requirements.

**Location:** `tests/src/test_<module>.c`. Each file declares an
aggregator `void run_<module>_tests(void)` which `test_task.c`
calls from `main()` after the existing kernel tests.

| File | Tests | Verifies | Notes |
|---|---:|---|---|
| `test_crc.c` | 8 | HLR-KRN-092, .1, .2 | Canonical CCITT-FALSE vector ("123456789" → 0x29B1), single-byte vectors, NULL/zero-length, repeatability, single-bit and length sensitivity. The HW peripheral path is short-circuited under HOST_TEST so the host suite verifies the portable bytewise fallback. |
| `test_cdc_rx.c` | 7 | HLR-KRN-090, .1, .2 | Init clears head/tail; push/drain FIFO order; 600-byte overflow into a 512-byte ring fills exactly to capacity-1; wraparound across the buffer end; zero-length push and read-on-empty are no-ops. |
| `test_event.c` | 9 | HLR-KRN-091, .1, .2, .3 | Init resets ring + squelch; emit/drain roundtrip preserving module_id, severity, event_id, payload; per-module squelch drops below-threshold events; module_id ≥ MAX dropped silently; payload >12 B truncated; partial drains preserve FIFO order; ring overflow caps at the maximum count. |
| `test_fs.c` | 16 | HLR-KRN-093, .1, .2 | Init/create/open/write/read/delete/list/stats; duplicate-name and invalid-name rejection; full-disk rejection; oversized-write rejection; offset reads; invalid-handle writes. |
| `test_tables.c` | 16 | HLR-KRN-094, .1, .2, .3 | Register/load/activate/dump roundtrip; NULL/duplicate/invalid-size register rejection; load against unknown id; load overflow; activate without load; schema_crc mismatch rejected before callback fires; activate-callback returning false leaves the active buffer untouched (verifying the priv↔thread split); chunked load reassembly. |

**Test Execution:**
- All 56 new tests run as part of the standard `make -C tests` host
  build alongside the existing kernel suite.
- Aggregators are wired into `test_task.c`'s single Unity entry point
  via `extern` declarations followed by `run_<module>_tests()` calls
  after the existing tests.

**Pass criteria (v0.3.0 baseline):** 196/196 host tests passing,
zero failures. The previous baseline was 140/140 — the +56 tests
brought total kernel coverage back to 91.8% line / 92.5% function
after the new modules expanded the coverage denominator.

### 4.7 Robustness Tests (v0.5.0)

**Scope:** Verify the v0.5.0 robustness changes. Each behaviour below
has a host test or a static check guarding it; most were first seen
failing on target hardware.

**Location:** `tests/src/test_<module>.c`, each with a
`run_<module>_tests()` aggregator called from `test_task.c`.

| File | Tests | Verifies | Notes |
|---|---:|---|---|
| `test_cdc.c` | 21 | HLR-BSP-027 to HLR-BSP-027.7 | `CDC_Write` / `CDC_WriteString` contract; ring ordering across text and binary writes; all-or-nothing vs partial; chunk cap and wrap-around; completion advances the tail; not-configured then configured; output queued before a host attaches is sent on port open; a link reset clears a wedged transfer; a foreign busy endpoint leaves data queued; port close discards the queued bytes but keeps the transfer in flight (also with a wrapped head, and when idle or empty); a reopen drops output written while the port was closed, and a report that does not change DTR drops nothing; the first open since boot keeps earlier output, and a link loss while open counts as a close; NULL/empty rejected. Manual completion is driven through the `__cdc_host_*` hooks |
| `test_stdio.c` | 7 | HLR-BSP-028 to HLR-BSP-028.2 | Console bytes reach the ring in order; not-configured and busy endpoint never wait; a full ring drops and counts; output recovers after drops; NULL/empty rejected |
| `test_svc_policy.c` | 12 | HLR-KRN-074.1–074.3, 074.6, 074.7, HLR-KRN-077 | `svc_buffer_allowed()` with the target memory map: flash and ITCM readable not writable; RAM_D1 including straddling ranges; main-stack window rejected for a task caller (read and write, including straddling ranges) and allowed for main-stack boot code; no window configured; NULL caller rejected; application DTCM half allowed, kernel half rejected; own data-pool slot only; privileged, device and system space rejected; NULL and wrap rejected, zero length allowed |
| `test_svc_guard.c` | 7 | HLR-KRN-076, HLR-KRN-096.2 | Nested gates detected, sequential gates allowed, depth unwinds; checksum mismatch callbacks may call kernel APIs (delivered in thread mode) |
| `test_bkpram.c` | 5 | HLR-KRN-078, HLR-KRN-078.1, HLR-KRN-078.2 | Round trip; last byte accepted, one past rejected; zero length and NULL rejected; offset wrap rejected; data survives simulated resets until cleared |
| `test_tables_load.c` | 10 | HLR-KRN-094.4 to HLR-KRN-094.7, HLR-KRN-092.3, HLR-KRN-090.3 | Identical retransmit idempotent; conflicting retransmit, gap, overrun and mixed schema rejected; abort; commit without prepare rejected; `tbl_get_info` copy-out; CRC wrapper matches the privileged implementation; CDC RX drop counter |
| `test_task.c` (2 of its 143) | 2 | HLR-KRN-098, HLR-KRN-098.1 | `test_semaphore_consume_timeout_counts_elapsed_ticks`: with each sleep lasting 100 ticks (as when two other tasks use their full 50-tick slices), a 1000-tick timeout gives up after 1000–1099 ticks, not 1000 sleeps. `test_semaphore_consume_timeout_immediate_and_try`: an available semaphore is taken without sleeping, `max_ticks` == 0 tries once, and a 500-tick wait started just before the tick-counter wrap ends after 500–599 ticks. Both set the tick advance per sleep with the host-only `__sched_host_set_ticks_per_sleep()`; on the v0.4 code, which counted sleeps, they fail with 100000 and 50000 ticks elapsed |

**Static checks:**

| Check | Verifies | When |
|---|---|---|
| `tools/check_svc_clobbers.py` | HLR-KRN-075, HLR-KRN-075.1: every SVC inline-asm block clobbers `"memory"` | `make -C tests` target `check-svc-asm`, before the Unity runner |
| `tools/check_svc_pointer_checks.py` | HLR-KRN-074, HLR-KRN-074.8: every SVC dispatch case that turns a caller argument into a pointer validates it first | `make -C tests` target `check-svc-ptr`, before the Unity runner |

**Target-only behaviour** (host builds cannot observe it; verified by
target integration test and review, see `test_traceability.md` §5): the
per-gate buffer checks inside `SVC_Handler_C()`, handler-mode detection in
`svc_caller_is_privileged()`, task/main-stack caller classification in
`svc_current_caller()` (and, by review only, that the main stack stays
within `_Min_Stack_Size`), the PRIMASK lock around the transmit ring,
retention of backup SRAM across a real reset, and the ROM bootloader
entry (HLR-BSP-029).

**Target FPU-context probe (HLR-KRN-016).** The context switch is
assembly and is replaced on the host by `tests/mocks/mock_asm.c`, so no
host test can see whether a task's floating-point registers survive a
switch. It is verified on target by the target FPU-context probe (two
FP-using tasks): each task fills S0–S31 with its own pattern, spins until
it has been switched out, and counts the registers that changed. Before
the fix the probe saw about 9,000 changed registers in 30 s; after it,
none in about 28,000 rounds (120 s), with no missed watchdog deadlines.
Pass criterion: zero changed registers.

The USB callbacks that drive the transmit ring on DTR changes, link loss
and bus resume are not built on the host; the ring functions they call
are host-tested. Close and reopen are exercised on hardware (the device
keeps working across a 30 s close and reopen); the discard behaviour is
host-tested only. The bus-resume path (PHY clock restart, then kick) is
host-tested only and not exercised on hardware.

**Pass criteria (v0.5.0 baseline):** 274/274 host tests passing, zero
failures, and both SVC static checks passing.


---

## 5. Structural Coverage

### 5.1 Coverage Requirements (DAL C)

| Coverage Type | Required | Tool |
|---------------|----------|------|
| Statement Coverage | 100% | lcov/gcov |
| Decision Coverage | Not required | - |
| MC/DC | Not required | - |

### 5.2 Coverage Analysis Process

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│  Run Tests   │───▶│   Collect    │───▶│   Generate   │
│  (Unity)     │    │   .gcda      │    │   Report     │
└──────────────┘    └──────────────┘    └──────────────┘
                                               │
                                               ▼
                                        ┌──────────────┐
                                        │   Analyze    │
                                        │    Gaps      │
                                        └──────────────┘
                                               │
                    ┌──────────────────────────┼──────────────────────────┐
                    ▼                          ▼                          ▼
             ┌──────────────┐          ┌──────────────┐          ┌──────────────┐
             │  Add Tests   │          │   Justify    │          │   Mark as    │
             │  (if needed) │          │  (dead code) │          │  Deactivated │
             └──────────────┘          └──────────────┘          └──────────────┘
```

### 5.3 Coverage Exclusions

The following code is excluded from coverage requirements:

| Category | Examples | Justification |
|----------|----------|---------------|
| Fault handlers | HardFault_Handler | Dead code - only on faults |
| Infinite loop tasks | os_idle_task | Requires target testing |
| Assembly code | context_switch.s | Not measurable on host |
| Vendor code | STM32 HAL | Third-party, not in scope |

See `ICARUS-VER-002 Deactivated Code Analysis` for complete list.

### 5.4 Current Coverage Status

| Component | Statement | Functions | Status |
|-----------|-----------|-----------|--------|
| `Core/Src/icarus/*.c` (aggregate) | ~92% stmt | ~92% fn | Host Unity tests (see `verification/coverage_analysis.md`); includes the v0.3.0 shared modules `cdc_rx`, `event`, `crc`, `fs`, `tables` |
| bsp/display.c | ~94% | 100% | Host tests |
| bsp/stm32h7xx_it.c | ~63% | 40% | Fault paths: target / review |
| **Overall (filtered host report, v0.3.0 baseline)** | **91.8%** | **92.5%** | 196 host tests; regenerate with `tests/make coverage-summary` |

**Adjusted (excluding justified deactivated code):** Documented in `deactivated_code.md` and refreshed with each baseline.

---

## 6. Static Analysis

### 6.1 Tools and Configuration

| Tool | Purpose | Configuration |
|------|---------|---------------|
| **cppcheck** | General static analysis | `--enable=all --std=c11` |
| **PC-lint Plus** | MISRA C:2012 | DAL C rule subset |
| **Clang-Tidy** | Modern C analysis | cert-*, bugprone-* |
| **Coverity** | Deep analysis | Planned (CI integration) |
| **check_svc_clobbers.py** | SVC inline asm has a `"memory"` clobber | `tools/`; run by `make -C tests` (see §6.4) |
| **check_svc_pointer_checks.py** | SVC dispatch cases validate caller pointers | `tools/`; run by `make -C tests` (see §6.4) |

### 6.2 MISRA Compliance

**Target:** MISRA C:2012 (DAL C subset)

| Category | Rules | Compliance |
|----------|-------|------------|
| Mandatory | 10 | 100% required |
| Required | 121 | 95%+ target |
| Advisory | 39 | Best effort |

**Deviation Process:**
1. Document deviation in code comment
2. Add to deviation log (Appendix A of SDP)
3. Review and approve deviation
4. Track in configuration management

### 6.3 Complexity Metrics

| Metric | Threshold | Action if Exceeded |
|--------|-----------|-------------------|
| Cyclomatic complexity | ≤15 | Refactor required |
| Function length | ≤100 lines | Refactor recommended |
| Nesting depth | ≤4 levels | Refactor required |
| Parameters | ≤6 | Consider struct |

### 6.4 SVC Static Checks (v0.5.0)

Host tests call the privileged implementations directly and never
execute the SVC inline assembly or the target dispatcher, so two classes
of defect are invisible to them. Two project scripts guard them:

| Script | Rule | Failure mode it prevents | Requirement |
|--------|------|--------------------------|-------------|
| `tools/check_svc_clobbers.py` | Every `__asm__ volatile` block that contains an `svc` instruction lists `"memory"` in its clobbers (default scope: `Core/Src/**/*.c`) | At -O2 the compiler keeps memory in registers across the SVC or assumes an out-parameter is unchanged; `tbl_activate()` read its out-parameters before the handler wrote them and every activation failed on target | HLR-KRN-075, HLR-KRN-075.1 |
| `tools/check_svc_pointer_checks.py` | In every SVC dispatch `switch`, each cast of a caller argument (or an expression built from it) to a data pointer is preceded, in the same case, by a caller-buffer validator on that argument; function-pointer casts only in allowlisted cases; unknown cast types and stale allowlist entries fail | A task makes the kernel copy, with privilege, into or out of memory it does not own; a new gate forgets the check | HLR-KRN-074, HLR-KRN-074.8 |

Both scripts exit non-zero and name each offending file and line. The
`check-svc-asm` and `check-svc-ptr` targets in `tests/Makefile` run them
before the Unity runner, so a missing clobber or an unvalidated caller
pointer fails `make -C tests` and CI. `check_svc_pointer_checks.py` is
textual: it does not prove that the validator's result gates the use or
that the length passed to it is right (the host policy tests cover the
policy itself).


---

## 7. AI Verification

### 7.1 AI-Specific Verification Challenges

| Challenge | ICARUS Approach |
|-----------|-----------------|
| Non-determinism | Fixed-point only, no floating-point |
| Opacity (black box) | Operator whitelist, layer-by-layer verification |
| Training data | Out of scope (model provided as input) |
| Edge cases | Input space coverage analysis |
| Timing variability | WCET analysis per layer |

### 7.2 AI Verification Methods

```
┌─────────────────────────────────────────────────────────────┐
│                 AI Verification Framework                    │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  1. MODEL VALIDATION (Pre-deployment)                       │
│     ├── Structure verification (whitelist operators)        │
│     ├── Size verification (fits in memory)                  │
│     ├── Integrity verification (CRC/hash)                   │
│     └── Quantization verification (int8/int16 only)         │
│                                                              │
│  2. INFERENCE VERIFICATION (Runtime)                        │
│     ├── Golden reference comparison                         │
│     ├── Output bounds checking                              │
│     ├── Timing measurement vs WCET budget                   │
│     └── Confidence threshold enforcement                    │
│                                                              │
│  3. OPERATOR VERIFICATION (Library)                         │
│     ├── Unit tests for each operator                        │
│     ├── Numerical accuracy tests                            │
│     ├── Boundary condition tests                            │
│     └── Overflow/underflow tests                            │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 7.3 AI Test Cases

| Test Category | Test Cases | Coverage |
|---------------|------------|----------|
| Operator correctness | Conv2D, Dense, ReLU, etc. | Per operator |
| Model loading | Valid, invalid, corrupted | All paths |
| Inference accuracy | Reference comparison | Representative inputs |
| Timing bounds | WCET measurement | All models |
| Memory bounds | Allocation verification | All models |

### 7.4 AI Certification Artifacts

| Artifact | Description | Status |
|----------|-------------|--------|
| Operator Library Test Report | Unit test results for all operators | Planned |
| Model Verification Report | Per-model validation results | Planned |
| WCET Analysis Report | Timing bounds for inference | Planned |
| AI Runtime Coverage Report | Code coverage for AI subsystem | Planned |

---

## 8. Test Procedures

### 8.1 Unit Test Execution

```bash
# Host-based unit tests
cd tests
make clean test              # Run all tests
make coverage-summary        # Show coverage summary
make coverage-html           # Generate HTML report

# Expected output (example, will evolve):
# check_svc_clobbers: N files OK
# check_svc_pointer_checks: 1 dispatch, N cases, ... OK
# 274 Tests 0 Failures 0 Ignored
# ~91% lines, ~89.5% functions (host, filtered kernel+BSP)
```

### 8.2 Integration Test Execution

```bash
# Target-based tests (requires hardware)
cd tests/integration
make flash                   # Flash test firmware
make run                     # Execute tests via serial
make collect                 # Collect results

# Or via debug probe:
arm-none-eabi-gdb -x test_script.gdb
```

### 8.3 Coverage Collection

```bash
# Collect and report coverage
cd tests
make coverage-html
open build/coverage/html/index.html

# CI enforcement
if [ $(lcov --summary coverage.info | grep lines | awk '{print $2}') -lt 80 ]; then
    echo "Coverage below threshold"
    exit 1
fi
```

---

## 9. Verification Records

### 9.1 Test Results Format

```
Test Run: ICARUS-TR-<date>-001
Date: <run date>
Environment: Host (macOS, GCC)
Configuration: Debug, HOST_TEST defined

Results:
  Total:    140    (run `cd tests && make test` for live count)
  Passed:   <n>
  Failed:   <n>
  Ignored:  <n>

Coverage:
  Lines:     ~91% (714/784, host filtered kernel+BSP)
  Functions: ~89.5% (102/114)
  
Deactivated Code: see VER-002 (deactivated_code.md)
Adjusted Coverage: refer to coverage_analysis.md §5
```

### 9.2 Verification Matrix

| Requirement | Test Case | Result | Coverage |
|-------------|-----------|--------|----------|
| KRN-001 | test_os_init | PASS | 100% |
| KRN-002 | test_os_register_task | PASS | 100% |
| ... | ... | ... | ... |

See `ICARUS-VER-003 Test Traceability Matrix` for complete matrix.

---

## 10. Verification Schedule

| Phase | Activity | Target Date | Status |
|-------|----------|-------------|--------|
| 1 | Host unit tests | 2025-01-26 | ✅ Complete |
| 2 | Static analysis (cppcheck) | 2025-01-26 | ✅ Complete |
| 3 | MISRA analysis | TBD | Planned |
| 4 | Target unit tests | TBD | Planned |
| 5 | Integration tests | TBD | Planned |
| 6 | System tests | TBD | Planned |
| 7 | AI verification | TBD | Planned |
| 8 | Final coverage analysis | TBD | Planned |

---

*End of Document*
