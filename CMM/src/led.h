/*
 * led.h
 *
 * Created: 25.08.2021 17:33:18
 *  Author: E1130513
 */ 

#ifndef LED_H_
#define LED_H_

#define LED_Off(led_gpio)     port_pin_set_output_level(led_gpio,true)
#define LED_On(led_gpio)      port_pin_set_output_level(led_gpio,false)
#define LED_Toggle(led_gpio)  port_pin_toggle_output_level(led_gpio) 

void signalize_3v3_not_ok(void);
void signalize_5v_not_ok(void);
void signalize_12v_not_ok(void);
void signalize_restart_system(void);
void signalize_learn_state(void);
void led_init(void);
void force_LED_to_green(void);
void do_led(void);

#endif /* LED_H_ */