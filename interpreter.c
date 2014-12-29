#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "eval.h"
#include "parser.h"
#include "portable_io.h"
#include "interpreter.h"

/*
程序在内存中的结构如下:
	行号 指令 变量 表达式

	* 行号是一个无符号整型数, 从1到65529. 0代表该行不存在. 65530~35的数表示特殊
	  的行.
	* 指令是一个字节. 其中
		* F 代表 FOR
		* N '''' NEXT
		* S '''' GOSUB
		* R '''' RETURN
		* G '''' GOTO
		* I '''' IF
		* U '''' INPUT
		* P '''' PRINT
		* L '''' LET
		* > '''' PUSH
		* < '''' POP
		* ' '''' REM
		* E '''' END
	* 变量是一个有符号字节, 代表指令涉及的独立变量(如INPUT里面的var)或某些特殊值.
		注记: 由于只有26个变量(A~Z), 所以多余的空间可以复用作特殊值.
	* 表达式是一个无符号整型数, 一般是表达式到Exp_mem头部的相对值, 有时也可能有
	  特殊作用.

	* 每条指令的结构如下:
		* FOR
			格式: F var exp2
				  (行号为65531的FOR_INFO语句)
			说明: var是循环变量; exp2指向一个表达式二元组的起始位置, 这个二元组由
				  两个表达式组成. 第一个表达式的第一个字节是是否第一次执行的标记,
				  从第二个字节开始是初始值的表达式; 第二个表达式是终止值.
		* NEXT
			格式: N var|* exp_sub
			说明: var是可选的循环变量, 只有说明作用. exp字段指向对应的FOR语句的
				  下标(不是行号!).
		* GOSUB
			格式: S * exp
			说明: var字段可以是任意值. exp指向一个表达式. 表达式求出的值是一个行
				  号, GOSUB语句跳转到这个行号对应的语句, 并在遇到第一个RETURN时
				  返回下一行.
		* RETURN
			格式: R * *
   			说明: RETURN语句返回到GOSUB栈的栈顶. 参数可以是任意值.
		* GOTO
			格式: G * exp
			说明: var字段可以是任意值. exp指向一个表达式. 表达式求出的值是一个行
				  号, GOTO语句跳转到这个行号对应的语句, 不作任何其他处理.
		* IF
			格式: I * exp
				  (行号为65530的一行语句)
			说明: exp指向一个表达式. 若这个表达式求值结果为真, 则执行紧接着的行
				  号为65530的语句; 否则跳过这个语句, 执行下面的正常行号的语句.
			注记: 这儿实现的是 "IF exp THEN statement"这种格式的IF. 由于事实上是
				  一个复合语句, 所以要占用两行空间, 并使用非标准行号.
		* INPUT
			格式: U var *
			说明: var字段是一个变量. INPUT语句读取键盘输入, 并将键盘输入存入这个
				  变量.
		* PRINT
			格式: P -1 exp | P -2 exp | P -3 exp | P -4 exp
			说明: PRINT语句有四种格式.
				  第一种格式对应"PRINT exp", 即将exp指向的表达式求值, 并打印出来;
				  第二种格式对应"PRINT "string", 即将exp指向的表达式空间中存储的
				    字符串打印出来.
				  第三种格式对应"PRINT exp;", 即不换行的第一种格式;
				  第四种格式对应"PRINT 'string', 即不换行的第二种格式.
		* LET
			格式: L var exp
			说明: LET语句用于将exp指向的值求出并赋给var.
			注记: LET语句输入时不输入LET, 直接用"var=exp"的形式.
		* PUSH
			格式: > * exp
			说明: PUSH语句将exp求值, 并压入栈中.
			注记: 本解释器没有提供数组功能, 可用变量也只有A~Z 26个. 为了弥补这一
				  不足, 提供一个栈用于暂存数据.
		* POP
			格式: < var *
			说明: POP语句将栈顶值弹出到var中.
		* REM
			格式: ' * exp
			说明: REM语句用于程序注释. 注释字串存储于表达式空间.
		* END
			格式: E * *
			说明: END语句用于在程序中间或结尾结束程序.
			注记: 在输入程序时, 不一定有END语句; 在存储程序(功能尚未实现)时, 一个
			行号为0的END语句代表程序的结束和表达式区域的开始.

	在初始状态时, 程序被行号为0的END语句填满.

Exp_mem的一般结构如下:
	(标记 表达式字串)*n
	* 标记用于判断表达式是否被引用. 若标记=1, 则说明有语句引用标记.
	  后面跟着表达式组里面的表达式的可读形式, 每一个都是一个ASCIZ字串.
	* 对FOR语句而言, 引用的第一个表达式字串和标记之间多一个字节, 用于存储是否第
	  一次执行.
    * 表达式存储空间也可存储诸如注释字串和PRINT字串的字符串信息.

*/

unsigned short isClean;			//若程序存储状态一致(即所有行按顺序, 所有跳转指针与
								//行号保持一致), 则为1; 否则为0.


unsigned short Pc;				//程序指针, 指向当前运行的Program下标
unsigned char Trace_stat;		//Trace_stat为1时提供程序跟踪功能

unsigned short Gosub_sp;		//GOSUB栈指针
unsigned short Gosub_stack[GOSUB_DEPTH];	//GOSUB栈

unsigned short Data_sp; 		//数据栈指针
int Data_stack[DSTACK_DEPTH];	//数据栈

unsigned short prog_linedel(unsigned short line);
unsigned short ln2pgline(unsigned short linenum);

//初始化程序空间.
void prog_init(void){
	unsigned int i;
	Prog_ptr=0;
	Exp_ptr=1;
	Gosub_sp=0;
	Data_sp=0;
	Pc=0;
	isClean=1;
	Trace_stat=0;

	for(i=0; i<MAXLINE; i++){
		Program[i].linenum=0;
		Program[i].statement=C_END;
		Program[i].var=-1;
		Program[i].expr=0;
	}
}


//输入一行程序并解析成内部格式.
//这一行程序的开始没有多余的空格, 所有关键字均大写.
//如果输入一个在内存中存在的行, 则作用为修改该行.
//如果输入一个无法解析的行, 并且该行在内存中存在
//那么作用为删除该行.
Errnum prog_input_line(char* line){
	char* cmd;
	Errnum parse_res;
	unsigned short linenum;
	linenum=strtol(line, &cmd, 10);
	//如果该行在内存中已经存在, 则删除之.
	if (ln2pgline(linenum)!=NO_LINENUM){
		prog_linedel(linenum);
	}
	if (cmd[0]==0){
			prog_linedel(linenum);
			return E_NOCMD;
	}

	parse_res=parse_line(cmd, linenum);
	//如果没有这个命令...

	if (parse_res==E_NOCMD){
		//那么删除一行.
		prog_linedel(linenum);
	}
	return parse_res;
}


void statement_name(char stat, char* name){

	if (stat==C_FOR){
		strcpy(name,"FOR");
	}else if (stat==C_NEXT){
		strcpy(name,"NEXT");
	}else if (stat==C_GOSUB){
		strcpy(name,"GOSUB");
	}else if (stat==C_RETURN){
		strcpy(name,"RETURN");
	}else if (stat==C_GOTO){
		strcpy(name,"GOTO");
	}else if (stat==C_IF){
		strcpy(name,"IF");
	}else if (stat==C_INPUT){
		strcpy(name,"INPUT");
	}else if (stat==C_PRINT){
		strcpy(name,"PRINT");
	}else if (stat==C_LET){
		strcpy(name,"LET");
	}else if (stat==C_REM){
		strcpy(name,"REM");
	}else if (stat==C_END){
		strcpy(name,"END");
	}else if (stat==C_PUSH){
		strcpy(name,"PUSH");
	}else if (stat==C_POP){
		strcpy(name,"POP");
	}
}

//列出一行程序. i是程序行的下标.
void print_line(unsigned short i){
	char statname[7];

	if (Program[i].statement!=C_LET){
		statement_name(Program[i].statement, statname);
		sprintf(Prbuf, "%s ", statname);
		portable_puts(Prbuf);
	}

	if (Program[i].statement==C_GOSUB ||
		Program[i].statement==C_GOTO ||
		Program[i].statement==C_PRINT ||
		Program[i].statement==C_REM ||
		Program[i].statement==C_PUSH){

		//处理PRINT.  只有PRINT有var=-1..-4的情况, 故可以跟GOSUB, GOTO等一起处理.
		if(Program[i].var==-2) {
			portable_puts("\""); //只有PRINT有-2的情况
		}else if(Program[i].var==-4){
			portable_puts("'");
		}
		sprintf(Prbuf, "%s", Exp_mem+Program[i].expr+1);
		portable_puts(Prbuf);
		if (Program[i].var==-3){
			portable_puts(";");
		}else if (Program[i].var==-4){
			portable_puts("'");
		}

	}else if (Program[i].statement==C_INPUT ||
		Program[i].statement==C_NEXT||
		Program[i].statement==C_POP){

		if (Program[i].var>=0 && Program[i].var<=25){
			sprintf(Prbuf, "%c", Program[i].var+'A');
			portable_puts(Prbuf);
		}
	}else if (Program[i].statement==C_LET){
		sprintf(Prbuf, "%c=%s", Program[i].var+'A', Exp_mem+Program[i].expr+1);
		portable_puts(Prbuf);
	}else if (Program[i].statement==C_FOR){
		sprintf(Prbuf, "%c=%s TO %s", Program[i].var+'A', Exp_mem+Program[i].expr+2,
			Exp_mem+Program[i].expr+2+strlen(Exp_mem+Program[i].expr+2)+2);
		portable_puts(Prbuf);
	}else if (Program[i].statement==C_IF){
		sprintf(Prbuf, "%s THEN ", Exp_mem+Program[i].expr+1);
		portable_puts(Prbuf);
		print_line(i+1);
	}
}

//按照输入的格式列出程序.
void prog_list(void){
	unsigned short i;
	//char* expr_p;
	for(i=0; i<Prog_ptr; i++){
		//if (Program[i].linenum==0) break;
		if (Program[i].linenum>0 && Program[i].linenum<=65529){
			sprintf(Prbuf, "%d ", Program[i].linenum);
			portable_puts(Prbuf);
			print_line(i);
			portable_puts("\n\r");
		}
	}
}

//由行号反查下标.
//这个子程序的效率对解释器的效率影响巨大!
//现在使用最简单低效的办法: 顺序查找
//待优化.
unsigned short ln2pgline(unsigned short linenum){
	unsigned short i;
//	printf("*%d\n", linenum);
	for(i=0; i<Prog_ptr; i++){
//		printf("*%d %d %d\n", linenum, i, Program[i].linenum);
		if (Program[i].linenum==linenum) return i;
		//if (Program[i].linenum==0) break;	//这行导致了0.01版的bug 2)
	}
	return NO_LINENUM;
}

//运行一行程序并根据情况更新progline_ptr指向的值.
//注意参数是一个地址!
Status line_run(unsigned short* progline_ptr){
	unsigned short curline;
	int eval_res;			//表达式求值的结果
	curline=*progline_ptr;
	(*progline_ptr)++;		//指向下一行...

	switch(Program[curline].statement){
		case C_FOR:
			if (Exp_mem[Program[curline].expr+1]==FOR_NOT_RUN){	//第一次执行, 设置初值

				Exp_mem[Program[curline].expr+1]=FOR_RUNNING;
				Var[Program[curline].var]=eval(Exp_mem+Program[curline].expr+2);
			}else{	//非第一次执行, 递增值
				Var[Program[curline].var]++;
			}
			break;
		case C_NEXT:
			eval_res=Program[curline].expr;
			if (eval(
				Exp_mem+Program[eval_res].expr+2+
				strlen(Exp_mem+Program[eval_res].expr+2)+2) ==
				Var[Program[Program[curline].expr].var]){	//如果对应的FOR的var的值达到了exp2的值
				Exp_mem[Program[Program[curline].expr].expr+1]=FOR_NOT_RUN;
			}else{	//goto FOR
				*progline_ptr=Program[curline].expr;
			}
			break;
		case C_GOSUB:
			Gosub_stack[Gosub_sp]=*progline_ptr;
			Gosub_sp++;
			*progline_ptr=ln2pgline(eval(Exp_mem+Program[curline].expr+1));
			break;
		case C_RETURN:
			Gosub_sp--;
			*progline_ptr=Gosub_stack[Gosub_sp];
			break;
		case C_GOTO:
			//表达式求出值, 并更新progline_ptr指向的位置
			*progline_ptr=ln2pgline(eval(Exp_mem+Program[curline].expr+1));
			break;
		case C_IF:
			//根据求出的表达式的值决定是否执行紧跟着的语句
			if (eval(Exp_mem+Program[curline].expr+1)){
				return line_run(progline_ptr);
			}else{
				(*progline_ptr)++;	//如果不执行, 下一行也要跳过!
			}
			break;
		case C_INPUT:
			//输入一个值, 并赋给一个变量
			portable_input_int(&eval_res);

			Var[Program[curline].var]=eval_res;
			break;
		case C_PRINT:
			switch(Program[curline].var){
			case -1:
			case -3:
				//打印表达式的值
				sprintf(Prbuf, "%d", eval(Exp_mem+Program[curline].expr+1));
				portable_puts(Prbuf);
				break;
			case -2:
			case -4:
				//打印字串
				//sprintf(Prbuf, "%s", Exp_mem+Program[curline].expr+1);
				portable_puts(Exp_mem+Program[curline].expr+1);

				break;

			default:
				break;
			}
			if (Program[curline].var==-1 || Program[curline].var==-2){
				portable_puts("\n\r");
			}
			break;
		case C_LET:
			Var[Program[curline].var]=eval(Exp_mem+Program[curline].expr+1);
			break;
		case C_PUSH:
			Data_stack[Data_sp]=eval(Exp_mem+Program[curline].expr+1);
			Data_sp++;
			break;
		case C_POP:
			Data_sp--;
			Var[Program[curline].var]=Data_stack[Data_sp];
			break;
		case C_REM:
			//不做任何事情.
			break;
		case C_END:
			//返回S_STOP, 使调用者停止执行.
			return S_STOP;
			break;
		default:
			break;
	}
	return S_RUNNING;
}

//从头开始运行程序.
void prog_run(void){
	Pc=0;
	while(1){
		//这就是trace功能, 就这么简单的三行...
		if (Trace_stat && Program[Pc].linenum!=0){
			sprintf(Prbuf, "[%d] ",Program[Pc].linenum);
			portable_puts(Prbuf);
		}
		if (line_run(&Pc)==S_STOP) break;
	}
}


//按照行号顺序对程序进行排序.
//排序时注意:
//	1. IF语句后面跟的行号为65530的语句与IF语句作为一个整体参与排序.
//	2. 所有的NEXT语句的exp字段需要重新生成
void sort_prog(void){
	unsigned short i,j,k;
	unsigned short start, min, minptr;
	char flag;
	struct Prog_line tmpline;

	//将行号为0的行都pack到后面!
	for (i=0; i<Prog_ptr; i++){
		flag=0;
		for(j=0; j<Prog_ptr; j++){		//0.01版bug 4): for(j=i...
			if (Program[j].linenum==0){
				tmpline=Program[j];
				Program[j]=Program[j+1];
				Program[j+1]=tmpline;
				flag=1;
			}
		}
		if (flag==0) break;
	}
/*
	for (i=0; i<Prog_ptr; i++){
		printf("*%d\n", Program[i].linenum);
	}
*/
	//重新设置Prog_ptr
	for (i=0; i<Prog_ptr; i++){
		if (Program[i].linenum==0){
			Prog_ptr=i;
			break;
		}
	}

	//pack完毕...
	start=0;
	for (i=0; i<Prog_ptr; i++){

		if (start>=Prog_ptr) break;
		//寻找从start开始的里面的最小行号的行
		min=65535;
		minptr=start;
		for(j=start; j<Prog_ptr; j++){
			if (Program[j].linenum<min && Program[j].linenum>0 &&
				Program[j].linenum<65530) {
				min=Program[j].linenum;
				minptr=j;
			}
			if (Program[j].linenum==0) break;
		}
		if (minptr!=start){ 	//如果需要交换的话
			//如果被交换的行(start)和换上来的行(minptr)都不是IF, 则直接交换
			if (Program[minptr].statement!=C_IF && Program[start].statement!=C_IF){
				tmpline=Program[minptr];
				Program[minptr]=Program[start];
				Program[start]=tmpline;
				start++;
			//如果被交换的行(start)和换上来的行(minptr)都是IF, 则交换两个
			}else if (Program[minptr].statement==C_IF && Program[start].statement==C_IF){
				tmpline=Program[minptr];
				Program[minptr]=Program[start];
				Program[start]=tmpline;

				tmpline=Program[minptr+1];
				Program[minptr+1]=Program[start+1];
				Program[start+1]=tmpline;
				start+=2;

			//如果后面的是if, 前面的不是if, 则将从前面的到后面的if为止的行全都向后
			//移动一位...
			}else if (Program[minptr].statement==C_IF && Program[start].statement!=C_IF){
				//Program[minptr]会被冲掉, 所以保留一份...
				tmpline=Program[minptr+1];
				for(k=minptr+1; k>start; k--){
					Program[k]=Program[k-1];
				}

				Program[start]=tmpline;

				//minptr+1位置和start+1位置交换
				tmpline=Program[minptr+1];
				Program[minptr+1]=Program[start+1];
				Program[start+1]=tmpline;

				//start和start+1位置交换
				tmpline=Program[start+1];
				Program[start+1]=Program[start];
				Program[start]=tmpline;

				start+=2;
			//其余情况(如果前面的是if, 后面的不是if):
			//
			}else{
				tmpline=Program[start];
				for(k=start; k<minptr; k++){
					Program[k]=Program[k+1];
				}
				Program[minptr]=tmpline;

				tmpline=Program[minptr-1];
				Program[minptr-1]=Program[start];
				Program[start]=tmpline;

				tmpline=Program[minptr-1];
				Program[minptr-1]=Program[minptr];
				Program[minptr]=tmpline;
				start++;
			}
		}else{
			start++;
			if (Program[minptr].statement==C_IF) start++;
		}
	}

	//一致化NEXT语句
	for (i=0; i<Prog_ptr; i++){
		if (Program[i].statement==C_NEXT){
			Program[i].expr=find_corresponding_FOR(i);
		}
	}
}


//垃圾回收!
//这是最麻烦的一个地方...
//需要动态申请一个内存区域作为表达式的替换表...
void prog_GC(void){
	unsigned short* sub_tab;
	unsigned short i, j, totalexp, dellen;
//	unsigned short p;
	//计算程序中的表达式总数, 在表达式存储区域进行标记, 并申请动态内存作替换表
	totalexp=0;
	for (i=0; i<Prog_ptr; i++){
		if (Program[i].statement==C_FOR){
			totalexp+=2;
			Exp_mem[Program[i].expr]=~Exp_mem[Program[i].expr];
			Exp_mem[Program[i].expr+2+strlen(Exp_mem+Program[i].expr+2)+1]=
				~Exp_mem[Program[i].expr+2+strlen(Exp_mem+Program[i].expr+2)+1];
			continue;
		}
		if (Program[i].statement==C_GOSUB ||
			Program[i].statement==C_GOTO ||
			Program[i].statement==C_IF ||
			Program[i].statement==C_PRINT ||
			Program[i].statement==C_LET ||
			Program[i].statement==C_REM ||
			Program[i].statement==C_PUSH){

			totalexp+=1;
			Exp_mem[Program[i].expr]=~Exp_mem[Program[i].expr];
			continue;
			}
	}

	//if (totalexp==0) return;

	sub_tab=malloc(sizeof(unsigned short)*totalexp);
	if (sub_tab==NULL) return;	//这儿需要增加错误处理!


	//再来一遍, 把sub_tab填满...
	totalexp=0;
	for (i=0; i<Prog_ptr; i++){
		if (Program[i].statement==C_FOR){

			sub_tab[totalexp]=Program[i].expr;
			totalexp+=1;

			sub_tab[totalexp]=
				Program[i].expr+2+strlen(Exp_mem+Program[i].expr+2)+1;
			totalexp+=1;
			continue;
		}
		if (Program[i].statement==C_GOSUB ||
			Program[i].statement==C_GOTO ||
			Program[i].statement==C_IF ||
			Program[i].statement==C_PRINT ||
			Program[i].statement==C_LET ||
			Program[i].statement==C_REM ||
			Program[i].statement==C_PUSH){

			sub_tab[totalexp]=Program[i].expr;
			totalexp+=1;
			continue;
			}
	}


/*
//调试时用于打印Exp_mem内容的程序段. 需要声明变量unsigned short p.
	for(p=1; p<Exp_ptr; p++){
		if (Exp_mem[p]>=32 && Exp_mem[p]<=126){
			printf("%c",Exp_mem[p]);
		}else if (Exp_mem[p]==1){
			printf("(%d^0)",p, Exp_mem[p]);
		}else if(Exp_mem[p]==-2){
			printf("(%d^1)",p, Exp_mem[p]);
		}else{
			printf("(%d*%d)",p, Exp_mem[p]);
		}
	}
	printf("\n\r");
*/

	//对没有标记到的表达式进行删除. 删除时随时更新替换表.
	i=1;	//Exp_mem的0位置不使用, 所以从1开始
	while(1){
		if (Exp_mem[i]==GCStat){
			dellen=strlen(Exp_mem+i)+1;		//加上末尾的0就是+1
			//待检查边界是否正确!
//			printf(">%d %d %d*\n", i, dellen, Exp_ptr);


			//printf(">%d %d %d %d*\n",Exp_ptr, i, i+dellen, Exp_ptr-(i+dellen));
			//下面是终止条件.
			//0.01版 bug 1)是由于终止条件不正确.
			if (Exp_ptr-(i+dellen)==0){
				Exp_ptr=i;
				break;
			}
			memmove(Exp_mem+i, Exp_mem+i+dellen, Exp_ptr-(i+dellen));

			//更新替换表: 比i大的项目都要减去dellen!
			for(j=0; j<totalexp; j++){
				if (sub_tab[j]>i) sub_tab[j]-=dellen;
			}
			i-=1;
			Exp_ptr-=dellen;
			//printf(">%d %d*\n", i, Exp_ptr);
		}
		i++;
		if (i>Exp_ptr) break;
	}

	//根据替换表的内容更新程序.
	//由于替换表与程序的顺序一致, 所以可以直接替换回去.
	totalexp=0;
	for (i=0; i<Prog_ptr; i++){
		if (Program[i].statement==C_FOR){
			//为了使用strlen计算长度的方便, FOR语句的表达式参数按两个表达式计算.
			//但第二个表达式总是跟在第一个表达式后面, 故毋须在程序中进行刷新.
			Program[i].expr=sub_tab[totalexp];
			totalexp+=2;
			continue;
		}
		if (Program[i].statement==C_GOSUB ||
			Program[i].statement==C_GOTO ||
			Program[i].statement==C_IF ||
			Program[i].statement==C_PRINT ||
			Program[i].statement==C_LET ||
			Program[i].statement==C_REM ||
			Program[i].statement==C_PUSH){

			Program[i].expr=sub_tab[totalexp];
			totalexp+=1;
			continue;
			}
	}
	//toggle GCStat
	GCStat=~GCStat;
	free(sub_tab);
}
//整理程序空间, 将其压缩(去掉被删除的行和没有引用的表达式等操作)并一致化(使各种跳
//转的指针与行号保持一致).
void prog_flush(void){
	//首先对程序存储空间进行排序
	sort_prog();

	//然后做垃圾回收...
	prog_GC();
}

//删除一行. 注意必须人工flush!
//返回删除的行的指针.
unsigned short prog_linedel(unsigned short line){
	unsigned short lnptr;
	lnptr=ln2pgline(line);
	if (lnptr!=NO_LINENUM){

		if (Program[lnptr].statement==C_IF){		//对于IF语句, 需要同时删除后面的特殊语句!
			Program[lnptr+1].linenum=0;
			Program[lnptr+1].statement=C_END;
			Program[lnptr+1].var=-1;
			Program[lnptr+1].expr=0;
		}

		Program[lnptr].linenum=0;
		Program[lnptr].statement=C_END;
		Program[lnptr].var=-1;
		Program[lnptr].expr=0;

	}
	return lnptr;

}


//解释一个输入行.
Status interpreter_immediate_line(char* line){
	//首先chomp该行, 除去所有行末空格.
	unsigned short i;
	if (strlen(line)!=0){
		for (i=strlen(line)-1; i<=strlen(line)-1; i--){
			if (line[i]==' '){
				line[i]=0;
			}else{
				break;
			}
		}
	}

	//如果开始是一个行号(1~9)...
	if (line[0]>'0' && line[0]<='9'){	//此处修正0.01版bug 3). 本来是小于九.
		//则清除isClean这个标签. 如果LIST或者RUN之前这个标签不一致, 则需要flush.
		isClean=0;
		prog_input_line(line);
	}else{
		//立即命令! 有LIST, RUN, NEW, BYE, TRON, TROFF六个
		//注意, 这个解释器不能直接执行BASIC指令!
		switch (line[0]){
		case 'L':
			if (!isClean){
				prog_flush();
				isClean=1;
			}
			prog_list();
			sprintf(Prbuf, "Prog_ptr: %d/%d lines.\n\rExp_ptr: %d/%d bytes.\n\r",
				Prog_ptr,MAXLINE, Exp_ptr-1, MAXEXPB);
			portable_puts(Prbuf);
			break;

		case 'R':
			if (!isClean){
				prog_flush();
				isClean=1;
			}
			prog_run();
			break;

		case 'N':
			prog_init();
			break;

		case 'B':
			return S_STOP;
			break;
		case 'T':
			switch(line[3]){
			case 'N':
				Trace_stat=1;
				break;
			case 'F':
				Trace_stat=0;
				break;
			default:
				break;
			}
		default:
			break;
		}
	}
	return S_RUNNING;
}

