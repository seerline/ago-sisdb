
#ifndef _SIS_SDS_H
#define _SIS_SDS_H

// #define WARN_NOUSE_REALLOC 
// 尽量不要去用realloc，会发生未知的错误

#define SDS_MAX_PREALLOC (1024*1024)
// const char *SDS_NOINIT;

#include <sis_os.h>


typedef char * s_sis_sds;

#pragma pack(push,1)

typedef struct s_sis_sds_save
{
	s_sis_sds     current_v;    // 当前配置 优先级最高
	s_sis_sds     father_v;     // 上级信息
	s_sis_sds     default_v;    // 默认配置 优先级最低
} s_sis_sds_save;

/* Note: sdshdr5 is never used, we just access the flags byte directly.
 * However is here to document the layout of type 5 SDS strings. */
struct sdshdr5 {
    unsigned char flags; /* 3 lsb of type, and 5 msb of string length */
    char buf[];
};
struct sdshdr8 {
    uint8_t len; /* used */
    uint8_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct sdshdr16 {
    uint16_t len; /* used */
    uint16_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct sdshdr32{
    uint32_t len; /* used */
    uint32_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct sdshdr64 {
    uint64_t len; /* used */
    uint64_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
#pragma pack(pop)


#define SDS_TYPE_5  0
#define SDS_TYPE_8  1
#define SDS_TYPE_16 2
#define SDS_TYPE_32 3
#define SDS_TYPE_64 4
#define SDS_TYPE_MASK 7
#define SDS_TYPE_BITS 3
#define SDS_HDR_VAR(T,s) struct sdshdr##T *sh = (struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T)));
#define SDS_HDR(T,s) ((struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T))))
#define SDS_TYPE_5_LEN(f) ((f)>>SDS_TYPE_BITS)

#ifdef __cplusplus
extern "C" {
#endif

static inline size_t sis_sdslen(const s_sis_sds s) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            return SDS_TYPE_5_LEN(flags);
        case SDS_TYPE_8:
            return SDS_HDR(8,s)->len;
        case SDS_TYPE_16:
            return SDS_HDR(16,s)->len;
        case SDS_TYPE_32:
            return SDS_HDR(32,s)->len;
        case SDS_TYPE_64:
            return SDS_HDR(64,s)->len;
    }
    return 0;
}

static inline size_t sis_sdsavail(const s_sis_sds s) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5: {
            return 0;
        }
        case SDS_TYPE_8: {
            SDS_HDR_VAR(8,s);
            return sh->alloc - sh->len;
        }
        case SDS_TYPE_16: {
            SDS_HDR_VAR(16,s);
            return sh->alloc - sh->len;
        }
        case SDS_TYPE_32: {
            SDS_HDR_VAR(32,s);
            return sh->alloc - sh->len;
        }
        case SDS_TYPE_64: {
            SDS_HDR_VAR(64,s);
            return sh->alloc - sh->len;
        }
    }
    return 0;
}

static inline void sis_sdssetlen(s_sis_sds s, size_t newlen) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            {
                unsigned char *fp = ((unsigned char*)s)-1;
                *fp = SDS_TYPE_5 | (newlen << SDS_TYPE_BITS);
            }
            break;
        case SDS_TYPE_8:
            SDS_HDR(8,s)->len = newlen;
            break;
        case SDS_TYPE_16:
            SDS_HDR(16,s)->len = newlen;
            break;
        case SDS_TYPE_32:
            SDS_HDR(32,s)->len = newlen;
            break;
        case SDS_TYPE_64:
            SDS_HDR(64,s)->len = newlen;
            break;
    }
}

static inline void sis_sdsinclen(s_sis_sds s, size_t inc) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            {
                unsigned char *fp = ((unsigned char*)s)-1;
                unsigned char newlen = SDS_TYPE_5_LEN(flags)+inc;
                *fp = SDS_TYPE_5 | (newlen << SDS_TYPE_BITS);
            }
            break;
        case SDS_TYPE_8:
            SDS_HDR(8,s)->len += inc;
            break;
        case SDS_TYPE_16:
            SDS_HDR(16,s)->len += inc;
            break;
        case SDS_TYPE_32:
            SDS_HDR(32,s)->len += inc;
            break;
        case SDS_TYPE_64:
            SDS_HDR(64,s)->len += inc;
            break;
    }
}

/* sis_sdsalloc() = sis_sdsavail() + sis_sdslen() */
static inline size_t sis_sdsalloc(const s_sis_sds s) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            return SDS_TYPE_5_LEN(flags);
        case SDS_TYPE_8:
            return SDS_HDR(8,s)->alloc;
        case SDS_TYPE_16:
            return SDS_HDR(16,s)->alloc;
        case SDS_TYPE_32:
            return SDS_HDR(32,s)->alloc;
        case SDS_TYPE_64:
            return SDS_HDR(64,s)->alloc;
    }
    return 0;
}

static inline void sis_sdssetalloc(s_sis_sds s, size_t newlen) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            /* Nothing to do, this type has no total allocation info. */
            break;
        case SDS_TYPE_8:
            SDS_HDR(8,s)->alloc = newlen;
            break;
        case SDS_TYPE_16:
            SDS_HDR(16,s)->alloc = newlen;
            break;
        case SDS_TYPE_32:
            SDS_HDR(32,s)->alloc = newlen;
            break;
        case SDS_TYPE_64:
            SDS_HDR(64,s)->alloc = newlen;
            break;
    }
}

#define SIS_SDS_SIZE(s) (s ? sis_sdslen(s) : 0)

s_sis_sds sis_sdsnewlen(const void *init, size_t initlen);
s_sis_sds sis_sdsnew(const char *init);
s_sis_sds sis_sdsempty(void);
s_sis_sds sis_sdsdup(const s_sis_sds s);
void *sis_sdspos(void *);
void sis_sdsfree(s_sis_sds s);

/* #define  sis_sdsfree(s)   { \
//     if (s) {  printf("%s %lld %p\n", __func__, (int64)sis_thread_self(), s);\
//     sis_free(sis_sdspos(s)); }}
*/
s_sis_sds sis_sdsgrowzero(s_sis_sds s, size_t len);
s_sis_sds sis_sdscatlen(s_sis_sds s, const void *t, size_t len);
s_sis_sds sis_sdscat(s_sis_sds s, const char *t);
s_sis_sds sis_sdscatsds(s_sis_sds s, const s_sis_sds t);
s_sis_sds sis_sdscpylen(s_sis_sds s, const char *t, size_t len);
s_sis_sds sis_sdscpy(s_sis_sds s, const char *t);

s_sis_sds sis_sdscatvprintf(s_sis_sds s, const char *fmt, va_list ap);
#ifdef __GNUC__
s_sis_sds sis_sdscatprintf(s_sis_sds s, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
#else
s_sis_sds sis_sdscatprintf(s_sis_sds s, const char *fmt, ...);
#endif

s_sis_sds sis_sdscatfmt(s_sis_sds s, char const *fmt, ...);
s_sis_sds sis_sdstrim(s_sis_sds s, const char *cset);
void sis_sdsrange(s_sis_sds s, ssize_t start, ssize_t end);
void sis_sdsupdatelen(s_sis_sds s);
void sis_sdsclear(s_sis_sds s);
int sis_sdscmp(const s_sis_sds s1, const s_sis_sds s2);
#ifndef WARN_NOUSE_REALLOC
s_sis_sds *sis_sdssplitlen(const char *s, ssize_t len, const char *sep, int seplen, int *count);
void sis_sdsfreesplitres(s_sis_sds *tokens, int count);
s_sis_sds *sis_sdssplitargs(const char *line, int *argc);
#endif
void sis_sdstolower(s_sis_sds s);
void sis_sdstoupper(s_sis_sds s);
s_sis_sds sis_sdsnewlong(long long value);
s_sis_sds sis_sdscatrepr(s_sis_sds s, const char *p, size_t len);
s_sis_sds sis_sdsmapchars(s_sis_sds s, const char *from, const char *to, size_t setlen);
s_sis_sds sis_sdsjoin(char **argv, int argc, char *sep);
s_sis_sds sis_sdsjoinsds(s_sis_sds *argv, int argc, const char *sep, size_t seplen);

/* Low level functions exposed to the user API */
s_sis_sds sis_sds_addlen(s_sis_sds s, size_t addlen);
void sis_sds_incrlen(s_sis_sds s, ssize_t incr);
s_sis_sds sis_sds_remove_freespace(s_sis_sds s);
size_t sis_sds_allocsize(s_sis_sds s);
void *sis_sds_allocptr(s_sis_sds s);

// 初始化信息
s_sis_sds_save *sis_sds_save_create(const char *cv_, const char *dv_);

void sis_sds_save_destroy(s_sis_sds_save *sdss_);
// 设置从其他地方转过来的信息
void sis_sds_save_set(s_sis_sds_save *, const char *fv_);
// 得到最可能的需要信息
s_sis_sds sis_sds_save_get(s_sis_sds_save *);

#ifdef __cplusplus
}
#endif


#endif
