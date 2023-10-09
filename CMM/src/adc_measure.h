/*
 * temperature.h
 *
 * Created: 16.11.2020 11:53:27
 *  Author: E1130513
 */ 

#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_

void adc_measure_init(void);
void check_voltage_ok(void);
uint8_t read_pwr_ok (void);
void voltages_get_values(void);
void load_learned_temp_values(void);
void learn_temp(void);
void do_measure(void);

#endif /* TEMPERATURE_H_ */