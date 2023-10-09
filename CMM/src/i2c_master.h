/*
 * i2c_master.h
 *
 * Created: 01.10.2021 10:38:53
 *  Author: E1130513
 */ 


#ifndef I2C_MASTER_H_
#define I2C_MASTER_H_

void i2c_init_master(void);
void initial_read_i2c_components(void);
void do_i2c_master(void);

#endif /* I2C_MASTER_H_ */