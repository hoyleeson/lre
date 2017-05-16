#ifndef _VECTOR_H
#define _VECTOR_H

/* vector definition */
typedef struct _vector {
	unsigned int	active;
	unsigned int	capacity;
	void		**slot;
} vector_t;

/* Some defines */
#define VECTOR_DEFAULT_SIZE 	(4)

/* Some usefull macros */
#define vector_slot(V,E) ((V)->slot[(E)])
#define vector_capacity(V)   ((V)->capacity)
#define vector_active(V) ((V)->active)
#define vector_foreach_slot(v,p,i) \
	for (i = 0; i < (v)->capacity && ((p) = (v)->slot[i]); i++)

#define vector_foreach_active_slot(v,p,i) \
	for (i = 0; i < (v)->active && ((p) = (v)->slot[i]); i++)

/* Prototypes */
vector_t *vector_alloc(void);
vector_t *vector_init(unsigned int);

void vector_alloc_slot(vector_t *);
void vector_insert_slot(vector_t *, int, void *);

vector_t *vector_copy(vector_t *);
void vector_ensure(vector_t *, unsigned int);
int vector_empty_slot(vector_t *);
int vector_set(vector_t *, void *);
void vector_set_slot(vector_t *, void *);
int vector_set_index(vector_t *, unsigned int, void *);
void *vector_lookup(vector_t *, unsigned int);
void *vector_lookup_ensure(vector_t *, unsigned int);
void vector_unset(vector_t *, unsigned int);

unsigned int vector_count(vector_t *);

void vector_slot_free(void *);
void vector_free(vector_t *);

void vector_dump(vector_t *);
void free_strvec(vector_t *);
void dump_strvec(vector_t *);

#endif

