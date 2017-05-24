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
	   	void *data)
{
	struct keyword_stub *keystub;

	keystub = xzalloc(sizeof(*keystub));

	keystub->type = type;
	keystub->keyword = strdup(keyword);
	keystub->data = data;

	if(type == KEYSTUB_TYPE_FUNC || 
			type == KEYSTUB_TYPE_CALL)
		keystub->subvec = vector_init(0);

	return keystub;
}

static void keyword_stub_destroy(struct keyword_stub *keystub)
{
	int i;
	struct keyword_stub *substub;

	free(keystub->keyword);

	vector_foreach_active_slot(keystub->subvec, substub, i) {
		if(!substub)
			continue;
		free(substub);
	}
	if(keystub->subvec)
		vector_free(keystub->subvec);
	free(keystub);
}

struct keyword_stub *keyword_install(int type, const char *keyword,
	   	void *data, struct keyword_stub *parent)
{
	struct keyword_stub *keystub;

	keystub = keyword_stub_create(type, keyword, data);
	keystub->parent = parent;

	keyword_stub_add(keystub, parent);
	return keystub;
}

void keyword_uninstall(struct keyword_stub *keystub)
{
	keyword_stub_destroy(keystub);
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


struct interp_context *interp_context_create(const char *code)
{
	struct interp_context *ctx;

	ctx = (struct interp_context *)xzalloc(sizeof(*ctx));

	ctx->code = strdup(code);
	ctx->root = NULL;

	ctx->wordbuf = xzalloc(CODE_MAX_LEN);
	ctx->tokens = xzalloc(sizeof(struct lex_token) * DEFAULT_TOKEN_COUNT);
	ctx->tokenidx = 0;
	ctx->tokencnt = 0;
	ctx->tokencap = DEFAULT_TOKEN_COUNT;

	ctx->wordptr = ctx->wordbuf;
	ctx->codeptr = (char *)ctx->code;

	ctx->results = LRE_RESULT_UNKNOWN;
	ctx->errcode = LRE_RET_OK;
	ctx->details = NULL;

	ctx->context = NULL;
	return ctx;
}

int interp_context_reload_code(struct interp_context *ctx, const char *code)
{
	free(ctx->code);
	ctx->code = strdup(code);

	memset(ctx->wordbuf, 0, CODE_MAX_LEN);
	memset(ctx->tokens, 0, sizeof(struct lex_token) * ctx->tokencap);
	ctx->tokenidx = 0;
	ctx->tokencnt = 0;

	ctx->wordptr = ctx->wordbuf;
	ctx->codeptr = (char *)ctx->code;

	ctx->results = LRE_RESULT_UNKNOWN;
	ctx->errcode = LRE_RET_OK;
	ctx->details = NULL;

	ctx->context = NULL;
	return 0;
}

void interp_context_destroy(struct interp_context *ctx)
{
	free(ctx->code);
	free(ctx->wordbuf);
	free(ctx->tokens);
	if(ctx->details)
		free(ctx->details);

	free(ctx);
}

static char *keystub_type_str[] = {
	[KEYSTUB_TYPE_UNKNOWN] = "unknown",
	[KEYSTUB_TYPE_FUNC] = "func",
	[KEYSTUB_TYPE_CALL] = "call",
	[KEYSTUB_TYPE_ARG] = "arg",
	[KEYSTUB_TYPE_EXPR] = "expr",
	[KEYSTUB_TYPE_VAR] = "var",
};

#define dump_blank(l) do {int i; for(i=0; i<level; i++)printf("    "); }while(0)
static void keyword_stub_dump(struct keyword_stub *keystub, int level)
{
	struct lrc_stub_base *base;

	dump_blank(level);
	printf("%s [%s]\n", keystub->keyword, keystub_type_str[keystub->type]);
	dump_blank(level);
	base = (struct lrc_stub_base *)keystub->data;
	printf("\t%s\n", base->description);
}

static void keystub_vec_dump(keystub_vec_t *vec, int level)
{
	int i;
	struct keyword_stub *keystub;

	vector_foreach_active_slot(vec, keystub, i) {
		if(!keystub)
			continue;

		keyword_stub_dump(keystub, level);
		keystub_vec_dump(keystub->subvec, level + 1);
	}
	if((level % 2) != 0)
		printf("\n");
}

void interpreter_dump(void)
{
	keystub_vec_t *vec;

	vec = get_root_keyvec();
	keystub_vec_dump(vec, 0);
}

int interpreter_execute(const char *code, struct lre_result *res)
{
	int ret;
	char *errstr;
	struct interp_context *ctx;

	ctx = interp_context_create(code);

repeat:
	ret = interp_lexer_analysis(ctx);
	if(ret) {
		errstr = "Interpreter err: lexer analysis error.";
		goto err;
	}

	interp_context_refresh(ctx);
	/*	dump_lex_tokens(ctx); */

	ret = interp_preprocess(ctx);
	if(ret == PREPROCESS_RES_REPEAT)
		goto repeat;
	else if(ret < 0) {
		errstr = "Interpreter err: preprocess error.";
		goto err;
	}

	ret = interp_syntax_parse(ctx);
	if(ret) {
		errstr = "Interpreter err: syntax parse error.";
		goto err;
	}

	interp_context_refresh(ctx);
	/*	dump_syntax_tree(ctx->root); */
	ret = interp_semantic_analysis(ctx);
	if(ret) {
		errstr = "Interpreter err: semantic analysis error.";
		goto err;
	}

	ret = interp_execute(ctx);
	if(ret) {
		errstr = "Interpreter err: exec error.";
		goto err;
	}

	res->result = interp_get_exec_results(ctx);
	res->errcode = interp_get_exec_errcode(ctx);
	res->details = (char*)interp_get_exec_details(ctx);
	ctx->details = NULL;
	
	interp_context_destroy(ctx);
	return 0;

err:
	res->result = LRE_RESULT_UNKNOWN;
	res->errcode = ret;
	res->details = strdup(errstr);
	interp_context_destroy(ctx);
	return ret;
}

int interpreter_init(void)
{
	root_kstub_vec = vector_init(0);
	return 0;
}

void interpreter_release(void)
{
	int i;
	struct keyword_stub *keystub;

	vector_foreach_active_slot(root_kstub_vec, keystub, i) {
		if(!keystub)
			continue;
		free(keystub);
	}
}
