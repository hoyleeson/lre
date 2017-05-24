#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "interpreter.h"


static int prase_macro_call_arg(struct interp_context *ctx, 
		char **args)
{
	int i;
	struct lex_token *token;

	/* Read symbol '(' */
	token = read_token(ctx);
	if(!token || !(token->type == TOKEN_TYPE_SYMBOL && 
			token->symbol == SYNTAX_SYM_START_PARENTHESIS)) {
		loge("Preprocess err: invaild macro call");
		return -EINVAL;
	}

	token = peek_curr_token(ctx);
	if(!token) {
		loge("Preprocess err: failed to get arg token");
		return -EINVAL;
	}

	if(token->type == TOKEN_TYPE_SYMBOL && 
			token->symbol == SYNTAX_SYM_END_PARENTHESIS) {
		token = read_token(ctx);
		return 0;
	}

	for(i=0; i<LRE_MACRO_ARGC_MAX; i++) {
		/* keyword or string */
		token = read_token(ctx);
		if(!token || !(token->type == TOKEN_TYPE_KEYWORD || 
					token->type == TOKEN_TYPE_STRING)) {
			loge("Preprocess err: invaild argument keyword");
			return -EINVAL;
		}
		args[i] = token->word;

		/* Expect ',' or ')' */
		token = read_token(ctx);
		if(!token || token->type != TOKEN_TYPE_SYMBOL) {
			loge("Preprocess err: failed to get arg token.");
			return -EINVAL;
		}

		if(token->symbol == SYNTAX_SYM_COMMA) {
			continue;
		} else if(token->symbol == SYNTAX_SYM_END_PARENTHESIS) {
			return i + 1;
		} else {
			loge("Preprocess err: unexpect arg token:'%s'", token->word);	
			return -EINVAL;
		}
	}

	loge("Preprocess err: too many macro args count.");
	return -EINVAL;
}

static int preprocess_content(struct interp_context *ctx)
{
	int argc;
	char *key;
	char *args[LRE_MACRO_ARGC_MAX];
	struct lex_token *token;
	struct lre_macro *macro;
	char *code;

	/* Read keyword */
	token = read_token(ctx);

	key = token->word;
	argc = prase_macro_call_arg(ctx, args);
	if(argc < 0) {
		loge("Preprocess err: parse macro call args error.");
		return -EINVAL;
	}

	macro = lre_find_macro(key, argc);
	if(!macro) {
		loge("Preprocess err: not found macro '%s'(argcount: %d)", key, argc);
		return -EINVAL;
	}
	code = lre_create_code_by_macro(macro, argc, (const char **)args);
	logd("Preprocess create code:%s", code);

	/* replace old code */
	interp_context_reload_code(ctx, code);
	return PREPROCESS_RES_REPEAT;
}

int interp_preprocess(struct interp_context *ctx)
{
	int ret;
	struct lex_token *token;

	token = peek_curr_token(ctx);
	if(!token || token->type != TOKEN_TYPE_KEYWORD) {
		loge("Preprocess err: Not found vaild token.");
		return -EINVAL;
	}

	ret = lre_has_macro(token->word);
	if(!ret) {
		return 0;
	}

	return preprocess_content(ctx);
}

