/*
 * smbus.c
 *
 * Created: 4/29/2021 6:42:47 PM
 *  Author: E1210640
 */ 

#include <asf.h>
#include <stdio.h>
#include <string.h>



#include "config.h"
#include "smbus.h"
#include "upgrade.h"
#include "crc.h"
#include "sys_timer.h"
#include "debug.h"
#include "env.h"


#ifndef BOOTLOADER

#define PEC_POLYNOMIAL				0x07

static struct i2c_slave_module i2c_slave_instance;
static uint8_t smbus_data_regs[256];	/* SMBus Registers*/
static uint8_t i2c_tx_buf[256];		/* Transmit buffer */
static uint8_t i2c_rx_buf[256];		/* Receive buffer */
static uint8_t i2c_tx_len;			/* Data length for transmission */
static uint8_t cmd_buf[256];		/* Command data buffer for deferred processing */
static uint8_t cmd_len;				/* Current command+data length */
static uint8_t smbus_status;		/* Device status */
static uint8_t current_pec;			/* Current PEC value */
static uint32_t activation_start;	/* Activation start time */


uint8_t smbus_get_input_reg(uint8_t nr)
{
	uint8_t ret;
	system_interrupt_enter_critical_section();
	ret = smbus_data_regs[nr];
	system_interrupt_leave_critical_section();
	
	return ret;
}

void smbus_set_input_reg(uint8_t nr, uint8_t val)
{
	system_interrupt_enter_critical_section();
	smbus_data_regs[nr] = val;
	system_interrupt_leave_critical_section();
}

static void smbus_set_status_bit(uint8_t new_status)
{
	/* Protect against the read callback to avoid simultaneous access */
	system_interrupt_enter_critical_section();
	smbus_status |= new_status;
	system_interrupt_leave_critical_section();
}

static void smbus_clear_status_bit(uint8_t new_status)
{
	/* Protect against the read callback to avoid simultaneous access */
	system_interrupt_enter_critical_section();
	smbus_status &= ~new_status;
	system_interrupt_leave_critical_section();
}

/* Initialize the PEC byte (must be called at the start of a transaction) */
static uint8_t smbus_pec_init(uint8_t *buf, int len)
{
	current_pec = crc8(0xff, buf, len, PEC_POLYNOMIAL, 0);
	return current_pec;
}

/* Adjust the PEC byte with more transaction data */
static uint8_t smbus_pec_adjust(uint8_t *buf, int len, int finish)
{
	current_pec = crc8(current_pec, buf, len, PEC_POLYNOMIAL, finish);
	return current_pec;
}

/* Verify the PEC byte for a write command */
static int smbus_pec_verify(int len, int expected_len)
{
	if (len == expected_len + 1 && current_pec) {
		printf("SMBUS UPGRADE: PEC mismatch\r\n");
		smbus_set_status_bit(SMBUS_STATUS_PEC_ERROR);
		return -1;
	} else {
		smbus_clear_status_bit(SMBUS_STATUS_PEC_ERROR);
	}
	return 0;
}

/* SMBus command processing functions */
static void smbus_process_read(uint8_t cmd)
{
	
	switch(cmd) {
		case SMBUS_CMD_GET_STATUS:
			i2c_tx_buf[0] = smbus_status;
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__5VAUX_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__5VAUX_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__5VAUX_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
			
		case SMBUS_REG__3V3_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__3V3_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__3V3_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
			
		case SMBUS_REG__5V_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__5V_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__5V_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
			
		case SMBUS_REG__12V_LOW_BYTE:
			
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__12V_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__12V_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
			
		case SMBUS_REG__M12V_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__M12V_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__M12V_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
			
		case SMBUS_REG__SEL_SS_PS_ON:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__SEL_SS_PS_ON];
			i2c_tx_len = 1;
			break;
			
		case SMBUS_REG__SS_PS_ON_IN:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__SS_PS_ON_IN];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__EXT_PS_ON_IN:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__EXT_PS_ON_IN];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__PS_ON_OUT_1:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__PS_ON_OUT_1];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__PS_ON_OUT_2:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__PS_ON_OUT_2];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__PS_ON_OUT_3:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__PS_ON_OUT_3];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__PS_ON_OUT_4:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__PS_ON_OUT_4];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__AC_OK:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__AC_OK];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__PWR_OK:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__PWR_OK];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__REMOTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__REMOTE];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__SET_FAN:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__SET_FAN];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__FAN_CURVE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__FAN_CURVE];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__FAN_TACHO_1_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__FAN_TACHO_1_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__FAN_TACHO_1_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
		
		case SMBUS_REG__FAN_TACHO_2_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__FAN_TACHO_2_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__FAN_TACHO_2_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
		
		case SMBUS_REG__FAN_TACHO_3_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__FAN_TACHO_3_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__FAN_TACHO_3_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
		
		case SMBUS_REG__FAN_TACHO_4_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__FAN_TACHO_4_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__FAN_TACHO_4_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
		
		case SMBUS_REG__FAN_TACHO_5_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__FAN_TACHO_5_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__FAN_TACHO_5_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
		
		case SMBUS_REG__FAN_TACHO_6_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__FAN_TACHO_6_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__FAN_TACHO_6_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
		
		case SMBUS_REG__FAN_UNIT_READY:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__FAN_UNIT_READY];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__FAN_FAIL:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__FAN_FAIL];
			i2c_tx_len = 1;
			break;
			
		case SMBUS_REG__FAN_SPEED:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__FAN_SPEED];
			i2c_tx_len = 1;
			break;
			
		case SMBUS_REG__TEMP_AIR_INLET:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__TEMP_AIR_INLET];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__TEMP_AIR_OUTLET1:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__TEMP_AIR_OUTLET1];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__TEMP_AIR_OUTLET2:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__TEMP_AIR_OUTLET2];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__TEMP_AIR_OUTLET3:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__TEMP_AIR_OUTLET3];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__TEMP_AIR_OUTLET4:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__TEMP_AIR_OUTLET4];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__TEMP_FAIL:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__TEMP_FAIL];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__TBPRES:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__TBPRES];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__TB1_EN:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__TB1_EN];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__TB1_DIR:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__TB1_DIR];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__TB2_EN:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__TB2_EN];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__TB2_DIR:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__TB2_DIR];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__TB3_EN:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__TB3_EN];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__TB3_DIR:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__TB3_DIR];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__TB4_EN:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__TB4_EN];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__TB4_DIR:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__TB4_DIR];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__ADD_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__ADD_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__ADD_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
		
		case SMBUS_REG__DATA:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__DATA];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__WRITE_DATA:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__WRITE_DATA];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__CLOCKMODUL_PRESENT:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__CLOCKMODUL_PRESENT];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__SYNC100_DIV:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__SYNC100_DIV];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__CLOCK_MODULE_FW_BYTE_1:
			i2c_tx_buf[0] = 10;
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__CLOCK_MODULE_FW_BYTE_1];
			i2c_tx_buf[2] = smbus_data_regs[SMBUS_REG__CLOCK_MODULE_FW_BYTE_2];
			i2c_tx_buf[3] = smbus_data_regs[SMBUS_REG__CLOCK_MODULE_FW_BYTE_3];
			i2c_tx_buf[4] = smbus_data_regs[SMBUS_REG__CLOCK_MODULE_FW_BYTE_4];
			i2c_tx_buf[5] = smbus_data_regs[SMBUS_REG__CLOCK_MODULE_FW_BYTE_5];
			i2c_tx_buf[6] = smbus_data_regs[SMBUS_REG__CLOCK_MODULE_FW_BYTE_6];
			i2c_tx_buf[7] = smbus_data_regs[SMBUS_REG__CLOCK_MODULE_FW_BYTE_7];
			i2c_tx_buf[8] = smbus_data_regs[SMBUS_REG__CLOCK_MODULE_FW_BYTE_8];
			i2c_tx_buf[9] = smbus_data_regs[SMBUS_REG__CLOCK_MODULE_FW_BYTE_9];
			i2c_tx_buf[10] = smbus_data_regs[SMBUS_REG__CLOCK_MODULE_FW_BYTE_10];
			i2c_tx_len = 11;
			break;
		
		case SMBUS_REG__CONFIG:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__CONFIG];
			i2c_tx_len = 1;
			break;
		
		case SMBUS_REG__MAX_SPEED:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__MAX_SPEED];
			i2c_tx_len = 1;
			break;
			
		case SMBUS_REG__CMM_FW_BYTE_1:
			i2c_tx_buf[0] = 10;
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__CMM_FW_BYTE_1];
			i2c_tx_buf[2] = smbus_data_regs[SMBUS_REG__CMM_FW_BYTE_2];
			i2c_tx_buf[3] = smbus_data_regs[SMBUS_REG__CMM_FW_BYTE_3];
			i2c_tx_buf[4] = smbus_data_regs[SMBUS_REG__CMM_FW_BYTE_4];
			i2c_tx_buf[5] = smbus_data_regs[SMBUS_REG__CMM_FW_BYTE_5];
			i2c_tx_buf[6] = smbus_data_regs[SMBUS_REG__CMM_FW_BYTE_6];
			i2c_tx_buf[7] = smbus_data_regs[SMBUS_REG__CMM_FW_BYTE_7];
			i2c_tx_buf[8] = smbus_data_regs[SMBUS_REG__CMM_FW_BYTE_8];
			i2c_tx_buf[9] = smbus_data_regs[SMBUS_REG__CMM_FW_BYTE_9];
			i2c_tx_buf[10] = smbus_data_regs[SMBUS_REG__CMM_FW_BYTE_10];	
			i2c_tx_len = 11;
			break;
			
		case SMBUS_REG__CMM_VERSION:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__CMM_VERSION];
			i2c_tx_len = 1;
			break;
			
		case SMBUS_REG__CMM_PDB_POWER_3V3_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__CMM_PDB_POWER_3V3_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__CMM_PDB_POWER_3V3_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
			
		case SMBUS_REG__CMM_PDB_MAX_POWER_3V3_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__CMM_PDB_MAX_POWER_3V3_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__CMM_PDB_MAX_POWER_3V3_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
			
		case SMBUS_REG__CMM_PDB_POWER_5V_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__CMM_PDB_POWER_5V_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__CMM_PDB_POWER_5V_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
			
		case SMBUS_REG__CMM_PDB_MAX_POWER_5V_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__CMM_PDB_MAX_POWER_5V_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__CMM_PDB_MAX_POWER_5V_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
			
		case SMBUS_REG__CMM_PDB_POWER_12V_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__CMM_PDB_POWER_12V_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__CMM_PDB_POWER_12V_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
		
		case SMBUS_REG__CMM_PDB_MAX_POWER_12V_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__CMM_PDB_MAX_POWER_12V_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__CMM_PDB_MAX_POWER_12V_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
		
		case SMBUS_REG__CMM_PDB_MAX_POWER_TOTAL_LOW_BYTE:
			i2c_tx_buf[0] = smbus_data_regs[SMBUS_REG__CMM_PDB_MAX_POWER_TOTAL_LOW_BYTE];
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__CMM_PDB_MAX_POWER_TOTAL_HIGH_BYTE];
			i2c_tx_len = 2;
			break;
		
		case SMBUS_REG__CMM_PDB_PRODUCT_NUM_Byte_1:	
			i2c_tx_buf[0] = 8;
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__CMM_PDB_PRODUCT_NUM_Byte_1];
			i2c_tx_buf[2] = smbus_data_regs[SMBUS_REG__CMM_PDB_PRODUCT_NUM_Byte_2];
			i2c_tx_buf[3] = smbus_data_regs[SMBUS_REG__CMM_PDB_PRODUCT_NUM_Byte_3];
			i2c_tx_buf[4] = smbus_data_regs[SMBUS_REG__CMM_PDB_PRODUCT_NUM_Byte_4];
			i2c_tx_buf[5] = smbus_data_regs[SMBUS_REG__CMM_PDB_PRODUCT_NUM_Byte_5];
			i2c_tx_buf[6] = smbus_data_regs[SMBUS_REG__CMM_PDB_PRODUCT_NUM_Byte_6];
			i2c_tx_buf[7] = smbus_data_regs[SMBUS_REG__CMM_PDB_PRODUCT_NUM_Byte_7];
			i2c_tx_buf[8] = smbus_data_regs[SMBUS_REG__CMM_PDB_PRODUCT_NUM_Byte_8];
			i2c_tx_len = 9;
		break;
		
		case SMBUS_REG__CMM_PDB_SERIAL_NUM_Byte_1:		
			i2c_tx_buf[0] = 12;
			i2c_tx_buf[1] = smbus_data_regs[SMBUS_REG__CMM_PDB_SERIAL_NUM_Byte_1];
			i2c_tx_buf[2] = smbus_data_regs[SMBUS_REG__CMM_PDB_SERIAL_NUM_Byte_2];
			i2c_tx_buf[3] = smbus_data_regs[SMBUS_REG__CMM_PDB_SERIAL_NUM_Byte_3];
			i2c_tx_buf[4] = smbus_data_regs[SMBUS_REG__CMM_PDB_SERIAL_NUM_Byte_4];
			i2c_tx_buf[5] = smbus_data_regs[SMBUS_REG__CMM_PDB_SERIAL_NUM_Byte_5];
			i2c_tx_buf[6] = smbus_data_regs[SMBUS_REG__CMM_PDB_SERIAL_NUM_Byte_6];
			i2c_tx_buf[7] = smbus_data_regs[SMBUS_REG__CMM_PDB_SERIAL_NUM_Byte_7];
			i2c_tx_buf[8] = smbus_data_regs[SMBUS_REG__CMM_PDB_SERIAL_NUM_Byte_8];
			i2c_tx_buf[9] = smbus_data_regs[SMBUS_REG__CMM_PDB_SERIAL_NUM_Byte_9];
			i2c_tx_buf[10] = smbus_data_regs[SMBUS_REG__CMM_PDB_SERIAL_NUM_Byte_10];
			i2c_tx_buf[11] = smbus_data_regs[SMBUS_REG__CMM_PDB_SERIAL_NUM_Byte_11];
			i2c_tx_buf[12] = smbus_data_regs[SMBUS_REG__CMM_PDB_SERIAL_NUM_Byte_12];
			i2c_tx_len = 13;
		break;
				
		/* TBD: add code for processing other read commands, if needed */
		default:
			i2c_tx_len = 0;		/* NACK this read */
			break;
	}
}

static void smbus_process_write(uint8_t *buf, int len)
{
	int cnt;

	switch (buf[0]) {
		case SMBUS_CMD_UPGRADE_START:
			if (smbus_pec_verify(len, 1) < 0) {
				break;
			}
			printf("SMBUS UPGRADE: START command received, erasing Flash...\r\n");
			if (upgrade_start()) {
				printf("SMBUS UPGRADE: Flash erase failed\r\n");
				smbus_set_status_bit(SMBUS_STATUS_UPGRADE_ERROR);
			} else {
				printf("SMBUS UPGRADE: Flash erase successful\r\n");
				smbus_clear_status_bit(SMBUS_STATUS_UPGRADE_ERROR);
			}
			break;
		case SMBUS_CMD_UPGRADE_SEND_DATA:
			cnt = buf[1];
			smbus_set_status_bit(SMBUS_STATUS_UPGRADE_ERROR);
			if (len < 2 || (len != cnt + 2 && len != cnt + 3)) {
				printf("SMBUS UPGRADE: invalid Send Data command length\r\n");
				smbus_set_status_bit(SMBUS_STATUS_UPGRADE_ERROR);
				break;
			}
			if (smbus_pec_verify(len, cnt + 2) < 0) {
				break;
			}
			if (upgrade_parse_ihex(buf + 2) >= 0) {
				smbus_clear_status_bit(SMBUS_STATUS_UPGRADE_ERROR);
			} else {
				smbus_set_status_bit(SMBUS_STATUS_UPGRADE_ERROR);
			}
			break;
		case SMBUS_CMD_UPGRADE_ACTIVATE:
			if (smbus_pec_verify(len, 1) < 0) {
				break;
			}
			printf("SMBUS UPGRADE: ACTIVATE command received, verifying firmware...\r\n");
			if (!upgrade_verify()) {
				printf("SMBUS UPGRADE: verified OK, scheduling activation...\r\n");
				smbus_clear_status_bit(SMBUS_STATUS_UPGRADE_ERROR);
				activation_start = get_jiffies();
			} else {
				printf("SMBUS UPGRADE: verification failed\r\n");
				smbus_set_status_bit(SMBUS_STATUS_UPGRADE_ERROR);
			}
			break;
			
		case SMBUS_REG__REMOTE:
			if (smbus_pec_verify(len, 2) < 0) {
				break;
			}
			smbus_data_regs[SMBUS_REG__REMOTE] = buf[1];
			break;
			
		case SMBUS_REG__SET_FAN:
			if (smbus_pec_verify(len, 2) < 0) {
				break;
			}
			smbus_data_regs[SMBUS_REG__SET_FAN] = buf[1];
			break;
			
		case SMBUS_REG__FAN_CURVE:
			if (smbus_pec_verify(len, 2) < 0) {
				break;
			}
			smbus_data_regs[SMBUS_REG__FAN_CURVE] = buf[1];
			env_set("fan_curve", smbus_data_regs[SMBUS_REG__FAN_CURVE]);
			break;
					
		case SMBUS_REG__TB1_EN:
			if (smbus_pec_verify(len, 2) < 0) {
				break;
			}
			smbus_data_regs[SMBUS_REG__TB1_EN] = buf[1];
			env_set("tb1en", smbus_data_regs[SMBUS_REG__TB1_EN]);
			break;
		
		case SMBUS_REG__TB1_DIR:
			if (smbus_pec_verify(len, 2) < 0) {
				break;
			}
			smbus_data_regs[SMBUS_REG__TB1_DIR] = buf[1];
			env_set("tb1dir", smbus_data_regs[SMBUS_REG__TB1_DIR]);
			break;
			
		case SMBUS_REG__TB2_EN:
			if (smbus_pec_verify(len, 2) < 0) {
				break;
			}
			smbus_data_regs[SMBUS_REG__TB2_EN] = buf[1];
			env_set("tb2en", smbus_data_regs[SMBUS_REG__TB2_EN]);
			break;
			
		case SMBUS_REG__TB2_DIR:
			if (smbus_pec_verify(len, 2) < 0) {
				break;
			}
			smbus_data_regs[SMBUS_REG__TB2_DIR] = buf[1];
			env_set("tb2dir", smbus_data_regs[SMBUS_REG__TB2_DIR]);
		break;
		
		case SMBUS_REG__TB3_EN:
			if (smbus_pec_verify(len, 2) < 0) {
				break;
			}
			smbus_data_regs[SMBUS_REG__TB3_EN] = buf[1];
			env_set("tb3en", smbus_data_regs[SMBUS_REG__TB3_EN]);
			break;
		
		case SMBUS_REG__TB3_DIR:
			if (smbus_pec_verify(len, 2) < 0) {
				break;
			}
			smbus_data_regs[SMBUS_REG__TB3_DIR] = buf[1];
			env_set("tb3dir", smbus_data_regs[SMBUS_REG__TB3_DIR]);
			break;
		
		case SMBUS_REG__TB4_EN:
			if (smbus_pec_verify(len, 2) < 0) {
				break;
			}
			smbus_data_regs[SMBUS_REG__TB4_EN] = buf[1];
			env_set("tb4en", smbus_data_regs[SMBUS_REG__TB4_EN]);
			break;
		
		case SMBUS_REG__TB4_DIR:
			if (smbus_pec_verify(len, 2) < 0) {
				break;
			}
			smbus_data_regs[SMBUS_REG__TB4_DIR] = buf[1];
			env_set("tb4dir", smbus_data_regs[SMBUS_REG__TB4_DIR]);
			break;
			
		case SMBUS_REG__ADD_LOW_BYTE:
			if (smbus_pec_verify(len, 3) < 0) {
				break;
			}
			smbus_data_regs[SMBUS_REG__ADD_LOW_BYTE] = buf[1];
			smbus_data_regs[SMBUS_REG__ADD_HIGH_BYTE] = buf[2];
			break;

		case SMBUS_REG__DATA:
			if (smbus_pec_verify(len, 2) < 0) {
				break;
			}
			smbus_data_regs[SMBUS_REG__DATA] = buf[1];
			break;
		
		case SMBUS_REG__WRITE_DATA:
			if (smbus_pec_verify(len, 2) < 0) {
				break;
			}
			smbus_data_regs[SMBUS_REG__WRITE_DATA] = buf[1];
			break;
		
		case SMBUS_REG__SYNC100_DIV:
			if (smbus_pec_verify(len, 2) < 0) {
				break;
			}
			smbus_data_regs[SMBUS_REG__SYNC100_DIV] = buf[1];
			break;
						
		/* TBD: add support for other write commands here, if required */
		default:
			break;
	}
}


/* The following function is called when a read request is received (AR), most likely after a repeated start */
static void i2c_read_request_callback(struct i2c_slave_module *const module)
{
	struct i2c_slave_packet packet;
	uint8_t len = i2c_tx_len;

	packet.data_length = len;
	packet.data = i2c_tx_buf;
	i2c_slave_write_packet_job(module, &packet);
}

/* The following function is called when a write request is received (AW) */
static void i2c_write_request_callback(struct i2c_slave_module *const module)
{
	struct i2c_slave_packet packet;

	/* Initialize the PEC (starting from the write address) */
	uint8_t addr = CFG_I2C_SLAVE_ADDRESS;
	smbus_pec_init(&addr, 1);

	/* Prepare RX buffer for receiving data */
	memset(i2c_rx_buf, sizeof(i2c_rx_buf), 1);
	packet.data_length = sizeof(i2c_rx_buf);
	packet.data = i2c_rx_buf;
	
	if (i2c_slave_read_packet_job(module, &packet) != STATUS_OK) {
		printf("SMBUS: i2c_slave_read_packet_job failed\r\n");
	}
}

/* The following function is called after a write transaction has been completed (i.e. after a stop or repeated start condition) */
static void i2c_write_complete_callback(struct i2c_slave_module *const module)
{
	uint8_t len = module->buffer - i2c_rx_buf;
	uint32_t flags = i2c_slave_get_status(module);
	
	if (len == 0) {
		return;
	}
	if ((flags & I2C_SLAVE_STATUS_ADDRESS_MATCH) && (flags & I2C_SLAVE_STATUS_REPEATED_START)) {
		/* SMBus read command: process immediately */
		smbus_process_read(i2c_rx_buf[0]);
		/* Add PEC byte */
		if (i2c_tx_len) {
			/* Add the current command and our slave address + read bit */
			uint8_t tmpbuf[2] = { i2c_rx_buf[0], CFG_I2C_SLAVE_ADDRESS | 1 };
			smbus_pec_adjust(tmpbuf, sizeof(tmpbuf), 0);
			/* Add the Tx data */
			i2c_tx_buf[i2c_tx_len] = smbus_pec_adjust(i2c_tx_buf, i2c_tx_len, 1);
			i2c_tx_len++;
		}
	} else if (!(smbus_status & SMBUS_STATUS_BUSY)) {
		/* SMBus write command: defer processing to the main loop (since write commands can take a long time to execute) */
		memcpy(cmd_buf, i2c_rx_buf, len);
		cmd_len = len;
		/* Set the busy status: it will be cleared once the command is processed */
		smbus_set_status_bit(SMBUS_STATUS_BUSY);
	}
}

/* The following function is called if an error condition (such as SCL Low Timeout) is detected */
static void i2c_error_last_transfer_callback(struct i2c_slave_module *const module)
{
	uint32_t flags = i2c_slave_get_status(module);
	if (flags & I2C_SLAVE_STATUS_SCL_LOW_TIMEOUT) {
		printf("SMBUS: SCL Low Timeout detected!\r\n");
	} else {
		printf("SMBUS: unknown I2C error (status = 0x%08lx)\r\n", flags);
	}
	/* Clear the error status (the bus will be released automatically) */
	i2c_slave_clear_status(module, flags);
}

void smbus_init(void)
{
	struct i2c_slave_config config_i2c_slave;
	struct system_gclk_gen_config gclk_slow_conf;
	struct system_gclk_chan_config gclk_slow_chan_conf;
	
	i2c_slave_get_config_defaults(&config_i2c_slave);
	config_i2c_slave.address = CFG_I2C_SLAVE_ADDRESS >> 1;
	config_i2c_slave.address_mode = I2C_SLAVE_ADDRESS_MODE_MASK;
	config_i2c_slave.pinmux_pad0 = CFG_I2C_SLAVE_PINMUX_PAD0;
	config_i2c_slave.pinmux_pad1 = CFG_I2C_SLAVE_PINMUX_PAD1;
#ifdef CFG_SMBUS_TIMEOUT_ENABLE
	/* Enable SCL Low timeout for SMBus compatibility */
	config_i2c_slave.scl_low_timeout = true;
#endif /* CFG_SMBUS_TIMEOUT_ENABLE */
	i2c_slave_init(&i2c_slave_instance, CFG_I2C_SLAVE_MODULE, &config_i2c_slave);
#ifdef CFG_SMBUS_TIMEOUT_ENABLE
	/*
		Configure and enable the SERCOMx_SLOW clock, which is used for
		SCL Low timeout detection. Note that the ASF I2C driver
		does not enable this clock (even when the scl_low_timeout
		option is set), so we have to do it separately.
	 */
	system_gclk_gen_get_config_defaults(&gclk_slow_conf);
	gclk_slow_conf.source_clock = GCLK_SOURCE_OSC32K;
	system_gclk_gen_set_config(CFG_SMBUS_TIMEOUT_CLOCK_GEN, &gclk_slow_conf);
	system_gclk_gen_enable(CFG_SMBUS_TIMEOUT_CLOCK_GEN);
	system_gclk_chan_get_config_defaults(&gclk_slow_chan_conf);
	gclk_slow_chan_conf.source_generator = CFG_SMBUS_TIMEOUT_CLOCK_GEN;
	system_gclk_chan_set_config(GCLK_CLKCTRL_ID_SERCOMX_SLOW, &gclk_slow_chan_conf);
	system_gclk_chan_enable(GCLK_CLKCTRL_ID_SERCOMX_SLOW);
#endif /* CFG_SMBUS_TIMEOUT_ENABLE */
	/* We can now enable the slave interface */
	i2c_slave_enable(&i2c_slave_instance);
	i2c_slave_register_callback(&i2c_slave_instance, i2c_read_request_callback, I2C_SLAVE_CALLBACK_READ_REQUEST);
	i2c_slave_enable_callback(&i2c_slave_instance, I2C_SLAVE_CALLBACK_READ_REQUEST);
	i2c_slave_register_callback(&i2c_slave_instance, i2c_write_request_callback, I2C_SLAVE_CALLBACK_WRITE_REQUEST);
	i2c_slave_enable_callback(&i2c_slave_instance, I2C_SLAVE_CALLBACK_WRITE_REQUEST);
	/* The ASF driver has a bug: the write and read complete callbacks are swapped! */
	i2c_slave_register_callback(&i2c_slave_instance, i2c_write_complete_callback, I2C_SLAVE_CALLBACK_READ_COMPLETE);
	i2c_slave_enable_callback(&i2c_slave_instance, I2C_SLAVE_CALLBACK_READ_COMPLETE);
	i2c_slave_register_callback(&i2c_slave_instance, i2c_error_last_transfer_callback, I2C_SLAVE_CALLBACK_ERROR_LAST_TRANSFER);
	i2c_slave_enable_callback(&i2c_slave_instance, I2C_SLAVE_CALLBACK_ERROR_LAST_TRANSFER);
	
	smbus_data_regs[SMBUS_REG__FAN_CURVE] = (uint8_t) env_get("fan_curve");
	smbus_data_regs[SMBUS_REG__TB1_EN] = (uint8_t) env_get("tb1en");
	smbus_data_regs[SMBUS_REG__TB1_DIR] = (uint8_t) env_get("tb1dir");
	smbus_data_regs[SMBUS_REG__TB2_EN] = (uint8_t) env_get("tb2en");
	smbus_data_regs[SMBUS_REG__TB2_DIR] = (uint8_t) env_get("tb2dir");
	smbus_data_regs[SMBUS_REG__TB3_EN] = (uint8_t) env_get("tb3en");
	smbus_data_regs[SMBUS_REG__TB3_DIR] = (uint8_t) env_get("tb3dir");
	smbus_data_regs[SMBUS_REG__TB4_EN] = (uint8_t) env_get("tb4en");
	smbus_data_regs[SMBUS_REG__TB4_DIR] = (uint8_t) env_get("tb4dir");
	
	
	char cmm_firmware_number[10] = CFG_FIRMWARE_NUMBER;
	
	for (uint8_t i=0; i<sizeof(cmm_firmware_number); i++)
	{
		smbus_data_regs[SMBUS_REG__CMM_FW_BYTE_1+i] = cmm_firmware_number[i];
	}
	
	smbus_data_regs[SMBUS_REG__CMM_VERSION] = CFG_FIRMWARE_VERSION;
}

void do_smbus(void)
{
	uint8_t status;
	
	static uint8_t init_done;
	
	if (!init_done) {
		smbus_data_regs[SMBUS_REG__CONFIG] = (ioport_get_pin_level(CFG_DIP4_LEARN)<<3)|(ioport_get_pin_level(CFG_DIP3)<<2)|(ioport_get_pin_level(CFG_DIP2_AC_OK)<<1)|(ioport_get_pin_level(CFG_DIP1_PS_ON_LOGIC));
		init_done = 1;
	}
	

	/* Check for scheduled activation */
	if (activation_start && get_jiffies() - activation_start > 3000) {
		printf("SMBUS UPGRADE: activating firmware...\r\n");
		if (upgrade_activate()) {
			printf("SMBUS UPGRADE: activation failed\r\n");
			smbus_set_status_bit(SMBUS_STATUS_UPGRADE_ERROR);
			activation_start = 0;
		}
		else{
			printf("SMBUS UPGRADE: activation finished, carry out power cycle. \r\n");
			activation_start = 0;
		}
	}
	
	/* Protect against the interrupt handler updating the status */
	system_interrupt_enter_critical_section();
	status = smbus_status;
	system_interrupt_leave_critical_section();
	
	if (!(status & SMBUS_STATUS_BUSY)) {
		/* No pending command */
		return;
	}

	/* Adjust the PEC to include the command data */
	smbus_pec_adjust(cmd_buf, cmd_len, 1);
	
	/* Process deferred write command */
	smbus_process_write(cmd_buf, cmd_len);

	/* Clear the busy status (ready to accept more commands) */
	smbus_clear_status_bit(SMBUS_STATUS_BUSY);
}

#endif /* BOOTLOADER */