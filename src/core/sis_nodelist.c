
#include <stdlib.h>
#include <os_malloc.h>
#include "sis_nodelist.h"
/* Create a new s_sis_list. The created s_sis_list can be freed with
 * AlFreeList(), but private value of every node need to be freed
 * by the user before to call AlFreeList().
 *
 * On error, NULL is returned. Otherwise the pointer to the new s_sis_list. */
s_sis_list *sis_list_create(void)
{
    struct s_sis_list *s_sis_list;

    if ((s_sis_list = sis_malloc(sizeof(*s_sis_list))) == NULL)
        return NULL;
    s_sis_list->head = s_sis_list->tail = NULL;
    s_sis_list->len = 0;
    s_sis_list->dup = NULL;
    s_sis_list->free = NULL;
    s_sis_list->match = NULL;
    return s_sis_list;
}

/* Remove all the elements from the s_sis_list without destroying the s_sis_list itself. */
void sis_list_clear(s_sis_list *s_sis_list)
{
    unsigned int len;
    s_sis_list_node *current, *next;

    current = s_sis_list->head;
    len = s_sis_list->len;
    while (len--)
    {
        next = current->next;
        if (s_sis_list->free)
            s_sis_list->free(current->value);
        sis_free(current);
        current = next;
    }
    s_sis_list->head = s_sis_list->tail = NULL;
    s_sis_list->len = 0;
}

/* Free the whole s_sis_list.
 *
 * This function can't fail. */
void _list_release(s_sis_list *s_sis_list)
{
    sis_list_clear(s_sis_list);
    sis_free(s_sis_list);
}

void sis_list_destroy(void *list_)
{
    s_sis_list *list = (s_sis_list *)list_;
    _list_release(list);
}
/* Add a new node to the s_sis_list, to head, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * s_sis_list remains unaltered).
 * On success the 's_sis_list' pointer you pass to the function is returned. */
s_sis_list *sis_list_add_node_head(s_sis_list *s_sis_list, void *value)
{
    s_sis_list_node *node;

    if ((node = sis_malloc(sizeof(*node))) == NULL)
    {
        return NULL;
    }
    node->value = value;
    if (s_sis_list->len == 0)
    {
        s_sis_list->head = s_sis_list->tail = node;
        node->prev = node->next = NULL;
    }
    else
    {
        node->prev = NULL;
        node->next = s_sis_list->head;
        s_sis_list->head->prev = node;
        s_sis_list->head = node;
    }
    s_sis_list->len++;
    return s_sis_list;
}

/* Add a new node to the s_sis_list, to tail, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * s_sis_list remains unaltered).
 * On success the 's_sis_list' pointer you pass to the function is returned. */
s_sis_list *sis_list_push(s_sis_list *s_sis_list, void *value)
{
    s_sis_list_node *node;

    if ((node = sis_malloc(sizeof(*node))) == NULL)
    {
        return NULL;
    }
    node->value = value;
    if (s_sis_list->len == 0)
    {
        s_sis_list->head = s_sis_list->tail = node;
        node->prev = node->next = NULL;
    }
    else
    {
        node->prev = s_sis_list->tail;
        node->next = NULL;
        s_sis_list->tail->next = node;
        s_sis_list->tail = node;
    }
    s_sis_list->len++;
    return s_sis_list;
}

s_sis_list *sis_list_insert_node(s_sis_list *s_sis_list, s_sis_list_node *old_node, void *value, int after)
{
    s_sis_list_node *node;

    if ((node = sis_malloc(sizeof(*node))) == NULL)
    {
        return NULL;
    }
    node->value = value;
    if (after)
    {
        node->prev = old_node;
        node->next = old_node->next;
        if (s_sis_list->tail == old_node)
        {
            s_sis_list->tail = node;
        }
    }
    else
    {
        node->next = old_node;
        node->prev = old_node->prev;
        if (s_sis_list->head == old_node)
        {
            s_sis_list->head = node;
        }
    }
    if (node->prev != NULL)
    {
        node->prev->next = node;
    }
    if (node->next != NULL)
    {
        node->next->prev = node;
    }
    s_sis_list->len++;
    return s_sis_list;
}

/* Remove the specified node from the specified s_sis_list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */
void sis_list_delete(s_sis_list *s_sis_list, s_sis_list_node *node)
{
    if (node->prev)
    {
        node->prev->next = node->next;
    }
    else
    {
        s_sis_list->head = node->next;
    }
    if (node->next)
    {
        node->next->prev = node->prev;
    }
    else
    {
        s_sis_list->tail = node->prev;
    }
    if (s_sis_list->free)
    {
        s_sis_list->free(node->value);
    }
    sis_free(node);
    s_sis_list->len--;
}

/* Returns a s_sis_list iterator 'iter'. After the initialization every
 * call to sis_list_next_iter() will return the next element of the s_sis_list.
 *
 * This function can't fail. */
s_sis_list_iter *sis_list_get_iter(s_sis_list *s_sis_list, int direction)
{
    s_sis_list_iter *iter;

    if ((iter = sis_malloc(sizeof(*iter))) == NULL)
    {
        return NULL;
    }
    if (direction == AL_START_HEAD)
    {
        iter->next = s_sis_list->head;
    }
    else
    {
        iter->next = s_sis_list->tail;
    }
    iter->direction = direction;
    return iter;
}

/* Release the iterator memory */
void sis_list_release_iter(s_sis_list_iter *iter)
{
    sis_free(iter);
}

/* Create an iterator in the s_sis_list private iterator structure */
void sis_list_rewind(s_sis_list *s_sis_list, s_sis_list_iter *li)
{
    li->next = s_sis_list->head;
    li->direction = AL_START_HEAD;
}

void sis_list_rewind_tail(s_sis_list *s_sis_list, s_sis_list_iter *li)
{
    li->next = s_sis_list->tail;
    li->direction = AL_START_TAIL;
}

/* Return the next element of an iterator.
 * It's valid to remove the currently returned element using
 * sis_list_delete(), but not to remove other elements.
 *
 * The function returns a pointer to the next element of the s_sis_list,
 * or NULL if there are no more elements, so the classical usage patter
 * is:
 *
 * iter = sis_list_get_iter(s_sis_list,<direction>);
 * while ((node = sis_list_next_iter(iter)) != NULL) {
 *     doSomethingWith(sis_list_get_value(node));
 * }
 *
 * */
s_sis_list_node *sis_list_next_iter(s_sis_list_iter *iter)
{
    s_sis_list_node *current = iter->next;

    if (current != NULL)
    {
        if (iter->direction == AL_START_HEAD)
        {
            iter->next = current->next;
        }
        else
        {
            iter->next = current->prev;
        }
    }
    return current;
}

/* Duplicate the whole s_sis_list. On out of memory NULL is returned.
 * On success a copy of the original s_sis_list is returned.
 *
 * The 'Dup' method set with sis_list_set_dup_method() function is used
 * to copy the node value. Otherwise the same pointer value of
 * the original node is used as value of the copied node.
 *
 * The original s_sis_list both on success or error is never modified. */
s_sis_list *sis_list_dup(s_sis_list *orig)
{
    s_sis_list *copy;
    s_sis_list_iter iter;
    s_sis_list_node *node;

    if ((copy = sis_list_create()) == NULL)
    {
        return NULL;
    }
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;
    sis_list_rewind(orig, &iter);
    while ((node = sis_list_next_iter(&iter)) != NULL)
    {
        void *value;

        if (copy->dup)
        {
            value = copy->dup(node->value);
            if (value == NULL)
            {
                _list_release(copy);
                return NULL;
            }
        }
        else
        {
            value = node->value;
        }
        if (sis_list_push(copy, value) == NULL)
        {
            _list_release(copy);
            return NULL;
        }
    }
    return copy;
}

/* Search the s_sis_list for a node matching a given key.
 * The match is performed using the 'match' method
 * set with sis_list_set_match_method(). If no 'match' method
 * is set, the 'value' pointer of every node is directly
 * compared with the 'key' pointer.
 *
 * On success the first matching node pointer is returned
 * (search starts from head). If no matching node exists
 * NULL is returned. */
s_sis_list_node *sis_list_search_key(s_sis_list *s_sis_list, void *key)
{
    s_sis_list_iter iter;
    s_sis_list_node *node;

    sis_list_rewind(s_sis_list, &iter);
    while ((node = sis_list_next_iter(&iter)) != NULL)
    {
        if (s_sis_list->match)
        {
            if (s_sis_list->match(node->value, key))
            {
                return node;
            }
        }
        else
        {
            if (key == node->value)
            {
                return node;
            }
        }
    }
    return NULL;
}

/* Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 the penultimate
 * and so on. If the index is out of range NULL is returned. */
s_sis_list_node *sis_list_get(s_sis_list *s_sis_list, long index)
{
    s_sis_list_node *n;

    if (index < 0)
    {
        index = (-index) - 1;
        n = s_sis_list->tail;
        while (index-- && n)
        {
            n = n->prev;
        }
    }
    else
    {
        n = s_sis_list->head;
        while (index-- && n)
        {
            n = n->next;
        }
    }
    return n;
}

/* Rotate the s_sis_list removing the tail node and inserting it to the head. */
void sis_list_rotate(s_sis_list *s_sis_list)
{
    s_sis_list_node *tail = s_sis_list->tail;

    if (sis_list_getsize(s_sis_list) <= 1)
    {
        return;
    }

    /* Detach current tail */
    s_sis_list->tail = tail->prev;
    s_sis_list->tail->next = NULL;
    /* Move it as head */
    s_sis_list->head->prev = tail;
    tail->prev = NULL;
    tail->next = s_sis_list->head;
    s_sis_list->head = tail;
}

/* Add all the elements of the s_sis_list 'o' at the end of the
 * s_sis_list 'l'. The s_sis_list 'other' remains empty but otherwise valid. */
void sis_list_join(s_sis_list *l, s_sis_list *o)
{
    if (o->head)
    {
        o->head->prev = l->tail;
    }

    if (l->tail)
    {
        l->tail->next = o->head;
    }
    else
    {
        l->head = o->head;
    }

    if (o->tail)
    {
        l->tail = o->tail;
    }
    l->len += o->len;

    /* Setup other as an empty s_sis_list. */
    o->head = o->tail = NULL;
    o->len = 0;
}
