/*
 * upgrade.h
 *
 * Created: 8/14/2020 3:50:55 PM
 *  Author: E1210640
 */ 


#ifndef __UPGRADE_H__
#define __UPGRADE_H__

int upgrade_start(void);
int upgrade_activate(void);
int upgrade_parse_ihex(uint8_t *buf);
int upgrade_verify(void);
int upgrade_copy_to_nvm(void);

#endif /* __UPGRADE_H__ */