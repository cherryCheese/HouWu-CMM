/*
 * adc_measure.c
 *
 * Created: 16.11.2020 11:53:05
 *  Author: E1130513
 */ 

#include <asf.h>

#include "adc_measure.h"
#include "sys_timer.h"
#include "config.h"
#include "uart.h"
#include "env.h"
#include "smbus.h"


#ifndef BOOTLOADER

static struct adc_module adc_instance;
uint16_t adc_result_buffer[CFG_ADC_SAMPLES];
volatile bool adc_read_done = false;
static uint32_t temperature_1sec_timer;
static uint8_t temperature[4];
static uint16_t temperature_adc_value[4];
static float voltage[5];
static uint16_t voltage_adc_value[5];
static uint16_t learned_temps_available;
static uint8_t pwr_ok=0;

static void adc_complete_callback(struct adc_module *const module);
static uint16_t start_adc(char channel);
static float determine_temperature(float adc_value);
static void temperture_get_values(void);
static void check_temp_fail(void);
static void measure_sync_to_smbus(void);


/*
 * If all ADC conversions are complete (CFG_ADC_SAMPLES), the Routine is called
 */
static void adc_complete_callback(struct adc_module *const module)
{
	adc_read_done = true; //All conversions are done
}

/*
 * Configure the ADC Module
 * ADC_CLK = 8MHz/8
 * ADC Reference 2,5V
 * The ACD Channel Pin is choosed at the function start_adc
 */
void adc_measure_init(void)
{
	struct adc_config config_adc;
	adc_get_config_defaults(&config_adc);
	config_adc.clock_source = GCLK_GENERATOR_1;
	config_adc.gain_factor = ADC_GAIN_FACTOR_1X;
	config_adc.clock_prescaler = ADC_CLOCK_PRESCALER_DIV8;
	config_adc.reference = ADC_REFERENCE_AREFA;
	config_adc.positive_input = ADC_POSITIVE_INPUT_PIN2;
	config_adc.resolution = ADC_RESOLUTION_12BIT;
	adc_init(&adc_instance, ADC, &config_adc);
	adc_enable(&adc_instance);
	
	adc_register_callback(&adc_instance, adc_complete_callback, ADC_CALLBACK_READ_BUFFER);
	adc_enable_callback(&adc_instance, ADC_CALLBACK_READ_BUFFER);
}


/*
 * function parameter: channel = ADC channel(Pin)
 * set ADC channel
 * Start conversion and wait until all conversions are done(CFG_ADC_SAMPLES)
 * return the average of all CFG_ADC_SAMPLES
 */
static uint16_t start_adc(char channel)
{
	static uint32_t adc_result_average=0;
	adc_set_positive_input(&adc_instance, channel);
	adc_read_done = false;
	adc_read_buffer_job(&adc_instance, adc_result_buffer, CFG_ADC_SAMPLES);
	while (adc_read_done == false) //wait until all conversions are done
	{
	}
	adc_result_average = 0;
	for(int i=0; i<CFG_ADC_SAMPLES; i++)
	{
		adc_result_average += adc_result_buffer[i];
	}
	adc_result_average = adc_result_average/CFG_ADC_SAMPLES; //determine the average of all conversions
	return (uint16_t) adc_result_average;
}

/*
 * Determine the temperature with a polynomial
 */
static float determine_temperature(float adc_value)
{
	float value;
	
	value = (4.7666*adc_value*adc_value*adc_value*adc_value*adc_value*adc_value)-(47.5107*adc_value*adc_value*adc_value*adc_value*adc_value)+(184.6817*adc_value*adc_value*adc_value*adc_value)-(370.3281*adc_value*adc_value*adc_value)+(415.0838*adc_value*adc_value)-(290.5634*adc_value)+154.4484; //NTC
	
	if(value < 0)
	{
		return 0;
	}
	else
	{
		return value;
	}
}

/*
 * Check whether the PXIe voltages are between the ATX specification
 */
void check_voltage_ok(void)
{
	if((CFG_REFERENCE_3V3_MIN < voltage[0]) && (voltage[0] < CFG_REFERENCE_3V3_MAX))
	{
		pwr_ok |= (1<<0);
	}
	else
	{
		pwr_ok &= ~(1<<0);
	}
	
	if((CFG_REFERENCE_5V_MIN < voltage[1]) && (voltage[1] < CFG_REFERENCE_5V_MAX))
	{
		pwr_ok |= (1<<1);
	}
	else
	{
		pwr_ok &= ~(1<<1);
	}
	
	if((CFG_REFERENCE_12V_MIN < voltage[3]) && (voltage[3] < CFG_REFERENCE_12V_MAX))
	{
		pwr_ok |= (1<<2);
	}
	else
	{
		pwr_ok &= ~(1<<2);
	}
}

/*
 * return the pwr_ok determined by check_voltage_ok()
 */
uint8_t read_pwr_ok (void)
{
	return pwr_ok;
}

/*
 * Measure the temperatures
 */
static void temperture_get_values(void)
{	
	temperature_adc_value[0] = start_adc(CFG_ADC_CHANNEL_TEMP_IN);
	temperature[0] = (uint8_t) determine_temperature(((float)2.5*temperature_adc_value[0])/4096);
	
	temperature_adc_value[1] = start_adc(CFG_ADC_CHANNEL_TEMP_OUT1);
	temperature[1] = (uint8_t) determine_temperature(((float)2.5*temperature_adc_value[1])/4096);
	
	temperature_adc_value[2] = start_adc(CFG_ADC_CHANNEL_TEMP_OUT2);
	temperature[2] = (uint8_t) determine_temperature(((float)2.5*temperature_adc_value[2])/4096);
	
	temperature_adc_value[3] = start_adc(CFG_ADC_CHANNEL_TEMP_OUT3);
	temperature[3] = (uint8_t) determine_temperature(((float)2.5*temperature_adc_value[3])/4096);
}

/*
 * Measure the PXIe voltages
 */
void voltages_get_values(void)
{	
	voltage_adc_value[0] = start_adc(CFG_ADC_CHANNEL_3V3);
	voltage[0] = 2 * 2.5 * (((float)voltage_adc_value[0])/4096);
	
	voltage_adc_value[1] = start_adc(CFG_ADC_CHANNEL_5V);
	voltage[1] = 3 * 2.5 * (((float)voltage_adc_value[1])/4096);
	
	voltage_adc_value[2] = start_adc(CFG_ADC_CHANNEL_5VAUX);
	voltage[2] = 3 * 2.5 * (((float)voltage_adc_value[2])/4096);
	
	voltage_adc_value[3] = start_adc(CFG_ADC_CHANNEL_12V);
	voltage[3] = 6 * 2.5 * (((float)voltage_adc_value[3])/4096);
	
	voltage_adc_value[4] = start_adc(CFG_ADC_CHANNEL_M12V);
	voltage[4] = 6 * 2.5 * (((float)voltage_adc_value[4])/4096)+0.29; //Add +0,29 because of the offset of the OP-Amplifier 
}

/*
 * Check, whether temperatures failed
 */
static void check_temp_fail(void)
{
	uint8_t alarm_threshold_out=0;
	uint8_t fan_curve=0;
	uint8_t temperature_fail=0;
	
	fan_curve = smbus_get_input_reg(SMBUS_REG__FAN_CURVE);
		
	if(fan_curve<4) 
	alarm_threshold_out = 45;
	if((fan_curve>3) && (fan_curve<8))
	alarm_threshold_out = 55;
	if((fan_curve>7) && (fan_curve<12))
	alarm_threshold_out = 65;
	if(fan_curve>11)
	alarm_threshold_out = 75;
	
	for(int i=0; i<4; i++)
	{		
		if((learned_temps_available & (1<<i)) == (1<<i))	
		{
			if(i==0)
			{
				if((temperature_adc_value[i] > 3800) || (smbus_get_input_reg(SMBUS_REG__TEMP_AIR_INLET)>55))
				{
					temperature_fail |= (1<<i);
				}
				else
				{
					temperature_fail &= ~(1<<i);
				}
			}
			else
			{
				if((temperature_adc_value[i] > 3800) || ((smbus_get_input_reg(SMBUS_REG__TEMP_AIR_INLET+i) > alarm_threshold_out) && (smbus_get_input_reg(SMBUS_REG__REMOTE)==0)))
				{
					temperature_fail |= (1<<i);
				}
				else
				{
					temperature_fail &= ~(1<<i);
				}
			}
		}	
	}
	smbus_set_input_reg(SMBUS_REG__TEMP_FAIL, temperature_fail);
}


/*
 * Load the temperature values from the learn mode
 */
void load_learned_temp_values(void)
{
	learned_temps_available = env_get("learned_temperature_sensors");
}

/*
 * Learn the temperatures
 */
void learn_temp(void)
{
	uint8_t temp_available=0;
	uint8_t temp_count=0;
	
	temperture_get_values();
	
	for(uint8_t i=0; i<4; i++)
	{
		if(temperature_adc_value[i] < 3800)
		{
			temp_available |= (1<<i);
			temp_count++;
		}
		else
		{
			temp_available &= ~(1<<i);
		}
	}
	
	printf("\r\nMeasure temperatures:\r\n");
	for(uint8_t i=0; i<4; i++)
	{
		if((temp_available & 1<<i) == 1<<i)
		{
			printf("Temp%d: %d\r\n", i, temperature[i]);
		}
	}
	
	env_set("learned_temperature_sensors", (uint32_t)temp_available);
	env_set("temperature_learned_sensor1", (uint32_t)temperature[0]);
	env_set("temperature_learned_sensor2", (uint32_t)temperature[1]);
	env_set("temperature_learned_sensor3", (uint32_t)temperature[2]);
	env_set("temperature_learned_sensor4", (uint32_t)temperature[3]);
}

/*
 * Sync temperatures and voltages to the SMBus
 */
static void measure_sync_to_smbus(void)
{
	smbus_set_input_reg(SMBUS_REG__TEMP_AIR_INLET, temperature[0]);
	smbus_set_input_reg(SMBUS_REG__TEMP_AIR_OUTLET1, temperature[1]);
	smbus_set_input_reg(SMBUS_REG__TEMP_AIR_OUTLET2, temperature[2]);
	smbus_set_input_reg(SMBUS_REG__TEMP_AIR_OUTLET3, temperature[3]);
	
	smbus_set_input_reg(SMBUS_REG__3V3_LOW_BYTE, ((uint32_t)(1000*voltage[0])) & 0xFF);
	smbus_set_input_reg(SMBUS_REG__3V3_HIGH_BYTE, (((uint32_t)(1000*voltage[0]))>>8) & 0xFF);
	smbus_set_input_reg(SMBUS_REG__5V_LOW_BYTE, ((uint32_t)(1000*voltage[1])) & 0xFF);
	smbus_set_input_reg(SMBUS_REG__5V_HIGH_BYTE, (((uint32_t)(1000*voltage[1]))>>8) & 0xFF);
	smbus_set_input_reg(SMBUS_REG__5VAUX_LOW_BYTE, ((uint32_t)(1000*voltage[2])) & 0xFF);
	smbus_set_input_reg(SMBUS_REG__5VAUX_HIGH_BYTE, (((uint32_t)(1000*voltage[2]))>>8) & 0xFF);
	smbus_set_input_reg(SMBUS_REG__12V_LOW_BYTE, ((uint32_t)(1000*voltage[3])) & 0xFF);
	smbus_set_input_reg(SMBUS_REG__12V_HIGH_BYTE, (((uint32_t)(1000*voltage[3]))>>8) & 0xFF);
	smbus_set_input_reg(SMBUS_REG__M12V_LOW_BYTE, ((uint32_t)(1000*voltage[4])) & 0xFF);
	smbus_set_input_reg(SMBUS_REG__M12V_HIGH_BYTE, (((uint32_t)(1000*voltage[4]))>>8) & 0xFF);
}

/*
 * Measure, check, sync the temperature and voltage every second
 */
void do_measure(void)
{
	if (get_jiffies() - temperature_1sec_timer >= 1000) 
	{
		temperature_1sec_timer = get_jiffies();
		temperture_get_values();
		voltages_get_values();
		check_temp_fail();
		check_voltage_ok();
		measure_sync_to_smbus();
	}
}

#endif /* BOOTLOADER */