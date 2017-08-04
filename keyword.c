#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "lre_internal.h"

static keystub_vec_t *root_kstub_vec = NULL;

keystub_vec_t *get_root_keyvec(void)
{
    return root_kstub_vec;
}

static int keyword_stub_add(struct keyword_stub *keystub,
                            struct keyword_stub *parent)
{
    keystub_vec_t *vec;

    if (parent)
        vec = parent->subvec;
    else
        vec = get_root_keyvec();

    vector_set(vec, keystub);
    return 0;
}


static struct keyword_stub *keyword_stub_create(int type, const char *keyword,
        void *data)
{
    struct keyword_stub *keystub;

    keystub = xzalloc(sizeof(*keystub));

    keystub->type = type;
    keystub->keyword = strdup(keyword);
    assert_ptr(keystub->keyword);
    keystub->data = data;

    if (type == KEYSTUB_TYPE_FUNC ||
        type == KEYSTUB_TYPE_CALL)
        keystub->subvec = vector_init(0);

    return keystub;
}

static void keyword_stub_destroy(struct keyword_stub *keystub)
{
    int i;
    struct keyword_stub *substub;

    free(keystub->keyword);

    if (keystub->subvec) {
        vector_foreach_active_slot(keystub->subvec, substub, i) {
            if (!substub)
                continue;
            keyword_stub_destroy(substub);
        }
    }
    if (keystub->subvec)
        vector_free(keystub->subvec);
    free(keystub);
}

struct keyword_stub *keyword_install(int type, const char *keyword,
                                     void *data, struct keyword_stub *parent)
{
    struct keyword_stub *keystub;

    keystub = keyword_stub_create(type, keyword, data);
    keystub->parent = parent;

    keyword_stub_add(keystub, parent);
    return keystub;
}

void keyword_uninstall(struct keyword_stub *keystub)
{
    int i;
    keystub_vec_t *pvec;
    struct keyword_stub *stub;

    if (keystub->parent) {
        pvec = keystub->parent->subvec;
    } else {
        pvec = get_root_keyvec();
    }

    vector_foreach_active_slot(pvec, stub, i) {
        if (!stub || stub != keystub)
            continue;
        vector_unset(pvec, i);
    }
    keyword_stub_destroy(keystub);
}


struct keyword_stub *find_stub_by_keyword(keystub_vec_t *kwvec,
        const char *keyword)
{
    int i;
    struct keyword_stub *keystub;

    vector_foreach_active_slot(kwvec, keystub, i) {
        if (!keystub || strcmp(keystub->keyword, keyword))
            continue;

        return keystub;
    }

    return NULL;
}

static char *keystub_type_str[] = {
    [KEYSTUB_TYPE_UNKNOWN] = "unknown",
    [KEYSTUB_TYPE_FUNC] = "func",
    [KEYSTUB_TYPE_CALL] = "call",
    [KEYSTUB_TYPE_ARG] = "arg",
    [KEYSTUB_TYPE_EXPR] = "expr",
    [KEYSTUB_TYPE_VAR] = "var",
};

#define dump_blank(l) do {int i; for(i=0; i<level; i++)printf("    "); }while(0)
static void keyword_stub_dump(struct keyword_stub *keystub, int level)
{
    struct lrc_stub_base *base;

    dump_blank(level);
    printf("%s [%s]\n", keystub->keyword, keystub_type_str[keystub->type]);
    dump_blank(level);
    base = (struct lrc_stub_base *)keystub->data;
    printf("\t%s\n", base->description);
}

static void keystub_vec_dump(keystub_vec_t *vec, int level)
{
    int i;
    struct keyword_stub *keystub;

    vector_foreach_active_slot(vec, keystub, i) {
        if (!keystub)
            continue;

        keyword_stub_dump(keystub, level);
        keystub_vec_dump(keystub->subvec, level + 1);
    }
    if ((level % 2) != 0)
        printf("\n");
}

void lre_keyword_dump(void)
{
    keystub_vec_t *vec;

    vec = get_root_keyvec();
    keystub_vec_dump(vec, 0);
}

int lre_keyword_init(void)
{
    root_kstub_vec = vector_init(0);
    return 0;
}

void lre_keyword_release(void)
{
    int i;
    struct keyword_stub *keystub;

    vector_foreach_active_slot(root_kstub_vec, keystub, i) {
        if (!keystub)
            continue;
        keyword_stub_destroy(keystub);
    }

    vector_free(root_kstub_vec);
}

