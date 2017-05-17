#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

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

int file_execute(lrc_obj_t *handle)
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
		file->permission = st.st_mode;
	} else if(ret == ENOENT) {
		file->exist = 0;
	}

	file->state = STATE_EXEC_SUCCESS;
	return 0;
}

static lrc_obj_t *func_file_handler(void)
{
	struct lrc_file *file;

	file = malloc(sizeof(*file));
	if(!file) {
		return (lrc_obj_t *)0;
	}
	file->base.execfunc = file_execute;

	return (lrc_obj_t *)file;
}

static int arg_path_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_file *file;

	file = (struct lrc_file *)handle;

	if(!lreval || lreval->type != LRE_ARG_TYPE_STRING || !lreval->valstring)
		return LRE_RET_ERROR;

	file->path = strdup(lreval->valstring);
	printf("path:%s\n", file->path);
	return LRE_RET_OK;
}

static int expr_exist_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	struct lrc_file *file;

	file = (struct lrc_file *)handle;
	if(file->state == STATE_EXEC_FAILED)
		return LRE_RET_ERROR;

	if(!lreval || lreval->type != LRE_ARG_TYPE_INT)
		return LRE_RET_ERROR;

	return (lreval->valint == file->exist);
}

static int expr_owner_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	struct lrc_file *file;
	char user[32];
	struct passwd *pw;

	file = (struct lrc_file *)handle;
	if(file->state == STATE_EXEC_FAILED)
		return LRE_RET_ERROR;

	if(!lreval || (lreval->type != LRE_ARG_TYPE_INT &&
		   	lreval->type != LRE_ARG_TYPE_STRING))
		return LRE_RET_ERROR;

	pw = getpwuid(file->owner);
	if(pw == 0) {
		sprintf(user, "%d", (int)file->owner);
	} else {
		strncpy(user, pw->pw_name, 32);
	}

	if(lreval->type == LRE_ARG_TYPE_INT) {
		char u[32];	
		sprintf(u, "%d", lreval->valint);
		return strcmp(u, user);
	}

	return strcmp(lreval->valstring, user);
}

static int expr_permission_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	struct lrc_file *file;

	file = (struct lrc_file *)handle;
	if(file->state == STATE_EXEC_FAILED)
		return LRE_RET_ERROR;

	if(!lreval || lreval->type != LRE_ARG_TYPE_STRING)
		return LRE_RET_ERROR;

	/*FIXME:*/
	//hex2int();

	return LRE_RESULT_TRUE;
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
		.handler 	 = func_file_handler,

		.args 	   = file_args,
		.argcount  = ARRAY_SIZE(file_args),
		.exprs     = file_exprs,
		.exprcount = ARRAY_SIZE(file_exprs),
	}
};

struct lrc_module lrc_file_mod = {
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
