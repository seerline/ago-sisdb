/* thank antirez redis of author */

#include "sis_dict.h"

/* Fast tolower() alike function that does not care about locale
 * but just returns a-z insetad of A-Z. */
static inline int _siptlw(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c+('a'-'A');
    } else {
        return c;
    }
}

/* Test of the CPU is Little Endian and supports not aligned accesses.
 * Two interesting conditions to speedup the function that happen to be
 * in most of x86 servers. */
#if defined(__X86_64__) || defined(__x86_64__) || defined (__i386__)
#define UNALIGNED_LE_CPU
#endif

#define ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))

#define U32TO8_LE(p, v)                                                        \
    (p)[0] = (uint8_t)((v));                                                   \
    (p)[1] = (uint8_t)((v) >> 8);                                              \
    (p)[2] = (uint8_t)((v) >> 16);                                             \
    (p)[3] = (uint8_t)((v) >> 24);

#define U64TO8_LE(p, v)                                                        \
    U32TO8_LE((p), (uint32_t)((v)));                                           \
    U32TO8_LE((p) + 4, (uint32_t)((v) >> 32));

#ifdef UNALIGNED_LE_CPU
#define U8TO64_LE(p) (*((uint64_t*)(p)))
#else
#define U8TO64_LE(p)                                                           \
    (((uint64_t)((p)[0])) | ((uint64_t)((p)[1]) << 8) |                        \
     ((uint64_t)((p)[2]) << 16) | ((uint64_t)((p)[3]) << 24) |                 \
     ((uint64_t)((p)[4]) << 32) | ((uint64_t)((p)[5]) << 40) |                 \
     ((uint64_t)((p)[6]) << 48) | ((uint64_t)((p)[7]) << 56))
#endif

#define U8TO64_LE_NOCASE(p)                                                    \
    (((uint64_t)(_siptlw((p)[0]))) |                                           \
     ((uint64_t)(_siptlw((p)[1])) << 8) |                                      \
     ((uint64_t)(_siptlw((p)[2])) << 16) |                                     \
     ((uint64_t)(_siptlw((p)[3])) << 24) |                                     \
     ((uint64_t)(_siptlw((p)[4])) << 32) |                                              \
     ((uint64_t)(_siptlw((p)[5])) << 40) |                                              \
     ((uint64_t)(_siptlw((p)[6])) << 48) |                                              \
     ((uint64_t)(_siptlw((p)[7])) << 56))

#define SIPROUND                                                               \
    do {                                                                       \
        v0 += v1;                                                              \
        v1 = ROTL(v1, 13);                                                     \
        v1 ^= v0;                                                              \
        v0 = ROTL(v0, 32);                                                     \
        v2 += v3;                                                              \
        v3 = ROTL(v3, 16);                                                     \
        v3 ^= v2;                                                              \
        v0 += v3;                                                              \
        v3 = ROTL(v3, 21);                                                     \
        v3 ^= v0;                                                              \
        v2 += v1;                                                              \
        v1 = ROTL(v1, 17);                                                     \
        v1 ^= v2;                                                              \
        v2 = ROTL(v2, 32);                                                     \
    } while (0)

uint64_t _siphash(const uint8_t *in, const size_t inlen, const uint8_t *k) {
#ifndef UNALIGNED_LE_CPU
    uint64_t hash;
    uint8_t *out = (uint8_t*) &hash;
#endif
    uint64_t v0 = 0x736f6d6570736575ULL;
    uint64_t v1 = 0x646f72616e646f6dULL;
    uint64_t v2 = 0x6c7967656e657261ULL;
    uint64_t v3 = 0x7465646279746573ULL;
    uint64_t k0 = U8TO64_LE(k);
    uint64_t k1 = U8TO64_LE(k + 8);
    uint64_t m;
    const uint8_t *end = in + inlen - (inlen % sizeof(uint64_t));
    const int left = inlen & 7;
    uint64_t b = ((uint64_t)inlen) << 56;
    v3 ^= k1;
    v2 ^= k0;
    v1 ^= k1;
    v0 ^= k0;

    for (; in != end; in += 8) {
        m = U8TO64_LE(in);
        v3 ^= m;

        SIPROUND;

        v0 ^= m;
    }

    switch (left) {
    case 7: b |= ((uint64_t)in[6]) << 48;
    case 6: b |= ((uint64_t)in[5]) << 40;
    case 5: b |= ((uint64_t)in[4]) << 32;
    case 4: b |= ((uint64_t)in[3]) << 24;
    case 3: b |= ((uint64_t)in[2]) << 16;
    case 2: b |= ((uint64_t)in[1]) << 8;
    case 1: b |= ((uint64_t)in[0]); break;
    case 0: break;
    }

    v3 ^= b;

    SIPROUND;

    v0 ^= b;
    v2 ^= 0xff;

    SIPROUND;
    SIPROUND;

    b = v0 ^ v1 ^ v2 ^ v3;
#ifndef UNALIGNED_LE_CPU
    U64TO8_LE(out, b);
    return hash;
#else
    return b;
#endif
}

uint64_t _siphash_nocase(const uint8_t *in, const size_t inlen, const uint8_t *k)
{
#ifndef UNALIGNED_LE_CPU
    uint64_t hash;
    uint8_t *out = (uint8_t*) &hash;
#endif
    uint64_t v0 = 0x736f6d6570736575ULL;
    uint64_t v1 = 0x646f72616e646f6dULL;
    uint64_t v2 = 0x6c7967656e657261ULL;
    uint64_t v3 = 0x7465646279746573ULL;
    uint64_t k0 = U8TO64_LE(k);
    uint64_t k1 = U8TO64_LE(k + 8);
    uint64_t m;
    const uint8_t *end = in + inlen - (inlen % sizeof(uint64_t));
    const int left = inlen & 7;
    uint64_t b = ((uint64_t)inlen) << 56;
    v3 ^= k1;
    v2 ^= k0;
    v1 ^= k1;
    v0 ^= k0;

    for (; in != end; in += 8) {
        m = U8TO64_LE_NOCASE(in);
        v3 ^= m;

        SIPROUND;

        v0 ^= m;
    }

    switch (left) {
    case 7: b |= ((uint64_t)_siptlw(in[6])) << 48;
    case 6: b |= ((uint64_t)_siptlw(in[5])) << 40;
    case 5: b |= ((uint64_t)_siptlw(in[4])) << 32;
    case 4: b |= ((uint64_t)_siptlw(in[3])) << 24;
    case 3: b |= ((uint64_t)_siptlw(in[2])) << 16;
    case 2: b |= ((uint64_t)_siptlw(in[1])) << 8;
    case 1: b |= ((uint64_t)_siptlw(in[0])); break;
    case 0: break;
    }

    v3 ^= b;

    SIPROUND;

    v0 ^= b;
    v2 ^= 0xff;

    SIPROUND;
    SIPROUND;

    b = v0 ^ v1 ^ v2 ^ v3;
#ifndef UNALIGNED_LE_CPU
    U64TO8_LE(out, b);
    return hash;
#else
    return b;
#endif
}

///////////////////
//
//////////////////


/* Using sis_dict_enable_resize() / sis_dict_disable_resize() we make possible to
 * enable/disable resizing of the hash table as needed. This is very important
 * for Redis, as we use copy-on-write and don't want to move too much memory
 * around when there is a child performing saving operations.
 *
 * Note that even when _dict_can_resize is set to 0, not all resizes are
 * prevented: a hash table is still allowed to grow if the ratio between
 * the number of elements and the buckets > SIS_DICT_RESZIE_RATIO. */

static int _dict_can_resize = 1;

#define SIS_DICT_RESZIE_RATIO    5

/* -------------------------- private prototypes ---------------------------- */

static int _dict_expand_if_needed(s_sis_dict *ht);
static unsigned long _dict_next_power(unsigned long size);
static long _dict_key_index(s_sis_dict *ht, const void *key, uint64_t hash, s_sis_dict_entry **existing);
static int _dict_init(s_sis_dict *ht, s_sis_dict_type *type, void *source);

/* -------------------------- hash functions -------------------------------- */

static uint8_t _dict_hash_function_seed[16];

void sis_dict_set_hash_function_seed(uint8_t *seed) {
    memcpy(_dict_hash_function_seed,seed,sizeof(_dict_hash_function_seed));
}

uint8_t *sis_dict_get_hash_function_seed(void) {
    return _dict_hash_function_seed;
}

uint64_t sis_dict_get_hash_func(const void *key, int len) {
    return _siphash(key,len,_dict_hash_function_seed);
}

uint64_t sis_dict_get_casehash_func(const unsigned char *buf, int len) {
    return _siphash_nocase(buf,len,_dict_hash_function_seed);
}
/* ----------------------------- API implementation ------------------------- */

/* Reset a hash table already initialized with ht_init().
 * NOTE: This function should only be called by ht_destroy(). */
static void _dict_reset(s_sis_dictht *ht)
{
    ht->table = NULL;
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
}

/* Create a new hash table */
s_sis_dict *sis_dict_create(s_sis_dict_type *type,
        void *source)
{
	s_sis_dict *d = (s_sis_dict *)sis_malloc(sizeof(*d));

    _dict_init(d,type,source);
    return d;
}

/* Initialize the hash table */
int _dict_init(s_sis_dict *d, s_sis_dict_type *type,
        void *source)
{
    _dict_reset(&d->ht[0]);
    _dict_reset(&d->ht[1]);
    d->type = type;
    d->source = source;
    d->rehashidx = -1;
    d->iterators = 0;
    return SIS_DICT_OK;
}

/* Resize the table to the minimal size that contains all the elements,
 * but with the invariant of a USED/BUCKETS ratio near to <= 1 */
int sis_dict_resize(s_sis_dict *d)
{
    unsigned long minimal;

    if (!_dict_can_resize || DICT_IS_REHASHING(d)) return SIS_DICT_ERR;
    minimal = d->ht[0].used;
    if (minimal < DICT_HT_INITIAL_SIZE)
        minimal = DICT_HT_INITIAL_SIZE;
    return sis_dict_expand(d, minimal);
}

/* Expand or create the hash table */
int sis_dict_expand(s_sis_dict *d, unsigned long size)
{
    /* the size is invalid if it is smaller than the number of
     * elements already inside the hash table */
    if (DICT_IS_REHASHING(d) || d->ht[0].used > size)
        return SIS_DICT_ERR;

    s_sis_dictht n; /* the new hash table */
    unsigned long realsize = _dict_next_power(size);
    
    /* Rehashing to the same table size is not useful. */
    if (realsize == d->ht[0].size) return SIS_DICT_ERR;

    /* Allocate the new hash table and initialize all pointers to NULL */
    n.size = realsize;
    n.sizemask = realsize-1;
    n.table = (s_sis_dict_entry **)sis_calloc(realsize*sizeof(s_sis_dict_entry*));
    n.used = 0;

    /* Is this the first initialization? If so it's not really a rehashing
     * we just set the first hash table so that it can accept keys. */
    if (d->ht[0].table == NULL) {
        d->ht[0] = n;
        return SIS_DICT_OK;
    }

    /* Prepare a second hash table for incremental rehashing */
    d->ht[1] = n;
    d->rehashidx = 0;
    return SIS_DICT_OK;
}

/* Performs N steps of incremental rehashing. Returns 1 if there are still
 * keys to move from the old to the new hash table, otherwise 0 is returned.
 *
 * Note that a rehashing step consists in moving a bucket (that may have more
 * than one key as we use chaining) from the old to the new hash table, however
 * since part of the hash table may be composed of empty spaces, it is not
 * guaranteed that this function will rehash even a single bucket, since it
 * will visit at max N*10 empty buckets in total, otherwise the amount of
 * work it does would be unbound and the function may block for a long time. */
int sis_dict_rehash(s_sis_dict *d, int n) {
    int empty_visits = n*10; /* Max number of empty buckets to visit. */
    if (!DICT_IS_REHASHING(d)) return 0;

    while(n-- && d->ht[0].used != 0) {
        s_sis_dict_entry *de, *nextde;

        /* Note that rehashidx can't overflow as we are sure there are more
         * elements because ht[0].used != 0 */
        assert(d->ht[0].size > (unsigned long)d->rehashidx);
        while(d->ht[0].table[d->rehashidx] == NULL) {
            d->rehashidx++;
            if (--empty_visits == 0) return 1;
        }
        de = d->ht[0].table[d->rehashidx];
        /* Move all the keys in this bucket from the old to the new hash HT */
        while(de) {
            uint64_t h;

            nextde = de->next;
            /* Get the index in the new hash table */
            h = DICT_HASH_KEY(d, de->key) & d->ht[1].sizemask;
            de->next = d->ht[1].table[h];
            d->ht[1].table[h] = de;
            d->ht[0].used--;
            d->ht[1].used++;
            de = nextde;
        }
        d->ht[0].table[d->rehashidx] = NULL;
        d->rehashidx++;
    }

    /* Check if we already rehashed the whole table... */
    if (d->ht[0].used == 0) {
        sis_free(d->ht[0].table);
        d->ht[0] = d->ht[1];
        _dict_reset(&d->ht[1]);
        d->rehashidx = -1;
        return 0;
    }

    /* More to rehash... */
    return 1;
}

/* This function performs just a step of rehashing, and only if there are
 * no safe iterators bound to our hash table. When we have iterators in the
 * middle of a rehashing we can't mess with the two hash tables otherwise
 * some element can be missed or duplicated.
 *
 * This function is called by common lookup or update operations in the
 * dictionary so that the hash table automatically migrates from H1 to H2
 * while it is actively used. */
static void _dict_rehash_step(s_sis_dict *d) {
    if (d->iterators == 0) sis_dict_rehash(d,1);
}

/* Add an element to the target hash table */
int sis_dict_add(s_sis_dict *d, void *key, void *val)
{
    s_sis_dict_entry *entry = sis_dict_add_raw(d,key,NULL);

    if (!entry) return SIS_DICT_ERR;
    DICT_SET_VAL(d, entry, val);
    return SIS_DICT_OK;
}

/* Low level add or find:
 * This function adds the entry but instead of setting a value returns the
 * s_sis_dict_entry structure to the user, that will make sure to fill the value
 * field as he wishes.
 *
 * This function is also directly exposed to the user API to be called
 * mainly in order to store non-pointers inside the hash value, example:
 *
 * entry = sis_dict_add_raw(s_sis_dict,mykey,NULL);
 * if (entry != NULL) DICT_SET_INT_VAL(entry,1000);
 *
 * Return values:
 *
 * If key already exists NULL is returned, and "*existing" is populated
 * with the existing entry if existing is not NULL.
 *
 * If key was added, the hash entry is returned to be manipulated by the caller.
 */
s_sis_dict_entry *sis_dict_add_raw(s_sis_dict *d, void *key, s_sis_dict_entry **existing)
{
    long index;
    s_sis_dict_entry *entry;
    s_sis_dictht *ht;

    if (DICT_IS_REHASHING(d)) _dict_rehash_step(d);

    /* Get the index of the new element, or -1 if
     * the element already exists. */
    if ((index = _dict_key_index(d, key, DICT_HASH_KEY(d,key), existing)) == -1)
        return NULL;

    /* Allocate the memory and store the new entry.
     * Insert the element in top, with the assumption that in a database
     * system it is more likely that recently added entries are accessed
     * more frequently. */
    ht = DICT_IS_REHASHING(d) ? &d->ht[1] : &d->ht[0];
    entry = sis_calloc(sizeof(*entry));
    entry->next = ht->table[index];
    ht->table[index] = entry;
    ht->used++;

    /* Set the hash entry fields. */
    DICT_SET_KEY(d, entry, key);
    return entry;
}

/* Add or Overwrite:
 * Add an element, discarding the old value if the key already exists.
 * Return 1 if the key was added from scratch, 0 if there was already an
 * element with such key and sis_dict_replace() just performed a value update
 * operation. */
int sis_dict_replace(s_sis_dict *d, void *key, void *val)
{
    s_sis_dict_entry *entry, *existing, auxentry;

    /* Try to add the element. If the key
     * does not exists sis_dict_add will succeed. */
    entry = sis_dict_add_raw(d,key,&existing);
    if (entry) {
        DICT_SET_VAL(d, entry, val);
        return 1;
    }

    /* Set the new value and free the old one. Note that it is important
     * to do that in this order, as the value may just be exactly the same
     * as the previous one. In this context, think to reference counting,
     * you want to increment (set), and then decrement (free), and not the
     * reverse. */
    auxentry = *existing;
    DICT_SET_VAL(d, existing, val);
    DICT_FREE_VAL(d, &auxentry);
    return 0;
}
int sis_dict_set_int(s_sis_dict *d, void *key, int64_t val)
{
    s_sis_dict_entry *entry, *existing;

    entry = sis_dict_add_raw(d,key,&existing);
    if (entry) {
        DICT_SET_INT_VAL(entry, val);
        return 1;
    }    
    DICT_SET_INT_VAL(existing, val);
    return 0;
}
int sis_dict_set_uint(s_sis_dict *d, void *key, uint64_t val)
{
    s_sis_dict_entry *entry, *existing;

    entry = sis_dict_add_raw(d,key,&existing);
    if (entry) {
        DICT_SET_UINT_VAL(entry, val);
        return 1;
    }    
    DICT_SET_UINT_VAL(existing, val);
    return 0;
}
/* Add or Find:
 * sis_dict_add_or_find() is simply a version of sis_dict_add_raw() that always
 * returns the hash entry of the specified key, even if the key already
 * exists and can't be added (in that case the entry of the already
 * existing key is returned.)
 *
 * See sis_dict_add_raw() for more information. */
s_sis_dict_entry *sis_dict_add_or_find(s_sis_dict *d, void *key) {
    s_sis_dict_entry *entry, *existing;
    entry = sis_dict_add_raw(d,key,&existing);
    return entry ? entry : existing;
}

/* Search and remove an element. This is an helper function for
 * sis_dict_delete() and sis_dict_unlink(), please check the top comment
 * of those functions. */
static s_sis_dict_entry *_dict_generic_delete(s_sis_dict *d, const void *key, int nofree) {
    uint64_t h, idx;
    s_sis_dict_entry *he, *prevHe;
    int table;

    if (d->ht[0].used == 0 && d->ht[1].used == 0) return NULL;

    if (DICT_IS_REHASHING(d)) _dict_rehash_step(d);
    h = DICT_HASH_KEY(d, key);

    for (table = 0; table <= 1; table++) {
        idx = h & d->ht[table].sizemask;
        he = d->ht[table].table[idx];
        prevHe = NULL;
        while(he) {
            if (key==he->key || DICT_COMP_KEYS(d, key, he->key)) {
                /* Unlink the element from the list */
                if (prevHe)
                    prevHe->next = he->next;
                else
                    d->ht[table].table[idx] = he->next;
                if (!nofree) {
                    DICT_FREE_KEY(d, he);
                    DICT_FREE_VAL(d, he);
                    sis_free(he);
                }
                d->ht[table].used--;
                return he;
            }
            prevHe = he;
            he = he->next;
        }
        if (!DICT_IS_REHASHING(d)) break;
    }
    return NULL; /* not found */
}

/* Remove an element, returning SIS_DICT_OK on success or SIS_DICT_ERR if the
 * element was not found. */
int sis_dict_delete(s_sis_dict *ht, const void *key) {
    return _dict_generic_delete(ht,key,0) ? SIS_DICT_OK : SIS_DICT_ERR;
}

/* Remove an element from the table, but without actually releasing
 * the key, value and dictionary entry. The dictionary entry is returned
 * if the element was found (and unlinked from the table), and the user
 * should later call `sis_dict_unlink_free()` with it in order to release it.
 * Otherwise if the key is not found, NULL is returned.
 *
 * This function is useful when we want to remove something from the hash
 * table but want to use its value before actually deleting the entry.
 * Without this function the pattern would require two lookups:
 *
 *  entry = sis_dict_find(...);
 *  // Do something with entry
 *  sis_dict_delete(dictionary,entry);
 *
 * Thanks to this function it is possible to avoid this, and use
 * instead:
 *
 * entry = sis_dict_unlink(dictionary,entry);
 * // Do something with entry
 * sis_dict_unlink_free(entry); // <- This does not need to lookup again.
 */
s_sis_dict_entry *sis_dict_unlink(s_sis_dict *ht, const void *key) {
    return _dict_generic_delete(ht,key,1);
}

/* You need to call this function to really free the entry after a call
 * to sis_dict_unlink(). It's safe to call this function with 'he' = NULL. */
void sis_dict_unlink_free(s_sis_dict *d, s_sis_dict_entry *he) {
    if (he == NULL) return;
    DICT_FREE_KEY(d, he);
    DICT_FREE_VAL(d, he);
    sis_free(he);
}

/* Destroy an entire dictionary */
int _dict_clear(s_sis_dict *d, s_sis_dictht *ht, void(callback)(void *)) {
    unsigned long i;

    /* Free all the elements */
    for (i = 0; i < ht->size && ht->used > 0; i++) {
        s_sis_dict_entry *he, *nextHe;

        if (callback && (i & 65535) == 0) callback(d->source);

        if ((he = ht->table[i]) == NULL) continue;
        while(he) {
            nextHe = he->next;
            DICT_FREE_KEY(d, he);
            DICT_FREE_VAL(d, he);
            sis_free(he);
            ht->used--;
            he = nextHe;
        }
    }
    /* Free the table and the allocated cache structure */
    sis_free(ht->table);
    /* Re-initialize the table */
    _dict_reset(ht);
    return SIS_DICT_OK; /* never fails */
}

/* Clear & Release the hash table */
void sis_dict_destroy(s_sis_dict *d)
{
    _dict_clear(d,&d->ht[0],NULL);
    _dict_clear(d,&d->ht[1],NULL);
    sis_free(d);
}

s_sis_dict_entry *sis_dict_find(s_sis_dict *d, const void *key)
{
    s_sis_dict_entry *he;
    uint64_t h, idx, table;

    if (sis_dict_getsize(d) == 0) return NULL; /* s_sis_dict is empty */
    if (DICT_IS_REHASHING(d)) _dict_rehash_step(d);
    h = DICT_HASH_KEY(d, key);
    for (table = 0; table <= 1; table++) {
        idx = h & d->ht[table].sizemask;
        he = d->ht[table].table[idx];
        while(he) {
            if (key==he->key || DICT_COMP_KEYS(d, key, he->key))
                return he;
            he = he->next;
        }
        if (!DICT_IS_REHASHING(d)) return NULL;
    }
    return NULL;
}

void *sis_dict_fetch_value(s_sis_dict *d, const void *key) {
    s_sis_dict_entry *he;

    he = sis_dict_find(d,key);
    return he ? sis_dict_getval(he) : NULL;
}

/* A fingerprint is a 64 bit number that represents the state of the dictionary
 * at a given time, it's just a few s_sis_dict properties xored together.
 * When an unsafe iterator is initialized, we get the s_sis_dict fingerprint, and check
 * the fingerprint again when the iterator is released.
 * If the two fingerprints are different it means that the user of the iterator
 * performed forbidden operations against the dictionary while iterating. */
long long _dict_finger_print(s_sis_dict *d) {
    long long integers[6], hash = 0;
    int j;

    integers[0] = (long) d->ht[0].table;
    integers[1] = d->ht[0].size;
    integers[2] = d->ht[0].used;
    integers[3] = (long) d->ht[1].table;
    integers[4] = d->ht[1].size;
    integers[5] = d->ht[1].used;

    /* We hash N integers by summing every successive integer with the integer
     * hashing of the previous sum. Basically:
     *
     * Result = hash(hash(hash(int1)+int2)+int3) ...
     *
     * This way the same set of integers in a different order will (likely) hash
     * to a different number. */
    for (j = 0; j < 6; j++) {
        hash += integers[j];
        /* For the hashing step we use Tomas Wang's 64 bit integer hash. */
        hash = (~hash) + (hash << 21); // hash = (hash << 21) - hash - 1;
        hash = hash ^ (hash >> 24);
        hash = (hash + (hash << 3)) + (hash << 8); // hash * 265
        hash = hash ^ (hash >> 14);
        hash = (hash + (hash << 2)) + (hash << 4); // hash * 21
        hash = hash ^ (hash >> 28);
        hash = hash + (hash << 31);
    }
    return hash;
}

s_sis_dict_iter *_dict_get_iter(s_sis_dict *d)
{
    s_sis_dict_iter *iter = sis_malloc(sizeof(*iter));

    iter->d = d;
    iter->table = 0;
    iter->index = -1;
    iter->safe = 0;
    iter->entry = NULL;
    iter->nextEntry = NULL;
    iter->fingerprint = 0;
    return iter;
}

s_sis_dict_iter *sis_dict_get_iter(s_sis_dict *d) {
    s_sis_dict_iter *i = _dict_get_iter(d);
    i->safe = 1;
    return i;
}

s_sis_dict_entry *sis_dict_next(s_sis_dict_iter *iter)
{
    while (1) {
        if (iter->entry == NULL) {
            s_sis_dictht *ht = &iter->d->ht[iter->table];
            if (iter->index == -1 && iter->table == 0) {
                if (iter->safe)
                  iter->d->iterators++;
                else
                  iter->fingerprint = _dict_finger_print(iter->d);
            }
            iter->index++;
            if (iter->index >= (long) ht->size) {
                if (DICT_IS_REHASHING(iter->d) && iter->table == 0) {
                    iter->table++;
                    iter->index = 0;
                    ht = &iter->d->ht[1];
                } else {
                    break;
                }
            }
            iter->entry = ht->table[iter->index];
        } else {
            iter->entry = iter->nextEntry;
        }
        if (iter->entry) {
            /* We need to save the 'next' here, the iterator user
             * may delete the entry we are returning. */
            iter->nextEntry = iter->entry->next;
            return iter->entry;
        }
    }
    return NULL;
}

void sis_dict_iter_free(s_sis_dict_iter *iter)
{
    if (!(iter->index == -1 && iter->table == 0)) {
        if (iter->safe)
            iter->d->iterators--;
        else
            assert(iter->fingerprint == _dict_finger_print(iter->d));
    }
    sis_free(iter);
}

/* Return a random entry from the hash table. Useful to
 * implement randomized algorithms */
s_sis_dict_entry *sis_dict_get_random_key(s_sis_dict *d)
{
    s_sis_dict_entry *he, *orighe;
    unsigned long h;
    int listlen, listele;

    if (sis_dict_getsize(d) == 0) return NULL;
    if (DICT_IS_REHASHING(d)) _dict_rehash_step(d);
	if (DICT_IS_REHASHING(d)) {
        do {
            /* We are sure there are no elements in indexes from 0
             * to rehashidx-1 */
            h = d->rehashidx + (random() % (DICT_SLOTS(d) - d->rehashidx));
            he = (h >= d->ht[0].size) ? d->ht[1].table[h - d->ht[0].size] :
                                      d->ht[0].table[h];
        } while(he == NULL);
    } else {
        do {
            h = random() & d->ht[0].sizemask;
            he = d->ht[0].table[h];
        } while(he == NULL);
    }

    /* Now we found a non empty bucket, but it is a linked
     * list and we need to get a random element from the list.
     * The only sane way to do so is counting the elements and
     * select a random index. */
    listlen = 0;
    orighe = he;
    while(he) {
        he = he->next;
        listlen++;
    }
    listele = random() % listlen;
    he = orighe;
    while(listele--) he = he->next;
    return he;
}

/* This function samples the dictionary to return a few keys from random
 * locations.
 *
 * It does not guarantee to return all the keys specified in 'count', nor
 * it does guarantee to return non-duplicated elements, however it will make
 * some effort to do both things.
 *
 * Returned pointers to hash table entries are stored into 'des' that
 * points to an array of s_sis_dict_entry pointers. The array must have room for
 * at least 'count' elements, that is the argument we pass to the function
 * to tell how many random elements we need.
 *
 * The function returns the number of items stored into 'des', that may
 * be less than 'count' if the hash table has less than 'count' elements
 * inside, or if not enough elements were found in a reasonable amount of
 * steps.
 *
 * Note that this function is not suitable when you need a good distribution
 * of the returned items, but only when you need to "sample" a given number
 * of continuous elements to run some kind of algorithm or to produce
 * statistics. However the function is much faster than sis_dict_get_random_key()
 * at producing N elements. */
unsigned int sis_dict_get_some_keys(s_sis_dict *d, s_sis_dict_entry **des, unsigned int count) {
    unsigned long j; /* internal hash table id, 0 or 1. */
    unsigned long tables; /* 1 or 2 tables? */
    unsigned long stored = 0, maxsizemask;
    unsigned long maxsteps;

    if (sis_dict_getsize(d) < count) count = sis_dict_getsize(d);
    maxsteps = count*10;

    /* Try to do a rehashing work proportional to 'count'. */
    for (j = 0; j < count; j++) {
        if (DICT_IS_REHASHING(d))
            _dict_rehash_step(d);
        else
            break;
    }

    tables = DICT_IS_REHASHING(d) ? 2 : 1;
    maxsizemask = d->ht[0].sizemask;
    if (tables > 1 && maxsizemask < d->ht[1].sizemask)
        maxsizemask = d->ht[1].sizemask;

    /* Pick a random point inside the larger table. */
    unsigned long i = random() & maxsizemask;
    unsigned long emptylen = 0; /* Continuous empty entries so far. */
    while(stored < count && maxsteps--) {
        for (j = 0; j < tables; j++) {
            /* Invariant of the s_sis_dict.c rehashing: up to the indexes already
             * visited in ht[0] during the rehashing, there are no populated
             * buckets, so we can skip ht[0] for indexes between 0 and idx-1. */
            if (tables == 2 && j == 0 && i < (unsigned long) d->rehashidx) {
                /* Moreover, if we are currently out of range in the second
                 * table, there will be no elements in both tables up to
                 * the current rehashing index, so we jump if possible.
                 * (this happens when going from big to small table). */
                if (i >= d->ht[1].size) 
                  i = d->rehashidx;  
                else
                    continue;
            }
            if (i >= d->ht[j].size) continue; /* Out of range for this table. */
            s_sis_dict_entry *he = d->ht[j].table[i];

            /* Count contiguous empty buckets, and jump to other
             * locations if they reach 'count' (with a minimum of 5). */
            if (he == NULL) {
                emptylen++;
                if (emptylen >= 5 && emptylen > count) {
                    i = random() & maxsizemask;
                    emptylen = 0;
                }
            } else {
                emptylen = 0;
                while (he) {
                    /* Collect all the elements of the buckets found non
                     * empty while iterating. */
                    *des = he;
                    des++;
                    he = he->next;
                    stored++;
                    if (stored == count) return stored;
                }
            }
        }
        i = (i+1) & maxsizemask;
    }
    return stored;
}

/* This is like sis_dict_get_random_key() from the POV of the API, but will do more
 * work to ensure a better distribution of the returned element.
 *
 * This function improves the distribution because the sis_dict_get_random_key()
 * problem is that it selects a random bucket, then it selects a random
 * element from the chain in the bucket. However elements being in different
 * chain lengths will have different probabilities of being reported. With
 * this function instead what we do is to consider a "linear" range of the table
 * that may be constituted of N buckets with chains of different lengths
 * appearing one after the other. Then we report a random element in the range.
 * In this way we smooth away the problem of different chain lenghts. */
#define GETFAIR_NUM_ENTRIES 15
s_sis_dict_entry *sis_dict_get_fair_random_key(s_sis_dict *d) {
    s_sis_dict_entry *entries[GETFAIR_NUM_ENTRIES];
    unsigned int count = sis_dict_get_some_keys(d,entries,GETFAIR_NUM_ENTRIES);
    /* Note that sis_dict_get_some_keys() may return zero elements in an unlucky
     * run() even if there are actually elements inside the hash table. So
     * when we get zero, we call the true sis_dict_get_random_key() that will always
     * yeld the element if the hash table has at least one. */
    if (count == 0) return sis_dict_get_random_key(d);
    unsigned int idx = rand() % count;
    return entries[idx];
}

/* Function to reverse bits. Algorithm from:
 * http://graphics.stanford.edu/~seander/bithacks.html#ReverseParallel */
static unsigned long _dict_reverse(unsigned long v) {
    unsigned long s = CHAR_BIT * sizeof(v); // bit size; must be power of 2
    unsigned long mask = ~0UL;
    while ((s >>= 1) > 0) {
        mask ^= (mask << s);
        v = ((v >> s) & mask) | ((v << s) & ~mask);
    }
    return v;
}

/* sis_dict_scan() is used to iterate over the elements of a dictionary.
 *
 * Iterating works the following way:
 *
 * 1) Initially you call the function using a cursor (v) value of 0.
 * 2) The function performs one step of the iteration, and returns the
 *    new cursor value you must use in the next call.
 * 3) When the returned cursor is 0, the iteration is complete.
 *
 * The function guarantees all elements present in the
 * dictionary get returned between the start and end of the iteration.
 * However it is possible some elements get returned multiple times.
 *
 * For every element returned, the callback argument 'fn' is
 * called with 'privdata' as first argument and the dictionary entry
 * 'de' as second argument.
 *
 * HOW IT WORKS.
 *
 * The iteration algorithm was designed by Pieter Noordhuis.
 * The main idea is to increment a cursor starting from the higher order
 * bits. That is, instead of incrementing the cursor normally, the bits
 * of the cursor are reversed, then the cursor is incremented, and finally
 * the bits are reversed again.
 *
 * This strategy is needed because the hash table may be resized between
 * iteration calls.
 *
 * s_sis_dict.c hash tables are always power of two in size, and they
 * use chaining, so the position of an element in a given table is given
 * by computing the bitwise AND between Hash(key) and SIZE-1
 * (where SIZE-1 is always the mask that is equivalent to taking the rest
 *  of the division between the Hash of the key and SIZE).
 *
 * For example if the current hash table size is 16, the mask is
 * (in binary) 1111. The position of a key in the hash table will always be
 * the last four bits of the hash output, and so forth.
 *
 * WHAT HAPPENS IF THE TABLE CHANGES IN SIZE?
 *
 * If the hash table grows, elements can go anywhere in one multiple of
 * the old bucket: for example let's say we already iterated with
 * a 4 bit cursor 1100 (the mask is 1111 because hash table size = 16).
 *
 * If the hash table will be resized to 64 elements, then the new mask will
 * be 111111. The new buckets you obtain by substituting in ??1100
 * with either 0 or 1 can be targeted only by keys we already visited
 * when scanning the bucket 1100 in the smaller hash table.
 *
 * By iterating the higher bits first, because of the inverted counter, the
 * cursor does not need to restart if the table size gets bigger. It will
 * continue iterating using cursors without '1100' at the end, and also
 * without any other combination of the final 4 bits already explored.
 *
 * Similarly when the table size shrinks over time, for example going from
 * 16 to 8, if a combination of the lower three bits (the mask for size 8
 * is 111) were already completely explored, it would not be visited again
 * because we are sure we tried, for example, both 0111 and 1111 (all the
 * variations of the higher bit) so we don't need to test it again.
 *
 * WAIT... YOU HAVE *TWO* TABLES DURING REHASHING!
 *
 * Yes, this is true, but we always iterate the smaller table first, then
 * we test all the expansions of the current cursor into the larger
 * table. For example if the current cursor is 101 and we also have a
 * larger table of size 16, we also test (0)101 and (1)101 inside the larger
 * table. This reduces the problem back to having only one table, where
 * the larger one, if it exists, is just an expansion of the smaller one.
 *
 * LIMITATIONS
 *
 * This iterator is completely stateless, and this is a huge advantage,
 * including no additional memory used.
 *
 * The disadvantages resulting from this design are:
 *
 * 1) It is possible we return elements more than once. However this is usually
 *    easy to deal with in the application level.
 * 2) The iterator must return multiple elements per call, as it needs to always
 *    return all the keys chained in a given bucket, and all the expansions, so
 *    we are sure we don't miss keys moving during rehashing.
 * 3) The reverse cursor is somewhat hard to understand at first, but this
 *    comment is supposed to help.
 */
unsigned long sis_dict_scan(s_sis_dict *d,
                       unsigned long v,
                       sis_dict_scan_function *cb_scan,
                       sis_dict_scan_bucket_function* cb_scan_bucket,
                       void *cb_source)
{
    s_sis_dictht *t0, *t1;
    const s_sis_dict_entry *de, *next;
    unsigned long m0, m1;

    if (sis_dict_getsize(d) == 0) return 0;

    /* Having a safe iterator means no rehashing can happen, see _dict_rehash_step.
     * This is needed in case the scan callback tries to do sis_dict_find or alike. */
    d->iterators++;

    if (!DICT_IS_REHASHING(d)) {
        t0 = &(d->ht[0]);
        m0 = t0->sizemask;

        /* Emit entries at cursor */
        if (cb_scan_bucket) cb_scan_bucket(cb_source, &t0->table[v & m0]);
        de = t0->table[v & m0];
        while (de) {
            next = de->next;
            cb_scan(cb_source, de);
            de = next;
        }

        /* Set unmasked bits so incrementing the reversed cursor
         * operates on the masked bits */
        v |= ~m0;

        /* Increment the reverse cursor */
        v = _dict_reverse(v);
        v++;
        v = _dict_reverse(v);

    } else {
        t0 = &d->ht[0];
        t1 = &d->ht[1];

        /* Make sure t0 is the smaller and t1 is the bigger table */
        if (t0->size > t1->size) {
            t0 = &d->ht[1];
            t1 = &d->ht[0];
        }

        m0 = t0->sizemask;
        m1 = t1->sizemask;

        /* Emit entries at cursor */
        if (cb_scan_bucket) cb_scan_bucket(cb_source, &t0->table[v & m0]);
        de = t0->table[v & m0];
        while (de) {
            next = de->next;
            cb_scan(cb_source, de);
            de = next;
        }

        /* Iterate over indices in larger table that are the expansion
         * of the index pointed to by the cursor in the smaller table */
        do {
            /* Emit entries at cursor */
            if (cb_scan_bucket) cb_scan_bucket(cb_source, &t1->table[v & m1]);
            de = t1->table[v & m1];
            while (de) {
                next = de->next;
                cb_scan(cb_source, de);
                de = next;
            }

            /* Increment the reverse cursor not covered by the smaller mask.*/
            v |= ~m1;
            v = _dict_reverse(v);
            v++;
            v = _dict_reverse(v);

            /* Continue while bits covered by mask difference is non-zero */
        } while (v & (m0 ^ m1));
    }

    /* undo the ++ at the top */
    d->iterators--;

    return v;
}

/* ------------------------- private functions ------------------------------ */

/* Expand the hash table if needed */
static int _dict_expand_if_needed(s_sis_dict *d)
{
    /* Incremental rehashing already in progress. Return. */
    if (DICT_IS_REHASHING(d)) return SIS_DICT_OK;

    /* If the hash table is empty expand it to the initial size. */
    if (d->ht[0].size == 0) return sis_dict_expand(d, DICT_HT_INITIAL_SIZE);

    /* If we reached the 1:1 ratio, and we are allowed to resize the hash
     * table (global setting) or we should avoid it but the ratio between
     * elements/buckets is over the "safe" threshold, we resize doubling
     * the number of buckets. */
    if (d->ht[0].used >= d->ht[0].size &&
        (_dict_can_resize ||
         d->ht[0].used/d->ht[0].size > SIS_DICT_RESZIE_RATIO))
    {
        return sis_dict_expand(d, d->ht[0].used*2);
    }
    return SIS_DICT_OK;
}

/* Our hash table capability is a power of two */
static unsigned long _dict_next_power(unsigned long size)
{
    unsigned long i = DICT_HT_INITIAL_SIZE;

    if (size >= LONG_MAX) return LONG_MAX + 1LU;
    while(1) {
        if (i >= size)
           return i;
        i *= 2;
    }
}

/* Returns the index of a free slot that can be populated with
 * a hash entry for the given 'key'.
 * If the key already exists, -1 is returned
 * and the optional output parameter may be filled.
 *
 * Note that if we are in the process of rehashing the hash table, the
 * index is always returned in the context of the second (new) hash table. */
static long _dict_key_index(s_sis_dict *d, const void *key, uint64_t hash, s_sis_dict_entry **existing)
{
    unsigned long idx, table;
    s_sis_dict_entry *he;
    if (existing) *existing = NULL;

    /* Expand the hash table if needed */
    if (_dict_expand_if_needed(d) == SIS_DICT_ERR)
        return -1;
    for (table = 0; table <= 1; table++) {
        idx = hash & d->ht[table].sizemask;
        /* Search if this slot does not already contain the given key */
        he = d->ht[table].table[idx];
        while(he) {
            if (key==he->key || DICT_COMP_KEYS(d, key, he->key)) {
                if (existing) *existing = he;
                return -1;
            }
            he = he->next;
        }
        if (!DICT_IS_REHASHING(d)) break;
    }
    return idx;
}

void sis_dict_empty(s_sis_dict *d, void(callback)(void*)) {
    _dict_clear(d,&d->ht[0],callback);
    _dict_clear(d,&d->ht[1],callback);
    d->rehashidx = -1;
    d->iterators = 0;
}

void sis_dict_enable_resize(void) {
    _dict_can_resize = 1;
}

void sis_dict_disable_resize(void) {
    _dict_can_resize = 0;
}

uint64_t sis_dict_get_hash(s_sis_dict *d, const void *key) {
    return DICT_HASH_KEY(d, key);
}

/* Finds the s_sis_dict_entry reference by using pointer and pre-calculated hash.
 * oldkey is a dead pointer and should not be accessed.
 * the hash value should be provided using sis_dict_get_hash.
 * no string / key comparison is performed.
 * return value is the reference to the s_sis_dict_entry if found, or NULL if not found. */
s_sis_dict_entry **sis_dict_findentry_refbyptr_and_hash(s_sis_dict *d, const void *oldptr, uint64_t hash) {
    s_sis_dict_entry *he, **heref;
    unsigned long idx, table;

    if (sis_dict_getsize(d) == 0) return NULL; /* s_sis_dict is empty */
    for (table = 0; table <= 1; table++) {
        idx = hash & d->ht[table].sizemask;
        heref = &d->ht[table].table[idx];
        he = *heref;
        while(he) {
            if (oldptr==he->key)
                return heref;
            heref = &he->next;
            he = *heref;
        }
        if (!DICT_IS_REHASHING(d)) return NULL;
    }
    return NULL;
}

/* ------------------------------- Debugging ---------------------------------*/

#define DICT_STATS_VECTLEN 50
size_t _dict_get_stats_ht(char *buf, size_t bufsize, s_sis_dictht *ht, int tableid) {
    unsigned long i, slots = 0, chainlen, maxchainlen = 0;
    unsigned long totchainlen = 0;
    unsigned long clvector[DICT_STATS_VECTLEN];
    size_t l = 0;

    if (ht->used == 0) {
        return snprintf(buf,bufsize,
            "No stats available for empty dictionaries\n");
    }

    /* Compute stats. */
    for (i = 0; i < DICT_STATS_VECTLEN; i++) clvector[i] = 0;
    for (i = 0; i < ht->size; i++) {
        s_sis_dict_entry *he;

        if (ht->table[i] == NULL) {
            clvector[0]++;
            continue;
        }
        slots++;
        /* For each hash entry on this slot... */
        chainlen = 0;
        he = ht->table[i];
        while(he) {
            chainlen++;
            he = he->next;
        }
        clvector[(chainlen < DICT_STATS_VECTLEN) ? chainlen : (DICT_STATS_VECTLEN-1)]++;
        if (chainlen > maxchainlen) maxchainlen = chainlen;
        totchainlen += chainlen;
    }

    /* Generate human readable stats. */
    l += snprintf(buf+l,bufsize-l,
        "Hash table %d stats (%s):\n"
        " table size: %ld\n"
        " number of elements: %ld\n"
        " different slots: %ld\n"
        " max chain length: %ld\n"
        " avg chain length (counted): %.02f\n"
        " avg chain length (computed): %.02f\n"
        " Chain length distribution:\n",
        tableid, (tableid == 0) ? "main hash table" : "rehashing target",
        ht->size, ht->used, slots, maxchainlen,
        (float)totchainlen/slots, (float)ht->used/slots);

    for (i = 0; i < DICT_STATS_VECTLEN-1; i++) {
        if (clvector[i] == 0) continue;
        if (l >= bufsize) break;
        l += snprintf(buf+l,bufsize-l,
            "   %s%ld: %ld (%.02f%%)\n",
            (i == DICT_STATS_VECTLEN-1)?">= ":"",
            i, clvector[i], ((float)clvector[i]/ht->size)*100);
    }

    /* Unlike snprintf(), teturn the number of characters actually written. */
    if (bufsize) buf[bufsize-1] = '\0';
    return strlen(buf);
}

void _dict_get_stats(char *buf, size_t bufsize, s_sis_dict *d) {
    size_t l;
    char *orig_buf = buf;
    size_t orig_bufsize = bufsize;

    l = _dict_get_stats_ht(buf,bufsize,&d->ht[0],0);
    buf += l;
    bufsize -= l;
    if (DICT_IS_REHASHING(d) && bufsize > 0) {
        _dict_get_stats_ht(buf,bufsize,&d->ht[1],1);
    }
    /* Make sure there is a NULL term at the end. */
    if (orig_bufsize) orig_buf[orig_bufsize-1] = '\0';
}

