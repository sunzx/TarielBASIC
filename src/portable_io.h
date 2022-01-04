#ifndef __PORTABLE_IO_H___
#define __PORTABLE_IO_H___

#define MAXLLEN 80		//I/O一行的长度 

extern char Prbuf[MAXLLEN];
void portable_puts(char* buf);
void portable_input_int(int* val);
void portable_gets(char* buf); 
void portable_fgets(char *buf, FILE *file);

#endif 
