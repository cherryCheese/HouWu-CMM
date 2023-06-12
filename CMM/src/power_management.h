/*
 * power_management.h
 *
 * Created: 25.08.2021 15:42:18
 *  Author: E1130513
 */ 

#ifndef POWER_MANAGEMENT_H_
#define POWER_MANAGEMENT_H_

void turn_3V3_on(void);
void turn_5V_on(void);
void turn_12V_on(void);
void turn_m12V_on(void);

void turn_voltages_on(void);
void turn_voltages_off(void);
void power_management_init(void);
void do_power_management(void);

#endif /* POWER_MANAGEMENT_H_ */