#ifndef __INTERPRETER_H___
#define __INTERPRETER___

#include "parser.h"

#define GOSUB_DEPTH 100
#define DSTACK_DEPTH 256
 
//程序执行状态. 由run_line返回. 
enum Status {
	S_STOP,			//程序停止(如执行到END) 
	S_RUNNING		//程序开始或正在运行 
};
typedef unsigned char Status;

extern unsigned short isClean;

 

void prog_init(void);
Errnum prog_input_line(char* line);
void prog_list(void);
void prog_run(void);
void prog_flush(void);
Status interpreter_immediate_line(char* line); 

#endif 
