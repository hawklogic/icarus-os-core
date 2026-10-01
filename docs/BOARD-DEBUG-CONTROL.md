# Bounded board feedback control

Source owner: `/root/board_debug_suite`, via native-integration-astra. Root granted
the four existing BSP/SVC files and focused tests in the 2026-10-01 11:20:11 UTC
coordination exchange. Isolated branch `feature/board-debug-control-v1` starts at
`9020c73`; do not change the parent OBC gitlink until sequential integration.
Current publication identity is corporate author and committer
`souhamb-hawk <souhamb@hawklogicsystems.com>`, as root subsequently required.
The old `73118a6` head is retained at local branch
`backup/board-debug-control-pre-corporate-73118a6`. Metadata-only replacement
`8afd9340bbee6f1d133b3ae9adeea456189f39fa` has identical content and was published
with an exact lease against `73118a64879c1515bbe96ef3179da4f32563f545`.

The single command definition is `BOARD_FEEDBACK_COMMANDS(X)` in
`Core/Inc/bsp/board_feedback.h`. It generates the C opcode enum and supplies the
host catalogue. Opcodes, names, bounds and help must not be maintained in another
table. `board_feedback_control(op, value)` crosses SVC 96 with scalar arguments
and returns the packed state documented in that header. Invalid arguments have
no side effect. Low-byte `PENDING` describes an incomplete transition; it never
claims a visible effect or a completed wake. Upper reserved bits remain zero.

Boot initializes display GRAM with panel scanning off and PWM off. Cell updates
while awake and scan-off do not turn on illumination. Runtime sleep rejects cell
writes until awake, preserving the caller's dirty state. No runtime callback uses
`HAL_Delay`; sleep-in and sleep-out barriers each require 120 OS ticks. Status or
the existing pulse-zero service call advances at most the bounded pending work.
The STM32 deployment uses a 1 ms tick. No new timer, task or framebuffer exists.
OFF cancels pending illumination; zero backlight remains zero when wake finishes.
An SLEEP request during wake waits for the wake barrier then enters sleep.

OBC must skip visual cell reconciliation while not ready, asleep or wake pending,
and must not mark skipped cells as shown. This implementation does not reset the
controller on wake, so valid GRAM persists; the OBC may invalidate its small shown
character cache after sleep to force a repaint without a pixel framebuffer.
I/O failure sets the failure state and disables the backlight. Software state and
requested LED pulses do not attest physical light, pin voltage or panel behavior.

Parent-allocated validation passed on Icarus DATA (2026-10-01):

```sh
cc -std=c11 -Wall -Wextra -Werror -DHOST_TEST -ICore/Inc -Itests/mocks Core/Src/bsp/board_feedback.c tests/board_debug_control_test.c -o /DATA/OWNED/board-debug-control-test
/DATA/OWNED/board-debug-control-test
```

The strict compile and executable passed in 0.071 seconds. Source binding:
`035383d3fc8f29d4d3a7458dbef0cabc0905073a98a661447905411a71263481`;
success receipt `4384f42ce3c4d255d248d65c9717f6b11bd9ed69f3b1678cc70cbc17747eb495`.
Independent exit receipt
`43933a9f70b4b50a8ed81246441738def3607a9a6aa0bdf7125db82fe19bd322`
confirms source PID 1258556/start 147325432 and observer PID 1258552/start
147325427 gone, cgroups absent and units inactive with success 0; 0.375 GiB returned.
Receipts: coordination `native-grader2-takeover/board-debug-core-controls-v1/retained`.

This executable uses an inert tick/LED implementation; it does not test the ARM
SVC instruction or physical SPI/PWM. Parent must separately compile the exact
ARM integration on Icarus once OBC and core pins agree. SVC source/header were
byte-pinned only in this job. This worker ran no local test/build or actual board
operation. Root alone reviews, merges and controls the physical board.

Root review 5378796420 found a P2 at the first service call after a wake deadline:
OFF/SLEEP/BACKLIGHT(0) could complete pending illumination before canceling it.
The author delta applies darkness intent before display servicing and adds an
ordered HOST_TEST panel/PWM trace for all three operations, including LED expiry.
The parent strict HOST_TEST compile/executable passed in 0.0712 seconds. Binding
`21bda55f6e2cac20cbe8a2691c2007704f8bfa0ba08dbdbdc83684469539f218`, success
`7bad40cb16e991a2e35afb0c07a4ea6fd61c423d6b991aca84f5aec5732f0956`, independent
exit `5c3b949e5807b03ae9cc2466249015f38b4b9ae9f86cbfeea28d33af6ffe8823`.
Source PID 1267992/start 147469170 and observer 1267988/start 147469165 were
confirmed absent, cgroups gone and units inactive/success 0; 0.375 GiB returned.
Retained evidence: coordination `board-debug-core-cancel-controls-v1/retained`.
Previous final-state-only controls did not cover the transient boundary. No
second independent reviewer or physical board test was started.
