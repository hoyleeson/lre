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

struct lrc_object {
	int type;
#define LRC_OBJECT_FUNC 		(0)
#define LRC_OBJECT_CALL 		(1)
	/* only use for LRC_OBJECT_CALL */
	int (*execfn)(struct lrc_object *, struct lre_value *);
};

typedef struct lrc_object lrc_obj_t;

#define LRE_RESULT_FALSE 		(0)
#define LRE_RESULT_TRUE 		(1)
#define LRE_RESULT_OK 			(0)
#define LRE_RESULT_ERROR 		(0xdeaddead)
#define LRE_RESULT_UNKNOWN 		(0xdeaddeed)

int lre_init(void);
int lre_execute(const char *code);
void lre_release(void);

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
	int (*handler)(lrc_obj_t *, struct lre_value *);
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

#ifdef __cplusplus
}
#endif

#endif

