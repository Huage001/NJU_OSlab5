#include "x86.h"

#define SERIAL_PORT  0x3F8

void initSerial(void) {
	outByte(SERIAL_PORT + 1, 0x00);
	outByte(SERIAL_PORT + 3, 0x80);
	outByte(SERIAL_PORT + 0, 0x01);
	outByte(SERIAL_PORT + 1, 0x00);
	outByte(SERIAL_PORT + 3, 0x03);
	outByte(SERIAL_PORT + 2, 0xC7);
	outByte(SERIAL_PORT + 4, 0x0B);
}

static inline int serialIdle(void) {
	return (inByte(SERIAL_PORT + 5) & 0x20) != 0;
}

void putChar(char ch) {
	while (serialIdle() != TRUE);
	outByte(SERIAL_PORT, ch);
}

/*******************Here are my functions for debug**********************/

void printStr(char* str){
	for(int i=0;str[i]!=0;i++){
		putChar(str[i]);
	}
	putChar('\n');
}

void printNum(uint32_t num){
	char buffer[10];
	int index=0;
	do{
		buffer[index++]=num%10+'0';
		num/=10;
	}while(num!=0);
	for(int i=index-1;i>=0;i--){
		putChar(buffer[i]);
	}
	putChar('\n');
}