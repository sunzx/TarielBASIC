#include <stdlib.h>
#include <string.h>
#include "eval.h"
//这是一个简单的表达式recursive descent parser
//来自1990年IOCCC获奖作品DDS-BASIC(Diomidis Spinellis)的
//Michael Somos注释版

int Var[26];      //Var[0]=A, Var[1]=B, etc...
static char* Parse_ptr;
int parse_lvl1(void);
int parse_lvl2(void);
int parse_lvl3(void);
int parse_lvl4(void);
int parse_lvl5(void);
int parse_lvl6(void);

//"="和"<>"
int parse_lvl1(void){
	int o=parse_lvl2();
	switch (*Parse_ptr++){
	case '=':
		return o==parse_lvl1();
	case '#':
		return o!=parse_lvl1();
	default:
	Parse_ptr--;
	return o;
	}
}

//"<"和">"
int parse_lvl2(void){
	int o=parse_lvl3() ;
	switch(*Parse_ptr++){
	case '<':
		return o<parse_lvl2();
	case '>':
		return o>parse_lvl2();
	default:
		Parse_ptr--;
		return o;
	}
}

//"<="和">="
int parse_lvl3(void){
	int o=parse_lvl4();
	switch (*Parse_ptr++){
	case '$':
		return o<=parse_lvl3();
	case '!':
		return o>=parse_lvl3();
	default:
		Parse_ptr--;
		return o;
	}
}

int parse_lvl4(void){
	int o=parse_lvl5() ;
	switch (*Parse_ptr++){
	case '+':
		return o+parse_lvl4();
	case '-':
		return o-parse_lvl4();
	default:
		Parse_ptr--;
		return o;
	}
}

int parse_lvl5(void){
	int o=parse_lvl6();
	switch (*Parse_ptr++){
	case '*':
		return o*parse_lvl5();
	case '/':
		return o/parse_lvl5();
	default:
		Parse_ptr--;
		return o ;
	}
}

int parse_lvl6(void){
	int o;
	if (*Parse_ptr== '-'){
		Parse_ptr++;
		return -parse_lvl6();
	}else{
		if (*Parse_ptr>='0' && *Parse_ptr<='9'){
			//strtol将Parse_ptr指向的内容后面的数字转换成long int类型,
			//并将Parse_ptr更新到这个数字后面
			return strtol(Parse_ptr,&Parse_ptr,0);
		}else{
			if (*Parse_ptr=='('){
				Parse_ptr++;
				o=parse_lvl1();
				Parse_ptr++;
				return o;
			}else{
				return Var[(*Parse_ptr++)-'A'];
			}
		}
	}
}


int eval(char* expr){
    char* p;
    int res;
    //替换表达式中的双字节算符, 便于求值
	while ((p=strstr(expr,"<>"))) {(*p++)='#'; *p=' ';};
	while ((p=strstr(expr,"<="))) {(*p++)='$'; *p=' ';};
	while ((p=strstr(expr,">="))) {(*p++)='!'; *p=' ';};
	Parse_ptr=expr;
	res=parse_lvl1();
	//替换回来便于显示...
	while ((p=strstr(expr,"#"))) {(*p++)='<'; *p='>';};
	while ((p=strstr(expr,"$"))) {(*p++)='<'; *p='=';};
	while ((p=strstr(expr,"!"))) {(*p++)='>'; *p='=';};

	return res;
}
