#include <stdio.h>
#include "../lre.h"

static char code1[] = "process(procname=\"sshd\", procpath=fuzzypath(path=\"/usr/*/sshd\")){running==1} && file(path=fuzzypath(basepath=processdir(procname=\"sshd\", procpath=fuzzypath(path=\"/usr/*/sshd\")), path=\"/etc/ssh/sshd_*\")){exist==1 && owner==root && permission==644}";

/* test file */
static char code2[] = "file(path=\"/usr/bin/apt\"){exist==1 && owner==root && permission==755}";

/* test process */
static char code3[] = "process(procname=\"sshd\", procpath=fuzzypath(path=\"/usr/*/sshd\")){running==1 && user != root}";

/* test test fuzzypath */
static char code4[] = "file(path=fuzzypath(path=\"/home/*ixinhai/wo*ace/linux-*/net/ipv4/netfilter/nf_*.c\")){exist==1}";

/* test test process mutlipath */
static char code5[] = "process(procname=\"vi\", procpath=\"/bin/bash:/usr/sbin/nscd:/usr/bin/vim.basic:/usr/sbin/smbd\"){running==1 && user != root}";

/* test processdir */
static char code6[] = "process(procname=\"vi\", procpath=fuzzypath(path=\"/usr/bin/*\")){running==1} && file(path=fuzzypath(basepath=processdir(procname=\"vi\", procpath=fuzzypath(path=\"/usr/*/*\")), path=\"../../etc/*/vimrc\")){exist==1 && owner==root && permission==644}";

/* test splicepath */
static char code7[] = "process(procname=\"vi\", procpath=fuzzypath(path=\"/usr/bin/*\")){running==1} && file(path=splicepath(basepath=processdir(procname=\"vi\", procpath=fuzzypath(path=\"/usr/*/*\")), path=\"../../etc/vim/vimrc\")){exist==1 && owner==root && permission==644}";

static char *code[] = {
	code1,
	code2,
	code3,
	code4,
	code5,
	code6,
	code7,
};

int main(int argc, char **argv)
{
	int i;
	int ret;
	struct lre_result result;
	lre_init();

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
		printf("\n");
	}

	lre_release();

	return 0;
}

