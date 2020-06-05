/* adlist.c - A generic doubly linked s_list implementation
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this s_list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this s_list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <os_malloc.h>
#include "sis_nodelist.h"
/* Create a new s_list. The created s_list can be freed with
 * AlFreeList(), but private value of every node need to be freed
 * by the user before to call AlFreeList().
 *
 * On error, NULL is returned. Otherwise the pointer to the new s_list. */
s_list *listCreate(void)
{
    struct s_list *s_list;

    if ((s_list = sis_malloc(sizeof(*s_list))) == NULL)
        return NULL;
    s_list->head = s_list->tail = NULL;
    s_list->len = 0;
    s_list->dup = NULL;
    s_list->free = NULL;
    s_list->match = NULL;
    return s_list;
}

/* Remove all the elements from the s_list without destroying the s_list itself. */
void listEmpty(s_list *s_list)
{
    unsigned long len;
    listNode *current, *next;

    current = s_list->head;
    len = s_list->len;
    while (len--)
    {
        next = current->next;
        if (s_list->free)
            s_list->free(current->value);
        sis_free(current);
        current = next;
    }
    s_list->head = s_list->tail = NULL;
    s_list->len = 0;
}

/* Free the whole s_list.
 *
 * This function can't fail. */
void listRelease(s_list *s_list)
{
    listEmpty(s_list);
    sis_free(s_list);
}

void sis_list_destroy(void *list_)
{
    s_list *list = (s_list *)list_;
    listRelease(list);
}
/* Add a new node to the s_list, to head, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * s_list remains unaltered).
 * On success the 's_list' pointer you pass to the function is returned. */
s_list *listAddNodeHead(s_list *s_list, void *value)
{
    listNode *node;

    if ((node = sis_malloc(sizeof(*node))) == NULL)
    {
        return NULL;
    }
    node->value = value;
    if (s_list->len == 0)
    {
        s_list->head = s_list->tail = node;
        node->prev = node->next = NULL;
    }
    else
    {
        node->prev = NULL;
        node->next = s_list->head;
        s_list->head->prev = node;
        s_list->head = node;
    }
    s_list->len++;
    return s_list;
}

/* Add a new node to the s_list, to tail, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * s_list remains unaltered).
 * On success the 's_list' pointer you pass to the function is returned. */
s_list *listAddNodeTail(s_list *s_list, void *value)
{
    listNode *node;

    if ((node = sis_malloc(sizeof(*node))) == NULL)
    {
        return NULL;
    }
    node->value = value;
    if (s_list->len == 0)
    {
        s_list->head = s_list->tail = node;
        node->prev = node->next = NULL;
    }
    else
    {
        node->prev = s_list->tail;
        node->next = NULL;
        s_list->tail->next = node;
        s_list->tail = node;
    }
    s_list->len++;
    return s_list;
}

s_list *listInsertNode(s_list *s_list, listNode *old_node, void *value, int after)
{
    listNode *node;

    if ((node = sis_malloc(sizeof(*node))) == NULL)
    {
        return NULL;
    }
    node->value = value;
    if (after)
    {
        node->prev = old_node;
        node->next = old_node->next;
        if (s_list->tail == old_node)
        {
            s_list->tail = node;
        }
    }
    else
    {
        node->next = old_node;
        node->prev = old_node->prev;
        if (s_list->head == old_node)
        {
            s_list->head = node;
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
    s_list->len++;
    return s_list;
}

/* Remove the specified node from the specified s_list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */
void listDelNode(s_list *s_list, listNode *node)
{
    if (node->prev)
    {
        node->prev->next = node->next;
    }
    else
    {
        s_list->head = node->next;
    }
    if (node->next)
    {
        node->next->prev = node->prev;
    }
    else
    {
        s_list->tail = node->prev;
    }
    if (s_list->free)
    {
        s_list->free(node->value);
    }
    sis_free(node);
    s_list->len--;
}

/* Returns a s_list iterator 'iter'. After the initialization every
 * call to listNext() will return the next element of the s_list.
 *
 * This function can't fail. */
listIter *listGetIterator(s_list *s_list, int direction)
{
    listIter *iter;

    if ((iter = sis_malloc(sizeof(*iter))) == NULL)
    {
        return NULL;
    }
    if (direction == AL_START_HEAD)
    {
        iter->next = s_list->head;
    }
    else
    {
        iter->next = s_list->tail;
    }
    iter->direction = direction;
    return iter;
}

/* Release the iterator memory */
void listReleaseIterator(listIter *iter)
{
    sis_free(iter);
}

/* Create an iterator in the s_list private iterator structure */
void listRewind(s_list *s_list, listIter *li)
{
    li->next = s_list->head;
    li->direction = AL_START_HEAD;
}

void listRewindTail(s_list *s_list, listIter *li)
{
    li->next = s_list->tail;
    li->direction = AL_START_TAIL;
}

/* Return the next element of an iterator.
 * It's valid to remove the currently returned element using
 * listDelNode(), but not to remove other elements.
 *
 * The function returns a pointer to the next element of the s_list,
 * or NULL if there are no more elements, so the classical usage patter
 * is:
 *
 * iter = listGetIterator(s_list,<direction>);
 * while ((node = listNext(iter)) != NULL) {
 *     doSomethingWith(listNodeValue(node));
 * }
 *
 * */
listNode *listNext(listIter *iter)
{
    listNode *current = iter->next;

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

/* Duplicate the whole s_list. On out of memory NULL is returned.
 * On success a copy of the original s_list is returned.
 *
 * The 'Dup' method set with listSetDupMethod() function is used
 * to copy the node value. Otherwise the same pointer value of
 * the original node is used as value of the copied node.
 *
 * The original s_list both on success or error is never modified. */
s_list *listDup(s_list *orig)
{
    s_list *copy;
    listIter iter;
    listNode *node;

    if ((copy = listCreate()) == NULL)
    {
        return NULL;
    }
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;
    listRewind(orig, &iter);
    while ((node = listNext(&iter)) != NULL)
    {
        void *value;

        if (copy->dup)
        {
            value = copy->dup(node->value);
            if (value == NULL)
            {
                listRelease(copy);
                return NULL;
            }
        }
        else
        {
            value = node->value;
        }
        if (listAddNodeTail(copy, value) == NULL)
        {
            listRelease(copy);
            return NULL;
        }
    }
    return copy;
}

/* Search the s_list for a node matching a given key.
 * The match is performed using the 'match' method
 * set with listSetMatchMethod(). If no 'match' method
 * is set, the 'value' pointer of every node is directly
 * compared with the 'key' pointer.
 *
 * On success the first matching node pointer is returned
 * (search starts from head). If no matching node exists
 * NULL is returned. */
listNode *listSearchKey(s_list *s_list, void *key)
{
    listIter iter;
    listNode *node;

    listRewind(s_list, &iter);
    while ((node = listNext(&iter)) != NULL)
    {
        if (s_list->match)
        {
            if (s_list->match(node->value, key))
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
listNode *listIndex(s_list *s_list, long index)
{
    listNode *n;

    if (index < 0)
    {
        index = (-index) - 1;
        n = s_list->tail;
        while (index-- && n)
        {
            n = n->prev;
        }
    }
    else
    {
        n = s_list->head;
        while (index-- && n)
        {
            n = n->next;
        }
    }
    return n;
}

/* Rotate the s_list removing the tail node and inserting it to the head. */
void listRotate(s_list *s_list)
{
    listNode *tail = s_list->tail;

    if (listLength(s_list) <= 1)
    {
        return;
    }

    /* Detach current tail */
    s_list->tail = tail->prev;
    s_list->tail->next = NULL;
    /* Move it as head */
    s_list->head->prev = tail;
    tail->prev = NULL;
    tail->next = s_list->head;
    s_list->head = tail;
}

/* Add all the elements of the s_list 'o' at the end of the
 * s_list 'l'. The s_list 'other' remains empty but otherwise valid. */
void listJoin(s_list *l, s_list *o)
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

    /* Setup other as an empty s_list. */
    o->head = o->tail = NULL;
    o->len = 0;
}
