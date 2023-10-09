/*
 * power_management.c
 *
 * Created: 25.08.2021 15:41:50
 *  Author: E1130513
 */ 
#include <asf.h>

#include "power_management.h"
#include "adc_measure.h"
#include "sys_timer.h"
#include "config.h"
#include "uart.h"
#include "env.h"
#include "smbus.h"
#include "fan.h"
#include "led.h"

#ifndef BOOTLOADER

static void set_ps_on(ioport_pin_t pin, bool level);
static void extint_detection_callback_int_15(void);
static void extint_detection_callback_int_10(void);
static void extint_detection_callback_int_11(void);
static void power_management_sync_to_smbus(void);

static uint32_t last_print;
static uint32_t delay_turn_voltages_off_at_startup=0;
static uint32_t voltages_on=0;

/*
 * Set PS_On signal for enable the PXIe voltages
 */
static void set_ps_on(ioport_pin_t pin, bool level)
{
	if(ioport_get_pin_level(CFG_DIP1_PS_ON_LOGIC))
	{
		ioport_set_pin_level(pin, !level);
	}
	else
	{
		ioport_set_pin_level(pin, level);
	}
}

/*
 * turn on 3V3
 */
void turn_3V3_on(void)
{
	set_ps_on(CFG_PS_ON_OUT_3_3V3_N, 0);
}


/*
 * turn on 5V
 */
void turn_5V_on(void)
{
	set_ps_on(CFG_PS_ON_OUT_1_5V_N, 0);
}

/*
 * turn on 12V
 */
void turn_12V_on(void)
{
	set_ps_on(CFG_PS_ON_OUT_2_12V_N, 0);
}

/*
 * turn on -12V
 */
void turn_m12V_on(void)
{
	set_ps_on(CFG_PS_ON_OUT_4_M12V_N, 0);
}

/*
 * turn on all PXIe voltages
 */
void turn_voltages_on(void)
{
	ioport_set_pin_level(CFG_EN_12V_FAN, 1);
	set_spinup_speed_of_fans();
	set_ps_on(CFG_PS_ON_OUT_2_12V_N, 0);
	//delay_cycles_ms(45);			//Delay resulting through the PDB!!! 
	set_ps_on(CFG_PS_ON_OUT_1_5V_N, 0);
	delay_cycles_ms(3);
	set_ps_on(CFG_PS_ON_OUT_3_3V3_N, 0);
	delay_cycles_ms(10);
	set_ps_on(CFG_PS_ON_OUT_4_M12V_N, 0);
	delay_cycles_ms(300); //Take care that the PWR_OK is set after voltages are stable 	
	initial_read_i2c_components();
	ioport_set_pin_level(CFG_PWR_OK_UC_N, 0); //Set Power OK to the Embedded Controller
	force_LED_to_green(); //force Front LED to green for 5 seconds
	delay_turn_voltages_off_at_startup = get_jiffies(); //force ignore checking Power off for 5 seconds 
	voltages_on = 1;
} 

/*
 * turn off all PXIe voltages
 */
void turn_voltages_off(void)
{
	ioport_set_pin_level(CFG_PWR_OK_UC_N, 1); //Clear Power OK to the Embedded Controller
	ioport_set_pin_level(CFG_EN_12V_FAN, 0);
	delay_cycles_ms(1);
	set_ps_on(CFG_PS_ON_OUT_4_M12V_N, 1);
	delay_cycles_ms(10);
	set_ps_on(CFG_PS_ON_OUT_3_3V3_N, 1);
	delay_cycles_ms(10);
	set_ps_on(CFG_PS_ON_OUT_2_12V_N, 1);
	delay_cycles_ms(10);
	set_ps_on(CFG_PS_ON_OUT_1_5V_N, 1);
	voltages_on = 0;	
}

/*
 * Initialize the power Management
 */
void power_management_init(void)
{
	ioport_set_pin_dir(CFG_DIP1_PS_ON_LOGIC, IOPORT_DIR_INPUT);
	ioport_set_pin_dir(CFG_DIP2_AC_OK, IOPORT_DIR_INPUT);	
	struct extint_chan_conf config_extint_15;
	extint_chan_get_config_defaults(&config_extint_15);
	config_extint_15.gpio_pin           = CFG_AC_OK_IN_INT;
	config_extint_15.gpio_pin_mux       = CFG_AC_OK_IN_MUX;
	config_extint_15.gpio_pin_pull      = EXTINT_PULL_NONE;
	if(ioport_get_pin_level(CFG_DIP2_AC_OK) == 0)
	{
		config_extint_15.detection_criteria = EXTINT_DETECT_RISING;
	}
	else
	{
		config_extint_15.detection_criteria = EXTINT_DETECT_FALLING;
	}
	extint_chan_set_config(15, &config_extint_15);
	
	//---Initialization SS_PS_ON_IN INTERRUPT---
	struct extint_chan_conf config_extint_10;
	extint_chan_get_config_defaults(&config_extint_10);
	config_extint_10.gpio_pin           = CFG_SS_PS_ON_IN_INT;
	config_extint_10.gpio_pin_mux       = CFG_SS_PS_ON_IN_MUX;
	config_extint_10.gpio_pin_pull      = EXTINT_PULL_NONE;
	config_extint_10.detection_criteria = EXTINT_DETECT_BOTH;
	extint_chan_set_config(10, &config_extint_10);

	//---Initialization EXT_PS_ON_IN INTERRUPT---
	struct extint_chan_conf config_extint_11;
	extint_chan_get_config_defaults(&config_extint_11);
	config_extint_11.gpio_pin           = CFG_EXT_PS_ON_IN_INT;
	config_extint_11.gpio_pin_mux       = CFG_EXT_PS_ON_IN_MUX;
	config_extint_11.gpio_pin_pull      = EXTINT_PULL_NONE;
	config_extint_11.detection_criteria = EXTINT_DETECT_BOTH;
	extint_chan_set_config(11, &config_extint_11);
	
	
	extint_register_callback(extint_detection_callback_int_15 ,15,EXTINT_CALLBACK_TYPE_DETECT);
	extint_chan_enable_callback(15 ,EXTINT_CALLBACK_TYPE_DETECT);
	
	extint_register_callback(extint_detection_callback_int_10 ,10,EXTINT_CALLBACK_TYPE_DETECT);
	extint_chan_enable_callback(10 ,EXTINT_CALLBACK_TYPE_DETECT);
	
	extint_register_callback(extint_detection_callback_int_11 ,11,EXTINT_CALLBACK_TYPE_DETECT);
	extint_chan_enable_callback(11 ,EXTINT_CALLBACK_TYPE_DETECT);
	
	
	ioport_set_pin_dir(CFG_DIP1_PS_ON_LOGIC, IOPORT_DIR_INPUT);
	ioport_set_pin_dir(CFG_SEL_SS_PS_ON, IOPORT_DIR_INPUT);
	ioport_set_pin_dir(CFG_SS_PS_ON_IN, IOPORT_DIR_INPUT);
	ioport_set_pin_dir(CFG_EXT_PS_ON_IN, IOPORT_DIR_INPUT);
	ioport_set_pin_dir(CFG_AC_OK_IN, IOPORT_DIR_INPUT);
	
	
	
	ioport_set_pin_dir(CFG_PS_ON_OUT_1_5V_N, IOPORT_DIR_OUTPUT);
	ioport_set_pin_dir(CFG_PS_ON_OUT_2_12V_N, IOPORT_DIR_OUTPUT);
	ioport_set_pin_dir(CFG_PS_ON_OUT_3_3V3_N, IOPORT_DIR_OUTPUT);
	ioport_set_pin_dir(CFG_PS_ON_OUT_4_M12V_N, IOPORT_DIR_OUTPUT);
	
	
	set_ps_on(CFG_PS_ON_OUT_1_5V_N, 1);
	set_ps_on(CFG_PS_ON_OUT_2_12V_N, 1);
	set_ps_on(CFG_PS_ON_OUT_3_3V3_N, 1);
	set_ps_on(CFG_PS_ON_OUT_4_M12V_N, 1);

}

/*
 * Turn the system of if AC Fail occur
 */
static void extint_detection_callback_int_15(void)
{
	//AC Fail need to be implemented if the PCB is ready for this
}

/*
 * Turn on/off the system depending the PS_ON from the System Module
 */
static void extint_detection_callback_int_10(void)
{
	delay_cycles_ms(5);
	
	if(ioport_get_pin_level(CFG_SEL_SS_PS_ON) == 0)
	{
		if(ioport_get_pin_level(CFG_SS_PS_ON_IN) == 0)
		{
			turn_voltages_on();
		}
		else
		{
			turn_voltages_off();
		}
	}
}

/*
 * Turn on off the system depending the External PS_On (Inhibit Mode)
 */
static void extint_detection_callback_int_11(void)
{
	delay_cycles_ms(5);
	
	if(ioport_get_pin_level(CFG_SEL_SS_PS_ON) == 1)
	{
		if(ioport_get_pin_level(CFG_EXT_PS_ON_IN) == 1)
		{
			turn_voltages_on();
		}
		else
		{
			turn_voltages_off();
		}
	}
}


/*
 * Sync the Power Management signals the smbus
 */
static void power_management_sync_to_smbus(void)
{
	smbus_set_input_reg(SMBUS_REG__SEL_SS_PS_ON, ioport_get_pin_level(CFG_SEL_SS_PS_ON));
	smbus_set_input_reg(SMBUS_REG__SS_PS_ON_IN, ioport_get_pin_level(CFG_SS_PS_ON_IN));
	smbus_set_input_reg(SMBUS_REG__EXT_PS_ON_IN, ioport_get_pin_level(CFG_EXT_PS_ON_IN));
	smbus_set_input_reg(SMBUS_REG__PS_ON_OUT_1, (~port_pin_get_output_level(CFG_PS_ON_OUT_1_5V_N))&1);
	smbus_set_input_reg(SMBUS_REG__PS_ON_OUT_2, (~port_pin_get_output_level(CFG_PS_ON_OUT_2_12V_N))&1);
	smbus_set_input_reg(SMBUS_REG__PS_ON_OUT_3, (~port_pin_get_output_level(CFG_PS_ON_OUT_3_3V3_N))&1);
	smbus_set_input_reg(SMBUS_REG__PS_ON_OUT_4, (~port_pin_get_output_level(CFG_PS_ON_OUT_4_M12V_N))&1);
	smbus_set_input_reg(SMBUS_REG__AC_OK, ioport_get_pin_level(CFG_AC_OK_IN));
	smbus_set_input_reg(SMBUS_REG__PWR_OK, !ioport_get_pin_level(CFG_PWR_OK_UC_N)&1);
}

/*
 * Do all power management tasks
 */
void do_power_management(void)
{	
	if(voltages_on == 0)
	{
		if((ioport_get_pin_level(CFG_SEL_SS_PS_ON) == 1) && (ioport_get_pin_level(CFG_EXT_PS_ON_IN) == 1))
		{
			turn_voltages_on();
		}
	}
	
	if(get_jiffies() - delay_turn_voltages_off_at_startup > 3000)
	{
		if (get_jiffies() - last_print > 1000)
		{			
			last_print = get_jiffies();
			
			power_management_sync_to_smbus();
				
			if((read_pwr_ok() != 7) && (voltages_on == 1))
			{
				ioport_set_pin_level(CFG_PWR_OK_UC_N, 1);
				turn_voltages_off();
			}
		}
	}
}

#endif /* BOOTLOADER */		
