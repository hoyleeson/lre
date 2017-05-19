#ifndef _LRE_LRE_H
#define _LRE_LRE_H

/*
 * lre: logic rule engine
 * lrc: logic rule component
 * */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _out
#define _out
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif


struct lre_value {
#define LRE_ARG_TYPE_UNKNOWN 	(0)
#define LRE_ARG_TYPE_INT 		(1)
#define LRE_ARG_TYPE_DOUBLE 	(2)
#define LRE_ARG_TYPE_STRING 	(3)
	int type;
	union {
		int valint;
		double  valdouble;
		char *valstring;
	};
};

struct lre_exec_detail {
	char *detail;
	int len;
	int cap;
};

struct lrc_object {
	int type;
#define LRC_OBJECT_FUNC 		(0)
#define LRC_OBJECT_CALL 		(1)
	union {
		int (*execfunc)(struct lrc_object *);
		int (*execcall)(struct lrc_object *, struct lre_value *);
	};
	struct lre_exec_detail *detail;
};

typedef struct lrc_object lrc_obj_t;

struct lre_result {
	int result;
	int errcode;
	char *details;
};

/* Logic operation results (true or false) */
#define LRE_RESULT_FALSE 		(0)
#define LRE_RESULT_TRUE 		(1)
#define LRE_RESULT_UNKNOWN 		(0xdeaddeed)

/* Program execute return (success or failed code) */
#define LRE_RET_OK 			(0)
/* Failed code the same to errno */
#define LRE_RET_ERROR 		(0xdeaddead)

int lre_macro_init(void);

int lre_calc_int(int a, int b, int op);
double lre_calc_dobule(double a, double b, int op);

int lre_compare_int(int a, int b, int op);
int lre_compare_double(double a, double b, int op);
int lre_compare_string(char *a, char *b, int op);


void lre_exec_detail_init(struct lre_exec_detail *detail, int cap);
void lre_exec_detail_release(struct lre_exec_detail *detail);
int lre_push_exec_detail(struct lrc_object *obj, const char *str);

int lre_init(void);
int lre_execute(const char *code, _out struct lre_result *res);
void lre_release(void);
int lre_get_results(void);

static inline int vaild_lre_results(int res)
{
	return (res == LRE_RESULT_TRUE || res == LRE_RESULT_FALSE);
}

/***********************************************************/

struct lrc_stub_arg {
	char *keyword;
	char *description;
	int (*handler)(lrc_obj_t *, struct lre_value *);
};

struct lrc_stub_expr {
	char *keyword;
	char *description;
	int (*handler)(lrc_obj_t *, int, struct lre_value *);
};

struct lrc_stub_var {
	char *keyword;
	char *description;
	int (*handler)(lrc_obj_t *, _out struct lre_value *);
};

struct lrc_stub_call {
	char *keyword;
	char *description;
	lrc_obj_t *(*handler)(void);

	struct lrc_stub_arg *args;
	int argcount;
	struct lrc_stub_var *vars;
	int varcount;
};

struct lrc_stub_func {
	char *keyword;
	char *description;
	lrc_obj_t *(*handler)(void);

	struct lrc_stub_arg *args;
	int argcount;
	struct lrc_stub_expr *exprs;
	int exprcount;
	struct lrc_stub_var *vars;
	int varcount;
	struct lrc_stub_func *funcs;
	int funccount;
};

struct lrc_module {
	char *name;
	struct lrc_stub_func *funcs;
	int funccount;
	struct lrc_stub_call *calls;
	int callcount;
};

int lrc_module_register(struct lrc_module *module);
void lrc_module_unregister(struct lrc_module *module);

#ifdef __cplusplus
}
#endif

#endif

