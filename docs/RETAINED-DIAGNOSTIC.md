# Generic retained diagnostic (source candidate)

This core change reserves backup-SRAM offsets `[64,192)` only. The OBC persist
v2 block remains `[0,60)`; `[60,64)` remains an alignment gap. No `.bkpsram`
object, SRAM4 DFU marker, RTC backup register, watchdog setting, or caller
model type is involved. The initial G1 image remains a separate baseline.

`bsp/retained_diag.h` is the shared ABI. Phase A `[64,96)` and phase B
`[96,128)` are 32-byte records. Fault `[128,192)` is a separate 64-byte
first-terminal-fault record. All fields are 32-bit little-endian words. Each
record has a format/version prefix, IEEE reflected CRC32 over all preceding
words, and a final commit word `0xD1A60C01`. The task-mode phase writer uses
the existing range-checked `bkpram_write/read` SVC gates, invalidates only
the destination's commit word, writes payload/checksum, then commits last.
The previous phase slot survives an interrupted update. The privileged
terminal writer never calls SVC and never writes a phase slot, so a resumed
task cannot overwrite fault evidence. The fault slot starts on a 32-byte
cache-line boundary and occupies exactly two lines. Existing MPU policy has
HFNMIENA clear, so HardFault can bypass the ordinary noncacheable mapping and
use the default write-back/write-allocate attributes while D-cache is on.
The writer cleans the last line after commit invalidation, both lines after
the body, and the last line again after final commit. Each vendored CMSIS
clean has DSB/ISB barriers. It touches no adjacent phase or persist line.
This is required for IWDG retention, but physical readback is still needed
to establish actual reset behavior. See the [Arm Cortex-M7 MPU/default-memory
description](https://documentation-service.arm.com/static/61efd8352dd99944d05142d1)
and [CMSIS D-cache clean contract](https://arm-software.github.io/CMSIS_6/latest/Core/group__Dcache__functions__m7.html).

`retained_diag_read()` returns all 128 raw bytes even if records are empty or
corrupt. Its parsed fields are advisory: `phase_valid`, `fault_valid`, and
`fault_bound` are independent. Binding requires the fault's captured phase
sequence, boot, attempt, and phase to match an exact valid phase slot; only
then does `fault_source_tag` come from that phase record. A short source tag
is context, not image attestation. The host must bind the full compiled
digest and flash/capture manifest independently.

`retained_diag_arm_after_ack()` rereads and compares **all 128 raw bytes** to
the caller's previously displayed/durably collected snapshot before changing
anything. This includes the empty/corrupt case: initialization is explicit,
never automatic at boot. It clears the prior fault commit and writes a new
phase-zero arm record, then publishes a volatile RAM_D1 armed context last.
The application must supply the same boot, attempt, and source tag to
`retained_diag_mark()`. Core serializes task-mode arm/mark with a nonblocking
RAM_D1 atomic guard; the terminal writer never acquires it. The caller still
owns show/ack ordering. The optional `sequence_out` is set only after
successful arm commit. A reset clears the volatile context, so a fault before
or during arm is recorded raw/unbound if the fault slot is available. A fault
after arm uses only a committed phase matching that context. The caller owns
phase meanings and durable readout/ack ordering.

HardFault, BusFault, UsageFault and the **terminal** MemManage path use naked
entries that capture raw MSP/PSP/EXC_RETURN before C prologue, claim a nested
trap guard with `LDREX/STREX`, switch MSP to a separate aligned 1 KiB emergency stack, and call
the fixed privileged writer. No SVC, HAL, FP, allocator, lock, task switch,
watchdog feed, or watchdog reset is in that path. If backup-SRAM access has
not been established by a successful read, it halts without touching the
retained region. The pre-existing recoverable MemManage DACCVIOL/PSP
instruction skip and `g_memmanage_fault_count` behavior return through the
original EXC_RETURN. The 31st recoverable-class fault escalates before
clearing CFSR or modifying stacked PC, retaining its original status and
faulting instruction for terminal capture.
Terminal handlers spin until the existing IWDG resets the board. Their old
LED blink patterns are intentionally absent because the fault writer cannot
safely call HAL.

Raw register and SCB status fields are saved regardless of optional PC
validity. PC/LR are read only for recognized EXC_RETURN, no stacking,
unstacking or lazy-FP status, selected SP within the known current task or
main-stack window, full basic/extended frame bounds, and required alignment
padding. Arm DDI 0403E.e Figure B1-4 (printed B1-538) places stacked PC at
exception SP `+0x18` in both basic and FP-extended frames; total reservation
differs. A pure range helper supports host boundary controls, but target
disassembly and an induced known-fault control are still required before a
physical revision. A pure IWDG reset has no exception frame and yields only
the last committed phase.

The earlier eight-file candidate passed focused host 8/8 and whole host
285/285, then a strict WERROR whole-core ARM compile/link. Its target object
showed raw MSP/PSP/LR capture before the stack switch, and `.su` reported a
240-byte terminal C frame. Review of that object also found a compiler
`memset` call in the terminal path and missing HardFault cache maintenance;
the current correction replaces that initialization with fixed volatile
byte stores and adds ordered line cleans. The previous results do **not**
validate the corrected bytes. The original first focused link failure
(Unity setup/teardown) and first strict compile failure (bad function cast)
remain retained alongside the passing second runs.

The corrected eight-file source binding `812b1082...` passed focused host
Unity 9/9 and a strict WERROR whole-core ARM link. Its object disassembly
shows the threshold call and terminal branch before the recovery PC/CFSR
stores. The writer has no library/FP/SVC/HAL call; its commit-clear, body,
and final-commit stages each precede the corresponding inline DCCMVAC
clean and DSB/ISB barriers. The measured terminal C frame is 248 bytes;
its largest direct callee frame is 16 bytes, for a 264-byte software chain.
The naked entry, NMI and nested HardFault veneers have zero compiler stack
frame. In the selected core baseline, configurable terminal faults and the
enabled SysTick/OTG IRQs are priority zero, so ordinary equal-priority IRQs
cannot preempt this writer. Up to two higher-priority nested exception frames
(HardFault then NMI), conservatively 104 bytes plus 4-byte alignment each,
put this bounded emergency-MSP use at 480 bytes of the 1 KiB stack. The
interrupted original PSP/MSP frame and the 24-byte recoverable MemManage
helper frame are separate from that emergency stack. This bound depends on
the selected priority policy and must be revisited if it changes.

The same ARM `.su` reports read 16, decode 176, arm 312, mark 296, BKPRAM
wrapper 4, and SVC handler 80 bytes. Those are separate task/MSP components,
not a whole OBC task-stack bound; final OBC caller frames and SVC exception
stacking still need to be joined on the composed image. On-target readback of
an intentionally induced known fault needs a separate root physical grant.
No host or compile result alone proves retention across reset or a valid
captured PC. The correction control receipt is under
`coordination/native-grader2-takeover/retained-diagnostic-core-correction-controls-v1/retained/`.
