#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

#include "../log.h"
#include "../utils.h"
#include "../lre.h"

struct lrc_file {
	struct lrc_object base;
	char *path;

	int exist;
	uid_t owner;
	gid_t group;
	mode_t permission;

	int state;
#define STATE_EXEC_FAILED 	(0)
#define STATE_EXEC_SUCCESS 	(1)
};

#define FILE_PERMISSION_MASK 	(0xfff)

static int file_execute(lrc_obj_t *handle)
{
	int ret;
	struct stat st;
	struct lrc_file *file;

	file = (struct lrc_file *)handle;
	if(!file->path) {
		file->state = STATE_EXEC_FAILED;
		return -EINVAL;
	}

	ret = stat(file->path, &st);
	if(ret == 0){
		file->exist = 1;
		file->owner = st.st_uid;
		file->group = st.st_gid;
		file->permission = st.st_mode & FILE_PERMISSION_MASK;
	} else {
		if(errno == ENOENT)
			file->exist = 0;
		else
			file->exist = -1;
		file->owner = -1;
		file->group = -1;
		file->permission = -1;
	}
	file->state = STATE_EXEC_SUCCESS;

	{
		char *p;
		int omit = 0;
		char buf[64] = {0};
		int len = strlen(file->path);
		if(len > 32) {
			omit = 1;
			p = file->path + len - 32;
		} else
			p = file->path;	

		snprintf(buf, 64, "file '%s%s'", omit ? "..." : "", p);
		file->base.output(handle, buf);
	}
	return 0;
}

static lrc_obj_t *file_constructor(void)
{
	struct lrc_file *file;

	file = malloc(sizeof(*file));
	if(!file) {
		return (lrc_obj_t *)0;
	}
	file->path = NULL;
	return (lrc_obj_t *)file;
}

static void file_destructor(lrc_obj_t *handle)
{
	struct lrc_file *file;

	file = (struct lrc_file *)handle;
	if(file->path)
		free(file->path);
	free(file);
}

static int arg_path_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	const char *str;
	struct lrc_file *file;

	file = (struct lrc_file *)handle;
	if(!lreval || !lre_value_is_string(lreval)) {
		loge("lrc 'file' err: path must be string");
		return LRE_RET_ERROR;
	}
	str = lre_value_get_string(lreval);
	if(!str)
		return LRE_RET_ERROR;

	file->path = strdup(str);
	assert_ptr(file->path);
	logd("lrc 'file': arg handler. path:%s", file->path);
	return LRE_RET_OK;
}

static int expr_exist_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	int ret;
	struct lrc_file *file;

	file = (struct lrc_file *)handle;
	if(file->state == STATE_EXEC_FAILED)
		return LRE_RET_ERROR;

	if(!lreval || !lre_value_is_int(lreval)) {
		loge("lrc 'file' err: exist expr acceptable val: 1 or 0");
		return LRE_RET_ERROR;
	}

	/*FIXME: verify val first */
	ret = lre_compare_int(file->exist, lre_value_get_int(lreval), opt);
	if(vaild_lre_results(ret)) {
		char buf[64] = {0};
		snprintf(buf, 64, "%sexist.", file->exist ? "":"not ");
		file->base.output(handle, buf);
	}
	return ret;
}

static int expr_owner_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	const char *str;
	struct lrc_file *file;
	char user[32];
	int ret;
	struct passwd *pw;

	file = (struct lrc_file *)handle;
	if(file->state == STATE_EXEC_FAILED)
		return LRE_RET_ERROR;

	if(!lreval || (!lre_value_is_int(lreval) &&
				!lre_value_is_string(lreval))) {
		loge("lrc 'file' err: unexpect owner val type.");
		return LRE_RET_ERROR;
	}

	pw = getpwuid(file->owner);
	if(pw == 0) {
		sprintf(user, "%d", (int)file->owner);
	} else {
		strncpy(user, pw->pw_name, 32);
	}

	if(lre_value_is_int(lreval)) {
		char u[32];	
		sprintf(u, "%d", lre_value_get_int(lreval));
		ret = lre_compare_string(user, u, opt);
		goto out;
	}
	str = lre_value_get_string(lreval);
	if(!str)
		return LRE_RET_ERROR;

	ret = lre_compare_string(user, (char *)str, opt);

out:
	if(vaild_lre_results(ret)) {
		char buf[64] = {0};
		snprintf(buf, 64, "expr 'owner' %smatched.", ret ? "" : "not ");
		file->base.output(handle, buf);
	}
	return ret;
}

static int int2perm(int val)
{
	int i = 0;
	int v = 0;

	while(val) {
		v |= (val % 10) << (i*3);
		val /= 10;
		i++;
	}
	return v;
}

static int perm2int(int val)  
{                             
	int i = 1;                
	int v = 0;                
	while(val) {              
		v = v + (val & 7) * i;
		val >>= 3;            
		i *= 10;              
	}                         
	return v;                 
}                             

static int file_perm_compare(unsigned long a, unsigned long b)
{
	int fperm = (int)a;
	int bperm = (int)b;

	if(fperm == bperm)
		return 0;

	return (fperm & (~bperm)) ? 1 : -1;
}

static int expr_permission_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	int ret;
	unsigned int val;
	struct lrc_file *file;

	file = (struct lrc_file *)handle;
	if(file->state == STATE_EXEC_FAILED)
		return LRE_RET_ERROR;

	if(!lreval || !lre_value_is_int(lreval)) {
		loge("lrc 'file' err: exist expr acceptable val: 1 or 0");
		return LRE_RET_ERROR;
	}

	val = int2perm(lre_value_get_int(lreval));
	ret = lre_compare(file->permission, val, opt, file_perm_compare);
	if(vaild_lre_results(ret)) {
		int len;
		char buf[64] = {0};
		len = snprintf(buf, 64, "'permission' %smatched.", ret ? "" : "not ");
		if(!ret) {
			len += snprintf(buf + len, 64 - len, " perm: %d", perm2int(file->permission));
		}
		file->base.output(handle, buf);
	}
	return ret;
}

static struct lrc_stub_arg file_args[] = {
	{
		.keyword  	 = "path",
		.description = "Type: string. Specify the path to a file",
		.handler 	 = arg_path_handler,
	}
};

static struct lrc_stub_expr file_exprs[] = {
	{
		.keyword  	 = "exist",
		.description = "File exist or not. val: 1 or 0, 1: exist, 0: not exist, example: exist==1",
		.handler 	 = expr_exist_handler,
	}, {
		.keyword  	 = "owner",
		.description = "File owner. example: owner==\"root\"",
		.handler 	 = expr_owner_handler,
	}, {
		.keyword  	 = "permission",
		.description = "File visit permission. example: permission==755, permission=644",
		.handler 	 = expr_permission_handler,
	}
};

static struct lrc_stub_func lrc_funcs[] = {
	{
		.keyword 	 = "file",
		.description = "Check file permissions,owner,is exist or not, etc.",
		.constructor = file_constructor,
		.exec  		 = file_execute,
		.destructor  = file_destructor,

		.args 	   = file_args,
		.argcount  = ARRAY_SIZE(file_args),
		.exprs     = file_exprs,
		.exprcount = ARRAY_SIZE(file_exprs),
	}
};

static struct lrc_module lrc_file_mod = {
	.name = "lrc_file",
	.funcs = lrc_funcs,
	.funccount = ARRAY_SIZE(lrc_funcs),
};

int lrc_file_init(void)
{
	int ret;

	ret = lrc_module_register(&lrc_file_mod);
	if(ret)
		loge("Failed to register '%s' modules", lrc_file_mod.name);
	return ret;
}


void lrc_file_release(void)
{
	lrc_module_unregister(&lrc_file_mod);
}
