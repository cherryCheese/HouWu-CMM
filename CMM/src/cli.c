/*
 * cli.c: command line interface
 *
 * Created: 8/14/2020 3:42:16 PM
 *  Author: E1210640
 */ 

#ifndef BOOTLOADER

#include <asf.h>
#include <string.h>

#include "config.h"
#include "uart.h"
#include "cli.h"
#include "spi_flash.h"
#include "sys_timer.h"
#include "watchdog.h"
#include "env.h"

#ifndef BOOTLOADER

#define CLI_INBUF_SIZE	256
#define CLI_MAX_ARGS	256
#define CLI_PROMPT		"FW> "

static uint8_t prompt = 1;
static uint8_t inbuf[CLI_INBUF_SIZE];
static uint32_t inbuf_ptr;


#define CLI_COMMANDS (int)(sizeof(cli_cmd_switch)/sizeof(*cli_cmd_switch))

struct cli_cmd_entry {
	const char *cmd;
	const char *args;
	const char *desc;
	int (*func)(int argc, char **argv);
};

/***************************************************************
 *                 CLI command handlers                        *
 ***************************************************************/


static int cli_cmd_reset(int argc, char **argv)
{
	SYSTEM_RESET;
	/* NOTREACHED */
	
	return 0;
}

static int cli_cmd_printenv(int argc, char **argv)
{
	char *var;
	uint32_t val;
	
	if (argc > 1) {
		printf("Invalid arguments\r\n");
		return -1;
	}
	if (argc == 0) {
		env_print_all();
		} else {
		var = argv[0];
		if (env_find(var) < 0) {
			printf("Variable %s not found\r\n", var);
			return -1;
		}
		val = env_get(var);
		printf("%s = %lu\r\n", var, val);
	}
	return 0;
}

static int cli_cmd_flash_read(int argc, char **argv)
{
	uint32_t addr, len;
	uint8_t buf[256];
	char *end;
	int i;
	
	if (argc != 2) {
		printf("Invalid arguments\r\n");
		return -1;
	}
	addr = strtoul(argv[0], &end, 0);
	len = strtoul(argv[1], &end, 0);
	if (len < 1 || len > 256) {
		printf("Invalid length: %lu\r\n", len);
		return -1;
	}
	if (spi_flash_read(addr, buf, len) < 0) {
		printf("ERROR: spi_flash_read failed\r\n");
		return -1;
	}
	for (i = 0; i < (int)len; i++) {
		printf("%02x ", buf[i]);
	}
	printf("\r\n");
	
	return 0;
}

static int cli_cmd_flash_erase(int argc, char **argv)
{
	uint32_t addr, len;
	char *end;
	
	if (!argc) {
		addr = 0;
		len = -1;
		printf("Erasing the entire Flash\r\n");
	} else if (argc != 2) {
		printf("Invalid arguments\r\n");
		return -1;
	} else {
		addr = strtoul(argv[0], &end, 0);
		len = strtoul(argv[1], &end, 0);
		printf("Erasing Flash @ 0x%06lx (%lu bytes)\r\n", addr, len);
	}
	if (spi_flash_erase(addr, len) < 0) {
		printf("ERROR: spi_flash_erase failed\r\n");
		return -1;
	}
	printf("Success\r\n");
	
	return 0;
}

static int cli_cmd_flash_write(int argc, char **argv)
{
	uint32_t addr;
	uint8_t buf[256];
	char *end;
	int i;
	
	if (argc < 2) {
		printf("Invalid arguments\r\n");
		return -1;
	}
	addr = strtoul(argv[0], &end, 0);
	printf("Writing %d bytes to Flash @ 0x%06lx\r\n", argc - 1, addr);
	for (i = 0; i < argc - 1; i++) {
		buf[i] = strtoul(argv[i + 1], &end, 0);
	}
	if (spi_flash_program(addr, buf, argc - 1) < 0) {
		printf("ERROR: spi_flash_program failed\r\n");
		return -1;
	}
	printf("Success\r\n");
	
	return 0;
}

static int cli_cmd_flash_copy(int argc, char **argv)
{
	uint8_t buf[256];
	uint32_t src, dst, len, chunk;
	char *end;
	
	dst = strtoul(argv[0], &end, 0);
	src = strtoul(argv[1], &end, 0);
	len = strtoul(argv[2], &end, 0);

	while (len > 0) {
		chunk = sizeof(buf);
		if (chunk > len) {
			chunk = len;
		}
		if (spi_flash_read(src, buf, chunk) < 0) {
			printf("ERROR: spi_flash_read failed\r\n");
			return -1;
		}
		if (spi_flash_program(dst, buf, chunk) < 0) {
			printf("ERROR: spi_flash_program failed\r\n");
			return -1;
		}
		src += chunk;
		dst += chunk;
		len -= chunk;
	}
	printf("Success\r\n");
	
	return 0;
}

static int cli_cmd_flash_compare(int argc, char **argv)
{
	uint8_t buf1[256], buf2[256];
	uint32_t addr1, addr2, len, i, chunk;
	char *end;
	
	addr1 = strtoul(argv[0], &end, 0);
	addr2 = strtoul(argv[1], &end, 0);
	len = strtoul(argv[2], &end, 0);
	
	while (len > 0) {
		chunk = sizeof(buf1);
		if (chunk > len) {
			chunk = len;
		}
		if (spi_flash_read(addr1, buf1, chunk) < 0) {
			printf("ERROR: spi_flash_read failed\r\n");
			return -1;
		}
		if (spi_flash_read(addr2, buf2, chunk) < 0) {
			printf("ERROR: spi_flash_read failed\r\n");
			return -1;
		}
		for (i = 0; i < chunk; i++) {
			if (buf1[i] != buf2[i]) {
				printf("%02x@%08lx != %02x@%08lx\r\n", buf1[i], addr1 + i, buf2[i], addr2 + i);
			}
		}
		addr1 += chunk;
		addr2 += chunk;
		len -= chunk;
		WDT_RESET;
	}
	
	return 0;
}

static int cli_cmd_flash_status(int argc, char **argv)
{
	uint8_t status;
	
	if (spi_flash_get_status(&status) < 0) {
		return -1;
	}
	printf("0x%02x\r\n", status);
	
	return 0;
}

static int cli_cmd_eeprom_commit(int argc, char **argv)
{
	eeprom_emulator_commit_page_buffer();
	
	return 0;
}

static int cli_cmd_hang(int argc, char **argv)
{
	printf("Hanging the CPU...\r\n");
	while (1)
		;
	/* NOTREACHED */
	
	return 0;
}

static int cli_cmd_systick(int argc, char **argv)
{
	printf("%ld\r\n", get_jiffies());
	
	return 0;
}



/***************************************************************
 *                   CLI command switch                        *
 ***************************************************************/

static int cli_cmd_help(int argc, char **argv);

static struct cli_cmd_entry cli_cmd_switch[] = {
	{
		"help",
		"",
		"Print help information",
		cli_cmd_help
	},
	{
		"printenv",
		"[var]",
		"Print an environment variable (or all variables, if var is omitted)",
		cli_cmd_printenv
	},
	{
		"reset",
		"",
		"Reset the CPU",
		cli_cmd_reset
	},
	{
		"systick",
		"",
		"Get current system timer counter",
		cli_cmd_systick
	},
	{
		"flash_read",
		"addr, len",
		"Read from SPI Flash",
		cli_cmd_flash_read
	},
	{
		"flash_erase",
		"addr, len",
		"Erase SPI Flash (erase the entire Flash if no arguments are specified)",
		cli_cmd_flash_erase
	},
	{
		"flash_write",
		"addr, len, byte1, byte2, ...",
		"Write to SPI Flash",
		cli_cmd_flash_write
	},
	{
		"flash_copy",
		"dst, src, len",
		"Copy SPI Flash data",
		cli_cmd_flash_copy
	},
	{
		"flash_compare",
		"addr1, addr2, len",
		"Compare SPI Flash data",
		cli_cmd_flash_compare
	},
	{
		"flash_status",
		"",
		"Get SPI Flash status",
		cli_cmd_flash_status
	},
	{
			"eeprom_commit",
			"",
			"Commit EEPROM page buffer to NVM",
			cli_cmd_eeprom_commit
	},
	{
		"hang",
		"",
		"Hang the CPU (for WDT testing)",
		cli_cmd_hang
	}
#endif /* CFG_DEVEL_COMMANDS_ENABLE */
};

/***************************************************************
 *                   Auxiliary Functions                       *
 ***************************************************************/

static int cli_cmd_lookup(char *cmd)
{
	int i;
	
	for (i = 0; i < CLI_COMMANDS; i++) {
		if (!strcasecmp(cmd, cli_cmd_switch[i].cmd)) {
			return i;
		}
	}
	
	return -1;
}

static int cli_cmd_help(int argc, char **argv)
{
	int cmd;
	
	if (argc > 0) {
		cmd = cli_cmd_lookup(argv[0]);
		if (cmd >= 0) {
			printf("%s %s\r\n  %s\r\n", cli_cmd_switch[cmd].cmd, cli_cmd_switch[cmd].args, cli_cmd_switch[cmd].desc);
		} else {
			printf("Command not supported: %s\r\n", argv[0]);
		}
	} else {
		printf("Supported commands:\r\n");
		for (cmd = 0; cmd < CLI_COMMANDS; cmd++) {
			printf("  %s\r\n", cli_cmd_switch[cmd].cmd);
		}
	}
	
	return 0;
}

static int iswhitespace(char c)
{
	return c == ' ' || c == '\t';
}

static void cli_parse(char *buf)
{
	char *argv[CLI_MAX_ARGS];
	int argc, cmd, ret;
	
	for (argc = 0; argc < CLI_MAX_ARGS && *buf; argc++) {
		while (*buf && iswhitespace(*buf)) {
			buf++;
		}
		if (!*buf) {
			break;
		}
		argv[argc] = buf;
		while (*buf && !iswhitespace(*buf)) {
			buf++;
		}
		if (*buf) {
			*buf++ = 0;
		}
	}
	if (!argc) {
		return;
	}
	
	cmd = cli_cmd_lookup(argv[0]);
	if (cmd >= 0) {
		ret = cli_cmd_switch[cmd].func(argc - 1, argv + 1);
		if (ret < 0) {
			printf("ERROR: %d\r\n", ret);
		}
	} else {
		printf("Invalid command: %s\r\n", argv[0]);
	}
}

void do_cli(void)
{
	char c;
	
	if (prompt) {
		uart_puts(CFG_CONSOLE_CHANNEL, CLI_PROMPT);
		prompt = 0;
	}
	if (!uart_gets(CFG_CONSOLE_CHANNEL, &c, 1)) {
		return;
	}
	if (c == '\n') {
		/* Do nothing: see '\r' below */
	} else if (c == '\r') {
		inbuf[inbuf_ptr] = 0;
		uart_puts(CFG_CONSOLE_CHANNEL, "\r\n");
		cli_parse((char *)inbuf);
		inbuf_ptr = 0;
		prompt = 1;
	} else if (c == 127 || c == 8) {
		/* Backspace/Delete */
		if (inbuf_ptr > 0) {
			inbuf[--inbuf_ptr] = 0;
			uart_putc(CFG_CONSOLE_CHANNEL, c);
		}
	} else if (c == 3) {
		/* CTRL+C */
		prompt = 1;
		inbuf_ptr = 0;
		uart_puts(CFG_CONSOLE_CHANNEL, "\r\n");
	} else if (inbuf_ptr < CLI_INBUF_SIZE - 1) {
		inbuf[inbuf_ptr++] = c;
		uart_putc(CFG_CONSOLE_CHANNEL, c);
	}
}

#endif /* BOOTLOADER */