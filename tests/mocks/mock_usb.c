/*
 * mock_usb.c
 * Mock USB CDC implementation for unit testing
 *
 * The transmit result and the DTR line are settable so tests can drive
 * the "not configured", "busy" and "nobody listening" paths.
 */

#include "mock_usb.h"

static USBD_StatusTypeDef g_tx_result = USBD_OK;
static uint32_t           g_tx_calls  = 0u;
static uint8_t            g_dtr       = 1u;

USBD_StatusTypeDef CDC_Transmit_FS(uint8_t* Buf, uint16_t Len) {
	(void)Buf;
	(void)Len;
	g_tx_calls++;
	return g_tx_result;
}

uint8_t CDC_IsDtrAsserted(void) {
	return g_dtr;
}

void mock_usb_set_transmit_result(USBD_StatusTypeDef result) {
	g_tx_result = result;
}

void mock_usb_set_dtr(uint8_t asserted) {
	g_dtr = asserted;
}

uint32_t mock_usb_transmit_calls(void) {
	return g_tx_calls;
}

void mock_usb_reset(void) {
	g_tx_result = USBD_OK;
	g_tx_calls  = 0u;
	g_dtr       = 1u;
}
