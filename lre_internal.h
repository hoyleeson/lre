#ifndef _LRE_LRE_INTERNAL_H
#define _LRE_LRE_INTERNAL_H

#include "lre.h"

int lre_calc_int(int a, int b, int op);
double lre_calc_dobule(double a, double b, int op);

void lre_exec_detail_init(struct lre_exec_detail *detail, int cap);
void lre_exec_detail_release(struct lre_exec_detail *detail);


#define LRE_MACRO_ARGC_MAX 	(16)
struct lre_macro;

int lre_macro_init(void);
void lre_macro_release(void);

struct lre_macro *lre_find_macro(const char *str, int argc);
int lre_has_macro(const char *str);
char *lre_create_code_by_macro(struct lre_macro *macro, 
		int argc, const char **args);


#endif

