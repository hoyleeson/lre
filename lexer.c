#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "interpreter.h"


#define T_EOF 		(1)
#define T_OK 		(0)
#define T_ERR 		(-1)

static int skip_blank(struct interp_context *ctx)
{
	char *x = ctx->codeptr;

	for (;;) {
		switch (*x) {
			case 0:
				ctx->codeptr = x;
				return T_EOF;
			case ' ':
			case '\t':
			case '\r':
			case '\n':
				x++;
				continue;
			default:
				goto out;
		}
	}

out:
	ctx->codeptr = x;
	return 0;
}

static int next_token(struct interp_context *ctx)
{
	int res;
	char c;
	char *x;
	char *s;
	int symid = -1;
	struct lex_token *token;
	struct symbol_word *symword = NULL;

	token = &ctx->tokens[ctx->tokenidx];
	lex_token_init(token);

	res = skip_blank(ctx);
	if(res == T_EOF) {
		goto done;
	}

	x = ctx->codeptr;
	s = ctx->wordptr;

	for (c = *x; c != '\0'; c = *(++x), ++s) {
		/* FIXME: isdigit(), islower(), isupper() */
		if((c >= 'a' && c <= 'z') || 
				(c >= 'A' && c <= 'Z') || 
				(c >= '0' && c <= '9') || c == '_') {
			*s = *x;
			continue;
		} else
			break;
	}

	if(x != ctx->codeptr) {
		token->word = ctx->wordptr;
		token->type = TOKEN_TYPE_KEYWORD;
		*(++s) = 0;
		ctx->wordptr = s;
		goto keyworddone;
	}

	switch (*x) {
		case 0:
			goto done;
		case '"':
			*s++ = *x++;
			for (;;) {
				switch (*x) {
					case '"':
						*s++ = *x++;
						token->word = ctx->wordptr;
						token->type = TOKEN_TYPE_STRING;
						*(++s) = 0;
						ctx->wordptr = s;
						goto strdone;
					case 0:
						/* unterminated quoted thing */
						res = T_ERR;
						goto done;
					default:
						*s++ = *x++;
				}
			}
			break;
		case '&':
			if(*(++x) == '&') {
				x++;
				symid = SYNTAX_SYM_AND;
				break;
			}
			symid = SYNTAX_SYM_BITAND;
			break;
		case '|':
			if(*(++x) == '|') {
				x++;
				symid = SYNTAX_SYM_OR;
				break;
			}
			symid = SYNTAX_SYM_BITOR;
			break;
		case '!':
			if(*(++x) == '=') {
				x++;
				symid = SYNTAX_SYM_NOT_EQUAL;
				break;
			}
			symid = SYNTAX_SYM_NOT;
			break;
		case '=':
			if(*(++x) == '=') {
				x++;
				symid = SYNTAX_SYM_EQUAL;
				break;
			}
			symid = SYNTAX_SYM_ASSIGN;
			break;
		case '>':
			if(*(++x) == '=') {
				x++;
				symid = SYNTAX_SYM_GT_OR_EQUAL;
				break;
			}
			symid = SYNTAX_SYM_GT;
			break;
		case '<':
			if(*(++x) == '=') {
				x++;
				symid = SYNTAX_SYM_LT_OR_EQUAL;
				break;
			}
			symid = SYNTAX_SYM_LT;
			break;
		case '{':
			x++;
			symid = SYNTAX_SYM_START_BRACE;
			break;
		case '}':
			x++;
			symid = SYNTAX_SYM_END_BRACE;
			break;
		case '(':
			x++;
			symid = SYNTAX_SYM_START_PARENTHESIS;
			break;
		case ')':
			x++;
			symid = SYNTAX_SYM_END_PARENTHESIS;
			break;
		case '$':
			x++;
			symid = SYNTAX_SYM_VARIABLE;
			break;
		case ',':
			x++;
			symid = SYNTAX_SYM_COMMA;
			break;
		case '%':
			x++;
			symid = SYNTAX_SYM_MOD;
			break;
		case '~':
			x++;
			symid = SYNTAX_SYM_INVERT;
			break;
		case '^':
			x++;
			symid = SYNTAX_SYM_BITXOR;
			break;
		case '+':
			if(*(++x) == '=') {
				x++;
				symid = SYNTAX_SYM_ADD_EQUAL;
				break;
			}
			symid = SYNTAX_SYM_ADD;
			break;
		case '-':
			if(*(++x) == '=') {
				x++;
				symid = SYNTAX_SYM_SUB_EQUAL;
				break;
			}
			symid = SYNTAX_SYM_SUB;
			break;
		case '*':
			if(*(++x) == '=') {
				x++;
				symid = SYNTAX_SYM_MUL_EQUAL;
				break;
			}
			symid = SYNTAX_SYM_MUL;
			break;
		case '/':
			if(*(++x) == '=') {
				x++;
				symid = SYNTAX_SYM_DIV_EQUAL;
				break;
			}
			symid = SYNTAX_SYM_DIV;
			break;
		default:
			break;
	}
	if(symid < 0) {
		res = T_ERR;
		goto done;
	}
	symword = get_symbol_by_id(symid);

	token->type = TOKEN_TYPE_SYMBOL;
	token->subtype = symword->type;
	token->word = symword->sym;
	token->symbol = symid;

keyworddone:
strdone:
	ctx->codeptr = x;
	ctx->tokenidx++;
	ctx->tokencnt++;
	res = T_OK;
done:
	return res;
}


static int lexer_read_token(struct interp_context *ctx)
{
	int ret;

	skip_blank(ctx);

	for( ;; ) {
		ret = next_token(ctx);
		if(ret == T_EOF)
			break;
		else if(ret == T_OK)
			continue;
		else {
			loge("Lexer err: get next token failed.");
			return -EINVAL;
		}
	}
	return 0;
}

int interp_lexer_analysis(struct interp_context *ctx)
{
	int ret;

	ret = lexer_read_token(ctx);
	return ret;
}


void dump_lex_tokens(struct interp_context *ctx)
{
	int i;
	struct lex_token *token;
	int len = 0;
	char data[CODE_MAX_LEN] = {0};

	if(!ctx)
		return;

	for(i = ctx->tokenidx; i < ctx->tokencnt; i++) {
		token = &ctx->tokens[i];

		len += sprintf(data + len, "%s", token->word);
		printf("token word:%s \ttype:%d \tsubtype:%d",
			   	token->word, token->type, token->subtype);
		if(token->type == TOKEN_TYPE_SYMBOL)
			printf(" \tsymid:%d\n", token->symbol);
		else
			printf("\n");
	}

	printf("---------------------------------------\n");
	printf("%s\n", data);
	printf("---------------------------------------\n");
}


