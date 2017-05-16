#include <stdio.h>
#include "../lre.h"
#include "../lrc/builtin_lrc.h"

//char code[1024] = "file(path=val1,fuzzypath=\"%s ads ^`!@#$%%%^^&&***(()_+=[]{};:'|<,>.\", arg3=fuzzypath(path=\"testpath/test\"), arg4=test()){(key1==val1 && key2==val2 && submodule(arg3=val3){key3==val3}) || key4 >= val4 || (key5 != val5 + (abc * def * add + (sda / $dad * adf) - $vvv) && submodule(arg3=val4){key3==val3 || keyn != valn})}";
//char code[1024] = "key1==abc";
//char code[1024] = "file(path=\"./core\"){exist==1 && owner==redis && permission==640}";

extern int lrc_yarascan_init(void);

char code[1024] = "process(procname=\"redis-server\", procdir=fuzzypath(path=\"/data/service/*\")) {running==1} && yarascan(rule=\"xx.yar\", target=fuzzypath(basepath=processdir(procname=\"redis-server\", procdir=fuzzypath(path=\"/data/service/*\")), path=\"/../conf/redis-*.conf\")) {matched == 1}";
int main(int argc, char **argv)
{
	int ret;

	lre_init();

	lrc_builtin_init();

	lrc_yarascan_init();

	ret = lre_execute(code);
	if(vaild_lre_results(ret))
		printf("exec result:%d\n", ret);
	else
		printf("exec error.\n");

	lre_release();

	return 0;
}

