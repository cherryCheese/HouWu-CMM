/*
 * config.h
 *
 * Created: 8/14/2020 3:42:16 PM
 *  Author: E1210640
 */ 

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <asf.h>

/******************************************************************
 *              Firmware configuration file                       *
 ******************************************************************/

#if 0
#define CFG_DEBUG_ENABLE
#endif

#define CFG_FIRMWARE_NUMBER			"6399831451"
#define CFG_FIRMWARE_VERSION		1

/*
 * Starting address of the firmware in NVM (after bootloader).
 * Must match the ".text=" definition in the linker memory settings!!!
 */
#define CFG_FIRMWARE_START			0x4000

/* Fuses (determined via Atmel Studio->Tools->Device Programming->Fuses) */
#define CFG_FUSES_USER_WORD_0		0xD8E0C7AF
#define CFG_FUSES_USER_WORD_1		0xFFFF3F5D

#define CFG_WDT_TIMEOUT				3 /* seconds */

/* debugging */
#define CFG_TEST_SIGNAL				PIN_PA17

/* LEDs */
#define CFG_HEARTBEAT_LED			PIN_PB11
#define CFG_LED_RED					PIN_PB31
#define CFG_LED_GRN					PIN_PA14

/* Config Dip Switches */
#define CFG_DIP1_PS_ON_LOGIC        PIN_PB22
#define CFG_DIP2_AC_OK		        PIN_PB23
#define CFG_DIP3					PIN_PA27
#define CFG_DIP4_LEARN              PIN_PA28

/* Power Management Signals */
#define CFG_PWR_OK_UC_N				PIN_PB30
#define CFG_SEL_SS_PS_ON			PIN_PA00
#define CFG_EN_12V_FAN				PIN_PB04
#define CFG_EXT_PS_ON_IN			PIN_PA11
#define CFG_SS_PS_ON_IN				PIN_PB10
#define CFG_PS_ON_OUT_1_5V_N		PIN_PA13
#define CFG_PS_ON_OUT_2_12V_N		PIN_PA12
#define CFG_PS_ON_OUT_3_3V3_N		PIN_PB15
#define CFG_PS_ON_OUT_4_M12V_N		PIN_PB14
#define CFG_AC_OK_IN				PIN_PA15

#define	CFG_AC_OK_IN_INT		  PIN_PA15A_EIC_EXTINT15
#define	CFG_AC_OK_IN_MUX		  MUX_PA15A_EIC_EXTINT15

#define	CFG_SS_PS_ON_IN_INT		  PIN_PB10A_EIC_EXTINT10
#define	CFG_SS_PS_ON_IN_MUX		  MUX_PB10A_EIC_EXTINT10

#define	CFG_EXT_PS_ON_IN_INT	  PIN_PA11A_EIC_EXTINT11
#define	CFG_EXT_PS_ON_IN_MUX	  MUX_PA11A_EIC_EXTINT11

/* Fan configuration */
#define CFG_PWM_MODULE					TC1
#define CFG_PWM_FREQUENCY				1250
#define CFG_PWM1_PIN					PIN_PA10E_TC1_WO0
#define CFG_PWM1_MUX					MUX_PA10E_TC1_WO0
#define CFG_MAX_FAN_COUNT				6
#define CFG_TACHO_MODULE				TC4
#define	CFG_INT0_PIN_FAN1				PIN_PB16A_EIC_EXTINT0
#define	CFG_INT0_MUX_FAN1				MUX_PB16A_EIC_EXTINT0
#define	CFG_INT1_PIN_FAN2				PIN_PB17A_EIC_EXTINT1
#define	CFG_INT1_MUX_FAN2				MUX_PB17A_EIC_EXTINT1
#define	CFG_INT4_PIN_FAN3				PIN_PA20A_EIC_EXTINT4
#define	CFG_INT4_MUX_FAN3				MUX_PA20A_EIC_EXTINT4
#define	CFG_INT5_PIN_FAN4				PIN_PA21A_EIC_EXTINT5
#define	CFG_INT5_MUX_FAN4				MUX_PA21A_EIC_EXTINT5
#ifdef SIX_FANs
#define	CFG_INT2_PIN_FAN5				PIN_PA25A_EIC_EXTINT13
#define	CFG_INT2_MUX_FAN5				MUX_PA25A_EIC_EXTINT13
#define	CFG_INT3_PIN_FAN6				PIN_PA24A_EIC_EXTINT12
#define	CFG_INT3_MUX_FAN6				MUX_PA24A_EIC_EXTINT12
#endif
#define CFG_PULSES_PER_ROTATION			2
#define CFG_PWM_INITIAL_VALUE			0
#define CFG_PWM_SPIN_UP_VALUE			30  //PWM duty cycle while spin up the fans in %
#define CFG_PWM_SPIN_UP_DELAY			100  //(value x 100msec). Delay PWM while spin up the fans.
#define CFG_PWM_CHANGE_DELAY			2	//100msec+(value x 100msec). Delay for change the PWM in % steps.
#define CFG_MAX_PWM						100
#define CFG_MIN_PWM 					20

/* adc_measure configuration */
#define CFG_ADC_SAMPLES					20 //number of ADC Conversions used for averaging the adc value result
#define CFG_ADC_CHANNEL_TEMP_IN			2
#define CFG_ADC_CHANNEL_TEMP_OUT1		15
#define CFG_ADC_CHANNEL_TEMP_OUT2		14
#define CFG_ADC_CHANNEL_TEMP_OUT3		13
#define CFG_ADC_CHANNEL_5V				8
#define CFG_ADC_CHANNEL_5VAUX			9
#define CFG_ADC_CHANNEL_12V				10
#define CFG_ADC_CHANNEL_M12V			11
#define CFG_ADC_CHANNEL_3V3				3
#define CFG_REFERENCE_3V3_MIN			2.97f
#define CFG_REFERENCE_3V3_MAX			3.63f
#define CFG_REFERENCE_5V_MIN			4.5f
#define CFG_REFERENCE_5V_MAX			5.5f
#define CFG_REFERENCE_12V_MIN			10.8f
#define CFG_REFERENCE_12V_MAX			13.2f

/* EEPROM emulation */
#define CFG_EEPROM_ENABLE
#define CFG_EEPROM_BOD33_LEVEL		39						/* Brown-out level: 2.84V */
#define CFG_EEPROM_PN_OFFSET		(0*EEPROM_PAGE_SIZE)	/* Part/serial numbers in page 0 */
#define CFG_EEPROM_HOLDING_OFFSET	(1*EEPROM_PAGE_SIZE)	/* Holding registers in page 1 */
#define CFG_EEPROM_ENV_OFFSET		(2*EEPROM_PAGE_SIZE)	/* Environment variables in page 2+ */

/*
 * UART/console configuration:
 *
 * CFG_UART_CHANNEL(channel, SERCOMx, baud_rate, parity, mux_setting, pinmux_pad0, pinmux_pad1, pinmux_pad2, pinmux_pad3)
 */
#define CFG_UART_RING_SIZE			1024
#define CFG_UART_CHANNELS			CFG_UART_CHANNEL(0, SERCOM1, CFG_CONSOLE_BAUD_RATE, USART_PARITY_NONE, USART_RX_3_TX_2_XCK_3, PINMUX_UNUSED, PINMUX_UNUSED, PINMUX_PA18D_SERCOM3_PAD2, PINMUX_PA19D_SERCOM3_PAD3)

#define CFG_CONSOLE_CHANNEL			0
#define CFG_CONSOLE_BAUD_RATE		115200


/* SPI Flash configuration */
#define CFG_SPI_FLASH_SS_PIN		PIN_PA05
#define CFG_SPI_FLASH_MUX_SETTING	SPI_SIGNAL_MUX_SETTING_E
#define CFG_SPI_FLASH_BAUDRATE		100000
#define CFG_SPI_FLASH_PINMUX_PAD0	PINMUX_PA04D_SERCOM0_PAD0
#define CFG_SPI_FLASH_PINMUX_PAD1	PINMUX_PA05D_SERCOM0_PAD1
#define CFG_SPI_FLASH_PINMUX_PAD2	PINMUX_PA06D_SERCOM0_PAD2
#define CFG_SPI_FLASH_PINMUX_PAD3	PINMUX_PA07D_SERCOM0_PAD3

/* I2C configuration */
#define CFG_I2C_SLAVE_MODULE		SERCOM2
#define CFG_I2C_SLAVE_ADDRESS		0x58				/* 8-bit address */
#define CFG_I2C_SLAVE_PINMUX_PAD0	PINMUX_PA08D_SERCOM2_PAD0
#define CFG_I2C_SLAVE_PINMUX_PAD1	PINMUX_PA09D_SERCOM2_PAD1

/* Additional SMBus configuration */
#define CFG_SMBUS_TIMEOUT_ENABLE	1
#define CFG_SMBUS_TIMEOUT_CLOCK_GEN	GCLK_GENERATOR_7	/* Clock generator to be used for the SERCOMx_SLOW clock */

/* I2C Master configuration */
#define CFG_I2C_MASTER_MODULE		SERCOM4
#define CFG_I2C_MASTER_PINMUX_PAD0	PINMUX_PB12C_SERCOM4_PAD0
#define CFG_I2C_MASTER_PINMUX_PAD1	PINMUX_PB13C_SERCOM4_PAD1
#define CFG_I2C_MASTER_TIMEOUT		50
#define CFG_I2C_MASTER_TB1_ADDRESS	0x20
#define CFG_I2C_MASTER_TB2_ADDRESS	0x21
#define CFG_I2C_MASTER_TB3_ADDRESS	0x22
#define CFG_I2C_MASTER_TB4_ADDRESS	0x23
#define CFG_I2C_MASTER_CLK_ADDRESS	0x54
#define CFG_I2C_MASTER_INA_3V3		0x45
#define CFG_I2C_MASTER_INA_5V		0x41
#define CFG_I2C_MASTER_INA_12V		0x44
#define CFG_I2C_MASTER_LM75_1		0x48
#define CFG_I2C_MASTER_LM75_2		0x49
#define CFG_I2C_MASTER_EEPROM		0x52
#define CFG_I2C_MASTER_PCA			0x27

/* PIN defines*/
#define CFG_ADD1					PIN_PA01
#define CFG_FAN_MAX_SPEED			PIN_PA02
#define CFG_FAN_PWM					PIN_PA10

/*
 * Non-volatile (persistent) configuration parameters:
 *
 * CFG_ENV_DESC(name, default_value)
 */
#define CFG_ENV_DESCRIPTORS			CFG_ENV_DESC("pulses_per_rotation", CFG_PULSES_PER_ROTATION) \
									CFG_ENV_DESC("pwm_frequency", CFG_PWM_FREQUENCY) \
									CFG_ENV_DESC("max_speed_learned_fan1", 0) \
									CFG_ENV_DESC("max_speed_learned_fan2", 0) \
									CFG_ENV_DESC("max_speed_learned_fan3", 0) \
									CFG_ENV_DESC("max_speed_learned_fan4", 0) \
									CFG_ENV_DESC("max_speed_learned_fan5", 0) \
									CFG_ENV_DESC("max_speed_learned_fan6", 0) \
									CFG_ENV_DESC("learned_fans", 0) \
									CFG_ENV_DESC("temperature_learned_sensor1", 0) \
									CFG_ENV_DESC("temperature_learned_sensor2", 0) \
									CFG_ENV_DESC("temperature_learned_sensor3", 0) \
									CFG_ENV_DESC("temperature_learned_sensor4", 0) \
									CFG_ENV_DESC("learned_temperature_sensors", 0) \
									CFG_ENV_DESC("fan_curve", 5) \
									CFG_ENV_DESC("tb1en", 0) \
									CFG_ENV_DESC("tb1dir", 0) \
									CFG_ENV_DESC("tb2en", 0) \
									CFG_ENV_DESC("tb2dir", 0) \
									CFG_ENV_DESC("tb3en", 0) \
									CFG_ENV_DESC("tb3dir", 0) \
									CFG_ENV_DESC("tb4en", 0) \
									CFG_ENV_DESC("tb4dir", 0) \
									CFG_ENV_DESC("learned", 0)



#endif /* __CONFIG_H__ */