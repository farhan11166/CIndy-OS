#ifndef STRING_H
#define STRING_H

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, int n);
int strlen(const char *s);
void* memset(void* dest, int val, unsigned int n);
void* memcpy(void* dest, const void* src, unsigned int n);

#endif