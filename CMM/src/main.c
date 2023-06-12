/*
 * main.c: bootloader/firmware entry point
 *
 * Created: 8/14/2020 3:42:16 PM
 *  Author: E1210640
 */ 

#include <asf.h>

#include "config.h"
#include "fuses.h"
#include "ring_buffer.h"
#include "uart.h"
#include "cli.h"
#include "spi_flash.h"
#include "bootloader.h"
#include "heartbeat.h"
#include "watchdog.h"
#include "sys_timer.h"
#include "smbus.h"
#include "eeprom_driver.h"
#include "adc_measure.h"
#include "env.h"
#include "learn.h"
#include "power_management.h"
#include "fan.h"
#include "led.h"
#include "i2c_master.h"



int main (void)
{
	/* Initialize all modules */
	system_init();
	delay_init();
	ioport_init();
	uart_init();
	
	WDT_DISABLE;
	
#ifdef BOOTLOADER
	bootloader();
#else	
	program_fuses();
	/* Following outputs need to be set instant after boot*/
	ioport_set_pin_dir(CFG_PWR_OK_UC_N, IOPORT_DIR_OUTPUT);
	ioport_set_pin_level(CFG_PWR_OK_UC_N, 1);
	ioport_set_pin_dir(CFG_EN_12V_FAN, IOPORT_DIR_OUTPUT);
	ioport_set_pin_level(CFG_EN_12V_FAN, 0);
	printf("\r\n\r\n");
	printf("========================================\r\n");
	printf(" CMM Firmware %s\r\n",CFG_FIRMWARE_NUMBER);
	printf("========================================\r\n");

	enum system_reset_cause reset_cause = system_get_reset_cause();
    printf("Reset cause: %s\r\n",
		reset_cause == SYSTEM_RESET_CAUSE_WDT ? "WDT" :
		reset_cause == SYSTEM_RESET_CAUSE_BOD12 ? "BOD12" :
		reset_cause == SYSTEM_RESET_CAUSE_BOD33 ? "BOD33" :
		reset_cause == SYSTEM_RESET_CAUSE_EXTERNAL_RESET ? "EXT" :
		reset_cause == SYSTEM_RESET_CAUSE_POR ? "POR" :
		reset_cause == SYSTEM_RESET_CAUSE_SOFTWARE ? "SOFT" : "N/A");
	
	power_management_init();
	eeprom_init();
	env_init();
	fan_init();
	spi_flash_init();
	smbus_init();
	i2c_init_master();
	adc_measure_init();
	led_init();
	
	
	/* Enable global interrupts */
	system_interrupt_enable_global();
	learn();
	load_learned_fan_values();
	load_learned_temp_values();
	
#ifdef CFG_WDT_TIMEOUT
	wdt_init(CFG_WDT_TIMEOUT);
#endif
	sys_timer_init();

	printf("\r\n");

		
	/* Main loop for the tasks */
	while (1) {
		WDT_RESET;
		do_env();
		do_heartbeat(10000);
		do_cli();
		do_smbus();
		do_fan();
		do_i2c_master();
		do_measure();
		do_power_management();
		do_led();
	}
#endif /* BOOTLOADER */
}
