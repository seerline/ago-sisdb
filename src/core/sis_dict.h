
#ifndef _SIS_DICT_H
#define _SIS_DICT_H

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "sis_os.h"
#include "os_str.h"
#include "os_time.h"

#define SIS_DICT_OK 0
#define SIS_DICT_ERR 1

typedef struct s_sis_dict_entry {
    void *key;
    union {
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;
    struct s_sis_dict_entry *next;
} s_sis_dict_entry;
// 字典类型
typedef struct s_sis_dict_type {
    uint64_t (*hashmake)(const void *key);//HASH生成函数
    void *(*kdup)(const void *key);//键值复制函数
    void *(*vdup)(const void *obj);//值复制函数
    int (*kcompare)(const void *key1, const void *key2);//键值比较函数
    void (*kfree)(void *obj);//键值析构函数
    void (*vfree)(void *obj);//值析构函数
} s_sis_dict_type;

/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table. */
typedef struct s_sis_dictht {
    s_sis_dict_entry **table;
    unsigned long size;
    unsigned long sizemask;
    unsigned long used;
} s_sis_dictht;

typedef struct s_sis_dict {
    s_sis_dict_type *type;
    void         *source;  // 可带入一个相关数据指针
    s_sis_dictht ht[2];
    long rehashidx; /* rehashing not in progress if rehashidx == -1 */
    unsigned long iterators; /* number of iterators currently running */
} s_sis_dict;

/* If safe is set to 1 this is a safe iterator, that means, you can call
 * sis_dict_add, sis_dict_find, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only sis_dict_next()
 * should be called while iterating. */
typedef struct s_sis_dict_iter {
    s_sis_dict *d;
    long index;
    int table, safe;
    s_sis_dict_entry *entry, *nextEntry;
    /* unsafe iterator fingerprint for misuse detection. */
    long long fingerprint;
} s_sis_dict_iter;

typedef void (sis_dict_scan_function)(void *cb_source, const s_sis_dict_entry *de);
typedef void (sis_dict_scan_bucket_function)(void *cb_source, s_sis_dict_entry **bucketref);

/* This is the initial size of every hash table */
#define DICT_HT_INITIAL_SIZE     4

/* ------------------------------- Macros ------------------------------------*/
#define DICT_FREE_VAL(d, entry) \
    if ((d)->type->vfree) \
        (d)->type->vfree((entry)->v.val)

#define DICT_SET_VAL(d, entry, _val_) do { \
    if ((d)->type->vdup) \
        (entry)->v.val = (d)->type->vdup(_val_); \
    else \
        (entry)->v.val = (_val_); \
} while(0)

#define DICT_SET_INT_VAL(entry, _val_) \
    do { (entry)->v.s64 = _val_; } while(0)

#define DICT_SET_UINT_VAL(entry, _val_) \
    do { (entry)->v.u64 = _val_; } while(0)

#define DICT_SET_DOUBLE_VAL(entry, _val_) \
    do { (entry)->v.d = _val_; } while(0)


#define DICT_FREE_KEY(d, entry) \
    if ((d)->type->kfree) \
        (d)->type->kfree((entry)->key)

#define DICT_SET_KEY(d, entry, _key_) do { \
    if ((d)->type->kdup) \
        (entry)->key = (d)->type->kdup(_key_); \
    else \
        (entry)->key = (_key_); \
} while(0)

#define DICT_COMP_KEYS(d, key1, key2) \
    (((d)->type->kcompare) ? \
        (d)->type->kcompare(key1, key2) : \
        (key1) == (key2))

#define DICT_HASH_KEY(d, key) (d)->type->hashmake(key)

#define DICT_SLOTS(d) ((d)->ht[0].size+(d)->ht[1].size)
#define DICT_IS_REHASHING(d) ((d)->rehashidx != -1)

#ifdef __cplusplus
extern "C" {
#endif

s_sis_dict *sis_dict_create(s_sis_dict_type *type, void *source);
void sis_dict_destroy(s_sis_dict *d);

int sis_dict_add(s_sis_dict *d, void *key, void *val);
int sis_dict_delete(s_sis_dict *d, const void *key);
void *sis_dict_fetch_value(s_sis_dict *d, const void *key);
s_sis_dict_entry * sis_dict_find(s_sis_dict *d, const void *key);
void sis_dict_empty(s_sis_dict *d, void(callback)(void*));

int sis_dict_replace(s_sis_dict *d, void *key, void *val);

#define sis_dict_getsize(d) ((d)->ht[0].used+(d)->ht[1].used)
#define sis_dict_getkey(he) ((he)->key)
#define sis_dict_getval(he) ((he)->v.val)

#define sis_dict_get_int(he) ((he)->v.s64)
#define sis_dict_get_uint(he) ((he)->v.u64)
#define sis_dict_get_double(he) ((he)->v.d)

int sis_dict_set_int(s_sis_dict *d, void *key, int64_t val);
int sis_dict_set_uint(s_sis_dict *d, void *key, uint64_t val);

s_sis_dict_iter *sis_dict_get_iter(s_sis_dict *d);
s_sis_dict_entry *sis_dict_next(s_sis_dict_iter *iter);
void sis_dict_iter_free(s_sis_dict_iter *iter);

// -------  //
uint64_t sis_dict_get_hash_func(const void *key, int len);
uint64_t sis_dict_get_casehash_func(const unsigned char *buf, int len);

int sis_dict_expand(s_sis_dict *d, unsigned long size);
s_sis_dict_entry *sis_dict_add_raw(s_sis_dict *d, void *key, s_sis_dict_entry **existing);
s_sis_dict_entry *sis_dict_add_or_find(s_sis_dict *d, void *key);
s_sis_dict_entry *sis_dict_unlink(s_sis_dict *ht, const void *key);
void sis_dict_unlink_free(s_sis_dict *d, s_sis_dict_entry *he);
int sis_dict_resize(s_sis_dict *d);
s_sis_dict_entry *sis_dict_get_random_key(s_sis_dict *d);
s_sis_dict_entry *sis_dict_get_fair_random_key(s_sis_dict *d);
unsigned int sis_dict_get_some_keys(s_sis_dict *d, s_sis_dict_entry **des, unsigned int count);
void sis_dict_enable_resize(void);
void sis_dict_disable_resize(void);
int sis_dict_rehash(s_sis_dict *d, int n);
uint64_t sis_dict_get_hash(s_sis_dict *d, const void *key);
void sis_dict_set_hash_function_seed(uint8_t *seed);
uint8_t *sis_dict_get_hash_function_seed(void);
unsigned long sis_dict_scan(s_sis_dict *d, unsigned long v, sis_dict_scan_function *cb_scan, sis_dict_scan_bucket_function *cb_scan_bucket, void *cb_source);
s_sis_dict_entry **sis_dict_findentry_refbyptr_and_hash(s_sis_dict *d, const void *oldptr, uint64_t hash);


#ifdef __cplusplus
}
#endif

#endif /* _SIS_DICT_H */
