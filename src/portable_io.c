#include <stdio.h>
#include "portable_io.h"



char Prbuf[MAXLLEN];

//将输入输出函数包出来.

void portable_puts(char* buf){
	while((*buf)!=0){
		putchar(*buf);
		buf++;
	}
}

void portable_input_int(int* val){
	portable_gets(Prbuf); 
	sscanf(Prbuf, "%d", val);
}

void portable_gets(char* buf){
	fgets(buf, MAXLLEN, stdin);	
}

void portable_fgets(char *buf, FILE *file){
	if (feof(file))
		*buf='B'; //结束程序
	else fgets(buf, MAXLLEN, file);
}
