/*
 * fan.c
 *
 * Created: 21.08.2020 12:45:12
 *  Author: E1130513
 */ 

#include <asf.h>

#include "fan.h" 
#include "config.h"
#include "uart.h"
#include "sys_timer.h"
#include "env.h"
#include "smbus.h"


#ifndef BOOTLOADER

static uint32_t cnt_tacho[CFG_MAX_FAN_COUNT];
static uint32_t last_pwm_adjust;
static uint32_t last_tacho_measure;
static uint32_t fan_speed_up_time;
static uint8_t fan_speed_up_flag;
static uint8_t current_pwm;
uint32_t fantacho[CFG_MAX_FAN_COUNT];
static uint32_t pwm_frequency;
static uint8_t pulses_per_rotation;
static volatile uint8_t ready_flag_get_fan_speed;
static uint8_t learned_fans_available;
static uint8_t pwm_autonomous = 20;
static struct tc_module tc_instance_pwm;
static struct tc_module tc_instance_tacho;

static void pwm_calculation_autonomous_mode(void);
static void set_pwm(void);
static void tc_callback_timer1(struct tc_module *const module_inst);
static void delete_extint_callbacks(void);
static void enable_extint_callbacks(void);
static void extint_detection_callback_int_0(void);
static void extint_detection_callback_int_1(void);
static void extint_detection_callback_int_4(void);
static void extint_detection_callback_int_5(void);
static void get_fan_speed(void);
static void check_fan_fail(void);
static void fan_sync_to_smbus(void);

static uint16_t cnt_spinup_delay_pwm = 0;

/*
 * Calculate the pwm in autonomous mode 
 */
static void pwm_calculation_autonomous_mode(void)
{
	uint8_t max_temp=0;
	static uint8_t max_temp_hysteresis=0, hysteresis_cnt;
	uint8_t temp_air_outlet_x = 0;
	float a,b,x=0;
	unsigned char min, max;
	
	switch(smbus_get_input_reg(SMBUS_REG__FAN_CURVE))
	{
		case 0: 	b = 4;			a = -60;  		min = 20;  max = 40;	break;
		case 1: 	b = 5.33;		a = -113.33;  	min = 25;  max = 40;	break;
		case 2: 	b = 8;			a = -220;  		min = 30;  max = 40;	break;
		case 3: 	b = 16;			a = -540;	  	min = 35;  max = 40;	break;
		case 4: 	b = 2.66;		a = -33.33;		min = 20;  max = 50;	break;
		case 5: 	b = 4;			a = -100;	  	min = 30;  max = 50;	break;
		case 6: 	b = 5.33;		a = -166.67;	min = 35;  max = 50;	break;
		case 7: 	b = 8;			a = -300;	  	min = 40;  max = 50;	break;
		case 8: 	b = 2;			a =	-20;  		min = 20;  max = 60;	break;
		case 9: 	b = 2.66;		a = -60;	  	min = 30;  max = 60;	break;
		case 10: 	b = 4;			a =	-140;  		min = 40;  max = 60;	break;
		case 11: 	b = 8;			a =	-380;	  	min = 50;  max = 60;	break;
		case 12: 	b = 1.6;		a =	-12;  		min = 20;  max = 70;	break;
		case 13: 	b = 2.28;		a =	-60;	  	min = 35;  max = 70;	break;
		case 14: 	b = 3.2;		a =	-124;  		min = 45;  max = 70;	break;
		case 15: 	b = 5.33;		a =	-273.33;	min = 55;  max = 70;	break;
		default:	b = 4;			a = -60;  		min = 20;  max = 40;	break;
	}
	
	//Look for the hottest outlet NTC and take its temperature
	for(int i=0; i<3; i++)
	{
		temp_air_outlet_x = smbus_get_input_reg(SMBUS_REG__TEMP_AIR_OUTLET1+i);		
		if(temp_air_outlet_x > max_temp)
		{
			max_temp = temp_air_outlet_x;
		}
	}
	
	if( (max_temp < (max_temp_hysteresis-2)) || (max_temp > (max_temp_hysteresis+2)) )
	{
		max_temp_hysteresis = max_temp;
	}

	if(hysteresis_cnt > 100)
	{
		hysteresis_cnt=0;
		max_temp_hysteresis = max_temp;
	}
	else
	{
		hysteresis_cnt++;
	}
		
	x = max_temp_hysteresis;
	
	if((smbus_get_input_reg(SMBUS_REG__FAN_FAIL) != 0) || (smbus_get_input_reg(SMBUS_REG__TEMP_FAIL) != 0) || (port_pin_get_input_level(CFG_FAN_MAX_SPEED) == 1))
	{
		if(smbus_get_input_reg(SMBUS_REG__PWR_OK) == 1)
		{
			pwm_autonomous = CFG_MAX_PWM;
		}
		else
		{
			pwm_autonomous = CFG_MIN_PWM;
		}
	}
	else
	{
		if(x<=min) //take the min pwm level, if temperatur<min
		{
			pwm_autonomous = CFG_MIN_PWM;
		}
		
		if(x>=max) //take the max pwm level, if temperatur>max
		{
			pwm_autonomous = CFG_MAX_PWM;
		}
		
		if(x<max && x>min) //take the PWM level from the choosed Temperature curve
		{
			pwm_autonomous = (unsigned char)(b*x+a);
		}
	}
}

/*
 * Set fans to spinup mode, to guarantee that the fans start to run 
 */
void set_spinup_speed_of_fans(void)
{
	cnt_spinup_delay_pwm=0;	
}

/*
 * Increase or decrease the pwm
 */
static void set_pwm(void)
{
	uint8_t pwm = 20;
	static volatile uint16_t cnt_change_delay_pwm = 0;
	static volatile uint8_t pwm_to_fan = CFG_PWM_INITIAL_VALUE;
	uint8_t pwm_to_fan_invert;

	if(smbus_get_input_reg(SMBUS_REG__REMOTE) > 0)
	{
		pwm = smbus_get_input_reg(SMBUS_REG__SET_FAN);
	}
	else
	{
		pwm = pwm_autonomous;
	}
	
	if(cnt_spinup_delay_pwm < CFG_PWM_SPIN_UP_DELAY)
	{
		cnt_spinup_delay_pwm++;
		pwm_to_fan = CFG_PWM_SPIN_UP_VALUE;
	}
	else 
	{
		if(cnt_change_delay_pwm < CFG_PWM_CHANGE_DELAY)
		{
			cnt_change_delay_pwm++;
		}
		else 
		{
			cnt_change_delay_pwm = 0;
			if(pwm_to_fan < pwm) 
			{
				pwm_to_fan++;
			}
		
			if(pwm_to_fan > pwm) 
			{
				pwm_to_fan--;
			}
		}
	}	
	
	
	if(pwm_to_fan < CFG_MIN_PWM)
	{	//take care, that pwm_to_fan >= MIN_PWMN
		pwm_to_fan = CFG_MIN_PWM;
	}
	
	if(pwm_to_fan > CFG_MAX_PWM)
	{	//take care, that pwm_to_fan is <= MAX_PWM
		pwm_to_fan = CFG_MAX_PWM;
	}
	
	current_pwm = pwm_to_fan;
	pwm_to_fan_invert = abs(pwm_to_fan - 100); //invert PWM1
	
	tc_set_compare_value(&tc_instance_pwm, TC_COMPARE_CAPTURE_CHANNEL_0, pwm_to_fan_invert);
}


/*
 * Timer Callback for measure the fanspeed
 */
static void tc_callback_timer1(struct tc_module *const module_inst)
{
	tc_stop_counter(&tc_instance_tacho);
	delete_extint_callbacks();
	
	for(uint8_t i=0; i<CFG_MAX_FAN_COUNT; i++)
	{
		fantacho[i] = cnt_tacho[i] * 2 * (60/pulses_per_rotation);
	}
	ready_flag_get_fan_speed = 1;
}

/*
 * Delete Tacho interrupts
 */
static void delete_extint_callbacks(void)
{
	extint_unregister_callback(extint_detection_callback_int_0 ,0,EXTINT_CALLBACK_TYPE_DETECT);
	extint_unregister_callback(extint_detection_callback_int_1 ,1,EXTINT_CALLBACK_TYPE_DETECT);
	extint_unregister_callback(extint_detection_callback_int_4 ,4,EXTINT_CALLBACK_TYPE_DETECT);
	extint_unregister_callback(extint_detection_callback_int_5 ,5,EXTINT_CALLBACK_TYPE_DETECT);
}

/*
 *Enable Tacho interrupts
 */
static void enable_extint_callbacks(void)
{
	extint_chan_enable_callback(0 ,EXTINT_CALLBACK_TYPE_DETECT);
	extint_chan_clear_detected(0);
	extint_register_callback(extint_detection_callback_int_0, 0 ,EXTINT_CALLBACK_TYPE_DETECT);
	
	extint_chan_enable_callback(1 ,EXTINT_CALLBACK_TYPE_DETECT);
	extint_chan_clear_detected(1);
	extint_register_callback(extint_detection_callback_int_1, 1 ,EXTINT_CALLBACK_TYPE_DETECT);
	
	extint_chan_enable_callback(4 ,EXTINT_CALLBACK_TYPE_DETECT);
	extint_chan_clear_detected(4);
	extint_register_callback(extint_detection_callback_int_4, 4 ,EXTINT_CALLBACK_TYPE_DETECT);
		
	extint_chan_enable_callback(5 ,EXTINT_CALLBACK_TYPE_DETECT);
	extint_chan_clear_detected(5);
	extint_register_callback(extint_detection_callback_int_5, 5 ,EXTINT_CALLBACK_TYPE_DETECT);
}


/*
 * Interrupt for measure the fan speed (Tacho 0)
 */
static void extint_detection_callback_int_0(void)
{	
	cnt_tacho[0]++;
}

/*
 * Interrupt for measure the fan speed (Tacho 1)
 */
static void extint_detection_callback_int_1(void)
{	
	cnt_tacho[1]++;
}

/*
 * Interrupt for measure the fan speed (Tacho 2)
 */
static void extint_detection_callback_int_4(void)
{	
	cnt_tacho[2]++;
}

/*
 * Interrupt for measure the fan speed (Tacho 3)
 */
static void extint_detection_callback_int_5(void)
{	
	cnt_tacho[3]++;
}


/*
 * Measure the speed of the fans
 */
static void get_fan_speed(void)
{
	for(uint8_t i=0; i<CFG_MAX_FAN_COUNT; i++)
	{
		cnt_tacho[i] = 0;
	}
	ready_flag_get_fan_speed = 0;
	tc_stop_counter(&tc_instance_tacho);
	tc_start_counter(&tc_instance_tacho);
	enable_extint_callbacks();
}


/*
 * Check whether there is a fan alarm.
 */
static void check_fan_fail(void)
{
	uint8_t fan_fail=0;
	
	if ((get_jiffies() - fan_speed_up_time) > 20000)
	{
		fan_speed_up_flag = 1;	
	}
	
	if((fan_speed_up_flag == 1))//only if PWM not increased and 20sec after startup, because of slow change in the fan speed
	{ 
		for(uint8_t i=0; i<CFG_MAX_FAN_COUNT; i++)
		{
			if(((learned_fans_available & (1<<i)) == (1<<i)) && (fantacho[i] < 300))
			{
				fan_fail |= 1<<i;
			}
		}
		smbus_set_input_reg(SMBUS_REG__FAN_FAIL, fan_fail);
	}
}

/*
 * Load the fan values which were learned in the learn mode.
 */
void load_learned_fan_values(void)
{
	learned_fans_available = env_get("learned_fans");
}

/*
 * Learn the fans (available).
 */	
void learn_fan(void)
{
		uint16_t fan_available=0;
		uint8_t fan_count=0;
	
		ioport_set_pin_level(CFG_EN_12V_FAN, 1);
	
		for (uint8_t i=0; i<71; i++ ) //rise the pwm slowly from 30% to 100% (inverted!)
		{
			tc_set_compare_value(&tc_instance_pwm, TC_COMPARE_CAPTURE_CHANNEL_0, 70-i);
			delay_cycles_ms(50);
		}
		
		delay_cycles_ms(5000); //Wait 5 sec to guarantee that the fans are at full speed 

		get_fan_speed();
		
		while(ready_flag_get_fan_speed == 0); //wait until the fanspeed of all fans is measured
		
		for(uint8_t i=0; i<CFG_MAX_FAN_COUNT; i++) //Check which the Fans run with more than 300rpm
		{
			if(fantacho[i]>300)
			{
				fan_available |= (1<<i);
				fan_count++;
			}
			else
			{
				fan_available  &= ~(1<<i);
			}
		}
		
		printf("Measure fan tacho at max speed:\r\n");
		for(uint8_t i=0; i<CFG_MAX_FAN_COUNT; i++)
		{
			if((fan_available & 1<<i) == 1<<i)
			{
				printf("Tacho%d: %ld\r\n", i, fantacho[i]);
			}
		}
			
		env_set("max_speed_learned_fan1", (uint32_t)fantacho[0]);
		env_set("max_speed_learned_fan2", (uint32_t)fantacho[1]);
		env_set("max_speed_learned_fan3", (uint32_t)fantacho[2]);
		env_set("max_speed_learned_fan4", (uint32_t)fantacho[3]);
		env_set("learned_fans", (uint32_t)fan_available);
		
		tc_set_compare_value(&tc_instance_pwm, TC_COMPARE_CAPTURE_CHANNEL_0, 100-CFG_PWM_INITIAL_VALUE); //set the pwm to 30%		
}

/*
 * Initialize all fan specific infrastructure
 */
void fan_init(void)
{	
	pwm_frequency = env_get("pwm_frequency"); //take the pwm frequency from the env
	printf("Fan PWM frequency: %d\r\n", (int)pwm_frequency);
	pulses_per_rotation = env_get("pulses_per_rotation"); //take the pulses per rotation of the fan from the env
	printf("Fan pulses per rotation: %d\r\n", pulses_per_rotation);
	
	struct tc_config config_tc_fan_pwm;
	tc_get_config_defaults(&config_tc_fan_pwm);
	config_tc_fan_pwm.counter_size = TC_COUNTER_SIZE_8BIT;
	
	switch(pwm_frequency)
	{
		case 80000: config_tc_fan_pwm.clock_prescaler = TC_CLOCK_PRESCALER_DIV1; 	break;
		case 40000:	config_tc_fan_pwm.clock_prescaler = TC_CLOCK_PRESCALER_DIV2; 	break;
		case 20000:	config_tc_fan_pwm.clock_prescaler = TC_CLOCK_PRESCALER_DIV4; 	break;
		case 10000:	config_tc_fan_pwm.clock_prescaler = TC_CLOCK_PRESCALER_DIV8; 	break;
		case 5000:	config_tc_fan_pwm.clock_prescaler = TC_CLOCK_PRESCALER_DIV16; 	break;
		case 1250:	config_tc_fan_pwm.clock_prescaler = TC_CLOCK_PRESCALER_DIV64; 	break;
		case 312:	config_tc_fan_pwm.clock_prescaler = TC_CLOCK_PRESCALER_DIV256; 	break;
		case 78:	config_tc_fan_pwm.clock_prescaler = TC_CLOCK_PRESCALER_DIV1024;	break;
		default:	config_tc_fan_pwm.clock_prescaler = TC_CLOCK_PRESCALER_DIV64;	break;
	}
	
	config_tc_fan_pwm.clock_source = GCLK_GENERATOR_1;
	config_tc_fan_pwm.wave_generation = TC_WAVE_GENERATION_NORMAL_PWM;
	config_tc_fan_pwm.counter_8_bit.value = 0;
	config_tc_fan_pwm.counter_8_bit.compare_capture_channel[0] = 100-CFG_PWM_INITIAL_VALUE;
	config_tc_fan_pwm.counter_8_bit.period = 100;
	config_tc_fan_pwm.pwm_channel[0].enabled = true;
	config_tc_fan_pwm.pwm_channel[0].pin_out = CFG_PWM1_PIN;
	config_tc_fan_pwm.pwm_channel[0].pin_mux = CFG_PWM1_MUX;
	tc_init(&tc_instance_pwm, CFG_PWM_MODULE, &config_tc_fan_pwm);
	tc_enable(&tc_instance_pwm);
		
	struct tc_config config_tc_tacho;
	tc_get_config_defaults(&config_tc_tacho);
	config_tc_tacho.counter_size = TC_COUNTER_SIZE_16BIT;
	config_tc_tacho.clock_source = GCLK_GENERATOR_1;
	config_tc_tacho.clock_prescaler = TC_CLOCK_PRESCALER_DIV256;
	config_tc_tacho.counter_16_bit.value = 0;
	config_tc_tacho.counter_16_bit.compare_capture_channel[TC_COMPARE_CAPTURE_CHANNEL_0] = 15625; //500ms
	tc_init(&tc_instance_tacho, CFG_TACHO_MODULE, &config_tc_tacho);
	tc_enable(&tc_instance_tacho);
	tc_register_callback(&tc_instance_tacho, tc_callback_timer1, TC_CALLBACK_CC_CHANNEL0);
	tc_enable_callback(&tc_instance_tacho, TC_CALLBACK_CC_CHANNEL0);
	tc_stop_counter(&tc_instance_tacho);
	
	struct extint_chan_conf config_extint_0;
	extint_chan_get_config_defaults(&config_extint_0);
	config_extint_0.gpio_pin           = CFG_INT0_PIN_FAN1;
	config_extint_0.gpio_pin_mux       = CFG_INT0_MUX_FAN1;
	config_extint_0.gpio_pin_pull      = EXTINT_PULL_UP;
	config_extint_0.detection_criteria = EXTINT_DETECT_RISING;
	extint_chan_set_config(0, &config_extint_0);
	
	struct extint_chan_conf config_extint_1;
	extint_chan_get_config_defaults(&config_extint_1);
	config_extint_1.gpio_pin           = CFG_INT1_PIN_FAN2;
	config_extint_1.gpio_pin_mux       = CFG_INT1_MUX_FAN2;
	config_extint_1.gpio_pin_pull      = EXTINT_PULL_UP;
	config_extint_1.detection_criteria = EXTINT_DETECT_RISING;
	extint_chan_set_config(1, &config_extint_1);
	
	struct extint_chan_conf config_extint_4;
	extint_chan_get_config_defaults(&config_extint_4);
	config_extint_4.gpio_pin           = CFG_INT4_PIN_FAN3;
	config_extint_4.gpio_pin_mux       = CFG_INT4_MUX_FAN3;
	config_extint_4.gpio_pin_pull      = EXTINT_PULL_UP;
	config_extint_4.detection_criteria = EXTINT_DETECT_RISING;
	extint_chan_set_config(4, &config_extint_4);
	
	struct extint_chan_conf config_extint_5;
	extint_chan_get_config_defaults(&config_extint_5);
	config_extint_5.gpio_pin           = CFG_INT5_PIN_FAN4;
	config_extint_5.gpio_pin_mux       = CFG_INT5_MUX_FAN4;
	config_extint_5.gpio_pin_pull      = EXTINT_PULL_UP;
	config_extint_5.detection_criteria = EXTINT_DETECT_RISING;
	extint_chan_set_config(5, &config_extint_5);
	
	
	ioport_set_pin_dir(CFG_FAN_MAX_SPEED, IOPORT_DIR_INPUT);
}

/*
 * Sync fan values to the SMBus
 */
static void fan_sync_to_smbus(void)
{
	smbus_set_input_reg(SMBUS_REG__FAN_TACHO_1_LOW_BYTE, fantacho[0] & 0xFF);
	smbus_set_input_reg(SMBUS_REG__FAN_TACHO_1_HIGH_BYTE, (fantacho[0]>>8) & 0xFF);
	smbus_set_input_reg(SMBUS_REG__FAN_TACHO_2_LOW_BYTE, fantacho[1] & 0xFF);
	smbus_set_input_reg(SMBUS_REG__FAN_TACHO_2_HIGH_BYTE, (fantacho[1]>>8) & 0xFF);
	smbus_set_input_reg(SMBUS_REG__FAN_TACHO_3_LOW_BYTE, fantacho[2] & 0xFF);
	smbus_set_input_reg(SMBUS_REG__FAN_TACHO_3_HIGH_BYTE, (fantacho[2]>>8) & 0xFF);
	smbus_set_input_reg(SMBUS_REG__FAN_TACHO_4_LOW_BYTE, fantacho[3] & 0xFF);
	smbus_set_input_reg(SMBUS_REG__FAN_TACHO_4_HIGH_BYTE, (fantacho[3]>>8) & 0xFF);
	smbus_set_input_reg(SMBUS_REG__MAX_SPEED, ioport_get_pin_level(CFG_FAN_MAX_SPEED));
	smbus_set_input_reg(SMBUS_REG__FAN_SPEED, current_pwm);
}


/*
 * Do the fan relevant functions
 */
void do_fan(void)
{
	uint32_t new_pwm_frequency;
	uint8_t new_pulses_per_rotation;
	
	if (get_jiffies() - last_pwm_adjust >= 100)
	{
		last_pwm_adjust = get_jiffies();
		pwm_calculation_autonomous_mode();
		set_pwm();
	}
	
	if (get_jiffies() - last_tacho_measure >= 2000) 
	{
		last_tacho_measure = get_jiffies();
		get_fan_speed();
		fan_sync_to_smbus();	
		check_fan_fail();
	}	
	
	new_pwm_frequency = env_get("pwm_frequency");
	if (new_pwm_frequency != pwm_frequency) 
	{
		printf("PWM: changing pwm frequency to %d\r\n", (int)new_pwm_frequency);
		pwm_frequency = new_pwm_frequency;
		tc_reset(&tc_instance_pwm);
		fan_init();
	}
	
	new_pulses_per_rotation = env_get("pulses_per_rotation");
	if (new_pulses_per_rotation != pulses_per_rotation) 
	{
		printf("Fan: changing pulses per rotation %d\r\n", new_pulses_per_rotation);
		pulses_per_rotation = new_pulses_per_rotation;
	}
}

#endif /* BOOTLOADER */