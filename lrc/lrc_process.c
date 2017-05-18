#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>

#include "../utils.h"
#include "../lre.h"


struct process_info {
	int pid;
	int ppid;
	char *name;
	char *user;
	char *cmdline;
	char *bindir;

	struct process_info *next;
};

struct process_info *processlist = NULL;

void process_info_add(struct process_info *psinfo)
{
	psinfo->next = processlist;
	processlist = psinfo;
}

static char *getrealdir(char *path)
{
	char *p;
	char buf[1024];
	int len;

	len = readlink(path, buf, 1023);
	if(len < 0)
		return NULL;

	buf[len] = 0;
	p = strrchr(buf, '/');
	if(!p) {
		return NULL;
	} else if(p == buf) {
		*(++p) = '\0';
	} else 
		*p = '\0';

	memset(path, 0, 1024);
	strncpy(path, buf, 1024);
	return path;
}

static char *nexttoksep(char **strp, char *sep)
{
    char *p = strsep(strp,sep);
    return (p == 0) ? "" : p;
}

static char *nexttok(char **strp)
{
    return nexttoksep(strp, " ");
}

static struct process_info *get_process_info(int pid)
{
	char user[32];
	char buf[1024];
	struct stat stats;
	int fd, r;
	char *ptr, *name;
	int ppid;
	struct passwd *pw;
	struct process_info *psinfo;

	psinfo = xzalloc(sizeof(*psinfo));
	memset(psinfo, 0, sizeof(*psinfo));

	psinfo->pid = pid;

	sprintf(buf, "/proc/%d", pid);
	stat(buf, &stats);

	pw = getpwuid(stats.st_uid);
	if(pw == 0) {
		sprintf(user,"%d",(int)stats.st_uid);
	} else {
		strcpy(user,pw->pw_name);
	}

	psinfo->user = strdup(user);

	sprintf(buf, "/proc/%d/exe", pid);
	ptr = getrealdir(buf);
	if(ptr)
		psinfo->bindir = strdup(ptr);
	else
		psinfo->bindir = NULL;

	sprintf(buf, "/proc/%d/stat", pid);
	fd = open(buf, O_RDONLY);
	if(fd <= 0) {
		free(psinfo);
		return NULL;
	}
	r = read(fd, buf, 1023);
	close(fd);
	if(r < 0) {
		free(psinfo);
		return NULL;
	}

	buf[r] = 0;

	ptr = buf;
	nexttok(&ptr); // skip pid
	ptr++;          // skip "("

	name = ptr;
	ptr = strrchr(ptr, ')'); // Skip to *last* occurence of ')',
	*ptr++ = '\0';           // and null-terminate name.

	psinfo->name = strdup(name);

	ptr++;          // skip " "
	nexttok(&ptr);
	ppid = atoi(nexttok(&ptr));

	psinfo->ppid = ppid;

	sprintf(buf, "/proc/%d/cmdline", pid);
	fd = open(buf, O_RDONLY);
	if(fd <= 0) {
		r = 0;
	} else {
		r = read(fd, buf, 1023);
		close(fd);
		if(r < 0) r = 0;
	}
	buf[r] = 0;

	psinfo->cmdline = strdup(buf);

	return psinfo;
}

int scan_processes(void)
{
	DIR *d;
	struct dirent *de;
	struct process_info *psinfo;

	d = opendir("/proc");
	if(d == 0)
		return -1;

	while((de = readdir(d)) != 0){
		if(isdigit(de->d_name[0])){
			int pid = atoi(de->d_name);
			psinfo = get_process_info(pid);
			if(!psinfo)
				continue;

			process_info_add(psinfo);
		}
	}
	closedir(d);

	return 0;
}

/****************************************************/

#define PROCESS_COUNT_MAX 	(1024)
struct lrc_process {
	struct lrc_object base;
	char *procname;
	char *procdir;

	struct process_info *psinfo[PROCESS_COUNT_MAX];
	int count;

	int state;
#define STATE_EXEC_FAILED 	(0)
#define STATE_EXEC_SUCCESS 	(1)

};

void fill_spec_process(struct lrc_process *process)
{
	int ret;
	struct process_info *psinfo = processlist;
	process->count = 0;

	while(psinfo) {
		ret = 1;

		if(process->procname)
			ret &= !strcmp(process->procname, psinfo->name);
		if(process->procdir)
			ret &= !strcmp(process->procname, psinfo->name);
		if(ret)
			process->psinfo[process->count++] = psinfo;

		psinfo = psinfo->next;
	}
}

int process_execute(lrc_obj_t *handle)
{
	struct lrc_process *process;

	process = (struct lrc_process *)handle;
	if(!process->procname && !process->procdir) {
		process->state = STATE_EXEC_FAILED;
		return -EINVAL;
	}

	fill_spec_process(process);

	process->state = STATE_EXEC_SUCCESS;
	return 0;
}


static lrc_obj_t *func_process_handler(void)
{
	struct lrc_process *process;

	process = malloc(sizeof(*process));
	if(!process) {
		return (lrc_obj_t *)0;
	}
	process->base.execfunc = process_execute;

	return (lrc_obj_t *)process;
}

static int arg_procname_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_process *process;

	process = (struct lrc_process *)handle;

	if(!lreval || lreval->type != LRE_ARG_TYPE_STRING || !lreval->valstring)
		return LRE_RET_ERROR;

	process->procname = strdup(lreval->valstring);
	printf("process name:%s\n", process->procname);
	return LRE_RET_OK;
}

static int arg_procdir_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_process *process;

	process = (struct lrc_process *)handle;

	if(!lreval || lreval->type != LRE_ARG_TYPE_STRING || !lreval->valstring)
		return LRE_RET_ERROR;

	process->procdir = strdup(lreval->valstring);
	printf("process directory:%s\n", process->procname);
	return LRE_RET_OK;
}

static int expr_running_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	struct lrc_process *process;

	process = (struct lrc_process *)handle;

	if(process->state != STATE_EXEC_SUCCESS)
		return LRE_RET_ERROR;

	if(!lreval || lreval->type != LRE_ARG_TYPE_INT)
		return LRE_RET_ERROR;

	/*FIXME: verify val first */
	return lre_compare_int(!!process->count, lreval->valint, opt);
}

static int expr_user_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	int i;
	int found = 0;
	char user[32];
	struct lrc_process *process;
	struct process_info *psinfo;

	process = (struct lrc_process *)handle;
	if(process->state != STATE_EXEC_SUCCESS)
		return LRE_RET_ERROR;

	if(!lreval || (lreval->type != LRE_ARG_TYPE_INT &&
		   	lreval->type != LRE_ARG_TYPE_STRING))
		return LRE_RET_ERROR;

	if(lreval->type == LRE_ARG_TYPE_INT) {
		sprintf(user, "%d", lreval->valint);
	} else {
		sprintf(user, "%s", lreval->valstring);
	}


	for(i=0; i<process->count; i++) {
		psinfo = process->psinfo[i];

		if(!strcmp(psinfo->user, user)) {
			found = 1;
			break;
		}
	}

	return lre_compare_int(found, 1, opt);
}

static int expr_cmdline_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	struct lrc_process *process;

	process = (struct lrc_process *)handle;

	if(!lreval || lreval->type != LRE_ARG_TYPE_INT)
		return LRE_RET_ERROR;

	/* FIXME: unsupport now */
	return LRE_RESULT_TRUE;
}

static struct lrc_stub_arg process_args[] = {
	{
		.keyword  	 = "procname",
		.description = "Type: string. Specify process name. example: procname=\"redis\"",
		.handler 	 = arg_procname_handler,
	}, {
		.keyword  	 = "procdir",
		.description = "Type: string. Specify process binary directory. example: procdir=\"/usr/local/\"",
		.handler 	 = arg_procdir_handler,
	}
};

static struct lrc_stub_expr process_exprs[] = {
	{
		.keyword  	 = "running",
		.description = "Check process running or not. val: 1 or 0, 1: running, 0: not running, example: running==1",
		.handler 	 = expr_running_handler,
	}, {
		.keyword  	 = "user",
		.description = "Check user who starts the process. example: user==root",
		.handler 	 = expr_user_handler,
	}, {
		.keyword  	 = "cmdline",
		.description = "Check Process cmdline. example: cmdline==\"-i\"",
		.handler 	 = expr_cmdline_handler,
	}
};

static struct lrc_stub_func lrc_funcs[] = {
	{
		.keyword 	 = "process",
		.description = "Check process user,cmdline,running or not, etc.",
		.handler 	 = func_process_handler,

		.args 	   = process_args,
		.argcount  = ARRAY_SIZE(process_args),
		.exprs     = process_exprs,
		.exprcount = ARRAY_SIZE(process_exprs),
	}
};

struct lrc_module lrc_process_mod = {
	.name = "lrc_process",
	.funcs = lrc_funcs,
	.funccount = ARRAY_SIZE(lrc_funcs),
};


int lrc_process_init(void)
{
	int ret;

	ret = lrc_module_register(&lrc_process_mod);
	if(ret)
		printf("Failed to register '%s' modules \n", lrc_process_mod.name);
	return ret;
}

