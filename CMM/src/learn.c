/*
 * learn.c
 *
 * Created: 15.06.2021 16:13:00
 *  Author: E1130513
 */ 
#include <asf.h>

#include "learn.h"
#include "config.h"
#include "env.h"
#include "uart.h"
#include "adc_measure.h"
#include "fan.h"
#include "led.h"
#include "power_management.h"

#ifndef BOOTLOADER

static void voltage_test(void);

/*
 * Check whether the voltages have the correct value and so check also the cabling of the power supply
 */
static void voltage_test(void)
{
	turn_3V3_on();
	delay_cycles_ms(2000);
	voltages_get_values();
	check_voltage_ok();
	if(read_pwr_ok()==1)
	{
		printf("\r\n3V3 is within the limits\r\n\r\n");
	}
	else
	{
		printf("\r\n3V3 is not within the limits. Powertest failed!\r\n\r\n");
		//signalize_3v3_not_ok();
	}
	
	turn_5V_on();
	delay_cycles_ms(2000);
	voltages_get_values();
	check_voltage_ok();
	if(read_pwr_ok()==3)
	{
		printf("\r\n5V is within the limits\r\n\r\n");
	}
	else
	{
		printf("\r\n5V is not within the limits. Powertest failed!\r\n\r\n");
		//signalize_5v_not_ok();
	}
	
	turn_12V_on();
	delay_cycles_ms(2000);
	voltages_get_values();
	check_voltage_ok();
	if(read_pwr_ok()==7)
	{
		printf("\r\n12V is within the limits\r\n\r\n");
	}
	else
	{
		printf("\r\n12V is not within the limits. Powertest failed!\r\n\r\n");
		//signalize_12v_not_ok();
	}
}

/*
 * Learn fan and temp values and signalize the values via the user interface at the front plate
 */
void learn(void)
{
	static uint8_t init_done;
	
	if (!init_done) {
		ioport_set_pin_dir(CFG_DIP4_LEARN, IOPORT_DIR_INPUT);
		init_done = 1;
	}
	
	if((env_get("learned") == 0) || (ioport_get_pin_level(CFG_DIP4_LEARN) == 0))
	{
		printf("\r\nPlease wait until the voltage test...\r\n\r\n");
		
		voltage_test();
		
		printf("\r\nPlease wait until the Fan Controller learned...\r\n\r\n");
		
		learn_fan();
		
		learn_temp();
		
		signalize_learn_state();
		
		env_set("learned", 1);
		
		do_env();
		
		printf("\r\nLearning process finnished\r\n\r\n");
		
		signalize_restart_system();
	}	
}

#endif /* BOOTLOADER */