#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "interpreter.h"

#define LR_GLOBAL_MACRO_PATH 	"global_macro.lr"


#define CONF_FLAGS_NEWLINE 		(1)
#define CONF_FLAGS_FOLLOWLINE 	(2)

#define MACRO_ARG_COUNT_MAX 	(16)
struct lre_macro_arg {
	char *keyword;
};

struct lre_macro {
	char *keyword;
	char *content;
	struct lre_macro_arg args[MACRO_ARG_COUNT_MAX];
	int count;
};

static int lre_macro_arg_parse(struct interp_context *ctx, 
		struct lre_macro_arg *args)
{
	int i;
	struct lex_token *token;

	token = peek_curr_token(ctx);
	if(!token)
		return -EINVAL;

	if(token->type == TOKEN_TYPE_SYMBOL && 
			token->symbol == SYNTAX_SYM_START_PARENTHESIS) {
		token = read_token(ctx);
		return 0;
	}

	for(i=0; i<MACRO_ARG_COUNT_MAX;	i++) {
		token = read_token(ctx);
		if(!token || (token->type == TOKEN_TYPE_KEYWORD))
			return -EINVAL;

		args->keyword = token->word;

		token = peek_curr_token(ctx);
		if(!token)
			return -EINVAL;

		if(token->type == TOKEN_TYPE_SYMBOL && 
				token->symbol == SYNTAX_SYM_START_PARENTHESIS) {
			token = read_token(ctx);
			return 0;
		}
	}

	return 0;
}

static int lre_macro_parse(struct interp_context *ctx)
{
	int ret;
	char *ptr;
	struct lex_token *token;
	struct lre_macro *macro;

	macro = xzalloc(sizeof(*macro));

	token = read_token(ctx);
	if(!token || (token->type != TOKEN_TYPE_KEYWORD))
		return -EINVAL;

	macro->keyword = token->word;

	ret = lre_macro_arg_parse(ctx, macro->args);
	if(ret) {
		return -EINVAL;
	}

	token = read_token(ctx);
	if(!token || (token->type != TOKEN_TYPE_STRING))
		return -EINVAL;
	ptr = token->word;
	if(ptr[0] == '"') {
		int l = strlen(ptr);
		ptr[l - 1] = 0;
		ptr++;
	}

	macro->content = ptr;

	return 0;
}

static int lre_macro_conf_load(const char *path);

static int lre_macro_conf_parse(char *mstr)
{
	int ret;
	struct lex_token *token;
	struct interp_context *ctx;

	ctx = interp_context_create(mstr);

	ret = interp_lexer_analysis(ctx);
	if(ret) {
		loge("Interpreter err: lexer analysis error.");
		interp_context_destroy(ctx);
		return -EINVAL;
	}

	interp_context_refresh(ctx);

	token = read_token(ctx);
	if(!token)
		return -EINVAL;

	if(!strcmp(token->word, "include")) {
		char *sub;
		token = read_token(ctx);
		if(!token || (token->type != TOKEN_TYPE_STRING)) {
			ret = -EINVAL;
			goto out;
		}

		sub = token->word;
		if(sub[0] == '"') {
			int l = strlen(sub);
			sub[l - 1] = 0;
			sub++;
		}
		ret = lre_macro_conf_load(sub);
	} else if(!strcmp(token->word, "define")) {
		ret = lre_macro_parse(ctx);
	}

out:
	interp_context_destroy(ctx);
	return 0;
}

static int read_macro_line(char *buf, int maxlen, FILE *confp, int flags)
{

repeat:
    if (xgetline(buf, maxlen, confp)) {
		char *p;
		int len;

        /* if the last character is a newline, shorten the string by 1 byte */
        len = strlen(buf);
        if (buf[len - 1] == '\n') {
            buf[--len] = '\0';
        }

        if (buf[len - 1] == '\\') {
			buf[--len] = '\0';
			len += read_macro_line(buf + len, maxlen - len, confp, CONF_FLAGS_FOLLOWLINE);
		}

        /* Skip any leading whitespace */
        p = buf;
        while (isspace(*p)) {
            p++;
        }
        /* ignore comments or empty lines */
        if ((*p == '#' || *p == '\0') && 
				flags != CONF_FLAGS_FOLLOWLINE)
            goto repeat;
		return len;
    }

	return 0;
}

static int lre_macro_conf_load(const char *path)
{
    FILE *confp;
    char *buf;
	int len;

	logd("load lre macro config.");
    confp = fopen(path, "r");
    if (!confp) {
        loge("Cannot open file %s", path);
        return 0;
    }
	
	buf = xzalloc(CODE_MAX_LEN * 2);

	while((len = read_macro_line(buf, CODE_MAX_LEN, confp, CONF_FLAGS_NEWLINE)) > 0) {
		logd("MACRO: '%s', len:%d", buf, len);
		lre_macro_conf_parse(buf);
	}

	return 0;
}

int lre_macro_init(void)
{
	int ret;
	ret = lre_macro_conf_load(LR_GLOBAL_MACRO_PATH);
	if(ret) {
		loge("Failed to load macro.");
	}

	return 0;
}

