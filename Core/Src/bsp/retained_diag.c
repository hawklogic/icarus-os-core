/** @file retained_diag.c @brief Fixed backup-SRAM phase and terminal-fault record. */
#include "bsp/retained_diag.h"
#include "icarus/kernel.h"

#include <stddef.h>
#include <string.h>

#ifndef HOST_TEST
#include "bsp/mpu.h"
#endif

_Static_assert(sizeof(retained_diag_phase_t) == RETAINED_DIAG_PHASE_BYTES,
               "phase slot must be 32 bytes");
_Static_assert(sizeof(retained_diag_fault_t) == RETAINED_DIAG_FAULT_BYTES,
               "fault slot must be 64 bytes");
_Static_assert(RETAINED_DIAG_PHASE_A >= 60u &&
               RETAINED_DIAG_PHASE_B == RETAINED_DIAG_PHASE_A + 32u &&
               RETAINED_DIAG_FAULT == RETAINED_DIAG_PHASE_B + 32u &&
               RETAINED_DIAG_FAULT + 64u == RETAINED_DIAG_OFFSET + RETAINED_DIAG_BYTES &&
               (RETAINED_DIAG_FAULT % 32u) == 0u,
               "diagnostic slots must stay in the agreed [64,192) reservation");

#define DIAG_BAD_FRAME_CFSR ( (1u << 3) | (1u << 4) | (1u << 5) | \
                              (1u << 11) | (1u << 12) | (1u << 13) )
#define DIAG_MMFAR_VALID (1u << 7)
#define DIAG_BFAR_VALID  (1u << 15)

/* RAM_D1 .bss is accessible to both thread code and the privileged trap.
 * Commit is zero after C startup, cleared before arm mutation, and set last.
 * This is a diagnostic consistency guard, not memory-corruption isolation. */
static volatile struct {
    uint32_t boot;
    uint32_t attempt;
    uint32_t source_tag;
    uint32_t commit;
} armed_context;
static volatile uint32_t storage_ready;
static volatile uint32_t storage_ready_inverse;
/* Serializes only task-mode arm/mark; terminal writer never takes this lock. */
static uint32_t thread_writer_busy;

static bool claim_thread_writer(void)
{
    return __atomic_exchange_n(&thread_writer_busy, 1u, __ATOMIC_ACQUIRE) == 0u;
}

static void release_thread_writer(void)
{
    __atomic_store_n(&thread_writer_busy, 0u, __ATOMIC_RELEASE);
}

static void diag_barrier(void)
{
#ifndef HOST_TEST
    __DMB();
#else
    __asm__ volatile ("" ::: "memory");
#endif
}

/* IEEE reflected CRC32, byte order exactly as retained in backup SRAM. */
uint32_t retained_diag_checksum(const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xffffffffu;
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= p[i];
        for (uint32_t bit = 0; bit < 8u; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
        }
    }
    return ~crc;
}

bool retained_diag_memmanage_should_terminate(uint32_t recovered_so_far)
{
    return recovered_so_far >= 30u;
}

static bool phase_valid(const retained_diag_phase_t *p)
{
    return p->commit == RETAINED_DIAG_COMMIT &&
           p->format == RETAINED_DIAG_FORMAT &&
           p->sequence != 0u &&
           p->checksum == retained_diag_checksum(p, offsetof(retained_diag_phase_t, checksum));
}

static bool fault_valid(const retained_diag_fault_t *f)
{
    uint32_t kind = (f->format_kind_flags >> 8) & 0xffu;
    uint32_t flags = f->format_kind_flags & 0xffu;
    return f->commit == RETAINED_DIAG_COMMIT &&
           (f->format_kind_flags & 0xffff0000u) == RETAINED_DIAG_FORMAT &&
           kind >= RETAINED_DIAG_KIND_HARD && kind <= RETAINED_DIAG_KIND_USAGE &&
           (flags & ~(RETAINED_DIAG_FLAG_MMFAR | RETAINED_DIAG_FLAG_BFAR |
                      RETAINED_DIAG_FLAG_PC)) == 0u &&
           f->checksum == retained_diag_checksum(f, offsetof(retained_diag_fault_t, checksum));
}

static bool matching_phase(const retained_diag_fault_t *f,
                           const retained_diag_phase_t *p)
{
    return phase_valid(p) && f->phase_sequence != 0u &&
           p->sequence == f->phase_sequence && p->boot == f->boot &&
           p->attempt == f->attempt && p->phase == f->phase;
}

static bool same_phase(const retained_diag_phase_t *a,
                       const retained_diag_phase_t *b)
{
    const uint32_t *aw = (const uint32_t *)a;
    const uint32_t *bw = (const uint32_t *)b;
    for (uint32_t i = 0u; i < 8u; ++i) {
        if (aw[i] != bw[i]) { return false; }
    }
    return true;
}

static void decode_snapshot(retained_diag_snapshot_t *out)
{
    retained_diag_phase_t a, b;
    retained_diag_fault_t f;
    (void)memcpy(&a, &out->raw[0], sizeof(a));
    (void)memcpy(&b, &out->raw[32], sizeof(b));
    (void)memcpy(&f, &out->raw[64], sizeof(f));
    bool av = phase_valid(&a);
    bool bv = phase_valid(&b);
    bool fv = fault_valid(&f);
    bool tie = av && bv && a.sequence == b.sequence && !same_phase(&a, &b);
    out->phase_valid = (av || bv) && !tie;
    out->fault_valid = fv;
    if (out->phase_valid) {
        out->phase = (!bv || (av && a.sequence > b.sequence)) ? a : b;
    } else {
        (void)memset(&out->phase, 0, sizeof(out->phase));
    }
    out->fault = fv ? f : (retained_diag_fault_t){0};
    out->fault_bound = fv && !tie &&
                       (matching_phase(&f, &a) || matching_phase(&f, &b));
    out->fault_source_tag = 0u;
    if (out->fault_bound) {
        out->fault_source_tag = matching_phase(&f, &a) ? a.source_tag : b.source_tag;
    }
    if (out->phase_valid || fv) {
        out->status = RETAINED_DIAG_VALID;
    } else {
        uint32_t i = 0u;
        while (i < RETAINED_DIAG_BYTES && out->raw[i] == 0u) { ++i; }
        out->status = (i == RETAINED_DIAG_BYTES) ? RETAINED_DIAG_EMPTY
                                                  : RETAINED_DIAG_CORRUPT;
    }
}

bool retained_diag_read(retained_diag_snapshot_t *out)
{
    if (out == NULL) { return false; }
    (void)memset(out, 0, sizeof(*out));
    if (!bkpram_read(out->raw, RETAINED_DIAG_OFFSET, RETAINED_DIAG_BYTES)) {
        return false;
    }
    storage_ready_inverse = ~RETAINED_DIAG_COMMIT;
    diag_barrier();
    storage_ready = RETAINED_DIAG_COMMIT;
    diag_barrier();
    decode_snapshot(out);
    return true;
}

#ifdef HOST_TEST
static uint32_t host_writes_remaining = UINT32_MAX;
void retained_diag_host_fail_after_writes(uint32_t count)
{
    host_writes_remaining = count;
}
#endif

static bool diag_write(const void *src, uint32_t offset, uint32_t len)
{
#ifdef HOST_TEST
    if (host_writes_remaining == 0u) { return false; }
    if (host_writes_remaining != UINT32_MAX) { --host_writes_remaining; }
#endif
    return bkpram_write(src, offset, len);
}

static bool write_phase(uint32_t offset, const retained_diag_phase_t *p)
{
    const uint32_t invalid = 0u;
    const uint32_t commit = RETAINED_DIAG_COMMIT;
    return diag_write(&invalid, offset + 28u, 4u) &&
           diag_write(p, offset, 28u) &&
           diag_write(&commit, offset + 28u, 4u);
}

static uint32_t next_phase_offset(const retained_diag_snapshot_t *s)
{
    retained_diag_phase_t a, b;
    (void)memcpy(&a, &s->raw[0], sizeof(a));
    (void)memcpy(&b, &s->raw[32], sizeof(b));
    bool av = phase_valid(&a), bv = phase_valid(&b);
    if (!av) { return RETAINED_DIAG_PHASE_A; }
    if (!bv) { return RETAINED_DIAG_PHASE_B; }
    return (a.sequence <= b.sequence) ? RETAINED_DIAG_PHASE_A
                                      : RETAINED_DIAG_PHASE_B;
}

static uint32_t next_sequence(const retained_diag_snapshot_t *s)
{
    retained_diag_phase_t a, b;
    (void)memcpy(&a, &s->raw[0], sizeof(a));
    (void)memcpy(&b, &s->raw[32], sizeof(b));
    uint32_t highest = 0u;
    if (phase_valid(&a)) { highest = a.sequence; }
    if (phase_valid(&b) && b.sequence > highest) { highest = b.sequence; }
    if (s->fault_valid && s->fault.phase_sequence > highest) {
        highest = s->fault.phase_sequence;
    }
    return highest == UINT32_MAX ? 0u : highest + 1u;
}

bool retained_diag_arm_after_ack(const retained_diag_snapshot_t *shown,
                                 uint32_t boot, uint32_t attempt,
                                 uint32_t source_tag, uint32_t *sequence_out)
{
    retained_diag_snapshot_t fresh;
    if (shown == NULL || !claim_thread_writer()) { return false; }
    if (!retained_diag_read(&fresh) ||
        memcmp(shown->raw, fresh.raw, RETAINED_DIAG_BYTES) != 0) {
        release_thread_writer();
        return false;
    }
    uint32_t seq = next_sequence(&fresh);
    if (seq == 0u) {
        release_thread_writer();
        return false;
    }
    uint32_t offset = next_phase_offset(&fresh);
    retained_diag_phase_t seed = {
        .format = RETAINED_DIAG_FORMAT, .sequence = seq,
        .boot = boot, .attempt = attempt,
        .phase = RETAINED_DIAG_PHASE_ARMED, .source_tag = source_tag,
        .checksum = 0u, .commit = RETAINED_DIAG_COMMIT
    };
    seed.checksum = retained_diag_checksum(&seed, offsetof(retained_diag_phase_t, checksum));

    /* After this point a trap must never inherit the previous armed context. */
    armed_context.commit = 0u;
    diag_barrier();
    const uint32_t invalid = 0u;
    if (!diag_write(&invalid, RETAINED_DIAG_FAULT + 60u, 4u) ||
        !write_phase(offset, &seed)) {
        release_thread_writer();
        return false;
    }
    armed_context.boot = boot;
    armed_context.attempt = attempt;
    armed_context.source_tag = source_tag;
    diag_barrier();
    armed_context.commit = RETAINED_DIAG_COMMIT;
    diag_barrier();
    if (sequence_out != NULL) { *sequence_out = seq; }
    release_thread_writer();
    return true;
}

bool retained_diag_mark(uint32_t boot, uint32_t attempt,
                        uint32_t source_tag, uint32_t phase)
{
    if (phase == RETAINED_DIAG_PHASE_ARMED ||
        armed_context.commit != RETAINED_DIAG_COMMIT ||
        armed_context.boot != boot || armed_context.attempt != attempt ||
        armed_context.source_tag != source_tag || !claim_thread_writer()) {
        return false;
    }
    retained_diag_snapshot_t s;
    if (!retained_diag_read(&s) || !s.phase_valid || s.fault_valid ||
        s.phase.boot != boot || s.phase.attempt != attempt ||
        s.phase.source_tag != source_tag) {
        release_thread_writer();
        return false;
    }
    uint32_t seq = next_sequence(&s);
    if (seq == 0u) {
        release_thread_writer();
        return false;
    }
    retained_diag_phase_t p = {
        .format = RETAINED_DIAG_FORMAT, .sequence = seq,
        .boot = boot, .attempt = attempt, .phase = phase,
        .source_tag = source_tag, .checksum = 0u,
        .commit = RETAINED_DIAG_COMMIT
    };
    p.checksum = retained_diag_checksum(&p, offsetof(retained_diag_phase_t, checksum));
    bool ok = write_phase(next_phase_offset(&s), &p);
    release_thread_writer();
    return ok;
}

bool retained_diag_frame_range(uint32_t raw_msp, uint32_t raw_psp,
                               uint32_t exc_return, uint32_t cfsr,
                               uintptr_t stack_low, uintptr_t stack_high,
                               uintptr_t *frame, uint32_t *frame_bytes)
{
    if (frame == NULL || frame_bytes == NULL ||
        (cfsr & DIAG_BAD_FRAME_CFSR) != 0u ||
        stack_high <= stack_low) {
        return false;
    }
    /* Armv7-M non-secure EXC_RETURN forms, basic and FP-extended. */
    uint32_t basic = exc_return | 0x10u;
    if (basic != 0xfffffff1u && basic != 0xfffffff9u &&
        basic != 0xfffffffdu) {
        return false;
    }
    uintptr_t sp = (exc_return & 4u) ? raw_psp : raw_msp;
    uint32_t bytes = (exc_return & 0x10u) ? 32u : 104u;
    if ((sp & 7u) != 0u || sp < stack_low ||
        bytes > stack_high - stack_low || sp > stack_high - bytes) {
        return false;
    }
    *frame = sp;
    *frame_bytes = bytes;
    return true;
}

#ifdef HOST_TEST
void retained_diag_host_reset_context(void)
{
    armed_context.commit = 0u;
    armed_context.boot = 0u;
    armed_context.attempt = 0u;
    armed_context.source_tag = 0u;
    storage_ready = 0u;
    storage_ready_inverse = 0u;
    thread_writer_busy = 0u;
    host_writes_remaining = UINT32_MAX;
}
#else

/* Terminal exception code uses no SVC, HAL, allocator, lock, or FP operation. */
static volatile uint32_t *diag_bkpram_words(uint32_t offset)
{
    return (volatile uint32_t *)(uintptr_t)(BSP_BKPSRAM_BASE + offset);
}

static void diag_read_words(void *out, const volatile uint32_t *src,
                            uint32_t count)
{
    uint32_t *dst = (uint32_t *)out;
    for (uint32_t i = 0u; i < count; ++i) { dst[i] = src[i]; }
}

static bool fault_phase_for_context(retained_diag_phase_t *out)
{
    if (armed_context.commit != RETAINED_DIAG_COMMIT) { return false; }
    diag_barrier();
    uint32_t boot = armed_context.boot;
    uint32_t attempt = armed_context.attempt;
    uint32_t tag = armed_context.source_tag;
    retained_diag_phase_t a, b;
    diag_read_words(&a, diag_bkpram_words(RETAINED_DIAG_PHASE_A), 8u);
    diag_read_words(&b, diag_bkpram_words(RETAINED_DIAG_PHASE_B), 8u);
    bool av = phase_valid(&a) && a.boot == boot && a.attempt == attempt &&
              a.source_tag == tag;
    bool bv = phase_valid(&b) && b.boot == boot && b.attempt == attempt &&
              b.source_tag == tag;
    if (!av && !bv) { return false; }
    if (av && bv && a.sequence == b.sequence && !same_phase(&a, &b)) {
        return false;
    }
    *out = (!bv || (av && a.sequence > b.sequence)) ? a : b;
    return true;
}

extern uint8_t _estack;
extern uint32_t _Min_Stack_Size;

static bool fault_stack_window(uint32_t exc_return,
                               uintptr_t *low, uintptr_t *high)
{
    if ((exc_return & 4u) != 0u) {
        uint8_t idx = current_task_index;
        if (idx >= (uint8_t)ICARUS_MAX_TASKS || os_running == 0u) {
            return false;
        }
        uint32_t *stack_base = __kernel_get_stack(idx);
        uintptr_t base = (uintptr_t)stack_base;
        *low = base;
        *high = base + (uintptr_t)ICARUS_STACK_WORDS * sizeof(uint32_t);
    } else {
        uintptr_t top = (uintptr_t)&_estack;
        uintptr_t size = (uintptr_t)&_Min_Stack_Size;
        if (size == 0u || top < size) { return false; }
        *low = top - size;
        *high = top;
    }
    return true;
}

void retained_diag_capture_terminal(uint32_t raw_msp, uint32_t raw_psp,
                                    uint32_t exc_return, uint32_t fault_kind)
{
    /* A fault before a successful task-gated BKPSRAM read cannot assume the
     * backup clock/regulator or MPU window is usable. Preserve the halt. */
    if (storage_ready != RETAINED_DIAG_COMMIT ||
        storage_ready_inverse != ~RETAINED_DIAG_COMMIT) {
        for (;;) { __asm__ volatile ("nop"); }
    }
    retained_diag_fault_t existing;
    diag_read_words(&existing, diag_bkpram_words(RETAINED_DIAG_FAULT), 16u);
    if (!fault_valid(&existing)) {
        retained_diag_fault_t f;
        /* Keep the terminal path independent of a compiler-emitted memset. */
        volatile unsigned char *clear = (volatile unsigned char *)&f;
        for (uint32_t i = 0u; i < sizeof(f); ++i) { clear[i] = 0u; }
        uint32_t cfsr = SCB->CFSR;
        f.format_kind_flags = RETAINED_DIAG_FORMAT | ((fault_kind & 0xffu) << 8);
        f.raw_msp = raw_msp;
        f.raw_psp = raw_psp;
        f.exc_return = exc_return;
        f.cfsr = cfsr;
        f.hfsr = SCB->HFSR;
        if ((cfsr & DIAG_MMFAR_VALID) != 0u) {
            f.mmfar = SCB->MMFAR;
            f.format_kind_flags |= RETAINED_DIAG_FLAG_MMFAR;
        }
        if ((cfsr & DIAG_BFAR_VALID) != 0u) {
            f.bfar = SCB->BFAR;
            f.format_kind_flags |= RETAINED_DIAG_FLAG_BFAR;
        }
        retained_diag_phase_t p;
        if (fault_phase_for_context(&p)) {
            f.phase_sequence = p.sequence;
            f.boot = p.boot;
            f.attempt = p.attempt;
            f.phase = p.phase;
        }
        uintptr_t low, high, frame;
        uint32_t frame_bytes;
        if (fault_stack_window(exc_return, &low, &high) &&
            retained_diag_frame_range(raw_msp, raw_psp, exc_return, cfsr,
                                      low, high, &frame, &frame_bytes)) {
            const volatile uint32_t *words = (const volatile uint32_t *)frame;
            uint32_t xpsr = words[7];
            if (((xpsr & (1u << 9)) == 0u ||
                 high - frame >= (uintptr_t)frame_bytes + 4u) &&
                (xpsr & (1u << 24)) != 0u) {
                f.stacked_lr = words[5];
                f.stacked_pc = words[6];
                f.format_kind_flags |= RETAINED_DIAG_FLAG_PC;
            }
        }
        f.checksum = retained_diag_checksum(&f, offsetof(retained_diag_fault_t, checksum));
        volatile uint32_t *dst = diag_bkpram_words(RETAINED_DIAG_FAULT);
        const uint32_t *src = (const uint32_t *)&f;
        /* HardFault bypasses our MPU (HFNMIENA=0), so BKPSRAM can be WBWA.
         * DSB alone does not write dirty D-cache lines back before IWDG.
         * The 64-byte fault slot occupies exactly two 32-byte cache lines. */
        dst[15] = 0u;
        SCB_CleanDCache_by_Addr((uint32_t *)(uintptr_t)
            (BSP_BKPSRAM_BASE + RETAINED_DIAG_FAULT + 32u), 32);
        for (uint32_t i = 0u; i < 15u; ++i) { dst[i] = src[i]; }
        SCB_CleanDCache_by_Addr((uint32_t *)(uintptr_t)
            (BSP_BKPSRAM_BASE + RETAINED_DIAG_FAULT), 64);
        dst[15] = RETAINED_DIAG_COMMIT;
        SCB_CleanDCache_by_Addr((uint32_t *)(uintptr_t)
            (BSP_BKPSRAM_BASE + RETAINED_DIAG_FAULT + 32u), 32);
    }
    for (;;) { __asm__ volatile ("nop"); }
}
#endif
