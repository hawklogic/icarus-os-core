/**
 * @file retained_diag.h
 * @brief Small generic diagnostic record in backup SRAM.
 *
 * Core owns the format and offsets; the caller owns phase numbers and
 * the full source identity shown to the host. No model type crosses this API.
 */
#ifndef BSP_RETAINED_DIAG_H
#define BSP_RETAINED_DIAG_H

#include <stdbool.h>
#include <stdint.h>

#define RETAINED_DIAG_OFFSET       64u
#define RETAINED_DIAG_BYTES        128u
#define RETAINED_DIAG_PHASE_A      64u
#define RETAINED_DIAG_PHASE_B      96u
#define RETAINED_DIAG_FAULT        128u
#define RETAINED_DIAG_PHASE_BYTES  32u
#define RETAINED_DIAG_FAULT_BYTES  64u
#define RETAINED_DIAG_COMMIT       0xD1A60C01u
#define RETAINED_DIAG_FORMAT       0xD1010000u
#define RETAINED_DIAG_KIND_PHASE   0u
#define RETAINED_DIAG_KIND_HARD    1u
#define RETAINED_DIAG_KIND_MEMORY  2u
#define RETAINED_DIAG_KIND_BUS     3u
#define RETAINED_DIAG_KIND_USAGE   4u
#define RETAINED_DIAG_FLAG_MMFAR   0x01u
#define RETAINED_DIAG_FLAG_BFAR    0x02u
#define RETAINED_DIAG_FLAG_PC      0x04u

/** Phase zero is the generic committed arm marker. Other IDs belong to caller. */
#define RETAINED_DIAG_PHASE_ARMED  0u

typedef struct {
    uint32_t format;
    uint32_t sequence;
    uint32_t boot;
    uint32_t attempt;
    uint32_t phase;
    uint32_t source_tag;
    uint32_t checksum;
    uint32_t commit;
} retained_diag_phase_t;

typedef struct {
    uint32_t format_kind_flags;
    uint32_t phase_sequence;
    uint32_t boot;
    uint32_t attempt;
    uint32_t phase;
    uint32_t raw_msp;
    uint32_t raw_psp;
    uint32_t exc_return;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t mmfar;
    uint32_t bfar;
    uint32_t stacked_pc;
    uint32_t stacked_lr;
    uint32_t checksum;
    uint32_t commit;
} retained_diag_fault_t;

typedef enum {
    RETAINED_DIAG_EMPTY = 0,
    RETAINED_DIAG_VALID = 1,
    RETAINED_DIAG_CORRUPT = 2
} retained_diag_status_t;

typedef struct {
    /* Exact bytes at backup-SRAM offsets [64,192), even when all are invalid. */
    uint8_t raw[RETAINED_DIAG_BYTES];
    retained_diag_phase_t phase;
    retained_diag_fault_t fault;
    retained_diag_status_t status;
    bool phase_valid;
    bool fault_valid;
    bool fault_bound; /* Exact phase sequence + boot + attempt join. */
    uint32_t fault_source_tag; /* Meaningful only when fault_bound is true. */
} retained_diag_snapshot_t;

/** IEEE reflected CRC32 used by both retained formats. */
uint32_t retained_diag_checksum(const void *bytes, uint32_t length);

/** Core exception policy: after 30 recoveries, preserve the next fault. */
bool retained_diag_memmanage_should_terminate(uint32_t recovered_so_far);

/** Read exact raw bytes and derive validity. True means the read succeeded. */
bool retained_diag_read(retained_diag_snapshot_t *out);

/**
 * Explicitly start an attempt after the caller has durably collected `shown`.
 * Re-reads and compares all 128 raw bytes before changing any retained byte.
 * Empty/corrupt prior storage is allowed only through this explicit call.
 * `source_tag` is a short context tag, never full image attestation.
 */
bool retained_diag_arm_after_ack(const retained_diag_snapshot_t *shown,
                                 uint32_t boot, uint32_t attempt,
                                 uint32_t source_tag, uint32_t *sequence_out);

/** Commit a scalar caller phase in an already armed matching attempt. */
bool retained_diag_mark(uint32_t boot, uint32_t attempt,
                        uint32_t source_tag, uint32_t phase);

/** Pure address/status guard; never dereferences the proposed exception frame. */
bool retained_diag_frame_range(uint32_t raw_msp, uint32_t raw_psp,
                               uint32_t exc_return, uint32_t cfsr,
                               uintptr_t stack_low, uintptr_t stack_high,
                               uintptr_t *frame, uint32_t *frame_bytes);

/* Core exception machinery only: terminal, privileged, does not return. */
#ifndef HOST_TEST
void retained_diag_capture_terminal(uint32_t raw_msp, uint32_t raw_psp,
                                    uint32_t exc_return, uint32_t fault_kind)
    __attribute__((noreturn));
#else
/** Simulate C runtime BSS reset while preserving the host backup-SRAM store. */
void retained_diag_host_reset_context(void);
/** Fail a retained write after this many successful writes (test only). */
void retained_diag_host_fail_after_writes(uint32_t count);
#endif

#endif /* BSP_RETAINED_DIAG_H */
