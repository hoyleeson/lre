#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "interpreter.h"

#define DEFAULT_TOKEN_COUNT 		(1024)

static struct symbol_word sym_words[] = {
	[SYNTAX_SYM_RESERVED]             = { "",   SYM_TYPE_UNKNOWN },
	[SYNTAX_SYM_AND]                  = { "&&", SYM_TYPE_OPT_LOGIC },
	[SYNTAX_SYM_OR]                   = { "||", SYM_TYPE_OPT_LOGIC },
	[SYNTAX_SYM_NOT]                  = { "!",  SYM_TYPE_OPT_LOGIC },
	[SYNTAX_SYM_ASSIGN]               = { "=",  SYM_TYPE_OPT_ASSIGN },
	[SYNTAX_SYM_EQUAL]                = { "==", SYM_TYPE_OPT_COMP },
	[SYNTAX_SYM_NOT_EQUAL]            = { "!=", SYM_TYPE_OPT_COMP },
	[SYNTAX_SYM_GT]         		  = { ">",  SYM_TYPE_OPT_COMP },
	[SYNTAX_SYM_LT]            		  = { "<",  SYM_TYPE_OPT_COMP },
	[SYNTAX_SYM_GT_OR_EQUAL] 		  = { ">=", SYM_TYPE_OPT_COMP },
	[SYNTAX_SYM_LT_OR_EQUAL]   		  = { "<=", SYM_TYPE_OPT_COMP },
	[SYNTAX_SYM_START_BRACE]          = { "{",  SYM_TYPE_BRACKET },
	[SYNTAX_SYM_END_BRACE]            = { "}",  SYM_TYPE_BRACKET },
	[SYNTAX_SYM_START_PARENTHESIS]    = { "(",  SYM_TYPE_BRACKET },
	[SYNTAX_SYM_END_PARENTHESIS]      = { ")",  SYM_TYPE_BRACKET },
	[SYNTAX_SYM_COMMA]                = { ",",  SYM_TYPE_COMMA },
	[SYNTAX_SYM_VARIABLE]             = { "$",  SYM_TYPE_UNKNOWN },
	/* Under sym unsupport now */            
	[SYNTAX_SYM_MOD]                  = { "%",  SYM_TYPE_OPT_ARITH },
	[SYNTAX_SYM_BITAND]               = { "&",  SYM_TYPE_OPT_ARITH },
	[SYNTAX_SYM_BITOR]                = { "|",  SYM_TYPE_OPT_ARITH },
	[SYNTAX_SYM_BITXOR]               = { "^",  SYM_TYPE_OPT_ARITH },
	[SYNTAX_SYM_ADD]                  = { "+",  SYM_TYPE_OPT_ARITH },
	[SYNTAX_SYM_SUB]                  = { "-",  SYM_TYPE_OPT_ARITH },
	[SYNTAX_SYM_MUL]                  = { "*",  SYM_TYPE_OPT_ARITH },
	[SYNTAX_SYM_DIV]                  = { "/",  SYM_TYPE_OPT_ARITH },
/*	[SYNTAX_SYM_INVERT]               = { "~",  SYM_TYPE_OPT_ARITH }, */
	[SYNTAX_SYM_ADD_EQUAL]            = { "+=", SYM_TYPE_OPT_ARITH_ASSIGN },
	[SYNTAX_SYM_SUB_EQUAL]            = { "-=", SYM_TYPE_OPT_ARITH_ASSIGN },
	[SYNTAX_SYM_MUL_EQUAL]            = { "*=", SYM_TYPE_OPT_ARITH_ASSIGN },
	[SYNTAX_SYM_DIV_EQUAL]            = { "/=", SYM_TYPE_OPT_ARITH_ASSIGN },
};

static keystub_vec_t *root_kstub_vec = NULL;


struct symbol_word *get_symbol_by_id(int sym)
{
	if(sym > SYNTAX_SYM_RESERVED && sym < SYNTAX_SYM_MAX) {
		return &sym_words[sym];
	}
	return NULL;
}

int get_symbol_type(int sym)
{
	if(sym > SYNTAX_SYM_RESERVED && sym < SYNTAX_SYM_MAX) {
		return sym_words[sym].type;
	}
	return SYM_TYPE_UNKNOWN;
}

const char *get_symbol_str(int sym)
{
	if(sym > SYNTAX_SYM_RESERVED && sym < SYNTAX_SYM_MAX) {
		return sym_words[sym].sym;
	}
	return NULL;
}


keystub_vec_t *get_root_keyvec(void)
{
	return root_kstub_vec;
}

static int keyword_stub_add(struct keyword_stub *keystub, 
		struct keyword_stub *parent)
{
	keystub_vec_t *vec;

	if(parent)
		vec = parent->subvec;
	else
		vec = get_root_keyvec();

	vector_set(vec, keystub);
	return 0;
}


static struct keyword_stub *keyword_stub_create(int type, const char *keyword,
	   	const char *description)
{
	struct keyword_stub *keystub;

	keystub = xzalloc(sizeof(*keystub));

	keystub->type = type;
	keystub->keyword = strdup(keyword);
	keystub->description = strdup(description);
	keystub->subvec = vector_init(0);

	return keystub;
}

struct keyword_stub *func_keyword_install(const char *keyword, const char *desc,
	   	lrc_obj_t *(*handler)(void),
		struct keyword_stub *parent)
{
	struct keyword_stub *keystub;

	keystub = keyword_stub_create(KEYSTUB_TYPE_FUNC, keyword, desc);
	keystub->func_handler = handler;
	keystub->parent = parent;

	keyword_stub_add(keystub, parent);
	return keystub;
}

struct keyword_stub *call_keyword_install(const char *keyword, const char *desc,
	   	lrc_obj_t *(*handler)(void),
		struct keyword_stub *parent)
{
	struct keyword_stub *keystub;

	keystub = keyword_stub_create(KEYSTUB_TYPE_CALL, keyword, desc);
	keystub->call_handler = handler;
	keystub->parent = parent;

	keyword_stub_add(keystub, parent);
	return keystub;
}

struct keyword_stub *arg_keyword_install(const char *keyword, const char *desc,
	   	int (*handler)(lrc_obj_t *, struct lre_value *),
		struct keyword_stub *parent)
{
	struct keyword_stub *keystub;

	keystub = keyword_stub_create(KEYSTUB_TYPE_ARG, keyword, desc);
	keystub->arg_handler = handler;
	keystub->parent = parent;

	keyword_stub_add(keystub, parent);
	return keystub;
}

struct keyword_stub *expr_keyword_install(const char *keyword, const char *desc,
	   	int (*handler)(lrc_obj_t *, int, struct lre_value *),
		struct keyword_stub *parent)
{
	struct keyword_stub *keystub;

	keystub = keyword_stub_create(KEYSTUB_TYPE_EXPR, keyword, desc);
	keystub->expr_handler = handler;
	keystub->parent = parent;

	keyword_stub_add(keystub, parent);
	return keystub;
}

struct keyword_stub *var_keyword_install(const char *keyword, const char *desc,
		int (*handler)(lrc_obj_t *, struct lre_value *),
		struct keyword_stub *parent)
{
	struct keyword_stub *keystub;

	keystub = keyword_stub_create(KEYSTUB_TYPE_VAR, keyword, desc);
	keystub->var_handler = handler;
	keystub->parent = parent;

	keyword_stub_add(keystub, parent);
	return keystub;
}


struct keyword_stub *find_stub_by_keyword(keystub_vec_t *kwvec, 
		const char *keyword)
{
	int i;
	struct keyword_stub *keystub;

	vector_foreach_active_slot(kwvec, keystub, i) {
		if(!keystub || strcmp(keystub->keyword, keyword))
			continue;

		return keystub;
	}

	return NULL;
}


static struct interp_context *prase_context_create(const char *code)
{
	struct interp_context *ctx;

	ctx = (struct interp_context *)xzalloc(sizeof(*ctx));

	ctx->code = code;
	ctx->root = NULL;

	ctx->wordbuf = xzalloc(CODE_MAX_LEN);
	ctx->tokens = xzalloc(sizeof(struct lex_token) * DEFAULT_TOKEN_COUNT);
	ctx->tokenidx = 0;
	ctx->tokencnt = 0;
	ctx->tokencap = DEFAULT_TOKEN_COUNT;

	ctx->wordptr = ctx->wordbuf;
	ctx->codeptr = (char *)ctx->code;

	ctx->context = NULL;
	ctx->results = LRE_RESULT_UNKNOWN;
	return ctx;
}

static void interp_context_destroy(struct interp_context *ctx)
{
	free(ctx->wordbuf);
	free(ctx->tokens);
	free(ctx);
}

int interpreter_execute(const char *code)
{
	int ret;
	struct interp_context *ctx;

	ctx = prase_context_create(code);

	ret = interp_lexer_analysis(ctx);
	if(ret) {
		loge("Interpreter err: lexer analysis error.");
		interp_context_destroy(ctx);
		return -EINVAL;
	}

	interp_context_refresh(ctx);

	ret = interp_syntax_parse(ctx);
	if(ret) {
		loge("Interpreter err: syntax parse error.");
		interp_context_destroy(ctx);
		return -EINVAL;
	}

	interp_context_refresh(ctx);

	ret = interp_semantic_analysis(ctx);
	if(ret) {
		loge("Interpreter err: semantic analysis error.");
		interp_context_destroy(ctx);
		return -EINVAL;
	}

	ret = interp_execute(ctx);
	if(ret) {
		loge("Interpreter err: exec error.");
		interp_context_destroy(ctx);
		return -EINVAL;
	}

	ret = interp_get_results(ctx);

	interp_context_destroy(ctx);
	return ret;
}

int interpreter_init(void)
{
	root_kstub_vec = vector_init(0);
	return 0;
}

