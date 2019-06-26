#ifndef __SERIAL_H__
#define __SERIAL_H__

void initSerial(void);
void putChar(char);

/*****************Here are my functions for debug*********************/
void printStr(char*);
void printNum(uint32_t);

#endif
