/*
 * mock_usb.h
 * Mock for USB CDC dependencies
 */

#ifndef MOCK_USB_H
#define MOCK_USB_H

#include <stdint.h>

// Mock USB types
typedef enum {
	USBD_OK = 0,
	USBD_BUSY = 1,
	USBD_FAIL = 2
} USBD_StatusTypeDef;

// Mock USB functions
USBD_StatusTypeDef CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);
uint8_t CDC_IsDtrAsserted(void);

// Test hooks
void     mock_usb_set_transmit_result(USBD_StatusTypeDef result);
void     mock_usb_set_dtr(uint8_t asserted);
uint32_t mock_usb_transmit_calls(void);
void     mock_usb_reset(void);

#endif // MOCK_USB_H
