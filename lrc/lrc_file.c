#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

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
		printf("lrc 'file' err: path must be string\n");
		return LRE_RET_ERROR;
	}
	str = lre_value_get_string(lreval);
	if(!str)
		return LRE_RET_ERROR;

	file->path = strdup(str);
	printf("path:%s\n", file->path);
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
		printf("lrc 'file' err: exist expr acceptable val: 1 or 0\n");
		return LRE_RET_ERROR;
	}

	/*FIXME: verify val first */
	ret = lre_compare_int(file->exist, lre_value_get_int(lreval), opt);
	if(!ret) {
		char buf[128] = {0};
		snprintf(buf, 128, "file '%s'%s exist", file->path, file->exist ? "":" not");
		lre_push_exec_detail(handle, buf);
	}
	return ret;
}

static int expr_owner_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	const char *str;
	struct lrc_file *file;
	char user[32];
	struct passwd *pw;

	file = (struct lrc_file *)handle;
	if(file->state == STATE_EXEC_FAILED)
		return LRE_RET_ERROR;

	if(!lreval || (!lre_value_is_int(lreval) &&
				!lre_value_is_string(lreval))) {
		printf("lrc 'file' err: unexpect owner val type.\n");
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
		return lre_compare_string(user, u, opt);
	}
	str = lre_value_get_string(lreval);
	if(!str)
		return LRE_RET_ERROR;

	return lre_compare_string(user, (char *)str, opt);
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

static int expr_permission_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	unsigned int val;
	struct lrc_file *file;

	file = (struct lrc_file *)handle;
	if(file->state == STATE_EXEC_FAILED)
		return LRE_RET_ERROR;

	if(!lreval || !lre_value_is_int(lreval)) {
		printf("lrc 'file' err: exist expr acceptable val: 1 or 0\n");
		return LRE_RET_ERROR;
	}

	val = int2perm(lre_value_get_int(lreval));
	return lre_compare_int(file->permission, val, opt);
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
		.description = "File visit permission. example: permission==0x0755, permission=0x644",
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
		printf("Failed to register '%s' modules \n", lrc_file_mod.name);
	return ret;
}


void lrc_file_release(void)
{
	lrc_module_unregister(&lrc_file_mod);
}
