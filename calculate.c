#include <stdio.h>
#include <string.h>

#include "lre_internal.h"

double lre_calc_dobule(double a, double b, int op)
{
#define CALC(opcode, op) \
	case SYNTAX_SYM_##opcode: \
		return a op b

	switch(op) {
		CALC(ADD, +);
		CALC(SUB, -);
		CALC(MUL, *);
		CALC(DIV, /);
		default:
		loge("Unsupport double calc opt:%d", op);
		return RET_DEAD_VAL;
	}
	return RET_DEAD_VAL;
#undef CALC
}

int lre_calc_int(int a, int b, int op)
{
#define CALC(opcode, op) \
	case SYNTAX_SYM_##opcode: \
		return a op b

	switch(op) {
		CALC(ADD, +);
		CALC(SUB, -);
		CALC(MUL, *);
		CALC(DIV, /);
		default:
		loge("Unsupport int calc opt:%d", op);
		return RET_DEAD_VAL;
	}
#undef CALC
	return RET_DEAD_VAL;
}


int lre_compare_int(int a, int b, int op)
{
#define COMPARE(opcode, op) \
	case SYNTAX_SYM_##opcode: \
		return a op b

	switch(op) {
		COMPARE(EQUAL, ==);
		COMPARE(NOT_EQUAL, !=);
		COMPARE(GT, >);
		COMPARE(LT, <);
		COMPARE(GT_OR_EQUAL, >=);
		COMPARE(LT_OR_EQUAL, <=);
		default:
		loge("Unsupport int compare op:%d", op);
		return RET_DEAD_VAL;
	}
#undef COMPARE
	return RET_DEAD_VAL;
}

int lre_compare_double(double a, double b, int op)
{
#define COMPARE(opcode, op) \
	case SYNTAX_SYM_##opcode: \
		return a op b

	switch(op) {
		COMPARE(EQUAL, ==);
		COMPARE(NOT_EQUAL, !=);
		COMPARE(GT, >);
		COMPARE(LT, <);
		COMPARE(GT_OR_EQUAL, >=);
		COMPARE(LT_OR_EQUAL, <=);
		default:
		loge("Unsupport double compare op:%d", op);
		return RET_DEAD_VAL;
	}
#undef COMPARE
	return RET_DEAD_VAL;
}

int lre_compare_string(char *a, char *b, int op)
{
	int ret;

	ret = strcmp(a, b);

#define COMPARE(opcode, op) \
	case SYNTAX_SYM_##opcode: \
		return ret op 0

	switch(op) {
		COMPARE(EQUAL, ==);
		COMPARE(NOT_EQUAL, !=);
		COMPARE(GT, >);
		COMPARE(LT, <);
		COMPARE(GT_OR_EQUAL, >=);
		COMPARE(LT_OR_EQUAL, <=);
		default:
		loge("Unsupport string compare op:%d", op);
		return RET_DEAD_VAL;
	}
#undef COMPARE
	return RET_DEAD_VAL;
}

