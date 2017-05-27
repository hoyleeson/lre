#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>

#include "../lre.h"

#define DEFAULT_TEST_PATH 	"tests.conf"

struct lre_context *ctx;

void logcb(int level, const char *log)
{
#if 0
	if(level > 3)
		return;
#endif
	fputs(log, stdout);
	fputc('\n', stdout);
	fflush(stdout);
}

static int lrtest_init(void) 
{
	lre_initX(".", logcb);
	ctx = lre_context_create();
	if(!ctx) {
		printf("failed to create lre context.\n");
		return -1;
	}
	return 0;
}

static void lrtest_release(void)
{
	lre_context_destroy(ctx);
	lre_release();
}

static void lrtest_exec(const char *code)
{
	struct lre_result *result;

	result = lre_execute(ctx, code);
	if(!result) {
		printf("not result.\n");
		return;
	}
	if(vaild_lre_results(result->result))
		printf("exec result:%d, detail:%s\n", result->result, result->details);
	else
		printf("exec error, errcode:%d, detail:%s\n", result->errcode, result->details);
	printf("\n");
}


static char *xgetline(char *buf, int size, FILE *file)
{
	int cnt = 0;
	int eof = 0;
	int eol = 0;
	int c;

	if (size < 1) {
		return NULL;
	}

	while (cnt < (size - 1)) {
		c = getc(file);
		if (c == EOF) {
			eof = 1;
			break;
		}

		*(buf + cnt) = c;
		cnt++;

		if (c == '\n') {
			eol = 1;
			break;
		}
	}

	/* Null terminate what we've read */
	*(buf + cnt) = '\0';

	if (eof) {
		if (cnt) {
			return buf;
		} else {
			return NULL;
		}
	} else if (eol) {
		return buf;
	} else {
		/* The line is too long.  Read till a newline or EOF.
		 *          * If EOF, return null, if newline, return an empty buffer.
		 *                   */
		while(1) {
			c = getc(file);
			if (c == EOF) {
				return NULL;
			} else if (c == '\n') {
				*buf = '\0';
				return buf;
			}
		}
	}
}

#define _FLAGS_NEWLINE 		(1)
#define _FLAGS_FOLLOWLINE 	(2)
static int read_rule_line(char *buf, int maxlen, FILE *confp, int flags)
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
			len += read_rule_line(buf + len, maxlen - len, confp, _FLAGS_FOLLOWLINE);
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

#define DEFAULT_CODEBUF_SIZE 	(16 * 1024)
static int lrtest_file_exec(const char *path)
{
    FILE *confp;
    char *buf;
	int len;
	int i = 0;

    confp = fopen(path, "r");
    if (!confp) {
        printf("cannot open lrtest file %s\n", path);
        return 0;
    }
	
	buf = malloc(DEFAULT_CODEBUF_SIZE);

	while((len = read_rule_line(buf, DEFAULT_CODEBUF_SIZE, confp, _FLAGS_NEWLINE)) > 0) {
		printf("RULE: '%s', len:%d\n", buf, len);
		lrtest_exec(buf);
		i++;
	}

	printf("load lrtest file: %s success.(count:%d)\n", path, i);
	free(buf);
	return 0;
}

int main(int argc, char **argv)
{
	char *path;

	if(argc < 2) {
		path = DEFAULT_TEST_PATH;
	} else {
		path = argv[1];
	}

	lrtest_init();

	lrtest_file_exec(path);

	lrtest_release();
	return 0;
}

