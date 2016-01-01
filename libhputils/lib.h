#ifndef _LIB_H_
#define _LIB_H_

#include <stdint.h>

#define NULL 0

void* memcpy(void *dst, const void *src, unsigned n);
void* memset(void *dst, int val, unsigned n);
char* strcat(char *dst, const char *src);
int strlen(const char *str);

void delay(int cycles);

uint8_t hex2byte(char hex);
uint32_t hex2word(char *hex, char **next);

void byte2hex(uint8_t byte, char *hex);

uint8_t bcd2byte(uint8_t bcd);

char* itoa(int val, char *str, int base);

#endif