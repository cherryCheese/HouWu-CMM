/*
 * led.c
 *
 * Created: 25.08.2021 17:33:35
 *  Author: E1130513
 */ 

#include <asf.h>

#include "sys_timer.h"
#include "config.h"
#include "uart.h"
#include "led.h"
#include "env.h"
#include "smbus.h"
#include "adc_measure.h"

#ifndef BOOTLOADER

static uint32_t led_1sec_timer;
static uint32_t force_led_to_green_delay = 0;

static void signalize_in_operating_mode(void);

/*
 * Initialize the System LED'S
 */
void led_init(void)
{
	ioport_set_pin_dir(CFG_LED_RED, IOPORT_DIR_OUTPUT);
	ioport_set_pin_dir(CFG_LED_GRN, IOPORT_DIR_OUTPUT);
	LED_Off(CFG_LED_RED);
	LED_Off(CFG_LED_GRN);
}

/*
 * Signalize that 3V3 are not within the limits while powertest
 */
void signalize_3v3_not_ok(void)
{
	while(1)
	{
		LED_On(CFG_LED_RED);
		LED_Off(CFG_LED_GRN);
	}
}

/*
 * Signalize that 5V are not within the limits while powertest
 */
void signalize_5v_not_ok(void)
{	
	while(1)
	{
		LED_Toggle(CFG_LED_RED);
		LED_Off(CFG_LED_GRN);
		delay_cycles_ms(500);
	}
}

/*
 * Signalize that 12V are not within the limits while powertest
 */
void signalize_12v_not_ok(void)
{
	while(1)
	{
		LED_Toggle(CFG_LED_RED);
		LED_Off(CFG_LED_GRN);
		delay_cycles_ms(166);
	}
}

/*
 * Signalize restart the system after learnmode
 */
void signalize_restart_system(void)
{
	while(1)
	{
		LED_On(CFG_LED_GRN);
		delay_cycles_ms(100);
		LED_Off(CFG_LED_GRN);
		delay_cycles_ms(100);
	}
}

/*
 * Signalize how many fans and temps are learned
 */
void signalize_learn_state(void)
{
	uint8_t learned_fans, learned_fans_count=0;
	uint8_t learned_temps, learned_temps_count=0;
	
	learned_fans = env_get("learned_fans");
	learned_temps = env_get("learned_temperature_sensors");
	
	for(uint8_t i=0; i<8; i++)
	{
		if(((learned_fans>>i)&1) == 1)
		{
			learned_fans_count++;
		}
		
		if(((learned_temps>>i)&1) == 1)
		{
			learned_temps_count++;
		}
	}
	
	LED_Off(CFG_LED_GRN); //LED's of and 2 sec delay, that the following cont blinking of the NTC's / Tacho's can be visualized
	LED_Off(CFG_LED_RED);
	delay_cycles_ms(2000);
	
	for(uint8_t i=0; i<learned_temps_count; i++)
	{
		LED_On(CFG_LED_GRN);
		delay_cycles_ms(500);
		LED_Off(CFG_LED_GRN);
		delay_cycles_ms(500);
	}
	
	LED_Off(CFG_LED_GRN);
	delay_cycles_ms(2000);
	
	for(uint8_t i=0; i<learned_fans_count; i++)
	{
		LED_On(CFG_LED_GRN);
		delay_cycles_ms(500);
		LED_Off(CFG_LED_GRN);
		delay_cycles_ms(500);
	}
	
	LED_Off(CFG_LED_GRN);
	delay_cycles_ms(2000);
}

/*
 * Signalize status in operating mode
 */
static void signalize_in_operating_mode(void)
{
	uint8_t led_state=0;
	
	if ((read_pwr_ok() == 7) && (smbus_get_input_reg(SMBUS_REG__TEMP_FAIL) == 0) && (smbus_get_input_reg(SMBUS_REG__FAN_FAIL) == 0)) //Normal operating
	{
		led_state = 0;
	}
	else
	{
		if (smbus_get_input_reg(SMBUS_REG__TEMP_FAIL) != 0) //Temp Fail
		{
			led_state = 1;
		}
		
		if (smbus_get_input_reg(SMBUS_REG__FAN_FAIL) != 0) //Fan Fail
		{
			led_state = 2;
		}
		
		if (read_pwr_ok() != 7) //Power Fail
		{
			led_state = 3;
			
			if((ioport_get_pin_level(CFG_SEL_SS_PS_ON) == 0 ) && (ioport_get_pin_level(CFG_SS_PS_ON_IN) == 1)) //Power off
			{
				led_state = 4; 
			}
			if((ioport_get_pin_level(CFG_SEL_SS_PS_ON) == 1) && (port_pin_get_input_level(CFG_EXT_PS_ON_IN) == 0)) //Power off
			{
				led_state = 4;
			}
		}
	}
	
	switch(led_state) //state of the Led
	{
		case 0: //Action to monitor.state = 0
		LED_Off(CFG_LED_RED);
		LED_On(CFG_LED_GRN);
		break;
		
		case 1: //Action to monitor.state = 1
		LED_Off(CFG_LED_RED);
		LED_Toggle(CFG_LED_GRN);
		break;
		
		case 2: //Action to monitor.state = 2
		LED_On(CFG_LED_RED);
		LED_Off(CFG_LED_GRN);
		break;
		
		case 3: //Action to monitor.state = 3
		LED_Toggle(CFG_LED_RED);
		LED_Off(CFG_LED_GRN);
		break;
		
		case 4: //Action to monitor.state = 4
		LED_Off(CFG_LED_RED);
		LED_Off(CFG_LED_GRN);
		
		default://Action to monitor.state > 4
		break;
	}
}

/*
 * Force the green LED to solid green
 */
void force_LED_to_green(void)
{
	force_led_to_green_delay = get_jiffies();
	LED_Off(CFG_LED_RED);
	LED_On(CFG_LED_GRN);
}

/*
 * Do the led relevant functions
 */
void do_led(void)
{
	if (get_jiffies() - force_led_to_green_delay >= 5000)
	{
		if (get_jiffies() - led_1sec_timer >= 1000)
		{
			led_1sec_timer = get_jiffies();
			signalize_in_operating_mode();
		}
	}	
}

#endif /* BOOTLOADER */