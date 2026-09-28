/**
 * @file    tables.c
 * @brief   Generic ground-loadable table engine implementation
 *
 * @details Maintains a registry of up to TBL_MAX_REGISTERED table
 *          descriptors. Each registered table has a staging buffer and
 *          an active buffer (double-buffered swap):
 *
 *            1. tbl_load()     — writes raw bytes to staging (chunked OK).
 *            2. tbl_activate() — validates CRC, calls activate callback,
 *                                copies staging → active on success.
 *            3. tbl_dump()     — returns the *active* buffer contents.
 *
 *          Registry, staging, and active buffers all live in
 *          DTCM_DATA_PRIV. Public access goes through SVC gates.
 *
 *          tbl_activate is split across two privileged calls so the
 *          user activate callback can run in thread mode between them:
 *
 *            __tbl_activate_prepare()  — validate + copy staging into a
 *                                        caller-provided scratch buffer
 *            user activate(scratch)    — runs in thread mode, no SVC
 *            __tbl_activate_commit()   — copy scratch into active
 *
 *          The engine itself is silent — no logging dependency.
 *
 * @author  Souham Biswas
 * @date    2026
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            https://github.com/ironhide23586/icarus-os-core
 *            Licensed under the Apache License, Version 2.0
 */

#include "icarus/icarus.h"
#include <string.h>

/* ---- Per-slot storage --------------------------------------------------- */

typedef struct {
    tbl_descriptor_t desc;

    /* Staging side */
    uint8_t  staging[TBL_MAX_SIZE];
    uint16_t staged_len;        /**< Bytes written to staging so far          */
    uint16_t staged_schema_crc; /**< schema_crc supplied with last tbl_load() */
    uint16_t staged_data_crc;   /**< CRC16 computed over the full staged data */
    bool     staged_valid;      /**< True once a full-size load completed     */

    /* Active side */
    uint8_t  active[TBL_MAX_SIZE];
    uint16_t active_len;        /**< Bytes in active buffer (0 = none)        */

    bool     prepared;          /**< activate_prepare done, commit pending    */
} tbl_slot_t;

DTCM_DATA_PRIV static tbl_slot_t registry[TBL_MAX_REGISTERED];
DTCM_DATA_PRIV static uint8_t    reg_count;

/* ---- Helper: find slot by id (priv mode only) -------------------------- */

ITCM_FUNC static tbl_slot_t *find_slot(tbl_id_t id) {
    for (uint8_t i = 0; i < reg_count; i++) {
        if (registry[i].desc.id == id) {
            return &registry[i];
        }
    }
    return NULL;
}

/* ---- Privileged implementations ---------------------------------------- */

ITCM_FUNC void __tbl_init(void) {
    (void)memset(registry, 0, sizeof(registry));
    reg_count = 0;
}

ITCM_FUNC bool __tbl_register(const tbl_descriptor_t *desc) {
    if (!desc) {
        return false;
    }
    if (reg_count >= (uint8_t)TBL_MAX_REGISTERED) {
        return false;
    }
    if ((desc->size == 0u) || (desc->size > (uint16_t)TBL_MAX_SIZE)) {
        return false;
    }
    /* Reject duplicate id */
    for (uint8_t i = 0; i < reg_count; i++) {
        if (registry[i].desc.id == desc->id) {
            return false;
        }
    }
    tbl_slot_t *slot = &registry[reg_count++];
    (void)memset(slot, 0, sizeof(*slot));
    (void)memcpy(&slot->desc, desc, sizeof(tbl_descriptor_t));
    /* Ensure NUL termination of name */
    slot->desc.name[TBL_NAME_LEN - 1] = '\0';
    return true;
}

/** @brief Discard any staged bytes for a slot. */
ITCM_FUNC static void staging_reset(tbl_slot_t *slot, uint16_t schema_crc) {
    (void)memset(slot->staging, 0, sizeof(slot->staging));
    slot->staged_len        = 0;
    slot->staged_valid      = false;
    slot->staged_schema_crc = schema_crc;
    slot->prepared          = false;
}

/**
 * @brief  Offset-addressed staging write.
 * @details
 *   - offset 0 starts a new load (discarding any previous staging).
 *   - offset == bytes staged so far appends the next chunk.
 *   - a chunk entirely inside the already-staged range is a retransmit:
 *     accepted if identical, rejected if it conflicts.
 *   - a gap (offset beyond the staged length), an overrun past the
 *     descriptor size, or a schema CRC that differs from the first chunk's
 *     is rejected and leaves staging unchanged.
 */
ITCM_FUNC bool __tbl_load_at(tbl_id_t id, uint16_t offset,
                             const uint8_t *data, uint16_t len,
                             uint16_t schema_crc) {
    if ((data == NULL) || (len == 0u)) {
        return false;
    }

    tbl_slot_t *slot = find_slot(id);
    if (slot == NULL) {
        return false;
    }

    uint32_t end = (uint32_t)offset + (uint32_t)len;
    if (end > (uint32_t)slot->desc.size) {
        return false;                                   /* overrun */
    }

    if (offset == 0u) {
        /* Identical retransmit of the first chunk of a load in progress
         * must not throw away the chunks that followed it. */
        bool retransmit = (slot->staged_len >= len) && !slot->staged_valid &&
                          (slot->staged_schema_crc == schema_crc) &&
                          (memcmp(slot->staging, data, len) == 0);
        if (retransmit) {
            return true;
        }
        staging_reset(slot, schema_crc);
    } else if (schema_crc != slot->staged_schema_crc) {
        return false;                                   /* mixed loads */
    } else if (end <= (uint32_t)slot->staged_len) {
        /* Retransmit of bytes we already hold. */
        return memcmp(&slot->staging[offset], data, len) == 0;
    } else if (offset != slot->staged_len) {
        return false;                                   /* gap or overlap */
    } else {
        /* sequential append */
    }

    (void)memcpy(&slot->staging[offset], data, len);
    slot->staged_len = (uint16_t)end;
    slot->prepared   = false;

    /* Mark valid and compute data CRC once we have a full descriptor-size load */
    if (slot->staged_len == slot->desc.size) {
        slot->staged_data_crc = crc16_ccitt(slot->staging, slot->staged_len);
        slot->staged_valid    = true;
    }

    return true;
}

ITCM_FUNC bool __tbl_load(tbl_id_t id, const uint8_t *data, uint16_t len,
                          uint16_t schema_crc) {
    tbl_slot_t *slot = find_slot(id);
    if (slot == NULL) {
        return false;
    }
    /* Legacy append semantics: a completed staging starts a new load. */
    uint16_t offset = slot->staged_valid ? 0u : slot->staged_len;
    return __tbl_load_at(id, offset, data, len, schema_crc);
}

ITCM_FUNC bool __tbl_abort(tbl_id_t id) {
    tbl_slot_t *slot = find_slot(id);
    if (slot == NULL) {
        return false;
    }
    staging_reset(slot, 0u);
    return true;
}

ITCM_FUNC bool __tbl_activate_prepare(tbl_id_t id, uint8_t *out_data,
                                      uint16_t *out_len,
                                      tbl_activate_fn *out_activate) {
    if ((out_data == NULL) || (out_len == NULL) || (out_activate == NULL)) {
        return false;
    }

    tbl_slot_t *slot = find_slot(id);
    if (slot == NULL) {
        return false;
    }

    /* Step 1: size match */
    if ((!slot->staged_valid) || (slot->staged_len != slot->desc.size)) {
        return false;
    }

    /* Step 2: schema CRC */
    if (slot->staged_schema_crc != slot->desc.schema_crc) {
        return false;
    }

    /* Step 3: recompute data CRC for safety */
    uint16_t computed = crc16_ccitt(slot->staging, slot->staged_len);
    if (computed != slot->staged_data_crc) {
        return false;
    }

    /* Step 4: copy into the caller-provided scratch buffer so the user
     *         activate callback can run from thread mode without touching
     *         DTCM_PRIV directly. */
    (void)memcpy(out_data, slot->staging, slot->staged_len);
    *out_len      = slot->staged_len;
    *out_activate = slot->desc.activate;
    slot->prepared = true;
    return true;
}

/**
 * @details Only accepted right after a successful prepare for the same
 *          table, with the descriptor-size length and data identical to
 *          what was staged, so a stray commit cannot install arbitrary
 *          bytes as the active table.
 */
ITCM_FUNC bool __tbl_activate_commit(tbl_id_t id, const uint8_t *data,
                                     uint16_t len) {
    if ((data == NULL) || (len == 0u) || (len > (uint16_t)TBL_MAX_SIZE)) {
        return false;
    }
    tbl_slot_t *slot = find_slot(id);
    if (slot == NULL) {
        return false;
    }
    if (!slot->prepared || (len != slot->desc.size) ||
        (memcmp(data, slot->staging, len) != 0)) {
        return false;
    }
    (void)memcpy(slot->active, data, len);
    slot->active_len = len;
    slot->prepared   = false;
    return true;
}

ITCM_FUNC int16_t __tbl_dump(tbl_id_t id, uint8_t *out, uint16_t max) {
    if ((out == NULL) || (max == 0u)) {
        return -1;
    }

    tbl_slot_t *slot = find_slot(id);
    if (slot == NULL) {
        return -1;
    }

    uint16_t len = slot->active_len;
    if (len == 0u) {
        return -1;
    }
    if (len > max) {
        len = max;
    }
    (void)memcpy(out, slot->active, len);
    return (int16_t)len;
}

ITCM_FUNC const tbl_descriptor_t *__tbl_get_descriptor(tbl_id_t id) {
    tbl_slot_t *slot = find_slot(id);
    return slot ? &slot->desc : NULL;
}

ITCM_FUNC bool __tbl_get_info(tbl_id_t id, tbl_info_t *out) {
    if (out == NULL) {
        return false;
    }
    tbl_slot_t *slot = find_slot(id);
    if (slot == NULL) {
        return false;
    }
    out->id           = slot->desc.id;
    (void)memcpy(out->name, slot->desc.name, sizeof(out->name));
    out->name[TBL_NAME_LEN - 1] = '\0';
    out->size         = slot->desc.size;
    out->schema_crc   = slot->desc.schema_crc;
    out->staged_len   = slot->staged_len;
    out->staged_valid = slot->staged_valid;
    out->active_len   = slot->active_len;
    return true;
}

ITCM_FUNC uint8_t __tbl_count(void) {
    return reg_count;
}
