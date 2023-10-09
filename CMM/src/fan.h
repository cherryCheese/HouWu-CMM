/*
 * fan.h
 *
 * Created: 21.08.2020 12:48:31
 *  Author: E1130513
 */ 


#ifndef FAN_H_
#define FAN_H_

#define SIX_FANs

extern uint32_t fanpwm_from_cli;
extern uint32_t fantacho1;
extern uint32_t fantacho2;
extern uint32_t fantacho3;
extern uint32_t fantacho4;
#ifdef SIX_FANs
extern uint32_t fantacho5;
extern uint32_t fantacho6;
#endif

void load_learned_fan_values(void);
void set_spinup_speed_of_fans(void);
void learn_fan(void);
void fan_init(void);
void do_fan(void);

#endif /* FAN_H_ */