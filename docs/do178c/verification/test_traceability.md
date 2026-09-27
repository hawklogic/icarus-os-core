# Test Traceability Matrix

**Document ID:** ICARUS-VER-003  
**Version:** 0.4
**Date:** 2026-09-27  
**Status:** Draft  
**Classification:** Public (Open Source)  

## 1. Purpose

This document provides traceability between software requirements, test cases, and source code in accordance with DO-178C objectives A-5 (Software Verification Traceability).

## 2. Traceability Overview

```
Requirements (HLR/LLR) → Test Cases → Source Code
         ↓                    ↓            ↓
    Verified by          Executes      Implements
```

## 3. Functional Requirements Traceability

### 3.1 Kernel - Task Management

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| KRN-001 | System shall initialize kernel state | `test_os_init` | `os_init()` |
| KRN-002 | System shall support task registration | `test_os_register_task`, `test_os_register_task_multiple` | `os_register_task()` |
| KRN-003 | System shall create tasks with stack allocation | `test_os_create_task_normal`, `test_os_create_task_long_name` | `os_create_task()` |
| KRN-004 | System shall enforce maximum task limit | `test_os_create_task_max_tasks`, `test_os_create_task_max_running_tasks` | `os_create_task()` |
| KRN-005 | System shall start task execution | `test_os_start_valid_tasks`, `test_os_start_no_tasks` | `os_start()` |
| KRN-006 | System shall support task yielding | `test_os_yield` | `os_yield()` |
| KRN-007 | System shall support task exit | `test_os_exit_task_with_running_count`, `test_os_exit_task_zero_running_count` | `os_exit_task()` |
| KRN-008 | System shall support task termination | `test_os_kill_process_valid`, `test_os_kill_process_suicide` | `os_kill_process()` |
| KRN-009 | System shall protect task 0 from termination | `test_os_kill_process_index_zero`, `test_os_kill_process_index_zero_error` | `os_kill_process()` |
| KRN-010 | System shall track cleanup tasks | `test_os_exit_task_cleanup_idx_max`, `test_os_kill_process_cleanup_idx_max` | `os_exit_task()`, `os_kill_process()` |

### 3.2 Kernel - Task Scheduling

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| KRN-020 | System shall support active sleep | `test_task_active_sleep` | `task_active_sleep()` |
| KRN-021 | System shall support blocking sleep | `test_task_blocking_sleep` | `task_blocking_sleep()` |
| KRN-022 | System shall support busy wait | `test_task_busy_wait_basic` | `task_busy_wait()` |
| KRN-023 | System shall maintain tick count | `test_os_get_tick_count` | `os_get_tick_count()` |
| KRN-024 | System shall track running task count | `test_os_get_running_task_count` | `os_get_running_task_count()` |
| KRN-025 | System shall provide current task name | `test_os_get_current_task_name`, `test_os_get_current_task_name_null_task` | `os_get_current_task_name()` |
| KRN-026 | System shall track task ticks remaining | `test_os_get_task_ticks_remaining` | `os_get_task_ticks_remaining()` |

### 3.3 Kernel - Critical Sections

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| KRN-030 | System shall support nested critical sections | `test_critical_sections`, `test_exit_critical_depth_nonzero` | `enter_critical()`, `exit_critical()` |
| KRN-031 | System shall re-enable scheduler on critical exit | `test_exit_critical_depth_zero` | `exit_critical()` |

### 3.4 Kernel - Semaphores

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| KRN-040 | System shall initialize semaphores | `test_semaphore_init_valid`, `test_semaphore_init_invalid_idx` | `semaphore_init()` |
| KRN-041 | System shall support semaphore feed | `test_semaphore_feed_valid`, `test_semaphore_feed_invalid_idx` | `semaphore_feed()` |
| KRN-042 | System shall support semaphore consume | `test_semaphore_consume_valid`, `test_semaphore_consume_invalid_idx` | `semaphore_consume()` |
| KRN-043 | System shall enforce bounded semaphore capacity | `test_semaphore_feed_valid` | `semaphore_feed()` |
| KRN-044 | System shall provide semaphore count query | `test_semaphore_get_count_valid`, `test_semaphore_get_count_invalid` | `semaphore_get_count()` |
| KRN-045 | System shall provide semaphore max count query | `test_semaphore_get_max_count_valid`, `test_semaphore_get_max_count_invalid` | `semaphore_get_max_count()` |

### 3.5 Kernel - Message Pipes

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| KRN-050 | System shall initialize message pipes | `test_pipe_init_valid`, `test_pipe_init_invalid_idx`, `test_pipe_init_zero_capacity` | `pipe_init()` |
| KRN-051 | System shall support pipe enqueue | `test_pipe_enqueue_valid`, `test_pipe_enqueue_invalid_idx` | `pipe_enqueue()` |
| KRN-052 | System shall support pipe dequeue | `test_pipe_dequeue_valid`, `test_pipe_dequeue_invalid_idx`, `test_pipe_dequeue_empty` | `pipe_dequeue()` |
| KRN-053 | System shall maintain FIFO ordering | `test_pipe_enqueue_valid`, `test_pipe_dequeue_valid` | `pipe_enqueue()`, `pipe_dequeue()` |
| KRN-054 | System shall enforce pipe capacity | `test_pipe_enqueue_valid` | `pipe_enqueue()` |
| KRN-055 | System shall provide pipe count query | `test_pipe_get_count_valid`, `test_pipe_get_count_invalid` | `pipe_get_count()` |
| KRN-056 | System shall provide pipe max count query | `test_pipe_get_max_count_valid`, `test_pipe_get_max_count_invalid` | `pipe_get_max_count()` |

### 3.6 Kernel - Print Buffer

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| KRN-060 | System shall enqueue characters to print buffer | `test_enqueue_print_buffer_basic` | `enqueue_print_buffer()` |
| KRN-061 | System shall detect buffer full condition | `test_enqueue_print_buffer_full` | `enqueue_print_buffer()` |
| KRN-062 | System shall wrap buffer index | `test_enqueue_print_buffer_wrap_around` | `enqueue_print_buffer()` |

### 3.7 BSP - Display

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| BSP-001 | System shall initialize display once | `test_display_init_first_call`, `test_display_init_second_call` | `display_init()` |
| BSP-002 | System shall render progress bar | `test_display_render_bar_normal`, `test_display_render_bar_zero_period` | `display_render_bar()` |
| BSP-003 | System shall clamp progress bar values | `test_display_render_bar_elapsed_exceeds_period`, `test_display_render_bar_filled_exceeds_width` | `display_render_bar()` |
| BSP-004 | System shall handle null task name | `test_display_render_bar_null_name` | `display_render_bar()` |
| BSP-005 | System shall render banner | `test_display_render_banner_on`, `test_display_render_banner_off` | `display_render_banner()` |
| BSP-006 | System shall render vertical bar | `test_display_render_vbar_normal`, `test_display_render_vbar_zero_max`, `test_display_render_vbar_value_exceeds_max` | `display_render_vbar()` |
| BSP-007 | System shall render pipe visualization | `test_display_render_pipe_normal`, `test_display_render_pipe_zero_capacity` | `display_render_pipe()` |
| BSP-008 | System shall render producer visualization | `test_display_render_producer_normal` | `display_render_producer()` |
| BSP-009 | System shall render consumer visualization | `test_display_render_consumer_normal` | `display_render_consumer()` |
| BSP-010 | System shall initialize message history | `test_msg_history_init` | `msg_history_init()` |
| BSP-011 | System shall add messages to history | `test_msg_history_add_normal`, `test_msg_history_add_wrap`, `test_msg_history_add_null_msg` | `msg_history_add()` |
| BSP-012 | System shall render message history | `test_display_render_msg_history_empty`, `test_display_render_msg_history_partial`, `test_display_render_msg_history_full`, `test_display_render_msg_history_wrapped` | `display_render_msg_history()` |

### 3.8 BSP - LED Control

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| BSP-010 | System shall turn LED on | `test_LED_On` | `LED_On()` |
| BSP-011 | System shall turn LED off | `test_LED_Off` | `LED_Off()` |
| BSP-012 | System shall blink LED with timing | `test_LED_Blink`, `test_LED_Blink_zero_delays` | `LED_Blink()` |

### 3.9 BSP - Platform I/O

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| BSP-020 | System shall write to I2C | `test_platform_write`, `test_platform_write_zero_length` | `platform_write()` |
| BSP-021 | System shall read from I2C | `test_platform_read`, `test_platform_read_zero_length` | `platform_read()` |
| BSP-022 | System shall initialize hardware | `test_hal_init` | `hal_init()` |

### 3.10 BSP - Standard I/O *(revised v0.5.0)*

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| HLR-BSP-028 | Console output (`_write()`, `__io_putchar`) queues what fits on the CDC transmit ring, drops the rest, reports every byte as written and never blocks; `stdio_write()` returns the bytes queued | `test_stdio_write_sends_bytes_and_counts_nothing`, `test_stdio_putchar_returns_char_and_keeps_order`, `test_stdio_not_configured_queues_without_retry`, `test_stdio_busy_endpoint_does_not_wait`, `test_stdio_write_rejects_null_and_empty`, `test_io_putchar_normal`, `test_io_putchar_newline`, `test_io_putchar_buffer_full` | `stdio_write()`, `__io_putchar()`, `_write()` |
| HLR-BSP-028.1 | Dropped console bytes are counted and readable by unprivileged code | `test_stdio_full_ring_drops_and_counts`, `test_stdio_recovers_after_drops` | `stdio_write()`, `stdio_get_tx_dropped()` |
| HLR-BSP-028.2 | No line buffering in the retarget layer | `test_ring_keeps_order_across_text_and_binary` (bytes reach the ring at once, no newline needed), `test_stdio_putchar_returns_char_and_keeps_order`, `test_io_putchar_newline`, `test_io_putchar_buffer_full` (the last two only check that `__io_putchar()` returns its argument on a newline and across 64 characters without blocking; they do not read the sink) | `__io_putchar()`, `stdio_write()` |
| HLR-BSP-028.3 | Each `_write()` / `stdio_write()` call is one masked copy; `printf()` callers serialise themselves | `test_ring_keeps_order_across_text_and_binary`, `test_stdio_write_sends_bytes_and_counts_nothing` (order and whole-call copy); atomicity under preemption by review and target integration test (see §5) | `stdio_write()`, `__cdc_tx_write()` |

**Retired in v0.5.0:** BSP-030 (buffer putchar output), BSP-031 (flush on
newline) and BSP-032 (flush on buffer full). The retarget layer no
longer keeps a line buffer; every byte goes straight to the CDC transmit
ring (HLR-BSP-027.1), and bytes that do not fit are dropped and counted
(HLR-BSP-028.1). The three `test_io_putchar_*` tests still run: they now
verify that `__io_putchar` returns its character and does not block at a
newline or after 64 characters, and are traced to HLR-BSP-028 and
HLR-BSP-028.2 above.

### 3.11 BSP - Interrupt Handlers

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| INT-001 | SysTick shall increment tick count | `test_SysTick_Handler`, `test_SysTick_Handler_multiple_ticks` | `SysTick_Handler()` |
| INT-002 | SysTick shall decrement task ticks | `test_SysTick_Handler`, `test_SysTick_Handler_scheduler_disabled` | `SysTick_Handler()` |
| INT-003 | SysTick shall trigger scheduler on timeout | `test_SysTick_Handler_trigger_scheduler` | `SysTick_Handler()` |
| INT-004 | SysTick shall respect os_running flag | `test_SysTick_Handler_os_not_running` | `SysTick_Handler()` |
| INT-005 | USB interrupt shall call HAL handler | `test_OTG_FS_IRQHandler` | `OTG_FS_IRQHandler()` |
| INT-006 | SVC handler shall be callable | `test_SVC_Handler` | `SVC_Handler()` |
| INT-007 | Debug monitor shall be callable | `test_DebugMon_Handler` | `DebugMon_Handler()` |

### 3.12 Kernel - SVC Dispatch

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| SVC-001 | SVC sem_can_feed shall return correct predicate | `test_svc_sem_can_feed_valid`, `test_svc_sem_can_feed_invalid` | `SVC_Handler_C()` |
| SVC-002 | SVC sem_can_consume shall return correct predicate | `test_svc_sem_can_consume_valid`, `test_svc_sem_can_consume_invalid` | `SVC_Handler_C()` |
| SVC-003 | SVC sem_increment shall atomically modify count | `test_svc_sem_increment_valid` | `SVC_Handler_C()` |
| SVC-004 | SVC sem_decrement shall atomically modify count | `test_svc_sem_decrement_valid` | `SVC_Handler_C()` |
| SVC-005 | SVC pipe_can_send shall return correct predicate | `test_svc_pipe_can_send_valid` | `SVC_Handler_C()` |
| SVC-006 | SVC pipe_can_receive shall return correct predicate | `test_svc_pipe_can_receive_valid` | `SVC_Handler_C()` |
| SVC-007 | SVC get_task_name shall return task name | `test_os_get_task_name_svc` | `SVC_Handler_C()` |
| SVC-008 | SVC get_num_tasks shall return created count | `test_os_get_num_created_tasks_svc` | `SVC_Handler_C()` |
| SVC-009 | SVC os_is_running shall return running flag | `test_os_is_running_svc` | `SVC_Handler_C()` |

### 3.13 Kernel - CRC16-CCITT helper *(v0.3.0)*

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| HLR-KRN-092 | Provide CRC16-CCITT (poly 0x1021, init 0xFFFF) | `test_crc16_ccitt_canonical_vector` | `crc16_ccitt()` |
| HLR-KRN-092.1 | Use STM32H7 on-chip CRC peripheral on target | covered by target smoke test (HW path short-circuited under HOST_TEST) | `crc16_ccitt()` |
| HLR-KRN-092.2 | Provide HOST_TEST fallback | `test_crc16_ccitt_canonical_vector`, `test_crc16_ccitt_single_byte_zero`, `test_crc16_ccitt_single_byte_ff`, `test_crc16_ccitt_zero_length`, `test_crc16_ccitt_null_data`, `test_crc16_ccitt_repeatable`, `test_crc16_ccitt_sensitive_to_single_bit`, `test_crc16_ccitt_length_sensitive` | `crc16_ccitt()` |
| HLR-KRN-092.3 *(v0.5.0)* | `crc16_ccitt()` from unprivileged thread mode computes through SVC 89 with buffer validation | `test_crc16_known_vector` (public wrapper and privileged implementation agree); the SVC path and its buffer check are target-only (see §5) | `crc16_ccitt()`, `__crc16_ccitt()` |

### 3.14 Kernel - CDC RX ring buffer *(v0.3.0)*

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| HLR-KRN-090 | Provide SPSC USB CDC receive ring buffer | `test_cdc_rx_init_empty`, `test_cdc_rx_push_and_drain`, `test_cdc_rx_fifo_order` | `cdc_rx_init()`, `cdc_rx_push()`, `cdc_rx_read_byte()` |
| HLR-KRN-090.1 | Non-blocking; overflow drops bytes silently | `test_cdc_rx_fills_to_capacity` | `cdc_rx_push()` |
| HLR-KRN-090.2 | Reads route through SVC gates so ring may live in DTCM_PRIV | `test_cdc_rx_wrap_around`, `test_cdc_rx_push_zero_length`, `test_cdc_rx_read_byte_when_empty` | `cdc_rx_read_byte()`, `cdc_rx_available()` |
| HLR-KRN-090.3 *(v0.5.0)* | Overflow drops are counted and readable through SVC 91 | `test_cdc_rx_counts_dropped_bytes` (in `test_tables_load.c`) | `cdc_rx_dropped()` |

### 3.15 Kernel - Event ring + per-module squelch *(v0.3.0)*

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| HLR-KRN-091 | Provide structured event ring buffer + squelch | `test_event_init_clears_state`, `test_event_emit_one_increments_count`, `test_event_drain_returns_what_was_emitted` | `event_init()`, `os_event()`, `event_drain()` |
| HLR-KRN-091.1 | Fixed 16-byte event entries | `test_event_drain_returns_what_was_emitted`, `test_event_payload_truncated_to_12` | `event_entry_t`, `os_event()` |
| HLR-KRN-091.2 | Transport-agnostic drain | `test_event_drain_partial`, `test_event_drain_empty_ring_returns_false`, `test_event_drain_twice_no_duplicates`, `test_event_drain_after_wrap_in_chunks`, `test_event_drain_emit_drain_interleaved` (v0.5.0: oldest entry after a wrap) | `event_drain()` |
| HLR-KRN-091.3 | O(1) non-blocking emit; ring overflow overwrites oldest | `test_event_squelch_drops_low_severity`, `test_event_invalid_module_id_dropped`, `test_event_drain_full_ring` | `os_event()` |

### 3.16 Kernel - Internal flat-file filesystem *(v0.3.0)*

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| HLR-KRN-093 | Provide create/open/read/write/delete/list/stats | `test_fs_init_clean_state`, `test_fs_create_basic`, `test_fs_open_existing`, `test_fs_open_missing_fails`, `test_fs_write_read_roundtrip`, `test_fs_write_appends`, `test_fs_delete_existing`, `test_fs_delete_missing_fails`, `test_fs_list`, `test_fs_stats_after_writes`, `test_fs_read_offset` | `fs_init/create/open/write/read/delete/list/stats` |
| HLR-KRN-093.1 | At least 16 files × 2 KB = 32 KB capacity | `test_fs_create_full_disk`, `test_fs_write_overflow_rejected`, `test_fs_create_duplicate_fails`, `test_fs_create_invalid_name`, `test_fs_write_invalid_handle` | `fs_create()`, `fs_write()` |
| HLR-KRN-093.2 | Opaque on-disk format | covered by API contract — exercised across the full test set | n/a |

### 3.17 Kernel - Ground-loadable table engine *(v0.3.0)*

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| HLR-KRN-094 | Provide table engine with double-buffered swap | `test_tbl_init_clears_registry`, `test_tbl_register_basic`, `test_tbl_register_null_fails`, `test_tbl_register_duplicate_fails`, `test_tbl_register_invalid_size_fails`, `test_tbl_load_unknown_id_fails`, `test_tbl_load_null_data_fails`, `test_tbl_load_overflow_rejected`, `test_tbl_dump_returns_active`, `test_tbl_dump_unknown_id_fails`, `test_tbl_chunked_load`, `test_tbl_get_descriptor_unknown` | `tbl_init/register/load/dump/get_descriptor/count` |
| HLR-KRN-094.1 | Schema CRC + data CRC16 gated activation | `test_tbl_activate_schema_crc_mismatch`, `test_tbl_activate_round_trip` | `tbl_activate()` |
| HLR-KRN-094.2 | Activate callback runs in thread mode against scratch copy | `test_tbl_activate_round_trip` (verifies the callback observes the staged bytes via the scratch buffer) | `tbl_activate()` |
| HLR-KRN-094.3 | Active buffer immutable until callback OKs | `test_tbl_activate_callback_rejection`, `test_tbl_activate_without_load_fails` | `tbl_activate()` |
| HLR-KRN-094.4 *(v0.5.0)* | Offset-addressed load; identical retransmits accepted (including the first chunk resent while the load is incomplete); gaps, overruns, conflicting retransmits and mixed schema CRCs rejected | `test_retransmitted_chunk_is_idempotent`, `test_conflicting_retransmit_rejected`, `test_gap_rejected`, `test_overrun_rejected_and_not_wedged`, `test_mixed_schema_rejected` | `tbl_load_at()` |
| HLR-KRN-094.5 *(v0.5.0)* | Abort discards staged bytes, active unchanged | `test_abort_discards_partial_load` | `tbl_abort()` |
| HLR-KRN-094.6 *(v0.5.0)* | Descriptor and state copied out to caller memory | `test_get_info_copies_descriptor` | `tbl_get_info()` |
| HLR-KRN-094.7 *(v0.5.0)* | Commit accepted only right after a matching prepare, with the descriptor-size length and the staged data | `test_commit_without_prepare_rejected` (no prepare); the length and data clauses by review (no host test yet, see §5) | `__tbl_activate_commit()` |

### 3.18 Kernel - Background checksum callback delivery *(v0.5.0)*

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| HLR-KRN-096.2 | Mismatch callback runs in the caller's thread mode after the privileged scan, so it may call kernel APIs | `test_cs_mismatch_callback_may_use_kernel_calls`, `test_cs_clean_scan_no_callback`, `test_cs_mismatch_reported_every_scan_until_rebaseline`, `test_cs_scan_without_callback_counts_failures` (all in `test_svc_guard.c`, run under the nested-SVC guard) | `cs_check_all()`, `__cs_check_all()` |

### 3.19 Kernel - SVC caller-buffer validation *(v0.5.0)*

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| HLR-KRN-074 | Every SVC that touches caller memory validates the buffer for its access first | Policy: all `test_policy_*` below. Per-gate enforcement: static check `tools/check_svc_pointer_checks.py` (HLR-KRN-074.8) and target integration test (see §5) | `SVC_Handler_C()`, `svc_user_buffer_ok()` |
| HLR-KRN-074.1 | Writes only to RAM_D1 (less the main-stack window for tasks), the application DTCM half, or the caller's own data-pool slot | `test_policy_ram_d1_read_write_and_straddle`, `test_policy_dtcm_app_half_allowed_kernel_half_rejected`, `test_policy_own_pool_slot_only` | `svc_buffer_allowed()` |
| HLR-KRN-074.2 | Reads also from internal flash and ITCM; address 0 never accepted | `test_policy_flash_readable_not_writable`, `test_policy_itcm_readable_not_writable_not_null` | `svc_buffer_allowed()` |
| HLR-KRN-074.3 | Everything else rejected; wrap and cross-region ranges rejected; zero length accepted | `test_policy_privileged_and_device_space_rejected`, `test_policy_null_and_wrap_rejected_zero_len_allowed`, `test_policy_ram_d1_read_write_and_straddle`, `test_policy_dtcm_app_half_allowed_kernel_half_rejected`, `test_policy_own_pool_slot_only`, `test_policy_main_stack_straddling_ranges` | `svc_buffer_allowed()` |
| HLR-KRN-074.4 | Rejected call returns its failure value, touches no memory | `test_bkpram_rejects_zero_len_and_null`, `test_stdio_write_rejects_null_and_empty`, `test_ring_rejects_null_and_empty`, `test_tbl_load_null_data_fails`; target dispatch path by target integration test (see §5) | `SVC_Handler_C()` |
| HLR-KRN-074.5 | Strings validated byte by byte up to their maximum length | Target integration test and code review (the host build does not execute `svc_user_string_ok()`, see §5) | `svc_user_string_ok()` |
| HLR-KRN-074.6 | Policy is a pure, host-testable function; a missing caller never grants access | All eleven `test_policy_*` tests that call `svc_buffer_allowed()` run the policy with the target memory map on the host (the twelfth, `test_policy_host_caller_is_privileged`, covers `svc_caller_is_privileged()`, HLR-KRN-077); NULL caller: `test_policy_window_size_zero_and_null_caller` | `svc_buffer_allowed()` |
| HLR-KRN-074.7 | Main-stack window rejected (read and write) for task callers; allowed for main-stack (boot) callers | `test_policy_main_stack_rejected_for_task_caller`, `test_policy_main_stack_allowed_for_main_stack_caller`, `test_policy_main_stack_straddling_ranges`, `test_policy_window_size_zero_and_null_caller`, `test_policy_ram_d1_read_write_and_straddle`; caller classification (frame on PSP) by target integration test, and the assumption that the main stack stays within `_Min_Stack_Size` by review (see §5) | `svc_buffer_allowed()`, `svc_current_caller()` |
| HLR-KRN-074.8 | Static check that every dispatch case validates caller pointers | Static check `tools/check_svc_pointer_checks.py`, run by `make -C tests` target `check-svc-ptr` (1 dispatch, 88 cases, 37 pointer uses validated, 2 function pointers allowlisted) | `SVC_Handler_C()` |

### 3.20 Kernel - SVC wrapper rules *(v0.5.0)*

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| HLR-KRN-075 | Every SVC inline-asm block clobbers `"memory"` | Static check `tools/check_svc_clobbers.py` (run by `make -C tests` before the Unity runner) | all SVC wrappers (`svc.c`, `fs.c`, `sb.c`, `crc.c`, `cdc.c`, `bootloader.c`) |
| HLR-KRN-075.1 | The clobber check fails the host test run | `make -C tests` target `check-svc-asm` | `tools/check_svc_clobbers.py` |
| HLR-KRN-076 | Host builds detect nested SVCs | `test_guard_detects_nested_gate`, `test_guard_allows_sequential_gates`, `test_guard_depth_unwinds_after_return`; every other host test runs with the default abort-on-nesting handler | `svc_host_gate_enter()`, `svc_host_gate_exit()`, `SVC_HOST_GATE` |
| HLR-KRN-077 | Handler-mode and privileged callers bypass the SVC | `test_policy_host_caller_is_privileged` (host); handler-mode behaviour by target integration test (see §5) | `svc_caller_is_privileged()`, `cdc_tx_write()`, `crc16_ccitt()` |

### 3.21 Kernel - Backup SRAM *(v0.5.0)*

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| HLR-KRN-078 | SVC-gated copy-in and copy-out of the 4 KB backup SRAM | `test_bkpram_round_trip` | `bkpram_write()`, `bkpram_read()` |
| HLR-KRN-078.1 | Zero length, out-of-range, overflowing and disallowed buffers rejected | `test_bkpram_last_byte_ok_one_past_rejected`, `test_bkpram_rejects_zero_len_and_null`, `test_bkpram_rejects_offset_wrap` | `bkpram_range_ok()`, `SVC_Handler_C()` |
| HLR-KRN-078.2 | Backup SRAM in its own non-cacheable, privileged MPU region; data survives reset | `test_bkpram_survives_until_cleared` (host store survives simulated resets); MPU attributes and retention across a real reset by target integration test (see §5) | `MPU_Config()` (region 8) |

### 3.22 BSP - USB CDC transmit ring *(v0.5.0)*

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| HLR-BSP-027 | `CDC_Write` / `CDC_WriteString` are all-or-nothing and non-blocking; false when the ring is full | `test_cdc_write_basic`, `test_cdc_write_zero_len_is_noop_success`, `test_cdc_write_null_data_with_nonzero_len_fails`, `test_cdc_write_failure_propagates`, `test_cdc_write_string_basic`, `test_cdc_write_string_null_is_success_noop`, `test_cdc_write_string_empty_is_success`, `test_ring_whole_is_all_or_nothing_partial_takes_what_fits` | `CDC_Write()`, `CDC_WriteString()` |
| HLR-BSP-027.1 | One transmit ring for all CDC output; order kept across producers | `test_ring_keeps_order_across_text_and_binary`, `test_stdio_putchar_returns_char_and_keeps_order` | `__cdc_tx_write()` |
| HLR-BSP-027.2 | All-or-nothing and partial modes; bytes refused for lack of space (and discarded on close/reopen) counted; SVC 93 for tasks | `test_ring_whole_is_all_or_nothing_partial_takes_what_fits`, `test_ring_rejects_null_and_empty`, `test_stdio_full_ring_drops_and_counts`; SVC path by target integration test (see §5) | `cdc_tx_write()`, `__cdc_tx_write()` |
| HLR-BSP-027.3 | Queueing never blocks, sleeps or spins | `test_stdio_busy_endpoint_does_not_wait`, `test_stdio_not_configured_queues_without_retry`, `test_ring_foreign_busy_endpoint_leaves_data_queued` | `__cdc_tx_write()` |
| HLR-BSP-027.4 | Atomic idle-check-and-start; chunk cap; bytes held until transfer complete | `test_ring_chunks_are_capped_and_wrap_around`, `test_ring_completion_advances_tail`; interrupt masking by target integration test (see §5) | `tx_start_locked()`, `__cdc_tx_on_complete()` |
| HLR-BSP-027.5 | USB reset or re-configuration clears in-flight state; bytes resent unless the port was open, in which case the link loss is handled as a close | `test_ring_link_reset_clears_wedged_transfer`; `test_ring_first_open_keeps_output_from_before_it` (link loss while open, reported as a close) | `__cdc_tx_on_link_reset()`, `__cdc_tx_on_dtr()`, `CDC_DeInit_FS()` |
| HLR-BSP-027.6 | Output queued while unconfigured or suspended is sent when the host opens the port or the bus resumes | `test_ring_not_configured_then_configured_retries`, `test_ring_kick_sends_output_queued_before_host_attached`, `test_stdio_not_configured_queues_without_retry`; the resume hook (PHY clock restart, `USBD_LL_Resume()`, kick) is host-tested only through the ring functions it calls and is not exercised on hardware (see §5) | `__cdc_tx_kick()`, `__cdc_tx_write()`, `HAL_PCD_ResumeCallback()` |
| HLR-BSP-027.7 | On close (DTR 1 → 0, or link loss) and on reopen after an earlier open, the bytes queued behind the transfer in flight are discarded and counted; the first open since boot keeps them; a report that does not change DTR discards nothing; the in-flight transfer (at most `CDC_TX_MAX_CHUNK` bytes) is never discarded and may arrive after the reopen | `test_ring_port_close_discards_queued_keeps_in_flight`, `test_ring_port_close_rewinds_wrapped_head`, `test_ring_port_close_idle_and_empty`, `test_ring_reopen_drops_output_written_while_closed`, `test_ring_first_open_keeps_output_from_before_it`. Close and reopen are exercised on hardware (the device keeps working across a 30 s close and reopen); the discard behaviour is host-tested only (see §5) | `__cdc_tx_on_dtr()`, `__cdc_tx_discard_queued()`, `CDC_Control_FS()`, `CDC_DeInit_FS()` |

### 3.23 BSP - Watchdog and K1 button *(v0.4.0, K1 level revised v0.5.0)*

| Req ID | Requirement | Test Case(s) | Source Function |
|--------|-------------|--------------|-----------------|
| HLR-BSP-025 | IWDG init, refresh, reset-reason query, flag clear | `test_iwdg_init_starts_module`, `test_iwdg_refresh_counts`, `test_iwdg_refresh_before_init_is_noop`, `test_iwdg_was_reset_default_false`, `test_iwdg_was_reset_after_inject`, `test_iwdg_clear_reset_flag`, `test_iwdg_init_clamps_oversized_args` | `IWDG_Init()`, `IWDG_Refresh()`, `IWDG_WasReset()`, `IWDG_ClearResetFlag()` |
| HLR-BSP-026 | K1 reads pressed while the pin is at `BSP_KEY_PRESSED_LEVEL` | `test_button_default_released`, `test_button_press_release`, `test_button_repeated_reads_stable` | `Button_IsPressed()` |

## 4. Test Case Summary

Run `cd tests && make test` to obtain current pass/fail results.

| Category | Test Cases |
|----------|------------|
| Kernel - Task Management | 18 |
| Kernel - Scheduling | 7 |
| Kernel - Critical Sections | 3 |
| Kernel - Semaphores | 16 |
| Kernel - Message Pipes | 19 |
| Kernel - Print Buffer | 5 |
| Kernel - SVC Dispatch | 11 |
| Kernel - CRC16-CCITT helper *(v0.3.0)* | 8 |
| Kernel - CDC RX ring buffer *(v0.3.0)* | 7 |
| Kernel - Event ring + squelch *(v0.3.0, 3 added v0.5.0)* | 12 |
| Kernel - Internal filesystem *(v0.3.0)* | 16 |
| Kernel - Ground-loadable table engine *(v0.3.0)* | 16 |
| Kernel - Table load/abort/info, CRC SVC, CDC RX drop counter *(v0.5.0)* | 10 |
| Kernel - SVC caller-buffer policy *(v0.5.0)* | 12 |
| Kernel - Nested-SVC guard + checksum callbacks *(v0.5.0)* | 7 |
| Kernel - Backup SRAM *(v0.5.0)* | 5 |
| BSP - Display | 21 |
| BSP - LED | 4 |
| BSP - Platform I/O | 7 |
| BSP - Standard I/O (`test_io_putchar_*` + `test_stdio.c`) | 10 |
| BSP - USB CDC write + transmit ring *(v0.5.0)* | 21 |
| BSP - IWDG + K1 button *(v0.4.0)* | 10 |
| BSP - Interrupts | 8 |
| **Total (Unity runner, v0.5.0)** | **274** |

> **Note:** the category rows for `test_task.c` are approximate and do not
> sum to the total; the canonical test count comes from the Unity runner
> output at test execution time. Exact per-file counts for v0.5.0:

| Test file | Tests |
|-----------|------:|
| `test_task.c` | 143 |
| `test_crc.c` | 8 |
| `test_cdc_rx.c` | 7 |
| `test_event.c` | 12 |
| `test_fs.c` | 16 |
| `test_tables.c` | 16 |
| `test_tables_load.c` | 10 |
| `test_iwdg.c` | 7 |
| `test_button.c` | 3 |
| `test_cdc.c` | 21 |
| `test_stdio.c` | 7 |
| `test_svc_guard.c` | 7 |
| `test_bkpram.c` | 5 |
| `test_svc_policy.c` | 12 |
| **Total** | **274** |

**Static checks** (run by `make -C tests` before the Unity runner, or as
noted):

| Check | Verifies | Status |
|-------|----------|--------|
| `tools/check_svc_clobbers.py` | HLR-KRN-075, HLR-KRN-075.1 | Runs with every `make -C tests` (target `check-svc-asm`) |
| `tools/check_svc_pointer_checks.py` | HLR-KRN-074, HLR-KRN-074.8 | Runs with every `make -C tests` (target `check-svc-ptr`) |

## 5. Untested Requirements

The following requirements require target integration testing:

| Req ID | Requirement | Reason |
|--------|-------------|--------|
| KRN-050 | Context switch shall save/restore registers | ARM assembly code |
| KRN-051 | Idle task shall run when no tasks ready | Infinite loop task |
| KRN-052 | Heartbeat task shall blink LED | Infinite loop task |
| KRN-053 | *Retired in v0.5.0:* printf task shall transmit buffered data | The printf task and print buffer were removed; console output goes through the CDC transmit ring (HLR-BSP-027, HLR-BSP-028) |
| INT-010 | Fault handlers shall halt system | Dead code by design |
| HLR-KRN-074, HLR-KRN-074.4, HLR-KRN-074.5 | Per-gate caller-buffer checks in the SVC dispatcher | `SVC_Handler_C()` and `svc_user_string_ok()` are target-only; the host build of `svc_user_buffer_ok()` only rejects NULL and wrap-around because host addresses are not target addresses. The policy itself is host-tested (§3.19) |
| HLR-KRN-077 | Handler-mode callers skip the SVC | `svc_caller_is_privileged()` reads IPSR/CONTROL on target only; always true on the host |
| HLR-KRN-078.2 | Backup SRAM MPU attributes and retention across a real reset | MPU configuration and reset are target-only |
| HLR-KRN-092.3 | CRC SVC path and its buffer check | The SVC asm path runs on target only |
| HLR-BSP-027.2, HLR-BSP-027.4, HLR-BSP-028.3 | SVC 93 path; interrupt masking around the idle-check-and-start and around each producer's copy | SVC asm and PRIMASK are target-only; the host runs the same ring logic unmasked |
| HLR-BSP-027.5, HLR-BSP-027.6, HLR-BSP-027.7 | Bus-resume hook (PHY clock restart, `USBD_LL_Resume()`, kick); DTR reports in `CDC_Control_FS()` and link loss in `CDC_DeInit_FS()` | USB device-stack callbacks, not built on the host; the ring functions they call are host-tested (§3.22). Bus resume: host-tested only; not exercised on hardware (the hardware tests do not suspend the bus). Close and reopen: exercised on hardware (the device keeps working across a 30 s close and reopen); discard behaviour host-tested only |
| HLR-KRN-074.7 | Task vs main-stack caller classification; main stack within `_Min_Stack_Size` | `svc_current_caller()` compares the SVC frame address with PSP and reads `_estack` / `_Min_Stack_Size` on target only; the policy is host-tested with both caller kinds (§3.19). That everything on the main stack (never-unwound `main()` / `os_start()` frames, SVC handler, nested interrupts) fits in `_Min_Stack_Size`, so the window covers the handler's frame, is checked by review only; MSP is not yet reset at first-task launch (SDD §4.2.8) |
| HLR-KRN-094.7 | Commit rejected when its length differs from the descriptor size or its data differs from what was staged | No host test yet (the host test covers commit without prepare); `__tbl_activate_commit()` checks both by `len != desc.size` and `memcmp()`, verified by review |
| HLR-BSP-029 | ROM bootloader entry via SVC 90 | Resets the chip and jumps to ROM; target only (the host build only records the request) |

## 6. Approval

| Role | Name | Signature | Date |
|------|------|-----------|------|
| Author | | | |
| Reviewer | | | |
| QA | | | |

## 7. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2025-01-26 | Souham Biswas | Initial draft |
| 0.2 | 2026-04-01 | Souham Biswas | Added SVC tests; updated counts to ~140; reconciled semaphore/pipe test names |
| 0.3 | 2026-04-11 | Souham Biswas | Added 56 host tests for the v0.3.0 shared modules across `test_crc.c` (8), `test_cdc_rx.c` (7), `test_event.c` (9), `test_fs.c` (16), and `test_tables.c` (16); total bumped 140 → 196 |
| 0.4 | 2026-09-27 | Souham Biswas | v0.5.0: BSP-030..032 (putchar line buffering) retired and replaced by HLR-BSP-028; added §3.18–§3.23 (checksum callback delivery, SVC caller-buffer validation, SVC wrapper rules, backup SRAM, USB CDC transmit ring, watchdog and K1 button); new rows for HLR-KRN-090.3, 092.3, 094.4–094.7; static checks traced; target-only paths added to §5; per-file test counts (274) |
