#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "lre_internal.h"


enum expr_val_type {
	EXPR_VAL_TYPE_UNKNOWN,
	EXPR_VAL_TYPE_NORMAL,
	EXPR_VAL_TYPE_VAR,
	EXPR_VAL_TYPE_BLOCK,
	EXPR_VAL_TYPE_CALL,
};

enum expr_type {
	EXPR_TYPE_UNKNOWN,
	EXPR_TYPE_EOF,
	EXPR_TYPE_NORMAL,
	EXPR_TYPE_FUNC,
	EXPR_TYPE_NEG_BLOCK,
	EXPR_TYPE_BLOCK,
};


#define CHECK_RET_GOTO(r, tag) 		do { if(!r) goto tag; } while(0)
#define CHECK_KEYWORD_GOTO(r, tag) 	CHECK_RET_GOTO(r, tag)
#define CHECK_BREAKET_GOTO(r, tag) 	CHECK_RET_GOTO(r, tag)


static struct syntax_content *syntax_content_create(struct interp_context *ctx)
{
	struct syntax_content *content;
	struct interp *interp = ctx->interp;

	//content = (struct syntax_content *)xzalloc(sizeof(*content));
	content = (struct syntax_content *)mem_alloc(&interp->syntax_mpool);

	syntax_root_init(&content->childs);

	return content;
}

static inline void syntax_content_destroy(struct interp_context *ctx, 
		struct syntax_content *content)
{
	struct interp *interp = ctx->interp;
	mem_free(&interp->syntax_mpool, content);
}

static struct syntax_block *syntax_block_create(struct interp_context *ctx)
{
	struct syntax_block *block;
	struct interp *interp = ctx->interp;

	//block = (struct syntax_block *)xzalloc(sizeof(*block));
	block = (struct syntax_block *)mem_alloc(&interp->syntax_mpool);

	syntax_root_init(&block->childs);
	syntax_node_init(&block->node, SYNTAX_NODE_TYPE_BLOCK);
	block->negative = 0;

	return block;
}

static inline void syntax_block_destroy(struct interp_context *ctx, 
		struct syntax_block *block)
{
	struct interp *interp = ctx->interp;
	mem_free(&interp->syntax_mpool, block);
}

static struct syntax_call *syntax_call_create(struct interp_context *ctx)
{
	int i;
	struct syntax_call *call;
	struct syntax_args *args;
	struct interp *interp = ctx->interp;

	//call = (struct syntax_call *)xzalloc(sizeof(*call));
	call = (struct syntax_call *)mem_alloc(&interp->syntax_mpool);
	syntax_node_init(&call->node, SYNTAX_NODE_TYPE_CALL);

	args = &call->args;
	for(i=0; i<SYNTAX_ARG_MAX; i++) {
		syntax_root_init(&args->args[i].valchilds);
	}

	return call;
}

static inline void syntax_call_destroy(struct interp_context *ctx, 
		struct syntax_call *call)
{
	struct interp *interp = ctx->interp;
	mem_free(&interp->syntax_mpool, call);
}

static struct syntax_func *syntax_func_create(struct interp_context *ctx)
{
	int i;
	struct syntax_func *func;
	struct syntax_args *args;
	struct interp *interp = ctx->interp;

	//func = (struct syntax_func *)xzalloc(sizeof(*func));
	func = (struct syntax_func *)mem_alloc(&interp->syntax_mpool);
	syntax_root_init(&func->body.childs);
	syntax_node_init(&func->node, SYNTAX_NODE_TYPE_FUNC);

	args = &func->args;
	for(i=0; i<SYNTAX_ARG_MAX; i++) {
		syntax_root_init(&args->args[i].valchilds);
	}

	return func;
}

static inline void syntax_func_destroy(struct interp_context *ctx, 
		struct syntax_func *func)
{
	struct interp *interp = ctx->interp;
	mem_free(&interp->syntax_mpool, func);
}

static struct syntax_expr *syntax_expr_create(struct interp_context *ctx)
{
	struct syntax_expr *expr;
	struct interp *interp = ctx->interp;

	//expr = (struct syntax_expr *)xzalloc(sizeof(*expr));
	expr = (struct syntax_expr *)mem_alloc(&interp->syntax_mpool);
	syntax_node_init(&expr->node, SYNTAX_NODE_TYPE_EXPR);
	syntax_root_init(&expr->valchilds);

	return expr;
}

static inline void syntax_expr_destroy(struct interp_context *ctx, 
		struct syntax_expr *expr)
{
	struct interp *interp = ctx->interp;
	mem_free(&interp->syntax_mpool, expr);
}

static struct syntax_val *syntax_val_create(struct interp_context *ctx)
{
	struct syntax_val *exprval;
	struct interp *interp = ctx->interp;

	//exprval = (struct syntax_val *)xzalloc(sizeof(*exprval));
	exprval = (struct syntax_val *)mem_alloc(&interp->syntax_mpool);
	syntax_node_init(&exprval->node, SYNTAX_NODE_TYPE_VAL);

	return exprval;
}

static inline void syntax_val_destroy(struct interp_context *ctx, 
		struct syntax_val *val)
{
	struct interp *interp = ctx->interp;
	mem_free(&interp->syntax_mpool, val);
}

static struct syntax_valblock *syntax_valblock_create(struct interp_context *ctx)
{
	struct syntax_valblock *exprvb;
	struct interp *interp = ctx->interp;

	//exprvb = (struct syntax_valblock *)xzalloc(sizeof(*exprvb));
	exprvb = (struct syntax_valblock *)mem_alloc(&interp->syntax_mpool);
	syntax_root_init(&exprvb->childs);
	syntax_node_init(&exprvb->node, SYNTAX_NODE_TYPE_VALBLOCK);

	return exprvb;
}

static inline void syntax_valblock_destroy(struct interp_context *ctx, 
		struct syntax_valblock *valblock)
{
	struct interp *interp = ctx->interp;
	mem_free(&interp->syntax_mpool, valblock);
}

static __attribute__((unused)) char *peek_keyword(struct interp_context *ctx)
{
	struct lex_token *token;

	token = peek_curr_token(ctx);
	if(!token)
		return NULL;

	if(!(token->type == TOKEN_TYPE_KEYWORD || 
				token->type == TOKEN_TYPE_STRING)) 
		return NULL;

	return token->word;
}

static char *read_keyword(struct interp_context *ctx)
{
	struct lex_token *token;

	token = read_token(ctx);
	if(!token)
		return NULL;

	if(!(token->type == TOKEN_TYPE_KEYWORD || 
				token->type == TOKEN_TYPE_STRING)) 
		return NULL;

	return token->word;
}


static int peek_operator(struct interp_context *ctx)
{
	struct lex_token *token;

	token = peek_curr_token(ctx);
	if(!token)
		return -EINVAL;

	if(token->type != TOKEN_TYPE_SYMBOL) 
		return -EINVAL;

	switch(token->symbol) {
		/* logic operator */
		case SYNTAX_SYM_AND:
		case SYNTAX_SYM_OR:
		case SYNTAX_SYM_NOT:
			/* assign operator */
		case SYNTAX_SYM_ASSIGN:
			/* comp operator */
		case SYNTAX_SYM_EQUAL:
		case SYNTAX_SYM_NOT_EQUAL:
		case SYNTAX_SYM_GT:
		case SYNTAX_SYM_LT:
		case SYNTAX_SYM_GT_OR_EQUAL:
		case SYNTAX_SYM_LT_OR_EQUAL:
			/* arithmetic operator */
		case SYNTAX_SYM_MOD:
		case SYNTAX_SYM_BITAND:
		case SYNTAX_SYM_BITOR:
		case SYNTAX_SYM_BITXOR:
		case SYNTAX_SYM_ADD:
		case SYNTAX_SYM_SUB:
		case SYNTAX_SYM_MUL:
		case SYNTAX_SYM_DIV:
		case SYNTAX_SYM_ADD_EQUAL:
		case SYNTAX_SYM_SUB_EQUAL:
		case SYNTAX_SYM_MUL_EQUAL:
		case SYNTAX_SYM_DIV_EQUAL:
			return token->symbol;
	}

	return -EINVAL;
}

static int read_operator(struct interp_context *ctx)
{
	struct lex_token *token;

	token = read_token(ctx);
	if(!token)
		return -EINVAL;

	if(token->type != TOKEN_TYPE_SYMBOL) 
		return -EINVAL;

	switch(token->symbol) {
		/* logic operator */
		case SYNTAX_SYM_AND:
		case SYNTAX_SYM_OR:
		case SYNTAX_SYM_NOT:
			/* assign operator */
		case SYNTAX_SYM_ASSIGN:
			/* comp operator */
		case SYNTAX_SYM_EQUAL:
		case SYNTAX_SYM_NOT_EQUAL:
		case SYNTAX_SYM_GT:
		case SYNTAX_SYM_LT:
		case SYNTAX_SYM_GT_OR_EQUAL:
		case SYNTAX_SYM_LT_OR_EQUAL:
			/* arithmetic operator */
		case SYNTAX_SYM_MOD:
		case SYNTAX_SYM_BITAND:
		case SYNTAX_SYM_BITOR:
		case SYNTAX_SYM_BITXOR:
		case SYNTAX_SYM_ADD:
		case SYNTAX_SYM_SUB:
		case SYNTAX_SYM_MUL:
		case SYNTAX_SYM_DIV:
		case SYNTAX_SYM_ADD_EQUAL:
		case SYNTAX_SYM_SUB_EQUAL:
		case SYNTAX_SYM_MUL_EQUAL:
		case SYNTAX_SYM_DIV_EQUAL:
			return token->symbol;
	}

	return -EINVAL;
}

static int peek_comma(struct interp_context *ctx)
{
	struct lex_token *token;

	token = peek_curr_token(ctx);
	if(!token)
		return -EINVAL;

	if(!(token->type == TOKEN_TYPE_SYMBOL && 
				token->symbol == SYNTAX_SYM_COMMA)) 
		return -EINVAL;

	return token->symbol;
}

static int read_comma(struct interp_context *ctx)
{
	struct lex_token *token;

	token = read_token(ctx);
	if(!token)
		return -EINVAL;

	if(!(token->type == TOKEN_TYPE_SYMBOL && 
				token->symbol == SYNTAX_SYM_COMMA)) 
		return -EINVAL;

	return token->symbol;
}


static __attribute__((unused)) int peek_bracket(struct interp_context *ctx)
{
	struct lex_token *token;

	token = peek_curr_token(ctx);
	if(!token)
		return -EINVAL;

	if(!(token->type == TOKEN_TYPE_SYMBOL && 
				(token->symbol == SYNTAX_SYM_START_PARENTHESIS || 
				 token->symbol == SYNTAX_SYM_START_BRACE))) 
		return -EINVAL;

	return token->symbol;
}

static int read_bracket(struct interp_context *ctx)
{
	struct lex_token *token;

	token = read_token(ctx);
	if(!token)
		return -EINVAL;

	if(!(token->type == TOKEN_TYPE_SYMBOL && 
				(token->symbol == SYNTAX_SYM_START_PARENTHESIS || 
				 token->symbol == SYNTAX_SYM_START_BRACE))) 
		return -EINVAL;

	return token->symbol;
}

static int peek_end_bracket(struct interp_context *ctx)
{
	struct lex_token *token;

	token = peek_curr_token(ctx);
	if(!token)
		return -EINVAL;

	if(!(token->type == TOKEN_TYPE_SYMBOL && 
				(token->symbol == SYNTAX_SYM_END_PARENTHESIS ||
				 token->symbol == SYNTAX_SYM_END_BRACE)))
		return -EINVAL;

	return token->symbol;
}

static int read_end_bracket(struct interp_context *ctx)
{
	struct lex_token *token;

	token = read_token(ctx);
	if(!token)
		return -EINVAL;

	if(!(token->type == TOKEN_TYPE_SYMBOL && 
				(token->symbol == SYNTAX_SYM_END_PARENTHESIS ||
				 token->symbol == SYNTAX_SYM_END_BRACE)))
		return -EINVAL;

	return token->symbol;
}


static __attribute__((unused)) int peek_negblock(struct interp_context *ctx)
{
	struct lex_token *token;

	token = peek_curr_token(ctx);
	if(!token)
		return -EINVAL;

	if(!(token->type == TOKEN_TYPE_SYMBOL && 
				token->symbol == SYNTAX_SYM_NOT)) 
		return -EINVAL;

	token = peek_next_token(ctx);
	if(!(token->type == TOKEN_TYPE_SYMBOL && 
				token->symbol == SYNTAX_SYM_START_PARENTHESIS)) 
		return -EINVAL;

	return 0;
}

static int skip_negblock(struct interp_context *ctx)
{
	struct lex_token *token;

	token = read_token(ctx);
	if(!token)
		return -EINVAL;

	if(!(token->type == TOKEN_TYPE_SYMBOL && 
				token->symbol == SYNTAX_SYM_NOT)) 
		return -EINVAL;

	token = read_token(ctx);
	if(!(token->type == TOKEN_TYPE_SYMBOL && 
				token->symbol == SYNTAX_SYM_START_PARENTHESIS)) 
		return -EINVAL;

	return 0;
}

static int peek_expr_val_type(struct interp_context *ctx)
{
	struct lex_token *token;

	token = peek_curr_token(ctx);
	if(!token)
		return EXPR_VAL_TYPE_UNKNOWN;

	switch(token->type) {
		case TOKEN_TYPE_KEYWORD:
			token = peek_next_token(ctx);
			if(!token)
				return EXPR_VAL_TYPE_UNKNOWN;

			if(token->type == TOKEN_TYPE_SYMBOL && 
					token->symbol == SYNTAX_SYM_START_PARENTHESIS)
				return EXPR_VAL_TYPE_CALL;
			return EXPR_VAL_TYPE_NORMAL;
		case TOKEN_TYPE_SYMBOL:
			/* keyword: block or neg block */
			if(token->symbol == SYNTAX_SYM_START_PARENTHESIS)
				return EXPR_VAL_TYPE_BLOCK;
			else if(token->symbol == SYNTAX_SYM_VARIABLE) {
				return EXPR_VAL_TYPE_VAR;
			}
			break;
		case TOKEN_TYPE_STRING:
			return EXPR_VAL_TYPE_NORMAL;
	}

	return EXPR_VAL_TYPE_UNKNOWN;
}

static int peek_expr_type(struct interp_context *ctx)
{
	struct lex_token *token;

	token = peek_curr_token(ctx);
	if(!token)
		return EXPR_TYPE_EOF;

	switch(token->type) {
		case TOKEN_TYPE_KEYWORD:
			/* keyword: normal expr or func */
			token = peek_next_token(ctx);
			if(!token)
				return EXPR_TYPE_UNKNOWN;
			if(token->type == TOKEN_TYPE_SYMBOL && 
					(token->symbol == SYNTAX_SYM_START_BRACE || 
					 token->symbol == SYNTAX_SYM_START_PARENTHESIS))
				return EXPR_TYPE_FUNC;
			return EXPR_TYPE_NORMAL;
			break;
		case TOKEN_TYPE_SYMBOL:
			/* keyword: block or neg block */
			if(token->symbol == SYNTAX_SYM_START_PARENTHESIS)
				return EXPR_TYPE_BLOCK;
			else if(token->symbol == SYNTAX_SYM_NOT) {
				token = peek_next_token(ctx);
				if(!token)
					return EXPR_TYPE_UNKNOWN;
				if(token->type == TOKEN_TYPE_SYMBOL && 
						token->symbol == SYNTAX_SYM_START_PARENTHESIS)
					return EXPR_TYPE_NEG_BLOCK;
			}
			break;
	}

	return EXPR_TYPE_UNKNOWN;
}


static int syntax_parse_func(struct interp_context *ctx, 
		struct syntax_func **ppfunc);
static int syntax_parse_block(struct interp_context *ctx, 
		struct syntax_block **ppblock);
static int syntax_parse_call(struct interp_context *ctx, 
		struct syntax_call **ppcall);

static int syntax_parse_val(struct interp_context *ctx, 
		struct syntax_root *parent)
{
	int ret;
	int type;
	int opt;
	int bracket;
	int isvar = 0;
	char *val;
	struct syntax_valblock *exprvb;
	struct syntax_val *exprval;
	struct syntax_call *exprvcall;
	struct syntax_node *node;

	/* Peek val type */
	type = peek_expr_val_type(ctx);
	switch(type) {
		case EXPR_VAL_TYPE_BLOCK:
			/* '(' */
			read_bracket(ctx);
			exprvb = syntax_valblock_create(ctx);
			syntax_parse_val(ctx, &exprvb->childs);
			node = &exprvb->node;
			/* ')' */
			bracket = read_end_bracket(ctx);
			if(bracket != SYNTAX_SYM_END_PARENTHESIS) {
				loge("Syntax err: expr block needs ')'.");
				syntax_valblock_destroy(ctx, exprvb);
				goto err;
			}
			break;
		case EXPR_VAL_TYPE_CALL:
			ret = syntax_parse_call(ctx, &exprvcall);
			if(ret) {
				loge("Syntax err: parse call failed.");
				goto err;
			}
			node = &exprvcall->node;
			break;
		case EXPR_VAL_TYPE_VAR:
			/* '$' */
			skip_token(ctx);
			isvar = 1;
		case EXPR_VAL_TYPE_NORMAL:	
			/* Normal */
			val = read_keyword(ctx);
			CHECK_KEYWORD_GOTO(val, err);

			exprval = syntax_val_create(ctx);
			exprval->val = val;
			exprval->isvar = isvar;
			node = &exprval->node;
			break;
		default:
			loge("Syntax err: unknown expr val type(%d).", type);
			goto err;
			break;
	}
	syntax_add(node, parent);

	opt = peek_operator(ctx);
	if(get_symbol_type(opt) != SYM_TYPE_OPT_ARITH)
		return 0;

	opt = read_operator(ctx);
	node->opt = opt;

	return syntax_parse_val(ctx, parent);
err:
	return -EINVAL;
}

static int syntax_parse_expr(struct interp_context *ctx, 
		struct syntax_expr **ppexpr)
{
	int ret;
	int opt;
	char *key;
	struct syntax_expr *expr;

	expr = syntax_expr_create(ctx);

	key = read_keyword(ctx);
	CHECK_KEYWORD_GOTO(key, err);
	expr->key = key;
	logv("Syntax: parse expr '%s'", expr->key);

	opt = read_operator(ctx);
	if(get_symbol_type(opt) != SYM_TYPE_OPT_COMP) {
		loge("Syntax err: operator error:%d.", opt);
		goto err;
	}
	expr->opt = opt;

	ret = syntax_parse_val(ctx, &expr->valchilds);
	if(ret) {
		loge("Syntax err: parse expr val error.");
		goto err;
	}

	*ppexpr = expr;

	logd("Syntax: parse expr '%s' success", expr->key);
	return 0;
err:
	syntax_expr_destroy(ctx, expr);
	return -EINVAL;
}

static int __syntax_parse_block(struct interp_context *ctx, 
		struct syntax_root *childroot)
{
	int ret;
	int opt;
	int type;
	int bracket;
	struct syntax_func *func;
	struct syntax_block *block;
	struct syntax_expr *expr;
	struct syntax_node *node;

	for( ;; ) {
		type = peek_expr_type(ctx);
		if(type == EXPR_TYPE_EOF)
			break;

		switch(type) {
			case EXPR_TYPE_UNKNOWN:
				loge("Syntax err: unknown expr type.");
				goto err;
			case EXPR_TYPE_FUNC:
				ret = syntax_parse_func(ctx, &func);
				if(ret)
					goto err;
				node = &func->node;
				break;
			case EXPR_TYPE_NEG_BLOCK:
				/* skip '!(' */
				skip_negblock(ctx);
				ret = syntax_parse_block(ctx, &block);
				if(ret)
					goto err;

				bracket = read_end_bracket(ctx);
				if(bracket != SYNTAX_SYM_END_PARENTHESIS) {
					loge("Syntax err: block end bracket error:%d.", bracket);
					goto err;
				}
				block->negative = 1;
				node = &block->node;
				break;
			case EXPR_TYPE_BLOCK:
				/* skip '(' */
				read_bracket(ctx);
				ret = syntax_parse_block(ctx, &block);
				if(ret)
					goto err;

				bracket = read_end_bracket(ctx);
				if(bracket != SYNTAX_SYM_END_PARENTHESIS) {
					loge("Syntax err: block end bracket error:%d..", bracket);
					goto err;
				}
				node = &block->node;
				break;
			case EXPR_TYPE_NORMAL:
			default:
				ret = syntax_parse_expr(ctx, &expr);
				if(ret)
					goto err;
				node = &expr->node;
				break;
		}

		syntax_add(node, childroot);
		/* && || ! */
		if (peek_operator(ctx) > 0) {
			opt = read_operator(ctx);
			if(get_symbol_type(opt) != SYM_TYPE_OPT_LOGIC) {
				loge("Syntax err: operator error:%d.", opt);
				goto err;
			}
			node->opt = opt;
			continue;
		} 

		if (peek_end_bracket(ctx) > 0) {
			/* Peek '}' or ')' */
			break;
		}
	}

	return 0;
err:
	return -EINVAL;
}

static int syntax_parse_block(struct interp_context *ctx, 
		struct syntax_block **ppblock)
{
	int ret;
	struct syntax_block *block;

	block = syntax_block_create(ctx);

	ret = __syntax_parse_block(ctx, &block->childs);
	if(ret) {
		loge("Syntax err: parse block error.");
		goto err;
	}
	*ppblock = block;

	return 0;
err:
	syntax_block_destroy(ctx, block);
	return -EINVAL;
}

static int syntax_parse_func_args(struct interp_context *ctx, 
		struct syntax_args *args)
{
	int i;
	char *argkey;
	int ret;
	int opt;

	args->count = 0;

	/* Not input args */
	if(peek_end_bracket(ctx) == SYNTAX_SYM_END_PARENTHESIS) {
		read_end_bracket(ctx);
		return 0;
	}

	for(i=0; i < SYNTAX_ARG_MAX; i++) {
		argkey = read_keyword(ctx);
		CHECK_KEYWORD_GOTO(argkey, err);
		args->args[i].key = argkey;
		logd("Syntax: parse args '%s'", args->args[i].key);

		opt = read_operator(ctx);
		if(get_symbol_type(opt) != SYM_TYPE_OPT_ASSIGN) {
			loge("Syntax err: arg expr error.");
			goto err;
		}

		ret = syntax_parse_val(ctx, &args->args[i].valchilds);
		if(ret) {
			loge("Syntax err: parse arg val error.");
			goto err;
		}

		args->count++;

		if(peek_comma(ctx) > 0) {
			read_comma(ctx);
			continue;
		}

		if(peek_end_bracket(ctx) == SYNTAX_SYM_END_PARENTHESIS) {
			read_end_bracket(ctx);
			break;
		}
	}

	return 0;
err:
	args->count = 0;
	return -EINVAL;
}

static int syntax_parse_call_args(struct interp_context *ctx, 
		struct syntax_args *args)
{
	return syntax_parse_func_args(ctx, args);
}

static int syntax_parse_func_body(struct interp_context *ctx, 
		struct syntax_func_body *body)
{
	return __syntax_parse_block(ctx, &body->childs);
}

static int syntax_parse_call(struct interp_context *ctx, 
		struct syntax_call **ppcall)
{
	int ret = 0;
	char *keyword;
	int bracket;
	struct syntax_call *call;
	struct lex_token *token;

	call = syntax_call_create(ctx);

	keyword = read_keyword(ctx);
	CHECK_KEYWORD_GOTO(keyword, err);
	call->keyword = keyword;

	logv("Syntax: parse call '%s'", call->keyword);
	bracket = read_bracket(ctx);
	if(bracket == SYNTAX_SYM_START_PARENTHESIS) {
		token = peek_next_token(ctx);
		if(token->type == TOKEN_TYPE_SYMBOL && 
				token->symbol == SYNTAX_SYM_START_PARENTHESIS) {
			call->args.count = 0;
		} else {
			ret = syntax_parse_call_args(ctx, &call->args);
			if(ret) {
				loge("Syntax err: parse call args failed.");
				goto err;	
			}
		}
	} else {
		loge("Syntax err: parse call failed. not found args and body");
		goto err;
	}
	*ppcall = call;
	logd("Syntax: parse call '%s' success", call->keyword);
	return 0;

err:
	syntax_call_destroy(ctx, call);
	return -EINVAL;
}

static int syntax_parse_func(struct interp_context *ctx, 
		struct syntax_func **ppfunc)
{
	int ret = 0;
	char *keyword;
	int bracket;
	struct syntax_func *func;

	func = syntax_func_create(ctx);

	keyword = read_keyword(ctx);
	CHECK_KEYWORD_GOTO(keyword, err);
	func->keyword = keyword;
	logv("Syntax: parse func '%s'", func->keyword);

	bracket = read_bracket(ctx);
	if(bracket == SYNTAX_SYM_START_PARENTHESIS) {
		ret = syntax_parse_func_args(ctx, &func->args);
		if(ret) {
			loge("Syntax err: parse func args failed.");
			goto err;	
		}
		bracket = read_bracket(ctx);
		if(bracket != SYNTAX_SYM_START_BRACE) {
			loge("Syntax err: parse func failed. not found body");
			goto err;
		}

		ret = syntax_parse_func_body(ctx, &func->body);
		if(ret) {
			loge("Syntax err: parse func body failed.");
			goto err;	
		}
	} else if(bracket == SYNTAX_SYM_START_BRACE) {
		ret = syntax_parse_func_body(ctx, &func->body);
		if(ret) {
			loge("Syntax err: parse func body failed..");
			goto err;	
		}
	} else {
		loge("Syntax err: parse func failed. not found args and body");
		goto err;
	}

	bracket = read_end_bracket(ctx);
	if(bracket != SYNTAX_SYM_END_BRACE) {
		loge("Syntax err: func end bracket error:%d.", bracket);
		goto err;
	}
	*ppfunc = func;
	logd("Syntax: parse func '%s' success", func->keyword);
	return 0;

err:
	syntax_func_destroy(ctx, func);
	return -EINVAL;
}

static int syntax_parse_content(struct interp_context *ctx)
{
	int ret;
	struct syntax_content *content;

	content = syntax_content_create(ctx);

	ret = __syntax_parse_block(ctx, &content->childs);
	if(ret) {
		loge("Syntax err: parse content err.");
		return -EINVAL;
	}
	ctx->tree = &content->childs;
	return 0;
}

int interp_syntax_parse(struct interp_context *ctx)
{
	int ret;

	ctx->tree = NULL;
	ret = syntax_parse_content(ctx);

	return ret;
}


static void destroy_syntax_childs(struct interp_context *ctx, struct syntax_root *root);
static void destroy_syntax_call(struct interp_context *ctx, struct syntax_call *call);

static void destroy_syntax_val(struct interp_context *ctx,
		struct syntax_root *root)
{
	struct syntax_node *node;
	struct syntax_val *exprval;
	struct syntax_valblock *exprvb;
	struct syntax_call *exprvcall;

	list_for_each_entry(node, &root->head, entry) {
		switch(node->type) {
			case SYNTAX_NODE_TYPE_VAL:	
				exprval = container_of(node, struct syntax_val, node);
				syntax_val_destroy(ctx, exprval);
				break;
			case SYNTAX_NODE_TYPE_VALBLOCK:	
				exprvb = container_of(node, struct syntax_valblock, node);
				destroy_syntax_val(ctx, &exprvb->childs);
				syntax_valblock_destroy(ctx, exprvb);
				break;
			case SYNTAX_NODE_TYPE_CALL:
				exprvcall = container_of(node, struct syntax_call, node);
				destroy_syntax_call(ctx, exprvcall);
				break;
			default:
				break;
		}
	}
}

static void destroy_syntax_expr(struct interp_context *ctx,
		struct syntax_expr *expr)
{
	destroy_syntax_val(ctx, &expr->valchilds);
	syntax_expr_destroy(ctx, expr);
}

static void destroy_syntax_func(struct interp_context *ctx,
		struct syntax_func *func)
{
	if(func->args.count > 0) {
		int i;
		struct syntax_args *args = &func->args;

		for(i=0; i<args->count; i++) {
			destroy_syntax_val(ctx, &args->args[i].valchilds);
		}
	}

	destroy_syntax_childs(ctx, &func->body.childs);
	syntax_func_destroy(ctx, func);
}

static void destroy_syntax_call(struct interp_context *ctx,
		struct syntax_call *call)
{
	if(call->args.count > 0) {
		int i;
		struct syntax_args *args = &call->args;

		for(i=0; i<args->count; i++) {
			destroy_syntax_val(ctx, &args->args[i].valchilds);
		}
	}
	syntax_call_destroy(ctx, call);
}

static void destroy_syntax_block(struct interp_context *ctx, 
		struct syntax_block *block)
{
	destroy_syntax_childs(ctx, &block->childs);
	syntax_block_destroy(ctx, block);
}

static void destroy_syntax_content(struct interp_context *ctx,
		struct syntax_content *content)
{
	destroy_syntax_childs(ctx, &content->childs);
	syntax_content_destroy(ctx, content);
}


static void destroy_syntax_childs(struct interp_context *ctx, 
		struct syntax_root *root)
{
	struct syntax_func *func;
	struct syntax_block *block;
	struct syntax_expr *expr;
	struct syntax_node *node;

	list_for_each_entry(node, &root->head, entry) {
		switch(node->type) {
			case SYNTAX_NODE_TYPE_FUNC:	
				func = container_of(node, struct syntax_func, node);
				destroy_syntax_func(ctx, func);
				break;
			case SYNTAX_NODE_TYPE_EXPR:	
				expr = container_of(node, struct syntax_expr, node);
				destroy_syntax_expr(ctx, expr);
				break;
			case SYNTAX_NODE_TYPE_BLOCK:	
				block = container_of(node, struct syntax_block, node);
				destroy_syntax_block(ctx, block);
				break;
			default:
				break;
		}
	}
}

void destroy_syntax_tree(struct interp_context *ctx,
		struct syntax_root *tree)
{
	struct syntax_content *content;

	if(!tree)
		return;

	content = container_of(tree, struct syntax_content, childs);
	destroy_syntax_content(ctx, content);
}


static int dump_syntax_childs(struct syntax_root *root, int level, char *ptr);
static int dump_syntax_call(struct syntax_call *call, int level, char *ptr);

static int dump_syntax_val(struct syntax_root *root, int level, char *ptr)
{
	int len = 0;
	struct syntax_node *node;
	struct syntax_val *exprval;
	struct syntax_valblock *exprvb;
	struct syntax_call *exprvcall;

	list_for_each_entry(node, &root->head, entry) {
		switch(node->type) {
			case SYNTAX_NODE_TYPE_VAL:	
				exprval = container_of(node, struct syntax_val, node);
				if(exprval->isvar) {
					printf("$");
					len += sprintf(ptr + len, "$");
				}
				printf("%s", exprval->val);
				len += sprintf(ptr + len, "%s", exprval->val);
				break;
			case SYNTAX_NODE_TYPE_VALBLOCK:	
				exprvb = container_of(node, struct syntax_valblock, node);
				len += sprintf(ptr + len, "(");
				printf("(");
				len += dump_syntax_val(&exprvb->childs, level + 1, ptr + len);
				len += sprintf(ptr + len, ")");
				printf(")");
				break;
			case SYNTAX_NODE_TYPE_CALL:
				exprvcall = container_of(node, struct syntax_call, node);
				len += dump_syntax_call(exprvcall, level + 1, ptr + len);
				break;
			default:
				break;
		}

		if(node->opt) {
			printf(" %s ", get_symbol_str(node->opt));
			len += sprintf(ptr + len, " %s ", get_symbol_str(node->opt));
		}
	}

	return len;
}

static int dump_syntax_expr(struct syntax_expr *expr, int level, char *ptr)
{
	int len = 0;
	len += sprintf(ptr + len, "%s %s ", expr->key, get_symbol_str(expr->opt));
	printf("expr: %s %s ", expr->key, get_symbol_str(expr->opt));

	len += dump_syntax_val(&expr->valchilds, level + 1, ptr + len);

	return len;
}

static int dump_syntax_func(struct syntax_func *func, int level, char *ptr)
{
	int len = 0;

	len = sprintf(ptr, "%s", func->keyword);
	printf("keyword: %s\n", func->keyword);

	if(func->args.count > 0) {
		int i;
		struct syntax_args *args = &func->args;

		len += sprintf(ptr + len, "(");
		for(i=0; i<args->count; i++) {
			len += sprintf(ptr + len, "%s=", args->args[i].key);
			printf("arg[%d]: %s\n", i, args->args[i].key);

			len += dump_syntax_val(&args->args[i].valchilds, level + 1, ptr + len);
			if(i < args->count - 1)
				len += sprintf(ptr + len, ",");
		}
		len += sprintf(ptr + len, ")");
	}

	len += sprintf(ptr + len, "{");
	len += dump_syntax_childs(&func->body.childs, level + 1, ptr + len);
	len += sprintf(ptr + len, "}");
	
	return len;
}

static int dump_syntax_call(struct syntax_call *call, int level, char *ptr)
{
	int len = 0;

	len = sprintf(ptr, "%s", call->keyword);
	printf("keyword: %s\n", call->keyword);

	if(call->args.count > 0) {
		int i;
		struct syntax_args *args = &call->args;

		len += sprintf(ptr + len, "(");
		for(i=0; i<args->count; i++) {
			len += sprintf(ptr + len, "%s=", args->args[i].key);
			printf("arg[%d]: %s\n", i, args->args[i].key);

			len += dump_syntax_val(&args->args[i].valchilds, level + 1, ptr + len);
			if(i < args->count - 1)
				len += sprintf(ptr + len, ",");
		}
		len += sprintf(ptr + len, ")");
	}
	
	return len;
}

static int dump_syntax_block(struct syntax_block *block, int level, char *ptr)
{
	int len = 0;

	if(block->negative)
		len = sprintf(ptr + len, "!");
	len += sprintf(ptr + len, "(");
	len += dump_syntax_childs(&block->childs, level + 1, ptr + len);
	len += sprintf(ptr + len, ")");

	return len;
}

static int dump_syntax_content(struct syntax_content *content, int level, char *ptr)
{
	int len = 0;

	len += dump_syntax_childs(&content->childs, level + 1, ptr + len);

	return len;
}


static int dump_syntax_childs(struct syntax_root *root, int level, char *ptr)
{
	int len = 0;
	struct syntax_func *func;
	struct syntax_block *block;
	struct syntax_expr *expr;
	struct syntax_node *node;

	list_for_each_entry(node, &root->head, entry) {
		switch(node->type) {
			case SYNTAX_NODE_TYPE_FUNC:	
				func = container_of(node, struct syntax_func, node);
				len += dump_syntax_func(func, level + 1, ptr + len);
				break;
			case SYNTAX_NODE_TYPE_EXPR:	
				expr = container_of(node, struct syntax_expr, node);
				len += dump_syntax_expr(expr, level + 1, ptr + len);
				break;
			case SYNTAX_NODE_TYPE_BLOCK:	
				block = container_of(node, struct syntax_block, node);
				len += dump_syntax_block(block, level + 1, ptr + len);
				break;
			default:
				break;
		}

		if(node->opt) {
			printf("\nopt:%s\n", get_symbol_str(node->opt));
			len += sprintf(ptr + len, " %s ", get_symbol_str(node->opt));
		}
	}
	return len;
}

void dump_syntax_tree(struct interp_context *ctx)
{
	int level = 0;
	int len;
	char *buf;
	struct syntax_content *content;
	struct syntax_root *tree = ctx->tree;
	struct interp *interp = ctx->interp;

	if(!tree)
		return;

	buf = xzalloc(interp->cbufsize);
	content = container_of(tree, struct syntax_content, childs);

	len += dump_syntax_content(content, level, buf);

	printf("\n-------------------------------------------\n");
	printf("%s\n", buf);
	printf("\n-------------------------------------------\n");
	free(buf);
}


