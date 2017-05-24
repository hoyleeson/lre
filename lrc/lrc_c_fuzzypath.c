#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "../log.h"
#include "../utils.h"
#include "../lre.h"

#define MULTIPATH_SPLIT_CH 	';'

struct lrc_fuzzypath {
	struct lrc_object base;
	char *basepath;
	char *path;
};

#define PATH_ARR_MAX  	(PATH_MAX * 10)

#define DIR_DEPTH_MAX 		(128)
#define FUZZY_POINT_MAX 	(4)

struct frag {
	char *str[FUZZY_POINT_MAX]; /* only use for fuzzy node */
	int count;
	int flags;
#define _FLAGS_START_FUZZY 		(1 << 0)
#define _FLAGS_END_FUZZY 		(1 << 1)
};

struct fuzzydirent {
	int type;
#define DIRENT_TYPE_NORMAL 		(1)
#define DIRENT_TYPE_FUZZY 		(2)
	union {
		char *name;
		struct frag frag;
	};
};

static void path_add(char *path, char *new)
{
	int len;

	//logv("fuzzypath add: %s", new);
	len = strlen(path);
	len += snprintf(path + len, PATH_ARR_MAX - len, "%s%c",
		   	new, MULTIPATH_SPLIT_CH);
//	strncat(path, new, PATH_ARR_MAX);
//	strncat(path, ":", PATH_ARR_MAX);
}

static int recurse_dir(char *path, int depth, 
		struct fuzzydirent *fdir, int depthmax, char *outpath)
{
	int i;
	DIR *dir;
	struct dirent *dp;
	int pathlen = strlen(path);
	char *ptr;
	char *subpath = xzalloc(sizeof(char)*PATH_MAX);
	if(!subpath)
		return -ENOMEM;

	//logv("recurse dir:%s", path);
	dir = opendir(path);
	if (dir == NULL) {
		// not a directory, carry on
		return -EINVAL;
	}

	while ((dp = readdir(dir)) != NULL) {
		struct stat st;
		if (strcmp(dp->d_name, ".") == 0 ||
				strcmp(dp->d_name, "..") == 0)
			continue;

		memset(subpath, 0, PATH_MAX);
		if (strlen(dp->d_name) + pathlen + 2/*NUL and slash*/ > PATH_MAX) {
			logi("Invalid path specified: too long");
			continue;
		}
		/*FIXME: check soft link */

		strcpy(subpath, path);
		strcat(subpath, "/");
		strcat(subpath, dp->d_name);

		if(lstat(subpath, &st) < 0) {
			logd("lstat %s error", subpath);
		   	continue;
		}

		if(!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode))
			continue;

		if(fdir->type == DIRENT_TYPE_NORMAL) {
			if(strcmp(fdir->name, dp->d_name))
				continue;
		} else {
			ptr = dp->d_name;
			ptr = strstr(ptr, fdir->frag.str[0]);
			if(ptr == NULL) {
				continue;
			} else if(!(fdir->frag.flags & _FLAGS_START_FUZZY) && (ptr != dp->d_name)) {
				/* The first fragment is not fuzzy: ptr don't move */
				continue;
			}

			for(i=1; i<fdir->frag.count; i++) {
				ptr = strstr(ptr, fdir->frag.str[i]);
				if(ptr == NULL) {
					break;
				}
			}

			if(fdir->frag.count != 0 && i != fdir->frag.count)
				continue;

			/* If last fragment is not fuzzy: compare str */
			if(ptr && !(fdir->frag.flags & _FLAGS_END_FUZZY)) {
				int l = fdir->frag.count;
				if(l > 0 && strcmp(ptr, fdir->frag.str[l - 1])) 
					continue;
			}
		}

		if(depth + 1 == depthmax) {
			path_add(outpath, subpath);
			continue;
		}

		if(S_ISDIR(st.st_mode)) {
			recurse_dir(subpath, depth + 1, fdir + 1, depthmax, outpath);
		}
	}
	free(subpath);
	closedir(dir);
	return 0;
}


static int fuzzy2path(char *path, char *outpath)
{
	char *token;
	int i;
	int cnt = 0;
	int len = 0;
	char buf[PATH_MAX];
	struct fuzzydirent fdir[DIR_DEPTH_MAX];
	memset(fdir, 0, sizeof(struct fuzzydirent) * DIR_DEPTH_MAX);

	if(path[0] == '/')
		len = sprintf(buf, "/");

	logv("Fuzzypath '%s' to path start.", path);
	/* Step 1. split */
	while((token = strsep(&path, "/")) != NULL) {
		char *p, *ptr;
		int j = 0;
		int l;

		if(!strcmp(token, ""))
			continue;

		ptr = token;
		fdir[cnt].frag.flags = 0;
		if(ptr[0] == '*')
			fdir[cnt].frag.flags |= _FLAGS_START_FUZZY;
		l = strlen(ptr);	
		if(l > 0 && ptr[l - 1] == '*')
			fdir[cnt].frag.flags |= _FLAGS_END_FUZZY;

		while((p = strchr(ptr, '*')) != NULL) {
			if(p != ptr) {
				fdir[cnt].frag.str[j] = ptr;
				if(++j >= FUZZY_POINT_MAX) {
					loge("Failed. The fuzzy point does not exceed %d.", FUZZY_POINT_MAX);
					return -EINVAL;
				}
			}

			*p = '\0';
			ptr = p + 1;

			fdir[cnt].type = DIRENT_TYPE_FUZZY;
		}
		if(fdir[cnt].type == DIRENT_TYPE_FUZZY) {
			/* The last fragment */
			fdir[cnt].frag.str[j++] = ptr;
			fdir[cnt].frag.count = j;
		} else {
			fdir[cnt].type = DIRENT_TYPE_NORMAL;
			fdir[cnt].name = token;
		}

		if(++cnt >= DIR_DEPTH_MAX) {
			loge("Failed. The fuzzy directory exceeds %d.", DIR_DEPTH_MAX);
			return -EINVAL;
		}
	}

	/*Step 2. walk real path */
	for (i=0; i<cnt; i++) {
		if(fdir[i].type == DIRENT_TYPE_NORMAL) {
			len += snprintf(buf + len, PATH_MAX - len, "%s/", fdir[i].name);
		} else {
			break;
		}
	}

	if(i > 0) {
		buf[--len] = '\0';
	}
	logv("lrc 'fuzzypath': basedir:%s", buf);

	if(i == cnt) {
		strncat(outpath, buf, PATH_ARR_MAX);
		len = strlen(outpath);
		logw("Warning:%s is not fuzzy path.", buf);
		return 0;
	}

	recurse_dir(buf, i, &fdir[i], cnt, outpath);
	len = strlen(outpath);
	if(len == 0) {
		return -EINVAL;	
	}

	if(outpath[len - 1] == MULTIPATH_SPLIT_CH)
		outpath[len - 1] = '\0';

	return 0;
}


static int fuzzypath_execute(lrc_obj_t *handle, struct lre_value *val)
{
	char path[PATH_MAX] = {0};
	int len = 0;
	struct lrc_fuzzypath *fuzzypath;
	char *outpath;
	int outlen = 0;

	fuzzypath = (struct lrc_fuzzypath *)handle;

	if(!fuzzypath->path) {
		logd("lrc 'fuzzypath': invaild path");
		return -EINVAL;
	}
#if 1
	outpath = xzalloc(PATH_ARR_MAX + 1);

	if(fuzzypath->basepath) {
		/* Support mutli path */
		char *p;
		char *ptr;
		char *tmppath;
		int ret;
		int i;
		char *patharr[1024];
		int cnt = 0;

		ptr = fuzzypath->basepath;
		while((p = strchr(ptr, MULTIPATH_SPLIT_CH)) != NULL) {
			*p++ = '\0';
			patharr[cnt++] = ptr;
			ptr = p;
		}
		patharr[cnt++] = ptr;

		tmppath = xzalloc(PATH_ARR_MAX + 1);
		for(i=0; i<cnt; i++) {
			memset(tmppath, 0, PATH_ARR_MAX + 1);
			len = snprintf(path, PATH_MAX, "%s/", patharr[i]);
			len += snprintf(path + len, PATH_MAX - len, "%s", fuzzypath->path);
			path[len] = '\0';

			logd("lrc 'fuzzypath': fuzzypath:%s", path);

			ret = fuzzy2path(path, tmppath);
			if(ret) {
				continue;	
			}
			outlen += snprintf(outpath + outlen, PATH_ARR_MAX - outlen, "%s%c", 
					tmppath, MULTIPATH_SPLIT_CH);
		}
		if(outlen > 0)
			outpath[outlen - 1] = '\0';
		free(tmppath);
	} else {
		len = snprintf(path, PATH_MAX, "%s", fuzzypath->path);
		path[len] = '\0';

		logd("lrc 'fuzzypath': fuzzypath:%s", path);

		fuzzy2path(path, outpath);
	}

#else
	if(fuzzypath->basepath)
		len += snprintf(path + len, PATH_MAX - len, "%s/", fuzzypath->basepath);

	if(fuzzypath->path)
		len += snprintf(path + len, PATH_MAX - len, "%s", fuzzypath->path);

	if(strlen(path) == 0) {
		logd("lrc 'fuzzypath': invaild path");
		return LRE_RET_ERROR;
	}

	logd("lrc 'fuzzypath': fuzzypath:%s", path);

	outpath = xzalloc(PATH_ARR_MAX);
	fuzzy2path(path, outpath);
#endif

	logd("lrc 'fuzzypath' realpath:%s", outpath);
	val->type = LRE_ARG_TYPE_STRING;
	val->valstring = outpath;
	return LRE_RET_OK;
}

static lrc_obj_t *func_fuzzypath_handler(void)
{
	struct lrc_fuzzypath *fuzzypath;

	fuzzypath = malloc(sizeof(*fuzzypath));
	if(!fuzzypath) {
		return (lrc_obj_t *)0;
	}

	fuzzypath->basepath = NULL;
	fuzzypath->path = NULL;

	return (lrc_obj_t *)fuzzypath;
}

static int arg_basepath_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_fuzzypath *fuzzypath;

	fuzzypath = (struct lrc_fuzzypath *)handle;
	if(!lreval || lreval->type != LRE_ARG_TYPE_STRING || !lreval->valstring) {
		printf("lrc 'fuzzypath' err: basepath must be string\n");
		return LRE_RET_ERROR;
	}

	fuzzypath->basepath = strdup(lreval->valstring);
	logd("lrc 'fuzzypath' arg: basepath:%s", fuzzypath->basepath);

	return LRE_RET_OK;
}

static int arg_path_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_fuzzypath *fuzzypath;

	fuzzypath = (struct lrc_fuzzypath *)handle;
	if(!lreval || lreval->type != LRE_ARG_TYPE_STRING || !lreval->valstring) {
		printf("lrc 'fuzzypath' err: path must be string\n");
		return LRE_RET_ERROR;
	}

	fuzzypath->path = strdup(lreval->valstring);
	logd("lrc 'fuzzypath' arg: path:%s", fuzzypath->path);
	return LRE_RET_OK;
}

static struct lrc_stub_arg fuzzypath_args[] = {
	{
		.keyword  	 = "basepath",
		.description = "Type: string. File base path",
		.handler 	 = arg_basepath_handler,
	}, {
		.keyword  	 = "path",
		.description = "Type: string. File fuzzy path",
		.handler 	 = arg_path_handler,
	}
};

static struct lrc_stub_call lrc_calls[] = {
	{
		.keyword 	 = "fuzzypath",
		.description = "Get path by fuzzypath.",
		.handler 	 = func_fuzzypath_handler,
		.exec 		 = fuzzypath_execute,

		.args 	   = fuzzypath_args,
		.argcount  = ARRAY_SIZE(fuzzypath_args),
	}
};

struct lrc_module lrc_fuzzypath_mod = {
	.name = "lrc_call_fuzzypath",
	.calls = lrc_calls,
	.callcount = ARRAY_SIZE(lrc_calls),
};

int lrc_c_fuzzypath_init(void)
{
	int ret;

	ret = lrc_module_register(&lrc_fuzzypath_mod);
	if(ret)
		loge("Failed to register '%s' modules ", lrc_fuzzypath_mod.name);
	return ret;
}

void lrc_c_fuzzypath_release(void)
{
	lrc_module_unregister(&lrc_fuzzypath_mod);
}
