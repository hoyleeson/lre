#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "lre.h"

struct lre_context *ctx;

static int lrexe_init(void)
{
    int ret;
    ret = lre_init();
    if (ret) {
        printf("Failed to initialize lre.");
        return -EINVAL;
    }
    ctx = lre_context_create();
    if (!ctx) {
        printf("failed to create lre context.");
        return -EINVAL;
    }
    return 0;
}

static void lrexe_release(void)
{
    lre_context_destroy(ctx);
    lre_release();
}

static void lrexe_execute(const char *code)
{
    struct lre_result *result;

    result = lre_execute(ctx, code);
    if (!result) {
        printf("not result.");
        return;
    }
    if (vaild_lre_results(result->result))
        printf("Logic rule exec result:%d, detail:%s\n", result->result,
               result->details);
    else
        printf("Logic rule exec error, errcode:%d, detail:%s\n", result->errcode,
               result->details);
    printf("\n");
}

int main(int argc, char **argv)
{
    int ret;
    FILE *fp;
    char *fpath;
    char *code;
    long size;

    if (argc < 2) {
        printf("not input rule files. example: \n\tlrexe [FILE]\n");
        exit(0);
    }

    fpath = argv[1];
    fp = fopen(fpath, "r");
    if (!fp) {
        printf("Cannot open rule file %s\n", fpath);
        exit(0);
    }

    fseek(fp, SEEK_SET, SEEK_END);
    size = ftell(fp);
    printf("%s size:%ld\n", fpath, size);

    rewind(fp);
    code = (char *)malloc(size + 1);
    ret = fread(code, 1, size, fp);
    if (ret != size) {
        printf("Read %s failed.\n", fpath);
    }
    code[size] = '\0';
    fclose(fp);

    ret = lrexe_init();
    if (ret)
        exit(0);

    printf("Execute rule file: %s\n", fpath);
    lrexe_execute(code);
    lrexe_release();
    free(code);
    return 0;
}

