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

struct lre_context;
struct lre_value;

struct lrc_object {
	int type;
#define LRC_OBJECT_FUNC 		(0)
#define LRC_OBJECT_CALL 		(1)

	void *context;
	void (*output)(struct lrc_object *, const char *);
};

typedef struct lrc_object lrc_obj_t;

struct lre_result {
	int result;
	int errcode;
	char *details;
};

/* Logic rule engine execute results (true or false) */
#define LRE_RESULT_FALSE 		(0)
#define LRE_RESULT_TRUE 		(1)
#define LRE_RESULT_UNKNOWN 		(0xdeaddeed)

/* Program execute return (success or errcode) */
#define LRE_RET_OK 			(0)
/* LRE errcode the same to errno */
#define LRE_RET_ERROR 		(0xdeaddead)

/* lrc protocols */
#define MULTIPATH_SPLIT_CH 	';'
#define DETAILS_UNIT_MAX 	(256)

int lre_initX(const char *confpath, void (*logcb)(int, const char *));
int lre_init(void);
void lre_release(void);

struct lre_context *lre_context_create(void);
void lre_context_destroy(struct lre_context *ctx);

struct lre_result *lre_execute(struct lre_context *ctx, const char *code);

void lre_help(void);

/******************** lre utils ****************************/

static inline int vaild_lre_results(int res)
{
	return (res == LRE_RESULT_TRUE || res == LRE_RESULT_FALSE);
}

void lre_value_dup2_string(struct lre_value *val, const char *str);
void lre_value_set_string(struct lre_value *val, char *str);
void lre_value_set_int(struct lre_value *val, int ival);
void lre_value_set_double(struct lre_value *val, double dval);

const char *lre_value_get_string(struct lre_value *val);
int lre_value_get_int(struct lre_value *val);
double lre_value_get_double(struct lre_value *val);

int lre_value_is_int(struct lre_value *val);
int lre_value_is_double(struct lre_value *val);
int lre_value_is_string(struct lre_value *val);

int lre_calc_int(int a, int b, int op);
double lre_calc_dobule(double a, double b, int op);

int lre_compare_int(int a, int b, int op);
int lre_compare_double(double a, double b, int op);
int lre_compare_string(char *a, char *b, int op);
/* 
 * cmp results:
 * a > b: [return > 0]
 * a < b: [return < 0]
 * a = b: [return = 0]
 * */
int lre_compare(unsigned long a, unsigned long b, int op,
	   	int (*cmp)(unsigned long a, unsigned long b));


/***********************************************************/

/* Virtual */
struct lrc_stub_base {
	char *keyword;
	char *description;
};

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
	lrc_obj_t *(*constructor)(void);
	int (*exec)(struct lrc_object *, struct lre_value *);
	void (*destructor)(struct lrc_object *);

	struct lrc_stub_arg *args;
	int argcount;
	struct lrc_stub_var *vars;
	int varcount;
};

struct lrc_stub_func {
	char *keyword;
	char *description;
	lrc_obj_t *(*constructor)(void);
	int (*exec)(struct lrc_object *);
	void (*destructor)(struct lrc_object *);

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

