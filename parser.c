#include <string.h>
#include "parser.h"
/* 

以下exp代表表达式, var代表变量, string代表字符串. 

解释器支持的26个变量均为32位有符号整数, 用单个英文字母A~Z表示. 

解释器支持的BASIC命令如下:
	* FOR..NEXT
		格式: FOR var=exp1 TO exp2
			  NEXT [var]
		作用: 循环从初值exp1到终值exp2的次数.
			  1. 控制流进入循环时, 对exp1进行求值, 将var设为exp1的值;
			  2. 控制流流到对应的NEXT时, 对exp2进行求值, 并将其与var进行比较.
			   	   若exp2=var, 则控制流向下继续;
				   若exp2!=var, 则var++, 控制流跳回FOR的下一行. 
		注记: 1. NEXT的变量名只有助记意义, 可以省略, 对其一致性不作判断. 
			  2. 在循环体内修改循环变量值时请注意求值顺序, 以便得到正确的结果.
			  
	* GOSUB..RETURN
		格式: GOSUB exp
			  RETURN
		作用: 将控制流转向给定的子程序. 
			  1. 控制流运行至GOSUB指令时, 会将exp进行求值, 然后将当前行号压栈, 
				   跳转到exp的值对应的行.
			  2. RETURN指令将栈中行号弹出, 跳转到对应行.
		注记: GOSUB指令跳转到的行号是表达式求值的结果. 对表达式的值是否真正对应
				行号目前的版本尚未进行判断, 请在使用此特性时自行保证.
				
	* GOTO
		格式: GOTO exp
		作用: 将控制流强行跳转到某地.
			     GOTO语句将exp进行求值, 并将控制流强行跳转到exp的值对应的行. 
		注记: 与GOSUB同.
		
	* IF
		格式: IF exp THEN statement
		作用: 条件执行某条指令
			  	 IF指令会对exp进行求值. 若exp的值不为零, 则执行statement所述
				   指令; 若exp的值为零, 则执行下一条指令. 
		注记: 1. IF指令的THEN后面必须跟一个完整的语句. 所以只能写
						IF A>B THEN GOTO 150
					不能写
						IF A>B THEN 150
					这一点与许多解释器不同.
			  2. IF指令在存储器中占两条指令的空间. 第一条是IF本身, 第二条是
			        statement.
		 
	* INPUT
		格式: INPUT var
		作用: 从键盘读入一个数字, 并赋值给var.
		注记: 无.			

	* PRINT		 
		格式: PRINT exp | PRINT "string | PRINT exp; | PRINT 'string'
		作用: PRINT指令用于打印一个数字或一个字符串.
			  1. PRINT后跟一个表达式时, 作用是在一个单独的行里打印出表达式的值.
			  2. 若PRINT后跟一个双引号, 之后的任何内容均会打印在一个单独的行里.
			  3. PRINT后跟一个表达式和分号时, 打印出表达式的值, 不换行.
			  4. PRINT后跟一个单引号时, 之后的内容打印到一行中, 不换行. 内容以
			  	 后面的单引号为结束符.
				    
		注记: 1. 尚未实现更高级的PRINT功能. 
			  2. PRINT exp;功能打印出变量后, 后面不跟空格! 
			  3. PRINT后跟双引号时不需要闭引号, 因为打印完成之后需要换行, 行尾空
			     格不影响打印效果.
			  4. PRINT后跟单引号时需要闭引号, 为了指定打印的空格数量. 闭引号是一
			     行中的最后一个引号. 
		
	* LET
		格式: var=exp
		作用: 将exp求值, 赋给var.
		注记: 虽然这条指令叫LET, 但是书写时只能写"A=1"的形式, 不能写"LET A=1"的
			  形式.
	* PUSH
		格式: PUSH exp
		作用: 将exp求值, 压入数据栈中.
		注记: 1. 本解释器只有26个变量, 并且没有数组功能. 为了弥补存储空间的不足,
			     本解释器提供一个专用的数据栈用于暂存数据.  
			  2. PUSH的参数可以是表达式, 不一定非得是一个变量.
			  
	* POP
		格式: POP var
		作用: 将数据栈顶元素弹出, 并存入var
		注记: 1. 本解释器尚未实现栈空和栈满检测, 使用时请注意.
			  2. 更高级的栈操作, 如SWAP, ROT3也尚未实现.
	
	* REM
		格式: REM string
		作用: 注释.
		注记: 无.			   
			
	* END
		格式: END
		作用: 停止程序执行.
		注记: 本指令可出现在程序的任意位置, 控制流遇到后立即停止程序的执行.
		 
*/ 
//const char STAT_EXP[2]={STAT_EXP_0,STAT_EXP_1};

struct Prog_line Program[MAXLINE];	//程序存储空间
char Exp_mem[MAXEXPB+1];			//表达式存储空间 

unsigned short Prog_ptr;			//指向程序存储空间末尾的指针 
unsigned short Exp_ptr;				//指向表达式存储空间末尾的指针 

char GCStat=STAT_EXP_0;				//垃圾回收器的状态 
Errnum parse_LET(char* cmd){
	//形式为"A=1+2*B"
	//var为A, exp为1+2*B
	if (cmd[0]<'A' || cmd[0]>'Z') return E_SYNTAX;
	Program[Prog_ptr].statement=C_LET; 
	Program[Prog_ptr].var=cmd[0]-'A';
	Program[Prog_ptr].expr=Exp_ptr;
	
	Exp_mem[Exp_ptr]=GCStat;
	Exp_ptr++;
	strcpy(Exp_mem+Exp_ptr, cmd+2);
	Exp_ptr+=strlen(cmd+2)+1;	//包含末尾\0. 
	Prog_ptr++; 
	return E_NOERR;	
} 

Errnum parse_FOR(char* cmd){
	char* exp1;
	char* exp2;
	char* pos_TO;
	
	//形式为"FOR I=1 TO 10" 
	if (cmd[4]<'A' || cmd[4]>'Z') return E_SYNTAX;
	pos_TO=strstr(cmd,"TO");
	if (pos_TO==NULL) return E_SYNTAX;

	Program[Prog_ptr].statement=C_FOR; 
	Program[Prog_ptr].var=cmd[4]-'A';	//第四位上为循环变量 
	Program[Prog_ptr].expr=Exp_ptr;
	exp1=cmd+6;
	pos_TO[-1]=0;
	exp2=pos_TO+3; 

	Exp_mem[Exp_ptr]=GCStat;
	Exp_ptr++;
	Exp_mem[Exp_ptr]=FOR_NOT_RUN;			//此FOR尚未执行或已退出 
	Exp_ptr++;
	strcpy(Exp_mem+Exp_ptr, exp1);
	Exp_ptr+=strlen(exp1)+1;	//包含末尾\0. 

	Exp_mem[Exp_ptr]=GCStat;
	Exp_ptr++;
	strcpy(Exp_mem+Exp_ptr, exp2);
	Exp_ptr+=strlen(exp2)+1;	//包含末尾\0. 

	Prog_ptr++;
	
	return E_NOERR;
}

//寻找对应的FOR.
//显然, 对应的FOR不一定是紧挨着的那个. 必须数前面有多少个FOR, 多少个NEXT... 
unsigned short find_corresponding_FOR(unsigned short ptr){
	unsigned char next_count;
	next_count=0;
	while(1){
		if (Program[ptr].statement==C_NEXT) next_count++; 
		if (Program[ptr].statement==C_FOR){
			next_count--; 
			if (next_count==0) return ptr;
		} 
		if (ptr==0) break;
		ptr--;
	}
	return NO_LINENUM;
}

Errnum parse_NEXT(char* cmd){
	//形式为"NEXT"或"NEXT I" 
	//这里可以现场算出对应的FOR来.
	//不过对程序进行sort时仍然会计算
	//因此这儿的计算代码都在行首注释掉了. 

//	unsigned short previous_FOR;
	Program[Prog_ptr].statement=C_NEXT;
//	previous_FOR=find_corresponding_FOR(Prog_ptr);
	if (cmd[4]!=0 && cmd[5]>='A' && cmd[5]<='Z'){
		Program[Prog_ptr].var=cmd[5]-'A';
	}else{
		Program[Prog_ptr].var=-1;
	} 
	Program[Prog_ptr].expr=0;
//	Program[Prog_ptr].expr=previous_FOR;
	Prog_ptr++;
//	//暂时不对这个错误进行处理
//	//排序后会自动修正 
//	if (previous_FOR==NO_LINENUM) return E_NEXT_WO_FOR;	
	 

	return E_NOERR;
}

Errnum parse_GOSUB_GOTO(char* cmd){
	char* pos_exp;
	//形式为"GOSUB exp".
	if (cmd[2]=='S'){ //GOSUB
		Program[Prog_ptr].statement=C_GOSUB; 
		pos_exp=cmd+6;
	}else if (cmd[2]=='T'){ //GOTO
		Program[Prog_ptr].statement=C_GOTO; 
		pos_exp=cmd+5;
		
	}else{
		return E_SYNTAX;
	}
	Program[Prog_ptr].var=-1;
	Program[Prog_ptr].expr=Exp_ptr;
	
	Exp_mem[Exp_ptr]=GCStat;
	Exp_ptr++;
	strcpy(Exp_mem+Exp_ptr, pos_exp);
	Exp_ptr+=strlen(pos_exp)+1;	//包含末尾\0. 
	Prog_ptr++; 
	return E_NOERR;	
}

Errnum parse_REM(char* cmd){
	//形式为"REM str". 
	Program[Prog_ptr].statement=C_REM; 
	Program[Prog_ptr].var=-1;
	Program[Prog_ptr].expr=Exp_ptr;
	
	Exp_mem[Exp_ptr]=GCStat;
	Exp_ptr++;
	strcpy(Exp_mem+Exp_ptr, cmd+4);
	Exp_ptr+=strlen(cmd+4)+1;	//包含末尾\0. 
	Prog_ptr++;
	return E_NOERR;
}

Errnum parse_RETURN(char* cmd){
	Program[Prog_ptr].statement=C_RETURN; 
	Program[Prog_ptr].var=-1;
	Program[Prog_ptr].expr=0;
	Prog_ptr++;
	return E_NOERR;
}

Errnum parse_IF(char* cmd){
	//形式为"IF A=B THEN GOTO 20"
	 
	char* pos_THEN;
	char* exp;
	char* then_stat; 
	pos_THEN=strstr(cmd,"THEN");
	if (pos_THEN==NULL) return E_SYNTAX;
	
	Program[Prog_ptr].statement=C_IF; 
	Program[Prog_ptr].var=-1;
	
	Program[Prog_ptr].expr=Exp_ptr;
	
	exp=cmd+3;
	pos_THEN[-1]=0;
	then_stat=pos_THEN+5;
	
 	Exp_mem[Exp_ptr]=GCStat;
	Exp_ptr++;
	strcpy(Exp_mem+Exp_ptr, exp);
	Exp_ptr+=strlen(exp)+1;	//包含末尾\0. 
	Prog_ptr++;
	
	return parse_line(then_stat, 65530);
}
Errnum parse_INPUT(char* cmd){
	//形式为"INPUT A"
	if (cmd[6]<'A' || cmd[6]>'Z') return E_SYNTAX;
	Program[Prog_ptr].statement=C_INPUT;
	Program[Prog_ptr].var=cmd[6]-'A';
	Program[Prog_ptr].expr=0;
	Prog_ptr++;
	return E_NOERR;
}

Errnum parse_PRINT(char* cmd){
	//形式是"PRINT exp"(var=-1)或 "PRINT "str "(var=-2)
	//或"PRINT exp;"(var=-3) 或 "PRINT 'str'"(var=-4) 
	//注意第二种形式的双引号只代表字串起始, 没有闭引号. 
	//第四种形式则有闭引号. 
	char* exp_start;
	Program[Prog_ptr].statement=C_PRINT;

	if (cmd[6]=='"'){
		Program[Prog_ptr].var=-2;
		exp_start=cmd+7;
	}else if (cmd[6]=='\''){
		Program[Prog_ptr].var=-4;
		//寻找闭引号并将其替换掉! 
		for(exp_start=cmd+strlen(cmd); exp_start>cmd+7; exp_start--){
			if (*exp_start=='\''){
				*exp_start=0;
				break; 
			}
		}
		exp_start=cmd+7;
	}else{
		exp_start=strstr(cmd+6, ";");
		if (exp_start==NULL){ 
			Program[Prog_ptr].var=-1;
		}else{
			Program[Prog_ptr].var=-3;
			*exp_start=0; 
		} 
		exp_start=cmd+6;
	}  
	
	Program[Prog_ptr].expr=Exp_ptr;
	Exp_mem[Exp_ptr]=GCStat;
	Exp_ptr++;
	strcpy(Exp_mem+Exp_ptr, exp_start);
	Exp_ptr+=strlen(exp_start)+1;	//包含末尾\0. 
	
	
	Prog_ptr++;
	return E_NOERR;	
} 

Errnum parse_END(char* cmd){
	Program[Prog_ptr].statement=C_END; 
	Program[Prog_ptr].var=-1;
	Program[Prog_ptr].expr=0;
	Prog_ptr++;
	return E_NOERR;
}

Errnum parse_PUSH(char* cmd){
	//形式为"PUSH A+B*3". 
	Program[Prog_ptr].statement=C_PUSH; 
	Program[Prog_ptr].var=-1;
	Program[Prog_ptr].expr=Exp_ptr;
	
	Exp_mem[Exp_ptr]=GCStat;
	Exp_ptr++;
	strcpy(Exp_mem+Exp_ptr, cmd+5);
	Exp_ptr+=strlen(cmd+5)+1;	//包含末尾\0. 
	Prog_ptr++;
	return E_NOERR;
}

Errnum parse_POP(char* cmd){
	//形式为"POP A"
	if (cmd[4]<'A' || cmd[4]>'Z') return E_SYNTAX;
	Program[Prog_ptr].statement=C_POP;
	Program[Prog_ptr].var=cmd[4]-'A';
	Program[Prog_ptr].expr=0;
	Prog_ptr++;
	return E_NOERR;
}


//解析一行. 命令是cmd, 行号是linenum. 
Errnum parse_line(char* cmd, unsigned short linenum){
	 
	Program[Prog_ptr].linenum=linenum;

	//去掉行号后面的空格 
	while(1){
		if (cmd[0]!=' ') break;
		cmd++;
	}
	//todo: 若干预处理, 去掉中间的多余空格等元素

	//解析指令
	if (cmd[1]=='='){	//LET 
		return parse_LET(cmd);
	}
	//由于全部都是return, 所以后面不用break! 
	switch(cmd[0]){
	case 'F':
		return parse_FOR(cmd);		
	case 'N':
		return parse_NEXT(cmd);
	case 'G':
		return parse_GOSUB_GOTO(cmd);
	case 'R':
		switch(cmd[2]){
		case 'M':
			return parse_REM(cmd); 
		case 'T':
			return parse_RETURN(cmd);			
		default:
			break;					
		} 
	case 'I':
		switch(cmd[1]){
		case 'F':
			return parse_IF(cmd);			
		case 'N':
			return parse_INPUT(cmd);		
		default: 
			break;
		}
	case 'P':
		switch(cmd[1]){ 
		case 'R': 
			return parse_PRINT(cmd);
		case 'U':
			return parse_PUSH(cmd);
		case 'O':
			return parse_POP(cmd); 
		} 
	case 'E':
		return parse_END(cmd); 	
	default:
		//没有这个命令... 
		return E_NOCMD; 
		break;	 
	}
}
