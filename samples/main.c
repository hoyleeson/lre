#include <stdio.h>
#include "../lre.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif


char code1[] =
    "process(procname=\"sshd\", procpath=fuzzypath(path=\"/usr/*/sshd\")){running==1} && file(path=fuzzypath(basepath=processdir(procname=\"sshd\", procpath=fuzzypath(path=\"/usr/*/sshd\")), path=\"/etc/ssh/sshd_*\")){exist==1 && owner==root && permission==644}";

/* test file */
char code2[] =
    "file(path=\"/usr/bin/apt\"){exist==1 && owner==root && permission==755}";

/* test process */
char code3[] =
    "process(procname=\"sshd\", procpath=fuzzypath(path=\"/usr/*/sshd\")){running==1 && user != root}";

/* test test fuzzypath */
char code4[] =
    "file(path=fuzzypath(path=\"/home/*ixinhai/wo*ace/linux-*/net/ipv4/netfilter/nf_*.c\")){exist==1}";

/* test test process mutlipath */
char code5[] =
    "process(procname=\"vi\", procpath=\"/bin/bash:/usr/sbin/nscd:/usr/bin/vim.basic:/usr/sbin/smbd\"){running==1 && user != root}";

/* test processdir */
char code6[] =
    "process(procname=\"vi\", procpath=fuzzypath(path=\"/usr/bin/*\")){running==1} && file(path=fuzzypath(basepath=processdir(procname=\"vi\", procpath=fuzzypath(path=\"/usr/*/*\")), path=\"../../etc/v*/vimrc\")){exist==1 && owner==root && permission==644}";

/* test splicepath */
char code7[] =
    "process(procname=\"vi\", procpath=fuzzypath(path=\"/usr/bin/*\")){running==1} && file(path=splicepath(basepath=processdir(procname=\"vi\", procpath=fuzzypath(path=\"/usr/*/*\")), path=\"../../etc/vim/vimrc\")){exist==1 && owner==root && permission==644}";

char code8[] = "file_exist(\"/usr/bin/apt\")";
char code9[] = "network_listening(445)";
char code10[] = "network_listening(tcp, 445)";
char code11[] = "network_listening(udp, 445)";


static char *code[] = {
#if 1
    code1,
    code2,
    code3,
    code4,
    code5,
    code6,
    code7,
    code8,
    code9,
    code10,
    code11,
#endif
};


void logcb(int level, const char *log)
{
    if (level > 3)
        return;

    fputs(log, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    int i;
    struct lre_result *result;
    struct lre_context *ctx;
    lre_initX(".", logcb);
    ctx = lre_context_create();

#if 0
    int ii = 10000;
    while (ii--)
#endif
        for (i = 0; i < ARRAY_SIZE(code); i++) {
            result = lre_execute(ctx, code[i]);
            if (!result) {
                printf("not result.");
                continue;
            }
            if (vaild_lre_results(result->result))
                printf("exec result:%d, detail:%s\n", result->result, result->details);
            else
                printf("exec error, errcode:%d, detail:%s\n", result->errcode, result->details);
            printf("\n");

        }

    lre_context_destroy(ctx);
    lre_release();

    return 0;
}

