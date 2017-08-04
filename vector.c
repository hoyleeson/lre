#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "vector.h"

/*
 * Initialize vector struct.
 * allocalted 'size' slot elements then return vector.
 */
vector_t *vector_alloc(void)
{
    vector_t *v = (vector_t *) xzalloc(sizeof(vector_t));
    return v;
}

vector_t *vector_init(unsigned int size)
{
    vector_t *v = vector_alloc();

    /* allocate at least one slot */
    if (size == 0)
        size = 4;

    v->capacity = size;
    v->active = 0;
    v->slot = (void *) xzalloc(sizeof(void *) * size);
    return v;
}

/* capacity one slot */
void vector_alloc_slot(vector_t *v)
{
    v->capacity += VECTOR_DEFAULT_SIZE;
    if (v->slot)
        v->slot = xrealloc(v->slot, sizeof (void *) * v->capacity);
    else
        v->slot = (void *) xzalloc(sizeof (void *) * v->capacity);
}

/* Insert a value into a specific slot */
void vector_insert_slot(vector_t *v, int index, void *value)
{
    int i;

    vector_alloc_slot(v);
    for (i = (v->capacity / VECTOR_DEFAULT_SIZE) - 2; i >= index; i--)
        v->slot[i + 1] = v->slot[i];

    v->slot[index] = value;
}

/* Copy / dup a vector */
vector_t *vector_copy(vector_t *v)
{
    unsigned int size;
    vector_t *new = vector_alloc();

    new->active = v->active;
    new->capacity = v->capacity;

    size = sizeof(void *) * (v->capacity);
    new->slot = (void *) xzalloc(size);
    memcpy(new->slot, v->slot, size);

    return new;
}

/* Check assigned index, and if it runs short double index pointer */
void vector_ensure(vector_t *v, unsigned int num)
{
    if (v->capacity > num)
        return;

    v->slot = xrealloc(v->slot, sizeof(void *) * (v->capacity * 2));
    memset(&v->slot[v->capacity], 0, sizeof(void *) * v->capacity);
    v->capacity *= 2;

    if (v->capacity <= num)
        vector_ensure(v, num);
}

/* This function only returns next empty slot index.  It dose not mean
 * the slot's index memory is assigned, please call vector_ensure()
 * after calling this function.
 */
int vector_empty_slot(vector_t *v)
{
    unsigned int i;

    if (v->active == 0)
        return 0;

    for (i = 0; i < v->active; i++) {
        if (v->slot[i] == 0) {
            return i;
        }
    }

    return i;
}

/* Set value to the smallest empty slot. */
int vector_set(vector_t *v, void *val)
{
    unsigned int i;

    i = vector_empty_slot(v);
    vector_ensure(v, i);

    v->slot[i] = val;

    if (v->active <= i)
        v->active = i + 1;

    return i;
}

/* Set a vector slot value */
void vector_set_slot(vector_t *v, void *value)
{
    unsigned int i = v->capacity - 1;

    v->slot[i] = value;
}

/* Set value to specified index slot. */
int vector_set_index(vector_t *v, unsigned int i, void *val)
{
    vector_ensure(v, i);

    v->slot[i] = val;

    if (v->active <= i)
        v->active = i + 1;

    return i;
}

/* Look up vector.  */
void *vector_lookup(vector_t *v, unsigned int i)
{
    if (i >= v->active)
        return NULL;
    return v->slot[i];
}

/* Lookup vector, ensure it. */
void *vector_lookup_ensure(vector_t *v, unsigned int i)
{
    vector_ensure(v, i);
    return v->slot[i];
}

/* Unset value at specified index slot. */
void vector_unset(vector_t *v, unsigned int i)
{
    if (i >= v->capacity)
        return;

    v->slot[i] = NULL;

    if (i + 1 == v->active) {
        v->active--;
        while (i && v->slot[--i] == NULL && v->active--)
            ;	/* Is this ugly ? */
    }
}

/* Count the number of not emplty slot. */
unsigned int vector_count(vector_t *v)
{
    unsigned int i;
    unsigned count = 0;

    for (i = 0; i < v->active; i++) {
        if (v->slot[i] != NULL) {
            count++;
        }
    }

    return count;
}

void vector_slot_free(void *slot)
{
    free(slot);
}

/* Free memory vector allocation */
void vector_free(vector_t *v)
{
    free(v->slot);
    free(v);
}

/* dump vector slots */
void vector_dump(vector_t *v)
{
    int i;

    printf("Vector Size : %d\n", v->capacity);

    for (i = 0; i < v->capacity; i++) {
        if (v->slot[i] != NULL) {
            printf("  Slot [%d]: %p\n", i, vector_slot(v, i));
        }
    }
}

/* String vector related */
void free_strvec(vector_t *strvec)
{
    int i;
    char *str;

    if (!strvec)
        return;

    for (i = 0; i < vector_capacity(strvec); i++) {
        if ((str = vector_slot(strvec, i)) != NULL) {
            free(str);
        }
    }

    vector_free(strvec);
}

void dump_strvec(vector_t *strvec)
{
    int i;
    char *str;

    if (!strvec)
        return;

    printf("String Vector : ");

    for (i = 0; i < vector_capacity(strvec); i++) {
        str = vector_slot(strvec, i);
        printf("[%i]=%s ", i, str);
    }
    printf("\n");
}

