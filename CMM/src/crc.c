/*
 * crc.c
 *
 * Created: 11/9/2020 4:12:56 PM
 *  Author: E1210640
 */ 

#include <asf.h>

#include "config.h"
#include "crc.h"

#ifndef BOOTLOADER

uint16_t crc16(uint16_t crc, const uint8_t *buf, int len, uint16_t polynomial, int pad)
{
	int i;
	uint8_t flag, c;

	if (pad) {
		pad = 2;
	}
	while (pad + len--) {
		c = len >= 0 ? *buf++ : 0;
		for (i = 0; i < 8; i++) {
			flag = !!(crc & 0x8000);
			crc <<= 1;
			if (c & 0x80) {
				crc |= 1;
			}
			if (flag) {
				crc ^= polynomial;
			}
			c <<= 1;
		}
	}
	
	return crc;
}

uint16_t crc16_env(uint16_t crc, const uint8_t *buf, uint32_t len, uint16_t polynomial)
{
	int i;
	uint8_t c, flag;
	
	while (len--) {
		c = *buf++;
		for (i = 0; i < 8; i++) {
			flag = !!(crc & 0x8000);
			crc <<= 1;
			if (c & 0x80) {
				crc |= 1;
			}
			if (flag) {
				crc ^= polynomial;
			}
			c <<= 1;
		}
	}
	return crc;
}

uint8_t crc8(uint8_t crc, const uint8_t *buf, int len, uint8_t polynomial, int pad)
{
	int i;
	uint8_t flag, c;
	
	if (pad) {
		pad = 1;
	}
	while (pad + len--) {
		c = len >= 0 ? *buf++ : 0;
		for (i = 0; i < 8; i++) {
			flag = !!(crc & 0x80);
			crc <<= 1;
			if (c & 0x80) {
				crc |= 1;
			}
			if (flag) {
				crc ^= polynomial;
			}
			c <<= 1;
		}
	}
	
	return crc;
}

#endif /* BOOTLOADER */