#ifndef _LRE_INTERPRETER_H
#define _LRE_INTERPRETER_H

#include <errno.h>

#include "lre.h"
#include "list.h"
#include "vector.h"
#include "utils.h"
#include "log.h"
#include "lre_internal.h"


#define SYNTAX_CODE_EOF 	(0xffff)
#define CODE_MAX_LEN 		(4096)

#define RET_DEAD_VAL 		LRE_RESULT_UNKNOWN

/*
 * Support operator: 
 * &&  ||  !
 * = 
 * ==  !=  >  <  >=  <=  
 *
 * bracket:
 * {  }  (  ) 
 *
 * */
enum syntax_sym_id {
	SYNTAX_SYM_RESERVED,
	SYNTAX_SYM_AND,
	SYNTAX_SYM_OR,
	SYNTAX_SYM_NOT,
	SYNTAX_SYM_ASSIGN,
	SYNTAX_SYM_EQUAL,
	SYNTAX_SYM_NOT_EQUAL,
	SYNTAX_SYM_GT,
	SYNTAX_SYM_LT,
	SYNTAX_SYM_GT_OR_EQUAL,
	SYNTAX_SYM_LT_OR_EQUAL,
	SYNTAX_SYM_START_BRACE,
	SYNTAX_SYM_END_BRACE,
	SYNTAX_SYM_START_PARENTHESIS,
	SYNTAX_SYM_END_PARENTHESIS,
	SYNTAX_SYM_COMMA,
	SYNTAX_SYM_VARIABLE,
	/* Under sym unsupport now */
	SYNTAX_SYM_MOD,
	SYNTAX_SYM_BITAND,
	SYNTAX_SYM_BITOR,
	SYNTAX_SYM_BITXOR,
	SYNTAX_SYM_ADD,
	SYNTAX_SYM_SUB,
	SYNTAX_SYM_MUL,
	SYNTAX_SYM_DIV,
	SYNTAX_SYM_INVERT,
	SYNTAX_SYM_ADD_EQUAL,
	SYNTAX_SYM_SUB_EQUAL,
	SYNTAX_SYM_MUL_EQUAL,
	SYNTAX_SYM_DIV_EQUAL,
	SYNTAX_SYM_MAX,
};

enum syntax_sym_type {
	SYM_TYPE_UNKNOWN,
	SYM_TYPE_OPT_LOGIC,
	SYM_TYPE_OPT_ASSIGN,
	SYM_TYPE_OPT_COMP,
	SYM_TYPE_OPT_ARITH,
	SYM_TYPE_OPT_ARITH_ASSIGN,
	SYM_TYPE_BRACKET,
	SYM_TYPE_COMMA,
	SYM_TYPE_VARIABLE,
};


enum syntax_node_type {
	SYNTAX_NODE_TYPE_UNKNOWN,
	SYNTAX_NODE_TYPE_SECTION,
	SYNTAX_NODE_TYPE_FUNC,
	SYNTAX_NODE_TYPE_CALL,
	SYNTAX_NODE_TYPE_BLOCK,
	SYNTAX_NODE_TYPE_EXPR,
	SYNTAX_NODE_TYPE_VAL,
	SYNTAX_NODE_TYPE_VALBLOCK,
};

enum token_type {
	TOKEN_TYPE_UNKNOWN,
	TOKEN_TYPE_KEYWORD,
	TOKEN_TYPE_STRING,
	TOKEN_TYPE_SYMBOL,
};


typedef vector_t keystub_vec_t; 

enum keystub_type {
	KEYSTUB_TYPE_UNKNOWN,
	KEYSTUB_TYPE_FUNC,
	KEYSTUB_TYPE_CALL,
	KEYSTUB_TYPE_ARG,
	KEYSTUB_TYPE_EXPR,
	KEYSTUB_TYPE_VAR,
};

struct keyword_stub {
	int type;
	char *keyword;
	union {
		lrc_obj_t *(*func_handler)(void);
		lrc_obj_t *(*call_handler)(void);
		int (*arg_handler)(lrc_obj_t *, struct lre_value *);
		int (*expr_handler)(lrc_obj_t *, int, struct lre_value *);
		int (*var_handler)(lrc_obj_t *, struct lre_value *);
	};

	void *data;
	struct keyword_stub *parent;
	keystub_vec_t *subvec;
};


/* 
 * syntax_root: base class 
 * */
struct syntax_root {
	struct list_head head;
};

/* 
 * syntax_node: base class 
 * */
struct syntax_node {
	int type;
	struct list_head entry;
	int opt;
};


struct syntax_valblock {
	struct syntax_root childs;
	struct syntax_node node;
};

struct syntax_val {
	struct syntax_node node;
	union {
		char *val; 	/* use for syntax parser */
		struct keyword_stub *valstub; 	/* use for semantic analysis */
	};
	int isvar;
};

struct sytax_keyval {
	union {
		char *key; 	/* use for syntax parser */
		struct keyword_stub *keystub; 	/* use for semantic analysis */
	};

	struct syntax_root valchilds;
//	char *val;
};


#define SYNTAX_ARG_MAX 		(8)
struct syntax_args {
	struct sytax_keyval args[SYNTAX_ARG_MAX];
	int count;
};

struct syntax_func_body {
	struct syntax_root childs;
};

struct syntax_expr {
	struct syntax_node node;
	union {
		char *key; 	/* use for syntax parser */
		struct keyword_stub *keystub; 	/* use for semantic analysis */
	};
	int opt;

	struct syntax_root valchilds;
};

struct syntax_call {
	struct syntax_node node;
	union {
		char *keyword; 	/* use for syntax parser */
		struct keyword_stub *keystub; 	/* use for semantic analysis */
	};
	struct syntax_args args;
};

struct syntax_func {
	struct syntax_node node;
	union {
		char *keyword; 	/* use for syntax parser */
		struct keyword_stub *keystub; 	/* use for semantic analysis */
	};
	struct syntax_args args;
	struct syntax_func_body body;
};

struct syntax_block {
	struct syntax_node node;
	struct syntax_root childs;
	int negative;
};

struct syntax_content {
	struct syntax_root childs;
};

struct symbol_word {
	char *sym;
	int type;
};

struct lex_token {
	char *word;
	int type;
	int subtype;

	int symbol;
};

struct interp_context {
	const char *code;

	/* Use for lexer analysis and syntax parse */
	char *wordbuf;
	struct lex_token *tokens;
	int tokenidx;
	int tokencnt;
	int tokencap;

	char *codeptr;
	char *wordptr;

	/* syntax tree */
	struct syntax_root *root;

	/* Execute results return and details */
	int results; /* Logic operation results (true or false) */
	int errcode;  /* Program execute return (success or failed code) */
	char *details;  /* Execute detail infomation (description string) */

	void *context;
};


static inline void syntax_root_init(struct syntax_root *root)
{
	INIT_LIST_HEAD(&root->head);
}

static inline void syntax_node_init(struct syntax_node *node, int type)
{
	node->opt = 0;
	node->type = type;
}


static inline void syntax_add(struct syntax_node *node, struct syntax_root *root)
{
	list_add_tail(&node->entry, &root->head);
}

static inline void syntax_del(struct syntax_node *node, struct syntax_root *root)
{
	list_del(&node->entry);
}

static inline void lex_token_init(struct lex_token *token)
{
	token->type = TOKEN_TYPE_UNKNOWN;
	token->subtype = 0;
	token->symbol = 0;
}

static inline void interp_context_refresh(struct interp_context *ctx)
{
	/* reset token idx */
	ctx->tokenidx = 0;
}

static inline int interp_get_exec_results(struct interp_context *ctx)
{
	return ctx->results;
}

static inline int interp_get_exec_errcode(struct interp_context *ctx)
{
	return ctx->errcode;
}

static inline const char *interp_get_exec_details(struct interp_context *ctx)
{
	return ctx->details;
}

static inline struct lex_token *peek_curr_token(struct interp_context *ctx)
{
	if(ctx->tokenidx < ctx->tokencnt) {
		return &ctx->tokens[ctx->tokenidx];
	}
	return NULL;
}

static inline struct lex_token *peek_next_token(struct interp_context *ctx)
{
	if(ctx->tokenidx + 1 < ctx->tokencnt) {
		return &ctx->tokens[ctx->tokenidx + 1];
	}
	return NULL;
}

static inline int skip_token(struct interp_context *ctx)
{
	if(ctx->tokenidx < ctx->tokencnt) {
		ctx->tokenidx++;
		return 0;
	}
	return -EINVAL;
}

static inline struct lex_token *read_token(struct interp_context *ctx)
{
	if(ctx->tokenidx < ctx->tokencnt) {
		return &ctx->tokens[ctx->tokenidx++];
	}
	return NULL;
}

void dump_syntax_tree(struct syntax_root *root);
void dump_lex_tokens(struct interp_context *ctx);

int interp_lexer_analysis(struct interp_context *ctx);
int interp_syntax_parse(struct interp_context *ctx);
int interp_semantic_analysis(struct interp_context *ctx);
int interp_execute(struct interp_context *ctx);

struct symbol_word *get_symbol_by_id(int sym);
int get_symbol_type(int sym);
const char *get_symbol_str(int sym);

struct interp_context *interp_context_create(const char *code);
void interp_context_destroy(struct interp_context *ctx);

int interpreter_init(void);
int interpreter_execute(const char *code, struct lre_result *res);
void interpreter_dump(void);


/* install/find keyword */

keystub_vec_t *get_root_keyvec(void);

struct keyword_stub *func_keyword_install(const char *keyword, void *data,
		lrc_obj_t *(*handler)(void),
		struct keyword_stub *parent);

struct keyword_stub *call_keyword_install(const char *keyword, void *data,
		lrc_obj_t *(*handler)(void),
		struct keyword_stub *parent);

struct keyword_stub *arg_keyword_install(const char *keyword, void *data,
		int (*handler)(lrc_obj_t *, struct lre_value *),
		struct keyword_stub *parent);

struct keyword_stub *expr_keyword_install(const char *keyword, void *data,
		int (*handler)(lrc_obj_t *, int, struct lre_value *),
		struct keyword_stub *parent);

struct keyword_stub *var_keyword_install(const char *keyword, void *data,
		int (*handler)(lrc_obj_t *, struct lre_value *),
		struct keyword_stub *parent);

struct keyword_stub *find_stub_by_keyword(keystub_vec_t *kwvec,
		const char *keyword);


#endif
