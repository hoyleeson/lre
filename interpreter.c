#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "lre_internal.h"


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

/*************************************************************/

int interp_expands_codebuf(struct interp *interp)
{
	char *ptr;
	int len;

	if(interp->cbufsize >= CODEBUF_SIZE_MAX)
		return -ENOMEM;

	len = min(interp->cbufsize * 2, CODEBUF_SIZE_MAX);
	ptr = realloc(interp->codebuf, interp->cbufsize);
	if(!ptr)
		return -ENOMEM;

	interp->codebuf = ptr;
	interp->cbufsize = len;
	return 0;
}

int interp_expands_workmem(struct interp *interp)
{
	char *ptr;
	int len;

	if(interp->wmemsize >= WORKMEM_SIZE_MAX)
		return -ENOMEM;

	len = min(interp->wmemsize * 2, WORKMEM_SIZE_MAX);
	ptr = realloc(interp->workmem, interp->wmemsize);
	if(!ptr)
		return -ENOMEM;

	interp->workmem = ptr;
	interp->wmemsize = len;
	return 0;
}

int interp_expands_stacks(struct interp *interp)
{
	char *ptr;
	int len;

	if(interp->stacksize >= STACKS_SIZE_MAX)
		return -ENOMEM;

	len = min(interp->stacksize * 2, STACKS_SIZE_MAX);
	ptr = realloc(interp->stacks, interp->stacksize);
	if(!ptr)
		return -ENOMEM;

	interp->stacks = ptr;
	interp->stacksize = len;
	return 0;
}

int interp_expands_outbuf(struct interp *interp)
{
	char *ptr;
	int len;

	if(interp->obufsize >= OUTBUF_SIZE_MAX)
		return -ENOMEM;

	len = min(interp->obufsize * 2, OUTBUF_SIZE_MAX);
	ptr = realloc(interp->outbuf, interp->obufsize);
	if(!ptr)
		return -ENOMEM;

	interp->outbuf = ptr;
	interp->obufsize = len;
	return 0;
}

static int interp_load_code(struct interp_context *ctx, const char *code)
{
	struct interp *interp;
	int len = strlen(code);

	interp = ctx->interp;
	if(interp->cbufsize <= len) {
		if(interp_expands_codebuf(interp)) {
			loge("Interp: load code failed. code too huge.(len:%d)", len);
			return -ENOMEM;
		}

		loge("Interp: expand code buf, new size: %d", interp->cbufsize);
		ctx->codeptr = interp->codebuf;
	}

	strncpy(interp->codebuf, code, len);
	return 0;
}

static void interp_context_init(struct interp *interp, struct interp_context *ctx)
{
	ctx->interp = interp;

	ctx->tree = NULL;

	/* Use for Lexer */
	ctx->codeptr = interp->codebuf;
	ctx->wordptr = interp->workmem;

	/* Use for Lexer and Syntax parser */
	ctx->tokens = NULL;
	ctx->tokenidx = 0;
	ctx->tokencnt = 0;
	ctx->tokencap = 0;

	/* Use for Executor */
	ctx->stack = NULL;
	ctx->stackidx = 0;
	ctx->stackdepth = 0;

	ctx->details = interp->outbuf;
	ctx->detailen = 0;
	ctx->detailcap = interp->obufsize;

	ctx->context = NULL;

	memset(interp->codebuf, 0, interp->cbufsize);
	memset(interp->workmem, 0, interp->wmemsize);
	memset(interp->stacks, 0, interp->stacksize);
	memset(interp->outbuf, 0, interp->obufsize);
}

struct interp_context *interp_context_create(struct interp *interp,
	   	const char *code)
{
	int ret;
	struct interp_context *ctx;

//	ctx = (struct interp_context *)xzalloc(sizeof(*ctx));
	ctx = &interp->context;

	interp_context_init(interp, ctx);
	ret = interp_load_code(ctx, code);
	if(ret) {
		free(ctx);
		loge("Interp: cannot load code to interp, create context failed.");
		return NULL;
	}
	return ctx;
}

int interp_context_reload_code(struct interp_context *ctx,
	   	const char *code)
{
	int ret;
	interp_context_init(ctx->interp, ctx);
	ret = interp_load_code(ctx, code);
	if(ret) {
		loge("Interp: cannot load code to interp.");
		return -ENOMEM;
	}
	return 0;
}

void interp_context_destroy(struct interp_context *ctx)
{
	destroy_syntax_tree(ctx, ctx->tree);
//	free(ctx);
}

int interpreter_execute(struct interp *interp, 
		const char *code)
{
	int ret;
	char *errstr;
	struct interp_context *ctx;

	ctx = interp_context_create(interp, code);
	if(!ctx) {
		ret = -ENOMEM;
		errstr = "Interpreter err: create context error.";
		goto err;
	}

repeat:
	ret = interp_lexer_analysis(ctx);
	if(ret) {
		errstr = "Interpreter err: lexer analysis error.";
		goto err;
	}

	interp_context_refresh(ctx);
	/* dump_lex_tokens(ctx); */

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
	/*	dump_syntax_tree(ctx); */
	ret = interp_semantic_analysis(ctx);
	if(ret) {
		errstr = "Interpreter err: semantic analysis error.";
		goto err;
	}

	ret = interp_execute(ctx);
	if(!vaild_lre_results(ret)) {
		errstr = "Interpreter err, execute error.";
		goto err;
	}

	interp->results = ret;
	interp->errcode = LRE_RET_OK;

	logd("Interp execute success, result:%d", ret);
	interp_context_destroy(ctx);
	return 0;

err:
	interp->results = LRE_RESULT_UNKNOWN;
	interp->errcode = ret;
	loge("%s, unexpect result: %d", errstr, ret);
	interp_context_destroy(ctx);
	return ret;
}

struct interp *interpreter_create(void)
{
	int ret;
	struct interp *interp;

	interp = (struct interp *)xzalloc(sizeof(*interp));
	interp->codebuf = xzalloc(DEFAULT_CODEBUF_SIZE);
	interp->cbufsize = DEFAULT_CODEBUF_SIZE;

	interp->workmem = xzalloc(DEFAULT_WORKMEM_SIZE);
	interp->wmemsize = DEFAULT_WORKMEM_SIZE;

	interp->stacks = xzalloc(DEFAULT_STACKS_SIZE);
	interp->stacksize = DEFAULT_STACKS_SIZE;

	interp->outbuf = xzalloc(DEFAULT_OUTBUF_SIZE);
	interp->obufsize = DEFAULT_OUTBUF_SIZE;

	ret = mempool_init(&interp->syntax_mpool, sizeof(struct syntax_mpool_node), 128);
	if(ret) {
	
	}

	return interp;
}

void interpreter_destroy(struct interp *interp)
{
	mempool_release(&interp->syntax_mpool);

	free(interp->codebuf);
	free(interp->workmem);
	free(interp->stacks);
	free(interp->outbuf);

	free(interp);
}


