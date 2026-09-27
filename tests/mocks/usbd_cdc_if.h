/*
 * mock_usbd_cdc_if.h
 * Minimal mock for usbd_cdc_if.h
 */

#ifndef __USBD_CDC_IF_H__
#define __USBD_CDC_IF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "mock_usbd_def.h"

// Functions used by kernel
USBD_StatusTypeDef CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);
uint8_t CDC_IsDtrAsserted(void);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CDC_IF_H__ */
