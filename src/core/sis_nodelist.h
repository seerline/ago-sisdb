
#ifndef _SIS_NODE_LIST_H
#define _SIS_NODE_LIST_H

/* Node, List, and Iterator are the only data structures used currently. */

typedef struct s_sis_list_node {
    struct s_sis_list_node *prev;
    struct s_sis_list_node *next;
    void *value;
} s_sis_list_node;

typedef struct s_sis_list_iter {
    s_sis_list_node *next;
    int direction;
} s_sis_list_iter;

typedef struct s_sis_list {
    s_sis_list_node *head;
    s_sis_list_node *tail;
    void *(*dup)(void *ptr);
    void (*free)(void *ptr);
    int (*match)(void *ptr, void *key);
    unsigned int len;
} s_sis_list;

/* Functions implemented as macros */
#define sis_list_getsize(l) ((l)->len)
#define sis_list_first(l) ((l)->head)
#define sis_list_last(l) ((l)->tail)
#define sis_list_prev(n) ((n)->prev)
#define sis_list_next(n) ((n)->next)
#define sis_list_get_value(n) ((n)->value)

#define sis_list_set_dup_method(l,m) ((l)->dup = (m))
#define sis_list_set_free_method(l,m) ((l)->free = (m))
#define sis_list_set_match_method(l,m) ((l)->match = (m))

#define sis_list_get_dup_method(l) ((l)->dup)
#define sis_list_get_free_method(l) ((l)->free)
#define sis_list_get_match_method(l) ((l)->match)

/* Prototypes */
s_sis_list *sis_list_create(void);
void sis_list_destroy(void *list_);
void sis_list_clear(s_sis_list *s_sis_list);

s_sis_list *sis_list_add_node_head(s_sis_list *s_sis_list, void *value);
s_sis_list *sis_list_push(s_sis_list *s_sis_list, void *value);
s_sis_list *sis_list_insert_node(s_sis_list *s_sis_list, s_sis_list_node *old_node, void *value, int after);
void sis_list_delete(s_sis_list *s_sis_list, s_sis_list_node *node);
s_sis_list_iter *sis_list_get_iter(s_sis_list *s_sis_list, int direction);
s_sis_list_node *sis_list_next_iter(s_sis_list_iter *iter);
void sis_list_release_iter(s_sis_list_iter *iter);
s_sis_list *sis_list_dup(s_sis_list *orig);
s_sis_list_node *sis_list_search_key(s_sis_list *s_sis_list, void *key);
s_sis_list_node *sis_list_get(s_sis_list *s_sis_list, long index);
void sis_list_rewind(s_sis_list *s_sis_list, s_sis_list_iter *li);
void sis_list_rewind_tail(s_sis_list *s_sis_list, s_sis_list_iter *li);
void sis_list_rotate(s_sis_list *s_sis_list);
void sis_list_join(s_sis_list *l, s_sis_list *o);

/* Directions for iterators */
#define AL_START_HEAD 0
#define AL_START_TAIL 1

#endif /* _SIS_NODE_LIST_H */
