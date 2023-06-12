/*
 * i2c_master.c
 *
 * Created: 01.10.2021 10:38:35
 *  Author: E1130513
 */ 
#include <asf.h>

#include "config.h"
#include "uart.h"
#include "sys_timer.h"
#include "i2c_master.h"
#include "smbus.h"

#ifndef BOOTLOADER

static struct i2c_master_module i2c_master_instance;
struct i2c_master_packet wr_packet;
struct i2c_master_packet rd_packet;
static bool i2c_master_write_complete = false;
static bool i2c_master_read_complete = false;
uint32_t i2c_master_timeout_counter;
static uint8_t tb_present=0;
static uint8_t send_buffer[20];
static uint8_t read_buffer[255];
static uint32_t last_i2c_master_write;
static uint8_t read_i2c_components = 0;

static void i2c_master_write_complete_callback(struct i2c_master_module *const module);
static void i2c_master_read_complete_callback(struct i2c_master_module *const module);
static void i2c_master_error_callback(struct i2c_master_module *const module);
static void i2c_master_write (uint8_t *write_buffer, uint16_t data_length, uint16_t slave_address);
static void i2c_master_read (uint8_t *read_buffer, uint16_t data_length, uint16_t slave_address);
static uint8_t i2c_slave_present (uint8_t *read_buffer, uint16_t data_length, uint16_t slave_address);
static void triggerbridge_present(void);
static void write_triggerbridge_values(void);
static void read_clock_module(void);
static void write_clock_module(void);

/*
 * Callback, if the master writes the pointer to the slave complete. The master can start reading the data from the slave
 */
static void i2c_master_write_complete_callback(struct i2c_master_module *const module)
{
	i2c_master_write_complete = true;
}

/*
 * callback, then the master reads the data from the slave
 */
static void i2c_master_read_complete_callback(struct i2c_master_module *const module)
{
	i2c_master_read_complete = true;
}

/*
 * Callback, if an error occurs at the master i2c Module
 */
static void i2c_master_error_callback(struct i2c_master_module *const module)
{
/*
	printf("\r\nI2C Error at Master i2c module: ");
	if(STATUS_OK == i2c_master_get_job_status(&i2c_master_instance))
	{
		printf("STATUS_OK");
	}
	if(STATUS_BUSY == i2c_master_get_job_status(&i2c_master_instance))
	{
		printf("STATUS_BUSY");
	}
	if(STATUS_ERR_DENIED == i2c_master_get_job_status(&i2c_master_instance))
	{
		printf("STATUS_ERR_DENIED");
	}
	if(STATUS_ERR_PACKET_COLLISION == i2c_master_get_job_status(&i2c_master_instance))
	{
		printf("STATUS_ERR_PACKET_COLLISION");
	}
	if(STATUS_ERR_BAD_ADDRESS == i2c_master_get_job_status(&i2c_master_instance))
	{
		printf("STATUS_ERR_BAD_ADDRESS");
	}
	if(STATUS_ERR_TIMEOUT == i2c_master_get_job_status(&i2c_master_instance))
	{
		printf("STATUS_ERR_TIMEOUT");
	}
	if(STATUS_ERR_OVERFLOW == i2c_master_get_job_status(&i2c_master_instance))
	{
		printf("STATUS_ERR_OVERFLOW");
	}
*/
}

/*
 * Configure the I2C Master module
 * 100kHz Bus frequency
 */
void i2c_init_master(void)
{
	struct i2c_master_config config_i2c_master;
	i2c_master_get_config_defaults(&config_i2c_master);
	config_i2c_master.buffer_timeout = 65535;
	config_i2c_master.pinmux_pad0 = CFG_I2C_MASTER_PINMUX_PAD0;
	config_i2c_master.pinmux_pad1 = CFG_I2C_MASTER_PINMUX_PAD1;
	while(i2c_master_init(&i2c_master_instance, CFG_I2C_MASTER_MODULE, &config_i2c_master)!= STATUS_OK);
	i2c_master_enable(&i2c_master_instance);
	i2c_master_register_callback(&i2c_master_instance, i2c_master_write_complete_callback,I2C_MASTER_CALLBACK_WRITE_COMPLETE);
	i2c_master_enable_callback(&i2c_master_instance,I2C_MASTER_CALLBACK_WRITE_COMPLETE);
	i2c_master_register_callback(&i2c_master_instance, i2c_master_read_complete_callback, I2C_MASTER_CALLBACK_READ_COMPLETE);
	i2c_master_enable_callback(&i2c_master_instance,I2C_MASTER_CALLBACK_READ_COMPLETE);
	i2c_master_register_callback(&i2c_master_instance, i2c_master_error_callback , I2C_MASTER_CALLBACK_ERROR);
	i2c_master_enable_callback(&i2c_master_instance,I2C_MASTER_CALLBACK_ERROR);
	
	smbus_set_input_reg(SMBUS_REG__SYNC100_DIV, 1);
}

/*
 * Function to write from master to slave
 */
static void i2c_master_write (uint8_t *write_buffer, uint16_t data_length, uint16_t slave_address)
{
	wr_packet.data = write_buffer;
	wr_packet.address     = slave_address;
	wr_packet.data_length = data_length;
	
	if(STATUS_OK!=i2c_master_write_packet_job(&i2c_master_instance, &wr_packet))
	{
		i2c_master_reset(&i2c_master_instance);
		i2c_init_master();
		printf("\r\nReset I2C Master because of write error");
	}
		
	i2c_master_write_complete = false;
	i2c_master_timeout_counter = 0;
	while(i2c_master_write_complete == false)
	{
		delay_cycles_ms(1);
		if(i2c_master_timeout_counter > CFG_I2C_MASTER_TIMEOUT)
		{
			i2c_master_write_complete = true;//Also set true in the i2c_master_write_complete_callback
		}
		else
		{
			i2c_master_timeout_counter++;
		}
	}
}

/*
 * Function to read from slave
 */
static void i2c_master_read (uint8_t *read_buf, uint16_t data_length, uint16_t slave_address)
{
	rd_packet.address     = slave_address;
	rd_packet.data_length = data_length;
	rd_packet.data        = read_buf;
	
	if(STATUS_OK!=i2c_master_read_packet_job(&i2c_master_instance, &rd_packet))
	{
		i2c_master_reset(&i2c_master_instance);
		i2c_init_master();
		printf("\r\nReset I2C Master because of write error");
	}
	
	i2c_master_read_complete = false;
	i2c_master_timeout_counter = 0;
	
	
	while(i2c_master_read_complete == false)
	{
		delay_cycles_ms(1);
		if(i2c_master_timeout_counter > CFG_I2C_MASTER_TIMEOUT)
		{
			i2c_master_read_complete = true;//Also set true in the i2c_master_write_complete_callback
		}
		else
		{
			i2c_master_timeout_counter++;
		}
	}
}

/*
 * Check if i2c slave is present
 */
static uint8_t i2c_slave_present (uint8_t *read_buf, uint16_t data_length, uint16_t slave_address)
{
	rd_packet.address     = slave_address;
	rd_packet.data_length = data_length;
	rd_packet.data        = read_buf;
	
	i2c_master_write_packet_job(&i2c_master_instance, &rd_packet);
	
	i2c_master_write_complete = false;
	i2c_master_timeout_counter = 0;
	
	while(i2c_master_write_complete == false)
	{
		if(i2c_master_timeout_counter > CFG_I2C_MASTER_TIMEOUT)
		{
			i2c_master_write_complete = true;//Also set true in the i2c_master_write_complete_callback
			return 0;
		}
		else
		{
			i2c_master_timeout_counter++;
		}
		delay_cycles_ms(1);
	}
	
	if(STATUS_ERR_BAD_ADDRESS == i2c_master_get_job_status(&i2c_master_instance))
	{
		return 0;
	}
	
	return 1;
}

/*
 * Check if triggerbridge is present
 */
static void triggerbridge_present(void)
{
	send_buffer[0] = 0;
	uint8_t tb1_present, tb2_present, tb3_present, tb4_present;
	
	tb1_present =	i2c_slave_present(send_buffer, 1, CFG_I2C_MASTER_TB1_ADDRESS);
	tb2_present =	i2c_slave_present(send_buffer, 1, CFG_I2C_MASTER_TB2_ADDRESS);
	tb3_present =	i2c_slave_present(send_buffer, 1, CFG_I2C_MASTER_TB3_ADDRESS);
	tb4_present =	i2c_slave_present(send_buffer, 1, CFG_I2C_MASTER_TB4_ADDRESS);
	
	tb_present = (tb4_present << 3) | (tb3_present << 2) | (tb2_present << 1) | tb1_present;
	smbus_set_input_reg(SMBUS_REG__TBPRES,tb_present);
}

/*
 * Write the triggerbridge values
 */
static void write_triggerbridge_values(void)
{
	uint8_t tb_en, tb_dir;
	
	for(uint8_t i=0; i<4; i++)
	{
		if(((tb_present>>i) & 1) == 1)
		{
			tb_en = smbus_get_input_reg(i*2+SMBUS_REG__TB1_EN);
			tb_dir = smbus_get_input_reg(i*2+1+SMBUS_REG__TB1_EN);
			
			send_buffer[0] = 6;
			send_buffer[1] = ~tb_en;
			i2c_master_write(send_buffer, 2, CFG_I2C_MASTER_TB1_ADDRESS + i);
			send_buffer[0] = 7;
			send_buffer[1] = ~tb_en;
			i2c_master_write(send_buffer, 2, CFG_I2C_MASTER_TB1_ADDRESS + i);
			
			send_buffer[0] = 2;
			send_buffer[1] = ~tb_dir;
			i2c_master_write(send_buffer, 2, CFG_I2C_MASTER_TB1_ADDRESS + i);
			send_buffer[0] = 3;
			send_buffer[1] = tb_dir;
			i2c_master_write(send_buffer, 2, CFG_I2C_MASTER_TB1_ADDRESS + i);
		}
	}
}

/*
 * Read the clock module values
 */
static void read_clock_module(void)
{
	uint8_t clockmodul_present=0;
	
	send_buffer[0] = 0;
	clockmodul_present = i2c_slave_present(send_buffer, 1, CFG_I2C_MASTER_CLK_ADDRESS);
	
	smbus_set_input_reg(SMBUS_REG__CLOCKMODUL_PRESENT, clockmodul_present);
	if(clockmodul_present == 1)
	{
		send_buffer[0] = 0;
		i2c_master_write(send_buffer, 1, CFG_I2C_MASTER_CLK_ADDRESS);
		i2c_master_read(read_buffer, 10, CFG_I2C_MASTER_CLK_ADDRESS);
		smbus_set_input_reg(SMBUS_REG__CLOCK_MODULE_FW_BYTE_1, read_buffer[0]);
		smbus_set_input_reg(SMBUS_REG__CLOCK_MODULE_FW_BYTE_2, read_buffer[1]);
		smbus_set_input_reg(SMBUS_REG__CLOCK_MODULE_FW_BYTE_3, read_buffer[2]);
		smbus_set_input_reg(SMBUS_REG__CLOCK_MODULE_FW_BYTE_4, read_buffer[3]);
		smbus_set_input_reg(SMBUS_REG__CLOCK_MODULE_FW_BYTE_5, read_buffer[4]);
		smbus_set_input_reg(SMBUS_REG__CLOCK_MODULE_FW_BYTE_6, read_buffer[5]);
		smbus_set_input_reg(SMBUS_REG__CLOCK_MODULE_FW_BYTE_7, read_buffer[6]);
		smbus_set_input_reg(SMBUS_REG__CLOCK_MODULE_FW_BYTE_8, read_buffer[7]);
		smbus_set_input_reg(SMBUS_REG__CLOCK_MODULE_FW_BYTE_9, read_buffer[8]);
		smbus_set_input_reg(SMBUS_REG__CLOCK_MODULE_FW_BYTE_10, read_buffer[9]);
	}	
}

/*
 * Write the clock module values
 */
static void write_clock_module(void)
{
	static uint8_t sync100_div_privious_value;
	
	if(smbus_get_input_reg(SMBUS_REG__WRITE_DATA) == 1) //Write only, if "Write_Data" is '1' which is set by i2c
	{
		smbus_set_input_reg(SMBUS_REG__WRITE_DATA, 0); //Clr "Write_Data"
		send_buffer[0] = 8; //PLL Address Register
		send_buffer[1] = smbus_get_input_reg(SMBUS_REG__ADD_HIGH_BYTE);
		send_buffer[2] = smbus_get_input_reg(SMBUS_REG__ADD_LOW_BYTE); 
		i2c_master_write(send_buffer, 3, CFG_I2C_MASTER_CLK_ADDRESS);
		
		send_buffer[0] = 12; //PLL Data Register
		send_buffer[1] = smbus_get_input_reg(SMBUS_REG__DATA);
		i2c_master_write(send_buffer, 2, CFG_I2C_MASTER_CLK_ADDRESS);
	}
	
	if(sync100_div_privious_value != smbus_get_input_reg(SMBUS_REG__SYNC100_DIV)) //Write only the sync 100 register, if a new valuie is available
	{
		send_buffer[0] = 4; //EEPROM Address of the clock modul of the register Sync100_div
		send_buffer[1] = smbus_get_input_reg(SMBUS_REG__SYNC100_DIV);
		i2c_master_write(send_buffer, 2, CFG_I2C_MASTER_CLK_ADDRESS);
		sync100_div_privious_value = smbus_get_input_reg(SMBUS_REG__SYNC100_DIV); //save the new value
	}
}

/*
 * Read the pdb FRU Informations (P/N, S/N, Power limits)
 */
/*
static void read_fru_info_pdb(void)
{
	send_buffer[0] = 0;
	for(int i = 0; i<sizeof(read_buffer); i++)
	{
		read_buffer[i] = 0;	
	}
	
	i2c_master_write(send_buffer, 1, CFG_I2C_MASTER_EEPROM);
	i2c_master_read(read_buffer, 28, CFG_I2C_MASTER_EEPROM);
	
	for(int i=0; i<8; i++)
	{
		smbus_set_input_reg(SMBUS_REG__CMM_PDB_PRODUCT_NUM_Byte_1+i, read_buffer[0+i]);	
	}
	
	for(int i=0; i<12; i++)
	{
		smbus_set_input_reg(SMBUS_REG__CMM_PDB_SERIAL_NUM_Byte_1+i, read_buffer[8+i]);
	}
	
	smbus_set_input_reg(SMBUS_REG__CMM_PDB_MAX_POWER_3V3_HIGH_BYTE, read_buffer[20]);
	smbus_set_input_reg(SMBUS_REG__CMM_PDB_MAX_POWER_3V3_LOW_BYTE, read_buffer[21]);
	smbus_set_input_reg(SMBUS_REG__CMM_PDB_MAX_POWER_5V_HIGH_BYTE, read_buffer[22]);
	smbus_set_input_reg(SMBUS_REG__CMM_PDB_MAX_POWER_5V_LOW_BYTE, read_buffer[23]);
	smbus_set_input_reg(SMBUS_REG__CMM_PDB_MAX_POWER_12V_HIGH_BYTE, read_buffer[24]);
	smbus_set_input_reg(SMBUS_REG__CMM_PDB_MAX_POWER_12V_LOW_BYTE, read_buffer[25]);
	smbus_set_input_reg(SMBUS_REG__CMM_PDB_MAX_POWER_TOTAL_HIGH_BYTE, read_buffer[26]);
	smbus_set_input_reg(SMBUS_REG__CMM_PDB_MAX_POWER_TOTAL_LOW_BYTE, read_buffer[27]);
}
*/

/*
 * Read the pdb values (Voltage, Current)
 */
/*
static void read_pdb(void)
{
	uint8_t ina_present_3V3=0, ina_present_5V=0, ina_present_12V=0;
	float  voltage, current;
	uint16_t power;
	
	send_buffer[0] = 0;
	ina_present_3V3 = i2c_slave_present(send_buffer, 1, CFG_I2C_MASTER_INA_3V3);
	ina_present_5V = i2c_slave_present(send_buffer, 1, CFG_I2C_MASTER_INA_5V);
	ina_present_12V = i2c_slave_present(send_buffer, 1, CFG_I2C_MASTER_INA_12V);

	if(ina_present_3V3 == 1)
	{
		send_buffer[0] = 2;
		read_buffer[0] = read_buffer[1] = 0;
		i2c_master_write(send_buffer, 1, CFG_I2C_MASTER_INA_3V3);
		i2c_master_read(read_buffer, 2, CFG_I2C_MASTER_INA_3V3);
		voltage = (float)((read_buffer[0]<<8) | (read_buffer[1])) * 0.00125;
		
		send_buffer[0] = 1;
		read_buffer[0] = read_buffer[1] = 0;
		i2c_master_write(send_buffer, 1, CFG_I2C_MASTER_INA_3V3);
		i2c_master_read(read_buffer, 2, CFG_I2C_MASTER_INA_3V3);
		current = (float)((read_buffer[0]<<8) | (read_buffer[1])) * 0.0000025 / 0.0005;
		
		power =  (uint16_t)((voltage * current));
		smbus_set_input_reg(SMBUS_REG__CMM_PDB_POWER_3V3_LOW_BYTE, power & 0xFF);
		smbus_set_input_reg(SMBUS_REG__CMM_PDB_POWER_3V3_HIGH_BYTE, power >> 8);
	}

	if(ina_present_5V == 1)
	{	
		send_buffer[0] = 2;
		read_buffer[0] = read_buffer[1] = 0;
		i2c_master_write(send_buffer, 1, CFG_I2C_MASTER_INA_5V);
		i2c_master_read(read_buffer, 2, CFG_I2C_MASTER_INA_5V);
		voltage = (float)((read_buffer[0]<<8) | (read_buffer[1])) * 0.00125;
				
		send_buffer[0] = 1;
		read_buffer[0] = read_buffer[1] = 0;
		i2c_master_write(send_buffer, 1, CFG_I2C_MASTER_INA_5V);
		i2c_master_read(read_buffer, 2, CFG_I2C_MASTER_INA_5V);
		current = (float)((read_buffer[0]<<8) | (read_buffer[1])) * 0.0000025 / 0.0005;
				
		power =  (uint16_t)((voltage * current));	
		smbus_set_input_reg(SMBUS_REG__CMM_PDB_POWER_5V_LOW_BYTE, power & 0xFF);
		smbus_set_input_reg(SMBUS_REG__CMM_PDB_POWER_5V_HIGH_BYTE, power >> 8);
	}
	
	if(ina_present_12V == 1)
	{
		send_buffer[0] = 2;
		read_buffer[0] = read_buffer[1] = 0;
		i2c_master_write(send_buffer, 1, CFG_I2C_MASTER_INA_12V);
		i2c_master_read(read_buffer, 2, CFG_I2C_MASTER_INA_12V);
		voltage = (float)((read_buffer[0]<<8) | (read_buffer[1])) * 0.00125;
				
		send_buffer[0] = 1;
		read_buffer[0] = read_buffer[1] = 0;
		i2c_master_write(send_buffer, 1, CFG_I2C_MASTER_INA_12V);
		i2c_master_read(read_buffer, 2, CFG_I2C_MASTER_INA_12V);
		current = (float)((read_buffer[0]<<8) | (read_buffer[1])) * 0.0000025 / 0.00025;
				
		power =  (uint16_t)((voltage * current));
		smbus_set_input_reg(SMBUS_REG__CMM_PDB_POWER_12V_LOW_BYTE, power & 0xFF);
		smbus_set_input_reg(SMBUS_REG__CMM_PDB_POWER_12V_HIGH_BYTE, power >> 8);
	}		
}
*/

/*
 * Set Flag to read i2c_components again (Trigger Bridges, Clockmodule)
 */
void initial_read_i2c_components(void)
{
	read_i2c_components = 1;
}


/*
 * Do the i2c master relevant functions
 */
void do_i2c_master(void)
{	
	if(read_i2c_components == 1)
	{
		triggerbridge_present();
		read_clock_module();
//		read_fru_info_pdb();
		read_i2c_components = 0;
	}
	
	if (get_jiffies() - last_i2c_master_write >= 1000)
	{
		last_i2c_master_write = get_jiffies();
		write_triggerbridge_values();
		write_clock_module();
//		read_pdb();
	}	
}

#endif /* BOOTLOADER */