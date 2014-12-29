#ifndef __PARSER_H___
#define __PARSER_H___

#define MAXLINE 1000
#define MAXEXPB 5000 

#define NO_LINENUM 65535		//找不到某行时返回的下标

enum Commands {
	C_FOR='F',
	C_NEXT='N',
	C_GOSUB='S',
	C_RETURN='R',
	C_GOTO='G',
	C_IF='I',
	C_INPUT='U',
	C_PRINT='P',
	C_LET='L',
	C_PUSH='>',
	C_POP='<',
	C_REM='\'',
	C_END='E' 
	};

enum Errors {
	E_NOERR=0,
	E_SYNTAX,			//语法错误 
	E_NEXT_WO_FOR,		//只有NEXT, 没有FOR 
	E_NOCMD 			//没有这个命令 
};


enum Info_in_Exp_mem {				//Expmem中可能出现的特殊字符.  

//STAT_EXP_?是表达式的状态, 垃圾回收时使用 
//这些的值不在正常的表达式中出现, 并且具有如下性质:
//	~STAT_EXP_0=STAT_EXP_1, ~STAT_EXP_1=STAT_EXP_0 
//初始状态为STAT_EXP[1].
//垃圾回收时根据程序的情况将程序中引用的所有字串的状态取反,
//然后在表达式存储区域搜索原状态, 删除每个状态未取反的字串. 

	STAT_EXP_0=1,
	STAT_EXP_1=-2,
	FOR_NOT_RUN=2,
	FOR_RUNNING=3
}; 


extern char GCStat;	//垃圾回收器用的状态, 必须让parser根据实际情况设置! 

typedef unsigned char Errnum;

struct Prog_line{
	unsigned short linenum;
	char statement;
	signed char var;
	unsigned short expr;
};

extern struct Prog_line Program[MAXLINE];	//程序存储空间
extern char Exp_mem[MAXEXPB+1];				//表达式存储空间 

extern unsigned short Prog_ptr;				//指向程序存储空间的指针 
extern unsigned short Exp_ptr;				//指向表达式存储空间末尾的指针 
 
Errnum parse_line(char* cmd, unsigned short linenum);
unsigned short find_corresponding_FOR(unsigned short ptr);
#endif
