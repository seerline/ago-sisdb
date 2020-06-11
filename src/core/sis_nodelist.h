/* adlist.h - A generic doubly linked s_list implementation
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
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

#ifndef _SIS_NODE_LIST_H
#define _SIS_NODE_LIST_H

/* Node, List, and Iterator are the only data structures used currently. */

typedef struct listNode {
    struct listNode *prev;
    struct listNode *next;
    void *value;
} listNode;

typedef struct listIter {
    listNode *next;
    int direction;
} listIter;

typedef struct s_list {
    listNode *head;
    listNode *tail;
    void *(*dup)(void *ptr);
    void (*free)(void *ptr);
    int (*match)(void *ptr, void *key);
    unsigned int len;
} s_list;

/* Functions implemented as macros */
#define listLength(l) ((l)->len)
#define listFirst(l) ((l)->head)
#define listLast(l) ((l)->tail)
#define listPrevNode(n) ((n)->prev)
#define listNextNode(n) ((n)->next)
#define listNodeValue(n) ((n)->value)

#define listSetDupMethod(l,m) ((l)->dup = (m))
#define listSetFreeMethod(l,m) ((l)->free = (m))
#define listSetMatchMethod(l,m) ((l)->match = (m))

#define listGetDupMethod(l) ((l)->dup)
#define listGetFree(l) ((l)->free)
#define listGetMatchMethod(l) ((l)->match)

/* Prototypes */
s_list *listCreate(void);
void listRelease(s_list *s_list);
void listEmpty(s_list *s_list);
s_list *listAddNodeHead(s_list *s_list, void *value);
s_list *listAddNodeTail(s_list *s_list, void *value);
s_list *listInsertNode(s_list *s_list, listNode *old_node, void *value, int after);
void listDelNode(s_list *s_list, listNode *node);
listIter *listGetIterator(s_list *s_list, int direction);
listNode *listNext(listIter *iter);
void listReleaseIterator(listIter *iter);
s_list *listDup(s_list *orig);
listNode *listSearchKey(s_list *s_list, void *key);
listNode *listIndex(s_list *s_list, long index);
void listRewind(s_list *s_list, listIter *li);
void listRewindTail(s_list *s_list, listIter *li);
void listRotate(s_list *s_list);
void listJoin(s_list *l, s_list *o);

/* Directions for iterators */
#define AL_START_HEAD 0
#define AL_START_TAIL 1

#define s_sis_list s_list
#define sis_list_create listCreate
void sis_list_destroy(void *list_);

#define sis_list_dup listDup
#define sis_list_clear listEmpty
#define sis_list_delete listDelNode
#define sis_list_push listAddNodeTail
#define sis_list_get listIndex
#define sis_list_getsize listLength
#define sis_list_first listFirst
#define sis_list_last  listLast
#define sis_list_prev listPrevNode
#define sis_list_next listNextNode

#define s_sis_list_node listNode

#endif /* _SIS_NODE_LIST_H */
