#include <stdio.h>
#include "../lre.h"
#include "../lrc/builtin_lrc.h"

static char code1[] = "file(path=\"/usr/bin/apt\"){exist==1 && owner==root && permission==755}";
static char code2[] = "process(procname=\"sshd\", procdir=fuzzypath(path=\"/usr/*/sshd\")){running==1 && user != root}";
static char code3[] = "process(procname=\"sshd\", procdir=fuzzypath(path=\"/usr/*/sshd\")){running==1} && file(path=fuzzypath(basepath=processdir(procname=\"sshd\", procdir=\"/usr/*/sshd\"), path=\"/etc/ssh/sshd_*\")){exist==1 && owner==root && permission==644}";

static char code4[] = "file(path=fuzzypath(path=\"/home/*ixinhai/wo*ace/linux-*/net/ipv4/netfilter/nf_*.c\")){exist==1}";

static char *code[] = {
	code1,
	code2,
	code3,
	code4,
};

int main(int argc, char **argv)
{
	int i;
	int ret;
	struct lre_result result;
	lre_init();

	lrc_builtin_init();

	for(i=0; i<ARRAY_SIZE(code); i++) {
		ret = lre_execute(code[i], &result);
		if(ret) {
			printf("lre execute err\n");
			continue;
		}

		if(vaild_lre_results(result.result))
			printf("exec result:%d, detail:%s\n", result.result, result.details);
		else
			printf("exec error, errcode:%d, detail:%s\n", result.errcode, result.details);
	}

	lre_release();

	return 0;
}

