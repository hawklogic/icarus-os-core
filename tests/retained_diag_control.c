/** Standalone host backing store for focused retained-diagnostic controls. */
#include "icarus/kernel.h"
#include "unity.h"

#include <string.h>

static uint8_t host_backup_sram[4096];

static bool range_ok(uint32_t offset, uint32_t len)
{
    return len != 0u && offset <= sizeof(host_backup_sram) &&
           len <= sizeof(host_backup_sram) - offset;
}

bool bkpram_write(const void *src, uint32_t offset, uint32_t len)
{
    if (src == NULL || !range_ok(offset, len)) { return false; }
    (void)memcpy(&host_backup_sram[offset], src, len);
    return true;
}

bool bkpram_read(void *dst, uint32_t offset, uint32_t len)
{
    if (dst == NULL || !range_ok(offset, len)) { return false; }
    (void)memcpy(dst, &host_backup_sram[offset], len);
    return true;
}

void __bkpram_host_clear(void)
{
    (void)memset(host_backup_sram, 0, sizeof(host_backup_sram));
}

extern void run_retained_diag_tests(void);
void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();
    run_retained_diag_tests();
    return UNITY_END();
}
