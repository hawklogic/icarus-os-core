/**
 * @file    tables.h
 * @brief   ICARUS OS — Generic ground-loadable table engine
 *
 * @details Maintains a registry of up to TBL_MAX_REGISTERED tables. Each
 *          registered slot has a staging buffer and an active buffer
 *          (double-buffered swap). Tables are validated by schema CRC and
 *          a CRC16-CCITT data checksum before the registered activate
 *          callback is called. Application-specific table-id constants
 *          live in downstream consumers; the engine itself is generic.
 *
 * @author  Souham Biswas
 * @date    2026
 *
 * @copyright Copyright 2025-2026 Souham Biswas
 *            https://github.com/ironhide23586/icarus-os-core
 *            Licensed under the Apache License, Version 2.0
 */

#ifndef ICARUS_TABLES_H
#define ICARUS_TABLES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ---- Configuration constants -------------------------------------------- */

#define TBL_MAX_REGISTERED  8    /**< Maximum number of registered tables    */
#define TBL_MAX_SIZE        512  /**< Maximum bytes per table buffer         */
#define TBL_NAME_LEN        12   /**< Maximum name length (incl. NUL)        */

/* ---- Generic table id ---------------------------------------------------- */

/**
 * @brief  Generic table identifier. Downstream consumers define their own
 *         id constants (e.g. \c TBL_FDIR_RULES = 0x01) and pass them as
 *         tbl_id_t values into the engine.
 */
typedef uint8_t tbl_id_t;

/* ---- Descriptor --------------------------------------------------------- */

/**
 * @brief  Callback invoked when a staged table passes all CRC checks.
 *
 * @param  data  Pointer to the staged (validated) table bytes.
 * @param  len   Number of bytes.
 * @return true on success; false to abort the swap (active unchanged).
 */
typedef bool (*tbl_activate_fn)(const void *data, uint16_t len);

/**
 * @brief  Static descriptor registered by a table producer.
 */
typedef struct {
    tbl_id_t        id;                 /**< Unique table identifier        */
    char            name[TBL_NAME_LEN]; /**< Human-readable name             */
    uint16_t        size;               /**< Expected table size in bytes   */
    uint16_t        schema_crc;         /**< Expected schema CRC            */
    tbl_activate_fn activate;           /**< Callback on tbl_activate()     */
} tbl_descriptor_t;

/**
 * @brief  Copy-out view of a registered table, safe for unprivileged use.
 */
typedef struct {
    tbl_id_t id;                  /**< Table identifier                     */
    char     name[TBL_NAME_LEN];  /**< Human-readable name (NUL-terminated) */
    uint16_t size;                /**< Expected table size in bytes         */
    uint16_t schema_crc;          /**< Expected schema CRC                  */
    uint16_t staged_len;          /**< Bytes currently staged               */
    bool     staged_valid;        /**< Full-size load staged and checked    */
    uint16_t active_len;          /**< Bytes in the active table (0 = none) */
} tbl_info_t;

/* ---- API ---------------------------------------------------------------- */

/*
 * From an unprivileged task, every buffer below must be memory the task may
 * pass to the kernel (see svc_buffer_allowed()).  A rejected buffer makes
 * the call fail (false, or -1 for tbl_dump()) without touching the table or
 * the buffer.
 */

/**
 * @brief  Initialise the table registry. Must be called before any other
 *         table function.
 */
void     tbl_init(void);

/**
 * @brief  Register a table descriptor.
 * @return true on success; false if registry is full or id is duplicate,
 *         or @p desc was rejected.
 */
bool     tbl_register(const tbl_descriptor_t *desc);

/**
 * @brief  Write a chunk of a table into staging at an explicit offset.
 *
 * Chunked loads over a lossy link: offset 0 starts a new load; each next
 * chunk must start where the staged data ends.  A chunk that falls
 * entirely within bytes already staged is treated as a retransmit and
 * accepted only if identical.  Gaps, overruns past the descriptor size,
 * conflicting retransmits and a schema CRC that differs from the first
 * chunk's are rejected without touching staging.  When the staged length
 * reaches the descriptor size the data CRC is computed and staging is
 * marked valid.
 *
 * @return true on success; false on any rejection, including a rejected
 *         @p data buffer (staging is not touched).
 */
bool     tbl_load_at(tbl_id_t id, uint16_t offset, const uint8_t *data,
                     uint16_t len, uint16_t schema_crc);

/**
 * @brief  Append bytes to the staging buffer (legacy form of tbl_load_at).
 *
 * Equivalent to tbl_load_at() at the current staged length, or at offset 0
 * once a previous load completed.  Prefer tbl_load_at() for anything that
 * can retransmit.
 *
 * @return true on success; false on unknown id, out-of-bounds write, or a
 *         rejected @p data buffer.
 */
bool     tbl_load(tbl_id_t id, const uint8_t *data, uint16_t len,
                  uint16_t schema_crc);

/**
 * @brief  Discard any staged (not yet activated) bytes for a table.
 * @return true on success; false on unknown id.
 */
bool     tbl_abort(tbl_id_t id);

/**
 * @brief  Activate a staged table after CRC validation.
 *
 * Steps:
 *   1. Verify staged_len == descriptor.size.
 *   2. Verify staged_schema_crc == descriptor.schema_crc.
 *   3. Recompute CRC16-CCITT over staged data; verify against stored crc.
 *   4. Call descriptor.activate(staged, size).
 *   5. On success, copy staging → active buffer.
 *
 * @return true on success; false on any validation failure or activate
 *         callback returning false.
 */
bool     tbl_activate(tbl_id_t id);

/**
 * @brief  Copy the *active* table bytes into @p out.
 * @return Number of bytes copied, or -1 on error (unknown id / no active,
 *         or @p out rejected for @p max bytes).
 */
int16_t  tbl_dump(tbl_id_t id, uint8_t *out, uint16_t max);

/**
 * @brief  Look up a registered descriptor by id.
 * @return Pointer to descriptor, or NULL if not found.
 * @warning The pointer refers to privileged memory.  Unprivileged code
 *          must not dereference it; use tbl_get_info() instead.
 */
const tbl_descriptor_t *tbl_get_descriptor(tbl_id_t id);

/**
 * @brief  Copy a table's descriptor fields and staging/active state out.
 * @param[out] out  Destination (caller memory).
 * @return true on success; false on unknown id, NULL @p out, or a rejected
 *         @p out (left untouched).
 */
bool     tbl_get_info(tbl_id_t id, tbl_info_t *out);

/**
 * @brief  Return the number of registered tables.
 */
uint8_t  tbl_count(void);

/* ---- Privileged implementations (internal — do not call directly) ----- */

void                    __tbl_init(void);
bool                    __tbl_register(const tbl_descriptor_t *desc);
bool                    __tbl_load(tbl_id_t id, const uint8_t *data,
                                   uint16_t len, uint16_t schema_crc);
bool                    __tbl_load_at(tbl_id_t id, uint16_t offset,
                                      const uint8_t *data, uint16_t len,
                                      uint16_t schema_crc);
bool                    __tbl_abort(tbl_id_t id);
bool                    __tbl_get_info(tbl_id_t id, tbl_info_t *out);
/**
 * @brief  Validate a staged table and copy it into a caller-provided
 *         scratch buffer for the activate callback. The active buffer
 *         is not touched yet.
 *
 * @param  id             Table id.
 * @param  out_data       Caller scratch buffer (must be ≥ TBL_MAX_SIZE).
 * @param  out_len        [out] Number of bytes written to scratch.
 * @param  out_activate   [out] Pointer to the user activate callback (or NULL).
 * @return true on validation success; false otherwise.
 */
bool                    __tbl_activate_prepare(tbl_id_t id, uint8_t *out_data,
                                               uint16_t *out_len,
                                               tbl_activate_fn *out_activate);
/**
 * @brief  Commit a previously prepared table into the active buffer.
 *         Called only after the user activate callback has succeeded.
 */
bool                    __tbl_activate_commit(tbl_id_t id, const uint8_t *data,
                                              uint16_t len);
int16_t                 __tbl_dump(tbl_id_t id, uint8_t *out, uint16_t max);
const tbl_descriptor_t *__tbl_get_descriptor(tbl_id_t id);
uint8_t                 __tbl_count(void);

#ifdef __cplusplus
}
#endif

#endif /* ICARUS_TABLES_H */
