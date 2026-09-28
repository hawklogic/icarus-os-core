# ICARUS OS Unit Tests

Unit testing framework using Unity for DO-178C compliance and code coverage.

## Overview

This test suite provides host-based unit tests for the ICARUS OS kernel. Tests run on your development machine (not the embedded target) and can generate code coverage reports.

## Directory Structure

```
tests/
├── unity/              # Unity testing framework
├── cmock/              # CMock (for future use)
├── src/                # Test source files (test_*.c)
├── mocks/              # Mock implementations for hardware dependencies
├── runners/            # Test runners (auto-generated)
├── build/              # Build output and coverage reports
└── Makefile            # Test build system
```

## Prerequisites

**Required:**
- GCC compiler (for host machine)
- Make

**Optional (for coverage):**
- lcov: `brew install lcov` (macOS) or `apt-get install lcov` (Ubuntu)

## Quick Start

```bash
cd tests

# Run tests
make test

# Run tests with coverage
make COVERAGE=yes test

# Generate coverage report
make coverage

# Generate HTML coverage report
make coverage-html

# View coverage summary
make coverage-summary

# Open HTML report in browser (macOS)
open build/coverage/html/index.html
```

**Coverage Report Location:**
- HTML report: `tests/build/coverage/html/index.html`
- Coverage info: `tests/build/coverage/coverage.info`

**Note:** The HTML report is generated in `tests/build/coverage/html/index.html` (relative to the `tests/` directory).

## Writing Tests

### Test File Structure

Create test files in `tests/src/` with naming convention `test_*.c`:

```c
#include "unity.h"
#include "icarus/task.h"

void setUp(void) {
    // Initialize test fixtures
}

void tearDown(void) {
    // Cleanup after test
}

void test_function_name(void) {
    // Test implementation
    TEST_ASSERT_EQUAL(expected, actual);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_function_name);
    return UNITY_END();
}
```

### Unity Assertions

Common Unity assertions:
- `TEST_ASSERT_EQUAL(expected, actual)` - Compare integers
- `TEST_ASSERT_TRUE(condition)` - Assert true
- `TEST_ASSERT_FALSE(condition)` - Assert false
- `TEST_ASSERT_NULL(ptr)` - Assert NULL pointer
- `TEST_ASSERT_NOT_NULL(ptr)` - Assert non-NULL pointer
- `TEST_ASSERT_GREATER_THAN(threshold, value)` - Value > threshold
- `TEST_ASSERT_LESS_THAN(threshold, value)` - Value < threshold

See Unity documentation for full list.

## Mocks

Hardware dependencies are mocked in `tests/mocks/`:

- `mock_display.c/h` - Display system mocks
- `mock_hal.c/h` - HAL (Hardware Abstraction Layer) mocks
- `mock_usb.c/h` - USB CDC mocks

Add more mocks as needed for other dependencies.

## Coverage

### Generating Coverage Reports

```bash
# Build and run tests with coverage
make COVERAGE=yes test

# Generate coverage report
make coverage-html

# Open in browser
open build/coverage/html/index.html
```

### Coverage Metrics

Coverage reports show:
- **Statement Coverage**: Which lines were executed
- **Branch Coverage**: Which branches (if/else) were taken
- **Function Coverage**: Which functions were called

### DO-178C Coverage Requirements

- **Level C**: 100% statement coverage
- **Level B**: 100% statement + decision coverage
- **Level A**: 100% statement + MC/DC coverage

## Test Organization

### Current Tests

The kernel host suite is **274 tests** as of v0.5.0, organised across
fourteen source files in `tests/src/`:

| File | Tests | Coverage |
|------|------:|----------|
| `test_task.c` | 143 | Kernel core: scheduler, task lifecycle, semaphores, pipes, critical sections, SVC gates, MPU helpers — single Unity entry point (`main()`) |
| `test_crc.c` | 8 | `crc16_ccitt` bytewise loop (HW peripheral path is short-circuited under HOST_TEST) — canonical CCITT-FALSE vector + edge cases |
| `test_cdc_rx.c` | 7 | SPSC ring buffer fill/drain, FIFO order, capacity overflow, wraparound, empty/zero-length corner cases |
| `test_event.c` | 12 | Generic event ring init/emit/drain, per-module squelch filtering, payload truncation, partial drain, full-ring overwrite, drain after wrap |
| `test_fs.c` | 16 | Filesystem create/open/write/read/delete/list/stats; duplicate-name rejection; full-disk; oversized writes; offset reads; invalid handles |
| `test_tables.c` | 16 | Table engine register/load/activate/dump; schema CRC mismatch; activate-callback rejection; chunked load; descriptor query |
| `test_tables_load.c` | 10 | Chunk offsets, identical retransmits, gaps and overruns, mixed schema, abort, commit without prepare, `tbl_get_info`, CRC vector, `cdc_rx` drop counter |
| `test_cdc.c` | 21 | USB CDC transmit ring: ordering, all-or-nothing vs partial writes, chunking and wrap, completion, unconfigured device, link reset, port close/reopen discard |
| `test_stdio.c` | 7 | `_write()` / `stdio_write()` / `__io_putchar()` on the transmit ring, drop counting |
| `test_svc_policy.c` | 12 | SVC caller-buffer allowlist against the target memory map, main-stack window, data-pool slots |
| `test_svc_guard.c` | 7 | Host nested-SVC guard (`SVC_HOST_GATE`); checksum-monitor callbacks run outside the handler and may make kernel calls |
| `test_bkpram.c` | 5 | Backup SRAM round trip, bounds, offset wrap, NULL/zero length |
| `test_iwdg.c` | 7 | Independent watchdog init, refresh, reset flag, argument clamping |
| `test_button.c` | 3 | K1 button active level |

Before the Unity runner, `make test` runs two static checks on the SVC
code (also available as their own targets):

- `make check-svc-asm` — `tools/check_svc_clobbers.py`: every SVC inline
  asm block that passes pointers clobbers `"memory"`.
- `make check-svc-ptr` — `tools/check_svc_pointer_checks.py`: every SVC
  dispatch case validates a caller pointer before using it.

`test_task.c` owns the single Unity `main()` entry point. Each
per-module file declares an aggregator `void run_<module>_tests(void)`
which `test_task.c` calls after the existing kernel tests, so a
single `make test` runs the whole suite.

### Coverage

Latest measured baseline (v0.3.0; not yet regenerated for v0.5.0):
- **Lines:** 91.8% (1081 of 1178)
- **Functions:** 92.5% (149 of 161)

Run `make COVERAGE=yes clean test coverage-summary` to recompute
locally; HTML reports land at `build/coverage/html/index.html`.

### Adding New Tests

1. Create `tests/src/test_<feature>.c`
2. Include `unity.h` and the kernel headers you're testing
3. Write test functions with the `test_` prefix
4. Add an aggregator `void run_<feature>_tests(void) { RUN_TEST(...); }`
5. Declare the aggregator in `test_task.c` near the top of `main()`
   and invoke it after the existing tests
6. Run: `make test`

## Integration with Main Build

The test system is separate from the embedded target build:

- **Target build**: `cd build && make` (ARM toolchain, for STM32)
- **Test build**: `cd tests && make` (Host GCC, for unit tests)

Coverage from tests can be integrated into DO-178C verification evidence.

## Troubleshooting

### "gcc: command not found"
- Install GCC: `xcode-select --install` (macOS) or `apt-get install gcc` (Ubuntu)

### "lcov: command not found"
- Install lcov: `brew install lcov` (macOS) or `apt-get install lcov` (Ubuntu)
- Coverage reports still work without lcov, but HTML reports won't generate

### Tests fail to compile
- Check that mocks exist for all hardware dependencies
- Verify include paths in Makefile
- Ensure kernel source compiles with `-DHOST_TEST` define

### No coverage data
- Ensure `COVERAGE=yes` when building: `make COVERAGE=yes test`
- Check that `.gcda` files are generated in `build/obj/`
- Run tests before generating coverage: `make test` then `make coverage`

## Next Steps

1. Add more test cases for kernel functions
2. Test task creation, scheduling, and state management
3. Add integration tests for task interactions
4. Achieve 100% statement coverage for kernel code
5. Add MC/DC coverage analysis for Level A compliance
