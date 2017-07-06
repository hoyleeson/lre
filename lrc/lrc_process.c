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
#include "../log.h"
#include "../lre.h"


#define PATH_ARR_MAX  	(PATH_MAX * 10)
#define PROCESS_ARG_MAX     (1024)
#define CMDLINE_LEN_MAX     (2048)

struct process_info {
	int pid;
	int ppid;
	char *name;
	char *user;
	char *binpath;
	char *cmdline;
    int cmdlen;

	struct process_info *next;
};

struct process_info *processlist = NULL;

static void process_info_add(struct process_info *psinfo)
{
	psinfo->next = processlist;
	processlist = psinfo;
}

char *getrealdir(char *path)
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

static char *path2dir(char *path, char *dir)
{
	char *p;

	strncpy(dir, path, PATH_MAX);
	p = strrchr(dir, '/');
	if(!p) {
		return dir;
	} else if(p == dir) {
		*(++p) = '\0';	
	} else {
		*p = '\0';
	}

	return dir;
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
	char user[32] = {0};
	char buf[CMDLINE_LEN_MAX] = {0};
	char path[PATH_MAX] = {0};
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
	assert_ptr(psinfo->user);

	sprintf(buf, "/proc/%d/exe", pid);
	r = readlink(buf, path, PATH_MAX - 1);
	if(r <= 0)
		psinfo->binpath = NULL;
	else {
		psinfo->binpath = strdup(path);
		assert_ptr(psinfo->binpath);
	}

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
	assert_ptr(psinfo->name);

	ptr++;          // skip " "
	nexttok(&ptr);
	ppid = atoi(nexttok(&ptr));

	psinfo->ppid = ppid;

	sprintf(buf, "/proc/%d/cmdline", pid);
	fd = open(buf, O_RDONLY);
	if(fd <= 0) {
		r = 0;
	} else {
		r = read(fd, buf, CMDLINE_LEN_MAX);
		close(fd);
		if(r < 0) r = 0;
	}
	buf[r] = 0;

	// psinfo->cmdline = strdup(buf);
	psinfo->cmdline = xzalloc(r);
	assert_ptr(psinfo->cmdline);
    memcpy(psinfo->cmdline, buf, r);
    psinfo->cmdlen = r;

	return psinfo;
}

static void process_info_free(struct process_info *psinfo)
{
	free(psinfo->name);
	free(psinfo->user);
	free(psinfo->cmdline);
	if(psinfo->binpath)
		free(psinfo->binpath);
	free(psinfo);
}

static int processes_info_load(void)
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

static void processes_info_release(void)
{
	struct process_info *psinfo, *p;

	psinfo = processlist;
	while(psinfo) {
		p = psinfo->next;
		process_info_free(psinfo);
		psinfo = p;
	}
}

/****************************************************/

#define PROCESS_COUNT_MAX 	(1024)
struct lrc_process {
	struct lrc_object base;
	char *procname;
	char *procpath;
	char *excludepath;

    /* just use for cmd_extract calls. */
    int argindex;
	char *argkey;

	struct process_info *psinfo[PROCESS_COUNT_MAX];
	int count;

	int state;
#define STATE_EXEC_FAILED 	(0)
#define STATE_EXEC_SUCCESS 	(1)

    int flags;
#define FLAGS_CMDLINE_EXTRACT   (1<<0)
};

static void fill_spec_process(struct lrc_process *process)
{
	int ret;
	struct process_info *psinfo = processlist;
	process->count = 0;

	while(psinfo) {
		ret = 1;

		if(process->procname) {
			if(!psinfo->name) {
				psinfo = psinfo->next;
				continue;
			}
			ret &= !strcmp(process->procname, psinfo->name);
		}
		if(process->procpath) {
			int r;
			char *ptr;

			if(!psinfo->binpath) {
				psinfo = psinfo->next;
				continue;
			}
			r = !strncmp(process->procpath, psinfo->binpath, strlen(psinfo->binpath));

			ptr = process->procpath;
			while((ptr = strchr(ptr, MULTIPATH_SPLIT_CH)) != NULL) {
				ptr++;
				r |= !strncmp(ptr, psinfo->binpath, strlen(psinfo->binpath));
			}
			ret &= r;
		}

		if(process->excludepath) {
			int r;
			char *ptr;

			if(!psinfo->binpath) {
				r = 1;
			} else {
				r = !!strncmp(process->excludepath, psinfo->binpath, strlen(psinfo->binpath));

				ptr = process->excludepath;
				while((ptr = strchr(ptr, MULTIPATH_SPLIT_CH)) != NULL) {
					ptr++;
					r &= !!strncmp(ptr, psinfo->binpath, strlen(psinfo->binpath));
				}
			}
			ret &= r;
		}

		if(ret) {
			process->psinfo[process->count++] = psinfo;
			logd("Get the process:%s, binpath:%s", 
					psinfo->name, psinfo->binpath ? psinfo->binpath : "(unknown)");
		}

		psinfo = psinfo->next;
	}
}

static int process_execute(lrc_obj_t *handle)
{
	struct lrc_process *process;
	if(processlist == NULL)
		processes_info_load();

	process = (struct lrc_process *)handle;
	if(!process->procname && !process->procpath) {
		process->state = STATE_EXEC_FAILED;
		loge("Failed execute 'process' func. Not specified process name or path.");
		return -EINVAL;
	}

	fill_spec_process(process);

	process->state = STATE_EXEC_SUCCESS;

	{
		int omit = 0;
		int len;
		char buf[DETAILS_UNIT_MAX] = {0};

		len = xsnprintf(buf, DETAILS_UNIT_MAX, "process");
		if(process->procname)
			len += xsnprintf(buf + len, DETAILS_UNIT_MAX - len, " '%s'", process->procname);
		if(process->procpath) {
			char *p;
			int l = strlen(process->procpath);
			if(l > 32) {
				omit = 1;
				p = process->procpath + l - 32;
			} else
				p = process->procpath;
			len += xsnprintf(buf + len, DETAILS_UNIT_MAX - len, " '%s%s'", omit ? "..." : "", p);
		}
		process->base.output(handle, buf);
	}
	return 0;
}


static lrc_obj_t *process_constructor(void)
{
	struct lrc_process *process;

	process = malloc(sizeof(*process));
	if(!process) {
		return (lrc_obj_t *)0;
	}
	process->procname = NULL;
	process->procpath = NULL;
	process->excludepath = NULL;
	process->count = 0;
	process->flags = 0;

	return (lrc_obj_t *)process;
}

static void process_destructor(lrc_obj_t *handle)
{
	struct lrc_process *process;

	process = (struct lrc_process *)handle;

	if(process->procname)
		free(process->procname);
	if(process->procpath)
		free(process->procpath);
	if(process->excludepath)
		free(process->excludepath);

	free(process);
}

static int arg_procname_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	const char *str;
	struct lrc_process *process;

	process = (struct lrc_process *)handle;
	if(!lreval || !lre_value_is_string(lreval)) {
		loge("lrc 'process' err: procname must be string");
		return LRE_RET_ERROR;
	}
	str = lre_value_get_string(lreval);
	if(!str)
		return LRE_RET_ERROR;

	process->procname = strdup(str);
	assert_ptr(process->procname);
	logd("lrc 'process' arg: procname: %s", process->procname);
	return LRE_RET_OK;
}

static int arg_procpath_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	const char *str;
	struct lrc_process *process;

	process = (struct lrc_process *)handle;
	if(!lreval || !lre_value_is_string(lreval)) {
		loge("lrc 'process' err: procpath must be string");
		return LRE_RET_ERROR;
	}
	str = lre_value_get_string(lreval);
	if(!str)
		return LRE_RET_ERROR;

	process->procpath = strdup(str);
	assert_ptr(process->procpath);
	logd("lrc 'process' arg: procpath: %s", process->procpath);
	return LRE_RET_OK;
}

static int arg_excludepath_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	const char *str;
	struct lrc_process *process;

	process = (struct lrc_process *)handle;
	if(!lreval || !lre_value_is_string(lreval)) {
		loge("lrc 'process' err: procpath must be string");
		return LRE_RET_ERROR;
	}
	str = lre_value_get_string(lreval);
	if(!str)
		return LRE_RET_ERROR;

	process->excludepath = strdup(str);
	assert_ptr(process->excludepath);
	logd("lrc 'process' arg: excludepath: %s", process->excludepath);
	return LRE_RET_OK;
}

static int arg_argkey_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	const char *str;
	struct lrc_process *process;

	process = (struct lrc_process *)handle;

    if((process->flags & FLAGS_CMDLINE_EXTRACT) == 0) {
		loge("lrc 'process' err: invaild arg 'argkey'");
		return LRE_RET_ERROR;
    }

	if(!lreval || !lre_value_is_string(lreval)) {
		loge("lrc 'process' err: argkey must be string");
		return LRE_RET_ERROR;
	}
	str = lre_value_get_string(lreval);
	if(!str)
		return LRE_RET_ERROR;

	process->argkey = strdup(str);
	assert_ptr(process->argkey);
	logd("lrc 'process' arg: argkey: %s", process->argkey);
	return LRE_RET_OK;
}

static int arg_argindex_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_process *process;

	process = (struct lrc_process *)handle;

    if((process->flags & FLAGS_CMDLINE_EXTRACT) == 0) {
		loge("lrc 'process' err: invaild arg 'argindex'");
		return LRE_RET_ERROR;
    }

	if(!lreval || !lre_value_is_int(lreval)) {
		loge("lrc 'process' err: argindex must be integer");
		return LRE_RET_ERROR;
	}

    process->argindex = lre_value_get_int(lreval);
	logd("lrc 'process' arg: argindex: %d", process->argindex);
	return LRE_RET_OK;
}


static int expr_running_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	int ret;
	struct lrc_process *process;

	process = (struct lrc_process *)handle;

	if(process->state != STATE_EXEC_SUCCESS)
		return LRE_RET_ERROR;

	if(!lreval || !lre_value_is_int(lreval)) {
		loge("lrc 'process': running expr err, val must be '1' or '0'");
		return LRE_RET_ERROR;
	}

	/*FIXME: verify val first */
	ret = lre_compare_int(!!process->count, lre_value_get_int(lreval), opt);
	if(vaild_lre_results(ret)) {
		int i;
		char buf[DETAILS_UNIT_MAX] = {0};
		int len;
		len = xsnprintf(buf, DETAILS_UNIT_MAX, "%srunning.", process->count ? "":"not ");
		if(process->excludepath && process->count > 0) {
			len += xsnprintf(buf + len, DETAILS_UNIT_MAX - len, " path:");
			for(i=0; i<process->count; i++) {
				struct process_info *psinfo;
				psinfo = process->psinfo[i];
				len += xsnprintf(buf + len, DETAILS_UNIT_MAX - len, "%s;", 
						psinfo->binpath ? psinfo->binpath : "(unknown)");
			}
		}
		process->base.output(handle, buf);
	}
	return ret;
}

static int expr_user_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	int i;
	int ret;
	int found = 0;
	char user[32];
	struct lrc_process *process;
	struct process_info *psinfo;

	process = (struct lrc_process *)handle;
	if(process->state != STATE_EXEC_SUCCESS)
		return LRE_RET_ERROR;

	if(!lreval || (!lre_value_is_int(lreval) &&
				!lre_value_is_string(lreval))) {
		loge("lrc 'process' err: procpath must be string or int");
		return LRE_RET_ERROR;
	}

	if(lre_value_is_int(lreval)) {
		sprintf(user, "%d", lre_value_is_int(lreval));
	} else {
		const char *str = lre_value_get_string(lreval);
		sprintf(user, "%s", str);
	}

	for(i=0; i<process->count; i++) {
		psinfo = process->psinfo[i];

		if(!strcmp(psinfo->user, user)) {
			found = 1;
			break;
		}
	}

	ret = lre_compare_int(found, 1, opt);
	if(vaild_lre_results(ret)) {
		char buf[DETAILS_UNIT_MAX] = {0};
		xsnprintf(buf, DETAILS_UNIT_MAX, "expr 'user' %smatched.", ret ? "" : "not ");
		process->base.output(handle, buf);
	}
	return ret;
}


/****************************************************/

static int processdir_execute(lrc_obj_t *handle, struct lre_value *val)
{
	int ret;
	struct process_info *psinfo;
	struct lrc_process *process;
	char *patharr[PROCESS_COUNT_MAX];
	int count = 0;
	char *outpath;
	int i;
	int len = 0;

	if(processlist == NULL)
		processes_info_load();

	process = (struct lrc_process *)handle;
	if(!process->procname && !process->procpath) {
		process->state = STATE_EXEC_FAILED;
		loge("Failed execute 'processdir' call. Not specified process name or path.");
		return -EINVAL;
	}

	psinfo = processlist;
	while(psinfo) {
		ret = 1;

		if(process->procname) {
			if(!psinfo->name) {
				psinfo = psinfo->next;
				continue;
			}
			ret &= !strcmp(process->procname, psinfo->name);
		}
		if(process->procpath) {
			int r;
			char *ptr;

			if(!psinfo->binpath) {
				psinfo = psinfo->next;
				continue;
			}
			r = !strncmp(process->procpath, psinfo->binpath, strlen(psinfo->binpath));

			ptr = process->procpath;
			while((ptr = strchr(ptr, MULTIPATH_SPLIT_CH)) != NULL) {
				ptr++;
				r |= !strncmp(ptr, psinfo->binpath, strlen(psinfo->binpath));
			}
			ret &= r;
		}

		if(ret && psinfo->binpath) {
			int repeat = 0;
			/* filter repeat path */
			for(i=0; i<count; i++) {
				if(!strcmp(patharr[i], psinfo->binpath))
					repeat = 1;
			}
			if(!repeat) {
				if(count >= PROCESS_COUNT_MAX) {
					logw("'processdir': path count %d > %d", count, PROCESS_COUNT_MAX);
					break;
				}
				patharr[count++] = psinfo->binpath;
				logd("'processdir' call matched process:%s, binpath:%s", 
					psinfo->name, psinfo->binpath);
			}
		}

		psinfo = psinfo->next;
	}

	if(count == 0) {
		logw("Exec processdir call, not found in line with process.");
		return 0;
	}

	outpath = xzalloc(PATH_ARR_MAX + 1);

	for(i=0; i< count; i++) {
		char dir[PATH_MAX] = {0};
		len += xsnprintf(outpath + len, PATH_ARR_MAX - len, "%s%c", 
				path2dir(patharr[i], dir), MULTIPATH_SPLIT_CH);
	}
	outpath[len - 1] = '\0';
	
	logd("Exec processdir call, result dir:%s.", outpath);

	lre_value_dup2_string(val, outpath);
	free(outpath);
	process->state = STATE_EXEC_SUCCESS;
	return 0;
}

static lrc_obj_t *processdir_constructor(void)
{
	struct lrc_process *process;

	process = malloc(sizeof(*process));
	if(!process) {
		return (lrc_obj_t *)0;
	}
	process->procname = NULL;
	process->procpath = NULL;
	process->excludepath = NULL;
	process->count = 0;
	process->flags = 0;

	return (lrc_obj_t *)process;
}

static void processdir_destructor(lrc_obj_t *handle)
{
	struct lrc_process *process;

	process = (struct lrc_process *)handle;

	if(process->procname)
		free(process->procname);
	if(process->procpath)
		free(process->procpath);
	if(process->excludepath)
		free(process->excludepath);

	free(process);
}

static int cmd2args(const char *cmdline, int len, char **argv)
{
    int i = 0;
    int argc = 0;
    char *ptr = (char *)cmdline;

    while(i < len) {
        if(ptr[i] == 0) {
            i++;
            continue;
        }
        argv[argc++] = ptr + i;
        i += strlen(ptr + i);
    }
    return argc;
}

#define CMDLINE_EXTRACT_LEN_MAX     (4095)
static int cmdline_extract_execute(lrc_obj_t *handle, struct lre_value *val)
{
	int i;
    int len;
    int keylen;
	struct lrc_process *process;
	struct process_info *psinfo;
    int argc;
    char *argv[PROCESS_ARG_MAX];
    int spacesplit = 0;
    char *outstr;

    if(processlist == NULL)
        processes_info_load();

    process = (struct lrc_process *)handle;
    if(!process->procname && !process->procpath) {
        process->state = STATE_EXEC_FAILED;
        loge("Failed execute 'process' func. Not specified process name or path.");
        return -EINVAL;
    }

    fill_spec_process(process);

    process->state = STATE_EXEC_SUCCESS;

    keylen = strlen(process->argkey);
    while (process->argkey[keylen - 1] == ' ') {
        spacesplit = 1;
        keylen--;
        if(keylen <= 0) {
            loge("lrc 'cmdline_extract': Invaild 'argkey'");
            return -EINVAL;
        }
    }

    outstr = xzalloc(CMDLINE_EXTRACT_LEN_MAX + 1);

    len = 0;
    for(i=0; i<process->count; i++) {
        int j;
        psinfo = process->psinfo[i];

        argc = cmd2args(psinfo->cmdline, psinfo->cmdlen, argv);

        if(process->argindex != 0 && 
                process->argindex < argc) {
            len += xsnprintf(outstr + len, CMDLINE_EXTRACT_LEN_MAX - len, "%s%c", 
                    argv[process->argindex], MULTIPATH_SPLIT_CH);
            continue;
        }
        for(j=0; j<argc; j++) {
            if(!strncmp(argv[j], process->argkey, keylen)) {
                break;
            }
        }

        if(spacesplit && (j + 1) < argc) {
            j++;
            len += xsnprintf(outstr + len, CMDLINE_EXTRACT_LEN_MAX - len, "%s%c", 
                    argv[j], MULTIPATH_SPLIT_CH);
        } else if(j < argc) {
            len += xsnprintf(outstr + len, CMDLINE_EXTRACT_LEN_MAX - len, "%s%c", 
                    argv[j] + strlen(process->argkey), MULTIPATH_SPLIT_CH);
        }
    }
    outstr[len - 1] = '\0';

	lre_value_dup2_string(val, outstr);
	free(outstr);
	return 0;
}

static lrc_obj_t *cmdline_extract_constructor(void)
{
	struct lrc_process *process;

	process = malloc(sizeof(*process));
	if(!process) {
		return (lrc_obj_t *)0;
	}
	process->procname = NULL;
	process->procpath = NULL;
	process->excludepath = NULL;
	process->count = 0;

	process->argkey = NULL;
	process->argindex = 0;

	process->flags = FLAGS_CMDLINE_EXTRACT;
	return (lrc_obj_t *)process;
}

static void cmdline_extract_destructor(lrc_obj_t *handle)
{
	struct lrc_process *process;

	process = (struct lrc_process *)handle;

	if(process->procname)
		free(process->procname);
	if(process->procpath)
		free(process->procpath);
	if(process->excludepath)
		free(process->excludepath);
	if(process->argkey)
        free(process->argkey);

	free(process);
}


static struct lrc_stub_arg process_args[] = {
	{
		.keyword  	 = "procname",
		.description = "Type: string. Specify process name. example: procname=\"bash\"",
		.handler 	 = arg_procname_handler,
	}, {
		.keyword  	 = "procpath",
		.description = "Type: string. Specify process binary directory. example: procpath=\"/usr/local/\"",
		.handler 	 = arg_procpath_handler,
	}, {
		.keyword  	 = "excludepath",
		.description = "Type: string. example: excludepath=\"/usr/local/\"",
		.handler 	 = arg_excludepath_handler,
	}
};

static struct lrc_stub_expr process_exprs[] = {
	{
		.keyword  	 = "running",
		.description = "Check process running or not. val: '1' or '0', 1: running, 0: not running, example: running==1",
		.handler 	 = expr_running_handler,
	}, {
		.keyword  	 = "user",
		.description = "Check user who starts the process. example: user==root",
		.handler 	 = expr_user_handler,
	}
};

static struct lrc_stub_func lrc_funcs[] = {
	{
		.keyword 	 = "process",
		.description = "Check process user,cmdline,running or not, etc.",
		.constructor = process_constructor,
		.exec 		 = process_execute,
		.destructor  = process_destructor,

		.args 	   = process_args,
		.argcount  = ARRAY_SIZE(process_args),
		.exprs     = process_exprs,
		.exprcount = ARRAY_SIZE(process_exprs),
	}
};

static struct lrc_stub_arg cmdline_extract_args[] = {
	{
		.keyword  	 = "procname",
		.description = "Type: string. Specify process name. example: procname=\"bash\"",
		.handler 	 = arg_procname_handler,
	}, {
		.keyword  	 = "procpath",
		.description = "Type: string. Specify process binary directory. example: procpath=\"/usr/local/\"",
		.handler 	 = arg_procpath_handler,
	}, {
		.keyword  	 = "excludepath",
		.description = "Type: string. example: excludepath=\"/usr/local/\"",
		.handler 	 = arg_excludepath_handler,
	}, {
        .keyword  	 = "argindex",
		.description = "Type: string. example: excludepath=\"/usr/local/\"",
		.handler 	 = arg_argindex_handler,
    }, {
		.keyword  	 = "argkey",
		.description = "Type: string. example: excludepath=\"/usr/local/\"",
		.handler 	 = arg_argkey_handler,
	}
};


static struct lrc_stub_call lrc_calls[] = {
	{
		.keyword 	 = "processdir",
		.description = "Get process directory.",
		.constructor = processdir_constructor,
		.exec 		 = processdir_execute,
		.destructor  = processdir_destructor,

		.args 	   = process_args,
		.argcount  = ARRAY_SIZE(process_args),
	}, {
		.keyword 	 = "cmdline_extract",
		.description = "extract arguments item in cmdline.",
		.constructor = cmdline_extract_constructor,
		.exec 		 = cmdline_extract_execute,
		.destructor  = cmdline_extract_destructor,

		.args 	   = cmdline_extract_args,
		.argcount  = ARRAY_SIZE(cmdline_extract_args),
	}
};

static struct lrc_module lrc_process_mod = {
	.name = "lrc_process",
	.funcs = lrc_funcs,
	.funccount = ARRAY_SIZE(lrc_funcs),
	.calls = lrc_calls,
	.callcount = ARRAY_SIZE(lrc_calls),
};


int lrc_process_init(void)
{
	int ret;

	ret = lrc_module_register(&lrc_process_mod);
	if(ret)
		loge("Failed to register '%s' modules", lrc_process_mod.name);
	return ret;
}

void lrc_process_release(void)
{
	if(processlist)
		processes_info_release();
	lrc_module_unregister(&lrc_process_mod);
}

