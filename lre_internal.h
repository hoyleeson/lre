#ifndef _LRE_LRE_INTERNAL_H
#define _LRE_LRE_INTERNAL_H

#include "lre.h"

int lre_calc_int(int a, int b, int op);
double lre_calc_dobule(double a, double b, int op);

int lre_macro_init(void);
void lre_macro_release(void);

void lre_exec_detail_init(struct lre_exec_detail *detail, int cap);
void lre_exec_detail_release(struct lre_exec_detail *detail);

#endif

