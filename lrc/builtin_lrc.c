#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "../lre.h"
#include "../log.h"
#include "builtin_lrc.h"

int lrc_builtin_init(void)
{
    int ret;
#define CHECK_ERR(ret, desc) \
	do {if(ret) { \
		loge("Builtin_lrc: Failed to init lrc %s.(%d)", desc, __LINE__); \
		goto err; \
	}} while(0)

    ret = lrc_file_init();
    CHECK_ERR(ret, "file");
    ret = lrc_process_init();
    CHECK_ERR(ret, "process");
    ret = lrc_network_init();
    CHECK_ERR(ret, "network");

    ret = lrc_c_fuzzypath_init();
    CHECK_ERR(ret, "fuzzypath");
    ret = lrc_c_splicepath_init();
    CHECK_ERR(ret, "splicepath");


    logi("Builtin_lrc: Logic rule component initialize success");
    return 0;
err:
    return -EINVAL;
}

void lrc_builtin_release(void)
{
    lrc_file_release();
    lrc_process_release();
    lrc_network_release();

    lrc_c_fuzzypath_release();
    lrc_c_splicepath_release();
}
