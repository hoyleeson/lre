#ifndef _LRE_LRE_INTERNAL_H
#define _LRE_LRE_INTERNAL_H

#include "lre.h"
#include "list.h"
#include "vector.h"
#include "utils.h"
#include "conf.h"
#include "log.h"
#include "mempool.h"
#include "interpreter.h"

#define LRE_MACRO_ARGC_MAX 	(16)

struct lre_context {
	struct interp *interp;
	struct lre_result res;
};

struct lre_value {
	int type;
#define LRE_ARG_TYPE_UNKNOWN 	(0)
#define LRE_ARG_TYPE_INT 		(1)
#define LRE_ARG_TYPE_DOUBLE 	(2)
#define LRE_ARG_TYPE_STRING 	(3)

	union {
		int valint;
		double valdouble;
		char *valstring;
	};

	void *ptr;
	int flags;
#define LRE_VALUE_F_NEEDFREE 	(1 << 0)
};

void lre_value_init(struct lre_value *val);
void lre_value_release(struct lre_value *val);

/************************* macro *********************************/

struct lre_macro;

int lre_macro_init(void);
void lre_macro_release(void);

struct lre_macro *lre_find_macro(const char *str, int argc);
int is_lre_macro(const char *str);
char *lre_create_code_by_macro(struct lre_macro *macro, 
		int argc, const char **args);

/************************* keyword *******************************/

int lre_keyword_init(void);
void lre_keyword_release(void);

void lre_keyword_dump(void);

/* install/find keyword */
struct keyword_stub *keyword_install(int type, const char *keyword,
		void *data, struct keyword_stub *parent);
void keyword_uninstall(struct keyword_stub *keystub);

struct keyword_stub *find_stub_by_keyword(keystub_vec_t *kwvec,
		const char *keyword);

keystub_vec_t *get_root_keyvec(void);

#endif

