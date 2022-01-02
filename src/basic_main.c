/*
	Tariel的BASIC解释器 v0.02 Alpha 

* 各种说明的位置
	指令说明在parser.c中 
	内部数据格式说明在interpreter.c中
	直接指令说明在下面: 
		本解释器实现了LIST, RUN, NEW, BYE, TRON, TROFF六个直接指令.
		作用分别为列出程序列表, 运行程序, 清空程序存储区域, 退出解释器, 开启
		跟踪功能, 关闭跟踪功能.
		 
* 各种常数定义的位置
	parser.h中定义最大行数MAXLINE(每一行占6字节的内存), 表达式存储区的字节数
		MAXEXPB(多出一个起始字节不使用).
	interpreter.h中定义GOSUB的深度GOSUB_DEPTH(每一层占2字节的内存), 数据栈的
		深度DSTACK_DEPTH(每一层占4字节的内存) 
	portable_io.h中定义最大的一个输入/输出行的字节数MAXLLEN 
	
	
	
* 技术状态
	几乎没有任何错误判断和处理. 输入程序时请先手工验证正确性. 
	能运行如下汉诺塔程序, 3层经过验证, 更多层尚未验证:
	
5 PRINT 'Input N: '
10 INPUT N
15 T=0
20 F=1
30 D=3
40 B=2
50 GOSUB 100
55 PRINT "
60 PRINT 'Total steps: '
70 PRINT T
80 END
100 REM Hanoi Tower
110 PUSH N
120 PUSH F
130 PUSH D
140 PUSH B
150 IF N>1 THEN GOTO 200
155 T=T+1
160 PRINT F;
170 PRINT '->'
180 PRINT D;
185 PRINT ' '
190 GOTO 450
200 N=N-1
210 REM Swap D and B
220 PUSH D
230 PUSH B
240 POP D
250 POP B
260 GOSUB 100
270 PUSH N
280 N=1
290 D=B
300 GOSUB 100
310 POP N
320 REM Restore F, D and B
330 POP B
340 POP D
350 POP F
360 PUSH F
370 PUSH D
380 PUSH B
390 REM Swap F and B
400 PUSH F
410 PUSH B
420 POP F
430 POP B
440 GOSUB 100
450 POP B
460 POP D
470 POP F
480 POP N
490 RETURN

* 与0.01的区别:
	修正了若干bug, 见本文件尾部的列表:

	增加了PRINT exp;和PRINT 'string'功能. 修正了关于PRINT的文档.
*/ 







#include <stdio.h>
#include <stdlib.h>

#include "eval.h"
#include "parser.h"
#include "interpreter.h"
#include "portable_io.h"



int main(int argc, char *argv[])
{	 
	prog_init();

	while(1){
		portable_puts(">"); 
		portable_gets(Prbuf);
		if (interpreter_immediate_line(Prbuf)==S_STOP) break;
	} 

	return 0;
}


/*
bug修正列表

0.02比0.01修正的:
		1)	关于表达式空间垃圾回收器的若干问题. 如: 
			>10 FOR I=1 TO 10
			>LIST
			10 FOR I=1 TO 10
			Prog_ptr: 1/1000 lines.
			Exp_ptr: 9/5000 bytes.
			>10
			>LIST	 
			(此时程序崩溃)
			以及GC的内存泄漏. 
			问题在interpreter.c的prog_GC()中.
			
		2)
			>10 RETURN
			>20 RETURN
			>10
			>20
			>LIST
			20 RETURN				(20不flush删不掉)
			Prog_ptr: 1/1000 lines.
			Exp_ptr: 1/5000 bytes. 
			>20
			>LIST
			Prog_ptr: 0/1000 lines.
			Exp_ptr: 1/5000 bytes.
			问题在interpreter.c的ln2pgline()中. 
			
		3) 9开头的行无法输入. 问题在interpreter.c的
			interpreter_immediate_line() 中.
			
		4) 
			>10 FOR I=1 TO 10
			>20 FOR J=1 TO 10
			>30 FOR K=1 TO 10
			>40 PRINT I;
			>50 PRINT ' '
			>60 PRINT J;
			>70 PRINT ' '
			>80 PRINT K
			>90 NEXT K
			>100 NEXT J
			>110 NEXT I
			>
			>LIST
			10 FOR I=1 TO 10
			20 FOR J=1 TO 10
			30 FOR K=1 TO 10
			40 PRINT I;
			50 PRINT ' '
			60 PRINT J;
			70 PRINT ' '
			80 PRINT K
			90 NEXT K
			100 NEXT J
			110 NEXT I
			Prog_ptr: 11/1000 lines.
			Exp_ptr: 40/5000 bytes.
			>
			>10 FOR I=1 TO 3
			>20 FOR J=1 TO 3
			>30 FOR K=1 TO 3
			>
			>LIST
			Prog_ptr: 0/1000 lines.
			Exp_ptr: 31/5000 bytes.
			>		 
			问题在interpreter.c的sort_prog中.  


*/ 
