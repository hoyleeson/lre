#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include "lre_internal.h"

#define LR_GLOBAL_MACRO_PATH 	"global_macro.lr"

#define DEFAULT_MACRO_HLIST_BIT (8)

static int macro_hlist_cap_bits;
static int macro_count;
static struct hlist_head *macro_hlists;
static struct interp *macro_interp;


struct lre_macro_arg {
	char *keyword;
	int wordlen; /* Reduce repeated computation */
};


struct lre_macro_token {
	int type;
#define MACRO_TOKEN_TYPE_ARG 	(1)
#define MACRO_TOKEN_TYPE_WORD 	(2)

	union {
		int argindex;
		char *wordptr;
	};
};

struct lre_macro {
	char *keyword;
	int argc;
	struct lre_macro_arg args[LRE_MACRO_ARGC_MAX];

	struct lre_macro_token *token;
	int tokencnt;

	char *macro;
	struct hlist_node hnode;
};


struct lre_macro_token *read_macro_token(struct lre_macro *macro, int i)
{
	if(i >= macro->tokencnt)
		return NULL;
	return &macro->token[i];
}

/* 
 * Find macro by keyword and arg_count 
 * */
struct lre_macro *lre_find_macro(const char *str, int argc)
{
	int hashidx;
	struct lre_macro *macro;

	hashidx = hash_str(str, macro_hlist_cap_bits);

	hlist_for_each_entry(macro, &macro_hlists[hashidx], hnode) {
		if(!strcmp(macro->keyword, str) && macro->argc == argc) {
			return macro;	
		}
	}
	return NULL;
}

int is_lre_macro(const char *str)
{
	int hashidx;
	struct lre_macro *macro;

	hashidx = hash_str(str, macro_hlist_cap_bits);

	hlist_for_each_entry(macro, &macro_hlists[hashidx], hnode) {
		if(!strcmp(macro->keyword, str)) {
			return 1;
		}
	}
	return 0;
}

char *lre_create_code_by_macro(struct lre_macro *macro, 
		int argc, const char **args)
{
	int i;
	int len = 0;
	char *ptr;
	int idx;
	char code[DEFAULT_CODEBUF_SIZE] = {0};

	if(macro->argc != argc)
		return NULL;

	for(i=0; i<macro->tokencnt; i++) {
		if(macro->token[i].type == MACRO_TOKEN_TYPE_WORD) {
			ptr = macro->token[i].wordptr;
			len += xsnprintf(code + len, DEFAULT_CODEBUF_SIZE - len, "%s",
					ptr);
		} else if(macro->token[i].type == MACRO_TOKEN_TYPE_ARG) {
			idx = macro->token[i].argindex;
			len += xsnprintf(code + len, DEFAULT_CODEBUF_SIZE - len, "%s", 
					args[idx]);
		}
	}

	ptr = strdup(code);
	assert_ptr(ptr);
	return ptr;
}


static void lre_macro_add(struct lre_macro *macro)
{
	int hashidx;

	/* FIXME: expand hashlist*/

	hashidx = hash_str(macro->keyword, macro_hlist_cap_bits);
	hlist_add_head(&macro->hnode, &macro_hlists[hashidx]);
	macro_count++;
}

static __attribute__((unused)) void lre_macro_dump(void)
{
	int i;
	struct lre_macro *macro;
	struct lre_macro_token *mtoken;

	for (i = 0; i < (1 << macro_hlist_cap_bits); ++i) {
		hlist_for_each_entry(macro, &macro_hlists[i], hnode) {
			int j;

			printf("%s\n", macro->macro);
			printf("macro keyword:'%s', arg count:%d\n", macro->keyword, macro->argc);
			for(j = 0; j < macro->argc; j++)
				printf("macro arg[%d]: '%s'\n", j, macro->args[j].keyword);

			for(j = 0; j < macro->tokencnt; j++) {
				mtoken = read_macro_token(macro, j);
				if(mtoken->type == MACRO_TOKEN_TYPE_ARG) {
					printf("macro token[%d]: (arg). '%s' argindex %d\n", 
							j, macro->args[mtoken->argindex].keyword, mtoken->argindex);
				} else
					printf("macro token[%d]: (content). '%s'\n", j, mtoken->wordptr);
			}
			printf("\n");
		}
	}
}

static int lre_macro_content2token(struct lre_macro *macro, char *content)
{
	int i;
	int cnt = 0;
	char *ptr, *p;
	struct lre_macro_arg *arg;
	struct lre_macro_token token[2 * LRE_MACRO_ARGC_MAX + 1];

	ptr = content;
	logv("Macro: content '%s' to token.", content);

	token[cnt].type = MACRO_TOKEN_TYPE_WORD;
	token[cnt].wordptr = ptr;
	cnt++;

	while(strchr(ptr, '$')) {
		p = ptr;
		ptr++;
		for(i=0; i<macro->argc; i++) {
			arg = &macro->args[i];
			if(!strncmp(ptr, arg->keyword, arg->wordlen)) {
				*p = '\0';
				token[cnt].type = MACRO_TOKEN_TYPE_ARG;
				token[cnt].argindex = i;
				cnt++;

				ptr += arg->wordlen;
				token[cnt].type = MACRO_TOKEN_TYPE_WORD;
				token[cnt].wordptr = ptr;
				cnt++;
				break;
			}
		}
	}

	macro->token = xzalloc(sizeof(struct lre_macro_token) * cnt);
	for(i=0; i<cnt; i++) {
		macro->token[i].type = token[i].type;
		if(macro->token[i].type == MACRO_TOKEN_TYPE_WORD) {
			macro->token[i].wordptr = strdup(token[i].wordptr);
			assert_ptr(macro->token[i].wordptr);
		} else
			macro->token[i].argindex = token[i].argindex;
	}
	macro->tokencnt = cnt;
	logv("Macro: token count:%d.", cnt);

	return 0;
}

static int lre_macro_arg_parse(struct interp_context *ctx, 
		struct lre_macro_arg *args)
{
	int i = 0;
	struct lex_token *token;
	struct lre_macro_arg *arg;

	logd("Macro: parse macro argument.");

	token = read_token(ctx);
	if(!token || (token->type != TOKEN_TYPE_SYMBOL ) || 
			(token->symbol != SYNTAX_SYM_START_PARENTHESIS)) {
		loge("Macro err: invaild macro format");
		return -EINVAL;
	}

	token = peek_curr_token(ctx);
	if(!token) {
		loge("Macro err: failed to get arg token");
		return -EINVAL;
	}

	if(token->type == TOKEN_TYPE_SYMBOL && 
			token->symbol == SYNTAX_SYM_END_PARENTHESIS) {
		token = read_token(ctx);
		return 0;
	}

	for(i=0; i<LRE_MACRO_ARGC_MAX;	i++) {
		arg = args + i;
		token = read_token(ctx);
		if(!token || (token->type != TOKEN_TYPE_KEYWORD)) {
			loge("Macro err: invaild argument keyword");
			return -EINVAL;
		}

		arg->keyword = strdup(token->word);
		assert_ptr(arg->keyword);
		arg->wordlen = strlen(arg->keyword);
		logv("arg[%d]: %s", i, arg->keyword);

		/* Expect ',' or ')' */
		token = read_token(ctx);
		if(!token || token->type != TOKEN_TYPE_SYMBOL) {
			loge("Macro err: failed to get arg token.");
			return -EINVAL;
		}

		if(token->symbol == SYNTAX_SYM_COMMA) {
			continue;
		} else if(token->symbol == SYNTAX_SYM_END_PARENTHESIS) {
			return i + 1;
		} else {
			loge("Macro err: unexpect arg token:'%s'", token->word);	
			return -EINVAL;
		}
	}

	loge("Macro err: macro args count no more than %d.", LRE_MACRO_ARGC_MAX);
	return -EINVAL;
}

static int lre_macro_parse(struct interp_context *ctx)
{
	char *ptr;
	struct lex_token *token;
	struct lre_macro *macro;

	macro = xzalloc(sizeof(*macro));

	macro->macro = strdup(ctx->codeptr);
	assert_ptr(macro->macro);

	token = read_token(ctx);
	if(!token || (token->type != TOKEN_TYPE_KEYWORD)) {
		loge("Macro err: invaild macro name");
		return -EINVAL;
	}

	macro->keyword = strdup(token->word);
	assert_ptr(macro->keyword);
	logv("macro: %s", macro->keyword);

	macro->argc = lre_macro_arg_parse(ctx, macro->args);
	if(macro->argc < 0) {
		loge("Macro err: parse macro args error.");
		return -EINVAL;
	}

	logd("Macro: macro args count:%d.", macro->argc);
	token = read_token(ctx);
	if(!token || (token->type != TOKEN_TYPE_STRING)) {
		loge("Macro err: not found macro content.");
		return -EINVAL;
	}
	ptr = token->word;
	if(ptr[0] == '"') {
		int l = strlen(ptr);
		ptr[l - 1] = 0;
		ptr++;
	}

	lre_macro_content2token(macro, ptr);
	lre_macro_add(macro);
	return 0;
}

static int lre_macro_conf_load(const char *path);

static int lre_macro_conf_parse(char *mstr)
{
	int ret;
	struct lex_token *token;
	struct interp_context *ctx;

	ctx = interp_context_create(macro_interp, mstr);

	ret = interp_lexer_analysis(ctx);
	if(ret) {
		loge("Macro err: lexer analysis error.");
		interp_context_destroy(ctx);
		return -EINVAL;
	}

	interp_context_refresh(ctx);

	token = read_token(ctx);
	if(!token) {
		loge("Macro err: not found effective token.");
		return -EINVAL;
	}

	if(!strcmp(token->word, "include")) {
		char *sub;
		token = read_token(ctx);
		if(!token || (token->type != TOKEN_TYPE_STRING)) {
			loge("Macro err: invaild config include.");
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
		if(ret) {
			loge("Macro err: failed to parse macro.");	
			goto out;
		}
	}

out:
	interp_context_destroy(ctx);
	return 0;
}


#define _FLAGS_NEWLINE 		(1)
#define _FLAGS_FOLLOWLINE 	(2)
static int read_macro_line(char *buf, int maxlen, FILE *confp, int flags)
{
	char *p;
	int len;

repeat:
    if (xgetline(buf, maxlen, confp)) {
        /* if the last character is a newline, shorten the string by 1 byte */
        len = strlen(buf);
        if (buf[len - 1] == '\n') {
            buf[--len] = '\0';
        }

        if (buf[len - 1] == '\\') {
			buf[--len] = '\0';
			len += read_macro_line(buf + len, maxlen - len, confp, _FLAGS_FOLLOWLINE);
		}

        /* Skip any leading whitespace */
        p = buf;
        while (isspace(*p)) {
            p++;
        }
        /* ignore comments or empty lines */
        if ((*p == '#' || *p == '\0') && 
				flags != _FLAGS_FOLLOWLINE)
            goto repeat;
		return len;
    }

	return 0;
}

static int lre_macro_conf_load(const char *fpath)
{
    FILE *confp;
    char *buf;
	int len;
	char path[PATH_MAX] = { 0 };

	xsnprintf(path, PATH_MAX, "%s/%s", lre_get_conf_path(), fpath);
	logv("Macro: load lre macro config: %s.", path);

    confp = fopen(path, "r");
    if (!confp) {
        loge("Macro: cannot open config file %s", path);
        return 0;
    }
	
	buf = xzalloc(DEFAULT_CODEBUF_SIZE * 2);

	while((len = read_macro_line(buf, DEFAULT_CODEBUF_SIZE, confp, _FLAGS_NEWLINE)) > 0) {
		logd("MACRO: '%s', len:%d", buf, len);
		lre_macro_conf_parse(buf);
	}

	logi("Macro: load lre macro config: %s success.", path);
	free(buf);
	return 0;
}

int lre_macro_init(void)
{
	int i;
	int sz;
	int ret;

	macro_interp = interpreter_create();

	macro_hlist_cap_bits = DEFAULT_MACRO_HLIST_BIT;
	macro_count = 0;
	sz = sizeof(struct hlist_head) * (1 << macro_hlist_cap_bits);
	macro_hlists = xzalloc(sz);

	for (i = 0; i < 1 << macro_hlist_cap_bits; ++i)
		INIT_HLIST_HEAD(macro_hlists + i);

	ret = lre_macro_conf_load(LR_GLOBAL_MACRO_PATH);
	if(ret) {
		loge("Macro: Failed to load macro.");
		return -EINVAL;
	}
	logi("Macro: Load macro success. count:%d", macro_count);
	/*	lre_macro_dump(); */
	return 0;
}

void lre_macro_release(void)
{
	int i;
	struct hlist_node *safe;
	struct lre_macro *macro;

	for (i = 0; i < (1 << macro_hlist_cap_bits); ++i) {
		hlist_for_each_entry_safe(macro, safe, &macro_hlists[i], hnode) {
			int j;

			hlist_del(&macro->hnode);
			free(macro->keyword);

			for(j=0; j<macro->argc; j++) {
				free(macro->args[j].keyword);
			}

			for(j=0; j<macro->tokencnt; j++) {
				if(macro->token[j].type == MACRO_TOKEN_TYPE_WORD)
					free(macro->token[j].wordptr);
			}
			free(macro->macro);
			free(macro->token);
			free(macro);
		}
	}
}

