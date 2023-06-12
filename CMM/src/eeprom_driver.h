/*
 * eeprom.h
 *
 * Created: 12/6/2020 10:53:03 AM
 *  Author: E1210640
 */ 

/*EEPROM Byte mapping
byte 0 = Controller P/N  (high) high_byte
byte 1 = Controller P/N  (high) low_byte
byte 2 = Controller P/N  (low) high_byte
byte 3 = Controller P/N  (low) low_byte

byte 4 = Controller S/N  (high) high_byte
byte 5 = Controller S/N  (high) low_byte
byte 6 = Controller S/N  (low) high_byte
byte 7 = Controller S/N  (low) low_byte

byte 8 to 49 = reserved
 */


#ifndef EEPROM_H_
#define EEPROM_H_

void eeprom_init(void);
int eeprom_read(uint8_t *buf, int offset, int len);
int eeprom_write(const uint8_t *buf, int offset, int len);

#endif /* EEPROM_H_ */