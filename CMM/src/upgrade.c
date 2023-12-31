/*
 * upgrade.c
 *
 * Created: 8/14/2020 3:42:16 PM
 *  Author: E1210640
 */

#include <asf.h>

#include "config.h"
#include "spi_flash.h"
#include "upgrade.h"
#include "heartbeat.h"
#include "watchdog.h"
#include "crc.h"

#define UPGRADE_MAGIC	0x12345678

/* Image header: stored at the beginning of Flash */
struct flash_header {
	uint32_t magic;				/* Magic number */
	uint32_t size;				/* Image size */
};

#ifndef BOOTLOADER

static uint32_t last_addr;		/* Last transmitted address + 1 */
static uint32_t flash_offset;	/* Image offset in Flash (block #1) */
static uint32_t ihex_upper;		/* Extended address upper bits */

/* Start the upgrade: erase Flash and set up the pointers */
int upgrade_start(void)
{
	last_addr = 0;
	flash_offset = spi_flash_get_block_size();
	if (spi_flash_erase(0, -1) < 0) {
		printf("ERROR: spi_flash_erase failed\r\n");
		return -1;
	}
	ihex_upper = 0;

	return 0;
}


/* Parse an input line (IHEX) and write the decoded data to Flash */
int upgrade_parse_ihex(uint8_t *buf)
{
	uint8_t len, type, i, tmp, done = 0;
	uint32_t addr;
	
	len = buf[0];
	if (len > 252) {
		printf("ERROR: bogus IHEX record length (%d)\r\n", len);
		return -1;
	}
	addr = (buf[1] << 8) | buf[2] | ihex_upper;
	type = buf[3];

	/* Verify checksum */
	for (i = tmp = 0; i < len + 5; i++) {
		tmp += buf[i];
	}
	if (tmp) {
		printf("ERROR: bad IHEX record checksum\r\n");
		return -1;
	}
	switch (type) {
		case 0:
			/* Data */
			addr -= CFG_FIRMWARE_START;
			if (addr + len > last_addr) {
				last_addr = addr + len;
			}
			if (spi_flash_program(addr + flash_offset, buf + 4, len) < 0) {
				printf("ERROR: Flash programming failed\r\n");
				return -1;
			}
			break;
		case 1:
			/* End of File */
			done = 1;
			break;
		case 4:
			ihex_upper = ((buf[4] << 8) | buf[5]) << 16;
			break;
		default:
			/* Ignore other codes */
			break;
	}
	
	return done;
}

/* Verify the integrity of the downloaded image */
int upgrade_verify(void)
{
	uint16_t stored_crc, crc = 0xFFFF;
	uint8_t buf[256];
	uint32_t addr, end = flash_offset + last_addr - 2, chunk;
	
	for (addr = flash_offset; addr < end; addr += chunk) {
		WDT_RESET;
		chunk = end - addr;
		if (chunk > sizeof(buf)) {
			chunk = sizeof(buf);
		}
		if (spi_flash_read(addr, buf, chunk) < 0) {
			printf("ERROR: spi_flash_read failed\r\n");
			return -1;
		}
		crc = crc16(crc, (const uint8_t *)buf, chunk, 0x1021, addr + chunk >= end);
	}
	if (spi_flash_read(flash_offset + last_addr - 2, (uint8_t *)&stored_crc, sizeof(stored_crc)) < 0) {
		printf("ERROR: spi_flash_read failed\r\n");
		return -1;
	}
	if (crc != stored_crc) {
		printf("ERROR: checksum verification failed (0x%04x != 0x%04x)\r\n", crc, stored_crc);
		return -1;
	}
	
	return 0;
}

/* Activate the downloaded image */
int upgrade_activate(void)
{
	struct flash_header hdr;

	/*
	 * The entire image has been received and verified.
	 * We can now program the header (block #0).
	 */
	hdr.magic = UPGRADE_MAGIC;
	hdr.size = last_addr;
	if (spi_flash_program(0, (uint8_t *)&hdr, sizeof(hdr)) < 0) {
		printf("ERROR: spi_flash_program failed\r\n");
		return -1;
	}
	/*
	 * All done: reset the processor
	 * (the bootloader will copy the image data into the NVM and start the new firmware)
	 */
	//SYSTEM_RESET; THe System reset has to be carried out by the customer.
	/* NOTREACHED */
	
	return 0; 
}

#else /* BOOTLOADER */

/*
 * Check the header and copy the new firmware into the NVM
 * (this function is called by the bootloader).
 */
int upgrade_copy_to_nvm(void)
{
	struct flash_header hdr;
	struct nvm_config cfg;
	uint8_t page_buffer[NVMCTRL_PAGE_SIZE];
	enum status_code error_code;
	uint32_t offset;
	
	if (spi_flash_read(0, (uint8_t *)&hdr, sizeof(hdr)) < 0) {
		printf("ERROR: spi_flash_read failed\r\n");
		return -1;
	}
	if (hdr.magic != UPGRADE_MAGIC) {
		return 0;
	}
	printf("Valid upgrade image detected in Flash: copying to NVM...\r\n");
	nvm_get_config_defaults(&cfg);
	cfg.manual_page_write = false;
	nvm_set_config(&cfg);
	
	for (offset = 0; offset < hdr.size; offset += NVMCTRL_ROW_PAGES * NVMCTRL_PAGE_SIZE) {
		do {
			error_code = nvm_erase_row(CFG_FIRMWARE_START + offset);
		} while (error_code == STATUS_BUSY);
		do_heartbeat(10);
	}
	for (offset = 0; offset < hdr.size; offset += NVMCTRL_PAGE_SIZE) {
		if (spi_flash_read(spi_flash_get_block_size() + offset, page_buffer, sizeof(page_buffer)) < 0) {
			printf("ERROR: spi_flash_read failed\r\n");
			return -1;
		}
		do {
			error_code = nvm_write_buffer(CFG_FIRMWARE_START + offset, page_buffer, sizeof(page_buffer));
		} while (error_code == STATUS_BUSY);
		do_heartbeat(10);
	}
	/* Upgrade successful: erase the first block to prevent upgrade on next reboot */
	spi_flash_erase(0, sizeof(hdr));
	
	return 0;
}

#endif /* BOOTLOADER */