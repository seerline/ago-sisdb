/* thank antirez redis of author */

#include "sis_sds.h"

static inline int sdsHdrSize(char type)
{
    switch (type & SDS_TYPE_MASK)
    {
    case SDS_TYPE_5:
        return sizeof(struct sdshdr5);
    case SDS_TYPE_8:
        return sizeof(struct sdshdr8);
    case SDS_TYPE_16:
        return sizeof(struct sdshdr16);
    case SDS_TYPE_32:
        return sizeof(struct sdshdr32);
    case SDS_TYPE_64:
        return sizeof(struct sdshdr64);
    }
    return 0;
}

static inline char sdsReqType(size_t string_size)
{
    if (string_size < 1 << 5)
    {
        return SDS_TYPE_5;
    }
    if (string_size < 1 << 8)
    {
        return SDS_TYPE_8;
    }
    if (string_size < 1 << 16)
    {
        return SDS_TYPE_16;
    }
#if (LONG_MAX == LLONG_MAX)
    if (string_size < 1ll << 32)
    {
        return SDS_TYPE_32;
    }
#endif
    return SDS_TYPE_64;
}

/* Create a new s_sis_sds string with the content specified by the 'init' pointer
 * and 'initlen'.
 * If NULL is used for 'init' the string is initialized with zero bytes.
 * If SDS_NOINIT is used, the buffer is left uninitialized;
 *
 * The string is always null-termined (all the s_sis_sds strings are, always) so
 * even if you create an s_sis_sds string with:
 *
 * mystring = sis_sdsnewlen("abc",3);
 *
 * You can print the string with printf() as there is an implicit \0 at the
 * end of the string. However the string is binary safe and can contain
 * \0 characters in the middle, as the length is stored in the s_sis_sds header. */
s_sis_sds sis_sdsnewlen(const void *init, size_t initlen)
{
    void *sh;
    s_sis_sds s;
    char type = sdsReqType(initlen);
    /* Empty strings are usually created in order to append. Use type 8
     * since type 5 is not good at this. */
    if (type == SDS_TYPE_5 && initlen == 0)
    {
        type = SDS_TYPE_8;
    }
    int hdrlen = sdsHdrSize(type);
    unsigned char *fp; /* flags pointer. */

    sh = sis_malloc(hdrlen + initlen + 1);
    if (!init)
    {
        memset(sh, 0, hdrlen + initlen + 1);
    }
    if (sh == NULL)
    {
        return NULL;
    }
    s = (char *)sh + hdrlen;
    fp = ((unsigned char *)s) - 1;
    switch (type)
    {
    case SDS_TYPE_5:
    {
        *fp = type | (initlen << SDS_TYPE_BITS);
        break;
    }
    case SDS_TYPE_8:
    {
        SDS_HDR_VAR(8, s);
        sh->len = initlen;
        sh->alloc = initlen;
        *fp = type;
        break;
    }
    case SDS_TYPE_16:
    {
        SDS_HDR_VAR(16, s);
        sh->len = initlen;
        sh->alloc = initlen;
        *fp = type;
        break;
    }
    case SDS_TYPE_32:
    {
        SDS_HDR_VAR(32, s);
        sh->len = initlen;
        sh->alloc = initlen;
        *fp = type;
        break;
    }
    case SDS_TYPE_64:
    {
        SDS_HDR_VAR(64, s);
        sh->len = initlen;
        sh->alloc = initlen;
        *fp = type;
        break;
    }
    }
    if (initlen && init)
    {
        memcpy(s, init, initlen);
    }
    s[initlen] = '\0';
    return s;
}

/* Create an empty (zero length) s_sis_sds string. Even in this case the string
 * always has an implicit null term. */
s_sis_sds sis_sdsempty(void)
{
    return sis_sdsnewlen("", 0);
}

/* Create a new s_sis_sds string starting from a null terminated C string. */
s_sis_sds sis_sdsnew(const char *init)
{
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return sis_sdsnewlen(init, initlen);
}

/* Duplicate an s_sis_sds string. */
s_sis_sds sis_sdsdup(const s_sis_sds s)
{
    return sis_sdsnewlen(s, sis_sdslen(s));
}

void *sis_sdspos(void *s_)
{
    s_sis_sds s = (s_sis_sds)s_;
    return (char *)s - sdsHdrSize(s[-1]);
}

/* Free an s_sis_sds string. No operation is performed if 's' is NULL. */
void sis_sdsfree(s_sis_sds s)
{
    if (s == NULL)
    {
        return;
    }
    sis_free((char *)s - sdsHdrSize(s[-1]));
}

/* Set the s_sis_sds string length to the length as obtained with strlen(), so
 * considering as content only up to the first null term character.
 *
 * This function is useful when the s_sis_sds string is hacked manually in some
 * way, like in the following example:
 *
 * s = sis_sdsnew("foobar");
 * s[2] = '\0';
 * sis_sdsupdatelen(s);
 * printf("%d\n", sis_sdslen(s));
 *
 * The output will be "2", but if we comment out the call to sis_sdsupdatelen()
 * the output will be "6" as the string was modified but the logical length
 * remains 6 bytes. */
void sis_sdsupdatelen(s_sis_sds s)
{
    size_t reallen = strlen(s);
    sis_sdssetlen(s, reallen);
}

/* Modify an s_sis_sds string in-place to make it empty (zero length).
 * However all the existing buffer is not discarded but set as free space
 * so that next append operations will not require allocations up to the
 * number of bytes previously available. */
void sis_sdsclear(s_sis_sds s)
{
    sis_sdssetlen(s, 0);
    s[0] = '\0';
}

/* Enlarge the free space at the end of the s_sis_sds string so that the caller
 * is sure that after calling this function can overwrite up to addlen
 * bytes after the end of the string, plus one more byte for nul term.
 *
 * Note: this does not change the *length* of the s_sis_sds string as returned
 * by sis_sdslen(), but only the free buffer space we have. */
s_sis_sds sis_sds_addlen(s_sis_sds s, size_t addlen)
{
    void *sh, *newsh;
    size_t avail = sis_sdsavail(s);
    size_t len, newlen;
    char type, oldtype = s[-1] & SDS_TYPE_MASK;
    int hdrlen;

    /* Return ASAP if there is enough space left. */
    if (avail >= addlen)
    {
        return s;
    }

    len = sis_sdslen(s);
    sh = (char *)s - sdsHdrSize(oldtype);
    newlen = (len + addlen);
    if (newlen < SDS_MAX_PREALLOC)
    {
        newlen *= 2;
    }
    else
    {
        newlen += SDS_MAX_PREALLOC;
    }

    type = sdsReqType(newlen);

    /* Don't use type 5: the user is appending to the string and type 5 is
     * not able to remember empty space, so sis_sds_addlen() must be called
     * at every appending operation. */
    if (type == SDS_TYPE_5)
    {
        type = SDS_TYPE_8;
    }

    hdrlen = sdsHdrSize(type);
#ifndef WARN_NOUSE_REALLOC
    if (oldtype == type)
    {
        newsh = sis_realloc(sh, hdrlen + newlen + 1);
        if (newsh == NULL)
        {
            return NULL;
        }
        s = (char *)newsh + hdrlen;
    }
    else
#endif
    {
        /* Since the header size changes, need to move the string forward,
         * and can't use realloc */
        newsh = sis_malloc(hdrlen + newlen + 1);
        if (newsh == NULL)
        {
            return NULL;
        }
        memcpy((char *)newsh + hdrlen, s, len + 1);
        sis_free(sh);
        s = (char *)newsh + hdrlen;
        s[-1] = type;
        sis_sdssetlen(s, len);
    }
    sis_sdssetalloc(s, newlen);
    return s;
}

/* Reallocate the s_sis_sds string so that it has no free space at the end. The
 * contained string remains not altered, but next concatenation operations
 * will require a reallocation.
 *
 * After the call, the passed s_sis_sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
s_sis_sds sis_sds_remove_freespace(s_sis_sds s)
{
    void *sh, *newsh;
    char type, oldtype = s[-1] & SDS_TYPE_MASK;
    int hdrlen, oldhdrlen = sdsHdrSize(oldtype);
    size_t len = sis_sdslen(s);
    sh = (char *)s - oldhdrlen;

    /* Check what would be the minimum SDS header that is just good enough to
     * fit this string. */
    type = sdsReqType(len);
    hdrlen = sdsHdrSize(type);

    /* If the type is the same, or at least a large enough type is still
     * required, we just realloc(), letting the allocator to do the copy
     * only if really needed. Otherwise if the change is huge, we manually
     * reallocate the string to use the different header type. */
#ifndef WARN_NOUSE_REALLOC
    if (oldtype == type || type > SDS_TYPE_8)
    {
        newsh = sis_realloc(sh, oldhdrlen + len + 1);
        if (newsh == NULL)
        {
            return NULL;
        }
        s = (char *)newsh + oldhdrlen;
    }
    else
#endif
    {
        newsh = sis_malloc(hdrlen + len + 1);
        if (newsh == NULL)
        {
            return NULL;
        }
        memcpy((char *)newsh + hdrlen, s, len + 1);
        sis_free(sh);
        s = (char *)newsh + hdrlen;
        s[-1] = type;
        sis_sdssetlen(s, len);
    }
    sis_sdssetalloc(s, len);
    return s;
}

/* Return the total size of the allocation of the specifed s_sis_sds string,
 * including:
 * 1) The s_sis_sds header before the pointer.
 * 2) The string.
 * 3) The free buffer at the end if any.
 * 4) The implicit null term.
 */
size_t sis_sds_allocsize(s_sis_sds s)
{
    size_t alloc = sis_sdsalloc(s);
    return sdsHdrSize(s[-1]) + alloc + 1;
}

/* Return the pointer of the actual SDS allocation (normally SDS strings
 * are referenced by the start of the string buffer). */
void *sis_sds_allocptr(s_sis_sds s)
{
    return (void *)(s - sdsHdrSize(s[-1]));
}

/* Increment the s_sis_sds length and decrements the left free space at the
 * end of the string according to 'incr'. Also set the null term
 * in the new end of the string.
 *
 * This function is used in order to fix the string length after the
 * user calls sis_sds_addlen(), writes something after the end of
 * the current string, and finally needs to set the new length.
 *
 * Note: it is possible to use a negative increment in order to
 * right-trim the string.
 *
 * Usage example:
 *
 * Using sis_sds_incrlen() and sis_sds_addlen() it is possible to mount the
 * following schema, to cat bytes coming from the kernel to the end of an
 * s_sis_sds string without copying into an intermediate buffer:
 *
 * oldlen = sis_sdslen(s);
 * s = sis_sds_addlen(s, BUFFER_SIZE);
 * nread = read(fd, s+oldlen, BUFFER_SIZE);
 * ... check for nread <= 0 and handle it ...
 * sis_sds_incrlen(s, nread);
 */
void sis_sds_incrlen(s_sis_sds s, ssize_t incr)
{
    unsigned char flags = s[-1];
    size_t len;
    switch (flags & SDS_TYPE_MASK)
    {
    case SDS_TYPE_5:
    {
        unsigned char *fp = ((unsigned char *)s) - 1;
        unsigned char oldlen = SDS_TYPE_5_LEN(flags);
        assert((incr > 0 && oldlen + incr < 32) || (incr < 0 && oldlen >= (unsigned int)(-incr)));
        *fp = SDS_TYPE_5 | ((oldlen + incr) << SDS_TYPE_BITS);
        len = oldlen + incr;
        break;
    }
    case SDS_TYPE_8:
    {
        SDS_HDR_VAR(8, s);
        assert((incr >= 0 && sh->alloc - sh->len >= incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
        len = (sh->len += incr);
        break;
    }
    case SDS_TYPE_16:
    {
        SDS_HDR_VAR(16, s);
        assert((incr >= 0 && sh->alloc - sh->len >= incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
        len = (sh->len += incr);
        break;
    }
    case SDS_TYPE_32:
    {
        SDS_HDR_VAR(32, s);
        assert((incr >= 0 && sh->alloc - sh->len >= (unsigned int)incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
        len = (sh->len += incr);
        break;
    }
    case SDS_TYPE_64:
    {
        SDS_HDR_VAR(64, s);
        assert((incr >= 0 && sh->alloc - sh->len >= (uint64_t)incr) || (incr < 0 && sh->len >= (uint64_t)(-incr)));
        len = (sh->len += incr);
        break;
    }
    default:
        len = 0; /* Just to avoid compilation warnings. */
    }
    s[len] = '\0';
}

/* Grow the s_sis_sds to have the specified length. Bytes that were not part of
 * the original length of the s_sis_sds will be set to zero.
 *
 * if the specified length is smaller than the current length, no operation
 * is performed. */
s_sis_sds sis_sdsgrowzero(s_sis_sds s, size_t len)
{
    size_t curlen = sis_sdslen(s);

    if (len <= curlen)
    {
        return s;
    }
    s = sis_sds_addlen(s, len - curlen);
    if (s == NULL)
    {
        return NULL;
    }

    /* Make sure added region doesn't contain garbage */
    memset(s + curlen, 0, (len - curlen + 1)); /* also set trailing \0 byte */
    sis_sdssetlen(s, len);
    return s;
}

/* Append the specified binary-safe string pointed by 't' of 'len' bytes to the
 * end of the specified s_sis_sds string 's'.
 *
 * After the call, the passed s_sis_sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
s_sis_sds sis_sdscatlen(s_sis_sds s, const void *t, size_t len)
{
    size_t curlen = sis_sdslen(s);

    s = sis_sds_addlen(s, len);
    if (s == NULL)
    {
        return NULL;
    }
    memcpy(s + curlen, t, len);
    sis_sdssetlen(s, curlen + len);
    s[curlen + len] = '\0';
    return s;
}

/* Append the specified null termianted C string to the s_sis_sds string 's'.
 *
 * After the call, the passed s_sis_sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
s_sis_sds sis_sdscat(s_sis_sds s, const char *t)
{
    return sis_sdscatlen(s, t, strlen(t));
}

/* Append the specified s_sis_sds 't' to the existing s_sis_sds 's'.
 *
 * After the call, the modified s_sis_sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
s_sis_sds sis_sdscatsds(s_sis_sds s, const s_sis_sds t)
{
    return sis_sdscatlen(s, t, sis_sdslen(t));
}

/* Destructively modify the s_sis_sds string 's' to hold the specified binary
 * safe string pointed by 't' of length 'len' bytes. */
s_sis_sds sis_sdscpylen(s_sis_sds s, const char *t, size_t len)
{
    if (sis_sdsalloc(s) < len)
    {
        s = sis_sds_addlen(s, len - sis_sdslen(s));
        if (s == NULL)
        {
            return NULL;
        }
    }
    memcpy(s, t, len);
    s[len] = '\0';
    sis_sdssetlen(s, len);
    return s;
}

/* Like sis_sdscpylen() but 't' must be a null-termined string so that the length
 * of the string is obtained with strlen(). */
s_sis_sds sis_sdscpy(s_sis_sds s, const char *t)
{
    if (!s)
    {
        s = sis_sdsempty();
    }
    return sis_sdscpylen(s, t, strlen(t));
}

/* Helper for sdscatlonglong() doing the actual number -> string
 * conversion. 's' must point to a string with room for at least
 * SDS_LLSTR_SIZE bytes.
 *
 * The function returns the length of the null-terminated string
 * representation stored at 's'. */
#define SDS_LLSTR_SIZE 21
int sis_sdsll2str(char *s, long long value)
{
    char *p, aux;
    unsigned long long v;
    size_t l;

    /* Generate the string representation, this method produces
     * an reversed string. */
    v = (value < 0) ? -value : value;
    p = s;
    do
    {
        *p++ = '0' + (v % 10);
        v /= 10;
    } while (v);
    if (value < 0)
    {    *p++ = '-';}

    /* Compute length and add null term. */
    l = p - s;
    *p = '\0';

    /* Reverse the string. */
    p--;
    while (s < p)
    {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}

/* Identical sis_sdsll2str(), but for unsigned long long type. */
int sis_sdsull2str(char *s, unsigned long long v)
{
    char *p, aux;
    size_t l;

    /* Generate the string representation, this method produces
     * an reversed string. */
    p = s;
    do
    {
        *p++ = '0' + (v % 10);
        v /= 10;
    } while (v);

    /* Compute length and add null term. */
    l = p - s;
    *p = '\0';

    /* Reverse the string. */
    p--;
    while (s < p)
    {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}

/* Create an s_sis_sds string from a long long value. It is much faster than:
 *
 * sis_sdscatprintf(sis_sdsempty(),"%lld\n", value);
 */
s_sis_sds sis_sdsnewlong(long long value)
{
    char buf[SDS_LLSTR_SIZE];
    int len = sis_sdsll2str(buf, value);

    return sis_sdsnewlen(buf, len);
}

/* Like sis_sdscatprintf() but gets va_list instead of being variadic. */
s_sis_sds sis_sdscatvprintf(s_sis_sds s, const char *fmt, va_list ap)
{
    va_list cpy;
    char staticbuf[1024], *buf = staticbuf, *t;
    size_t buflen = strlen(fmt) * 2;

    /* We try to start using a static buffer for speed.
     * If not possible we revert to heap allocation. */
    if (buflen > sizeof(staticbuf))
    {
        buf = sis_malloc(buflen);
        if (buf == NULL)
        {    return NULL;}
    }
    else
    {
        buflen = sizeof(staticbuf);
    }

    /* Try with buffers two times bigger every time we fail to
     * fit the string in the current buffer size. */
    while (1)
    {
        buf[buflen - 2] = '\0';
        va_copy(cpy, ap);
        vsnprintf(buf, buflen, fmt, cpy);
        va_end(cpy);
        if (buf[buflen - 2] != '\0')
        {
            if (buf != staticbuf)
            {    sis_free(buf);}
            buflen *= 2;
            buf = sis_malloc(buflen);
            if (buf == NULL)
            {    return NULL;}
            continue;
        }
        break;
    }

    /* Finally concat the obtained string to the SDS string and return it. */
    t = sis_sdscat(s, buf);
    if (buf != staticbuf)
    {    sis_free(buf);}
    return t;
}

/* Append to the s_sis_sds string 's' a string obtained using printf-alike format
 * specifier.
 *
 * After the call, the modified s_sis_sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = sis_sdsnew("Sum is: ");
 * s = sis_sdscatprintf(s,"%d+%d = %d",a,b,a+b).
 *
 * Often you need to create a string from scratch with the printf-alike
 * format. When this is the need, just use sis_sdsempty() as the target string:
 *
 * s = sis_sdscatprintf(sis_sdsempty(), "... your format ...", args);
 */
s_sis_sds sis_sdscatprintf(s_sis_sds s, const char *fmt, ...)
{
    va_list ap;
    char *t;
    va_start(ap, fmt);
    t = sis_sdscatvprintf(s, fmt, ap);
    va_end(ap);
    return t;
}

/* This function is similar to sis_sdscatprintf, but much faster as it does
 * not rely on sprintf() family functions implemented by the libc that
 * are often very slow. Moreover directly handling the s_sis_sds string as
 * new data is concatenated provides a performance improvement.
 *
 * However this function only handles an incompatible subset of printf-alike
 * format specifiers:
 *
 * %s - C String
 * %S - SDS string
 * %i - signed int
 * %I - 64 bit signed integer (long long, int64_t)
 * %u - unsigned int
 * %U - 64 bit unsigned integer (unsigned long long, uint64_t)
 * %% - Verbatim "%" character.
 */
s_sis_sds sis_sdscatfmt(s_sis_sds s, char const *fmt, ...)
{
    size_t initlen = sis_sdslen(s);
    const char *f = fmt;
    long i;
    va_list ap;

    va_start(ap, fmt);
    f = fmt;     /* Next format specifier byte to process. */
    i = initlen; /* Position of the next byte to write to dest str. */
    while (*f)
    {
        char next, *str;
        size_t l;
        long long num;
        unsigned long long unum;

        /* Make sure there is always space for at least 1 char. */
        if (sis_sdsavail(s) == 0)
        {
            s = sis_sds_addlen(s, 1);
        }

        switch (*f)
        {
        case '%':
            next = *(f + 1);
            f++;
            switch (next)
            {
            case 's':
            case 'S':
                str = va_arg(ap, char *);
                l = (next == 's') ? strlen(str) : sis_sdslen(str);
                if (sis_sdsavail(s) < l)
                {
                    s = sis_sds_addlen(s, l);
                }
                memcpy(s + i, str, l);
                sis_sdsinclen(s, l);
                i += l;
                break;
            case 'i':
            case 'I':
                if (next == 'i')
                {    num = va_arg(ap, int);}
                else
                {    num = va_arg(ap, long long);}
                {
                    char buf[SDS_LLSTR_SIZE];
                    l = sis_sdsll2str(buf, num);
                    if (sis_sdsavail(s) < l)
                    {
                        s = sis_sds_addlen(s, l);
                    }
                    memcpy(s + i, buf, l);
                    sis_sdsinclen(s, l);
                    i += l;
                }
                break;
            case 'u':
            case 'U':
                if (next == 'u')
                {    unum = va_arg(ap, unsigned int);}
                else
                {    unum = va_arg(ap, unsigned long long);}
                {
                    char buf[SDS_LLSTR_SIZE];
                    l = sis_sdsull2str(buf, unum);
                    if (sis_sdsavail(s) < l)
                    {
                        s = sis_sds_addlen(s, l);
                    }
                    memcpy(s + i, buf, l);
                    sis_sdsinclen(s, l);
                    i += l;
                }
                break;
            default: /* Handle %% and generally %<unknown>. */
                s[i++] = next;
                sis_sdsinclen(s, 1);
                break;
            }
            break;
        default:
            s[i++] = *f;
            sis_sdsinclen(s, 1);
            break;
        }
        f++;
    }
    va_end(ap);

    /* Add null-term */
    s[i] = '\0';
    return s;
}

/* Remove the part of the string from left and from right composed just of
 * contiguous characters found in 'cset', that is a null terminted C string.
 *
 * After the call, the modified s_sis_sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = sis_sdsnew("AA...AA.a.aa.aHelloWorld     :::");
 * s = sis_sdstrim(s,"Aa. :");
 * printf("%s\n", s);
 *
 * Output will be just "Hello World".
 */
s_sis_sds sis_sdstrim(s_sis_sds s, const char *cset)
{
    char *start, *end, *sp, *ep;
    size_t len;

    sp = start = s;
    ep = end = s + sis_sdslen(s) - 1;
    while (sp <= end && strchr(cset, *sp))
    {    sp++;}
    while (ep > sp && strchr(cset, *ep))
    {    ep--;}
    len = (sp > ep) ? 0 : ((ep - sp) + 1);
    if (s != sp)
    {    memmove(s, sp, len);}
    s[len] = '\0';
    sis_sdssetlen(s, len);
    return s;
}

/* Turn the string into a smaller (or equal) string containing only the
 * substring specified by the 'start' and 'end' indexes.
 *
 * start and end can be negative, where -1 means the last character of the
 * string, -2 the penultimate character, and so forth.
 *
 * The interval is inclusive, so the start and end characters will be part
 * of the resulting string.
 *
 * The string is modified in-place.
 *
 * Example:
 *
 * s = sis_sdsnew("Hello World");
 * sis_sdsrange(s,1,-1); => "ello World"
 */
void sis_sdsrange(s_sis_sds s, ssize_t start, ssize_t end)
{
    size_t newlen, len = sis_sdslen(s);

    if (len == 0)
    {    return;}
    if (start < 0)
    {
        start = len + start;
        if (start < 0)
        {    start = 0;}
    }
    if (end < 0)
    {
        end = len + end;
        if (end < 0)
        {    end = 0;}
    }
    newlen = (start > end) ? 0 : (end - start) + 1;
    if (newlen != 0)
    {
        if (start >= (ssize_t)len)
        {
            newlen = 0;
        }
        else if (end >= (ssize_t)len)
        {
            end = len - 1;
            newlen = (start > end) ? 0 : (end - start) + 1;
        }
    }
    else
    {
        start = 0;
    }
    if (start && newlen)
    {    memmove(s, s + start, newlen);}
    s[newlen] = 0;
    sis_sdssetlen(s, newlen);
}

/* Apply tolower() to every character of the s_sis_sds string 's'. */
void sis_sdstolower(s_sis_sds s)
{
    size_t len = sis_sdslen(s), j;

    for (j = 0; j < len; j++)
    {    s[j] = tolower(s[j]);}
}

/* Apply toupper() to every character of the s_sis_sds string 's'. */
void sis_sdstoupper(s_sis_sds s)
{
    size_t len = sis_sdslen(s), j;

    for (j = 0; j < len; j++)
    {    s[j] = toupper(s[j]);}
}

/* Compare two s_sis_sds strings s1 and s2 with memcmp().
 *
 * Return value:
 *
 *     positive if s1 > s2.
 *     negative if s1 < s2.
 *     0 if s1 and s2 are exactly the same binary string.
 *
 * If two strings share exactly the same prefix, but one of the two has
 * additional characters, the longer string is considered to be greater than
 * the smaller one. */
int sis_sdscmp(const s_sis_sds s1, const s_sis_sds s2)
{
    size_t l1, l2, minlen;
    int cmp;

    l1 = sis_sdslen(s1);
    l2 = sis_sdslen(s2);
    minlen = (l1 < l2) ? l1 : l2;
    cmp = memcmp(s1, s2, minlen);
    if (cmp == 0)
    {    return l1 > l2 ? 1 : (l1 < l2 ? -1 : 0);}
    return cmp;
}

/* Split 's' with separator in 'sep'. An array
 * of s_sis_sds strings is returned. *count will be set
 * by reference to the number of tokens returned.
 *
 * On out of memory, zero length string, zero length
 * separator, NULL is returned.
 *
 * Note that 'sep' is able to split a string using
 * a multi-character separator. For example
 * sdssplit("foo_-_bar","_-_"); will return two
 * elements "foo" and "bar".
 *
 * This version of the function is binary-safe but
 * requires length arguments. sdssplit() is just the
 * same function but for zero-terminated strings.
 */
#ifndef WARN_NOUSE_REALLOC
s_sis_sds *sis_sdssplitlen(const char *s, ssize_t len, const char *sep, int seplen, int *count)
{
    int elements = 0, slots = 5;
    long start = 0, j;
    s_sis_sds *tokens;

    if (seplen < 1 || len < 0)
    {    return NULL;}

    tokens = sis_malloc(sizeof(s_sis_sds) * slots);
    if (tokens == NULL)
    {    return NULL;}

    if (len == 0)
    {
        *count = 0;
        return tokens;
    }
    for (j = 0; j < (len - (seplen - 1)); j++)
    {
        /* make sure there is room for the next element and the final one */
        if (slots < elements + 2)
        {
            s_sis_sds *newtokens;

            slots *= 2;
            newtokens = sis_realloc(tokens, sizeof(s_sis_sds) * slots);
            if (newtokens == NULL)
            {    goto cleanup;}
            tokens = newtokens;
        }
        /* search the separator */
        if ((seplen == 1 && *(s + j) == sep[0]) || (memcmp(s + j, sep, seplen) == 0))
        {
            tokens[elements] = sis_sdsnewlen(s + start, j - start);
            if (tokens[elements] == NULL)
            {    goto cleanup;}
            elements++;
            start = j + seplen;
            j = j + seplen - 1; /* skip the separator */
        }
    }
    /* Add the final element. We are sure there is room in the tokens array. */
    tokens[elements] = sis_sdsnewlen(s + start, len - start);
    if (tokens[elements] == NULL)
    {    goto cleanup;}
    elements++;
    *count = elements;
    return tokens;

cleanup:
{
    int i;
    for (i = 0; i < elements; i++)
    {    sis_sdsfree(tokens[i]);}
    sis_free(tokens);
    *count = 0;
    return NULL;
}
}

/* Free the result returned by sis_sdssplitlen(), or do nothing if 'tokens' is NULL. */
void sis_sdsfreesplitres(s_sis_sds *tokens, int count)
{
    if (!tokens)
    {    return;}
    while (count--)
    {    sis_sdsfree(tokens[count]);}
    sis_free(tokens);
}
#endif
/* Append to the s_sis_sds string "s" an escaped string representation where
 * all the non-printable characters (tested with isprint()) are turned into
 * escapes in the form "\n\r\a...." or "\x<hex-number>".
 *
 * After the call, the modified s_sis_sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
s_sis_sds sis_sdscatrepr(s_sis_sds s, const char *p, size_t len)
{
    s = sis_sdscatlen(s, "\"", 1);
    while (len--)
    {
        switch (*p)
        {
        case '\\':
        case '"':
            s = sis_sdscatprintf(s, "\\%c", *p);
            break;
        case '\n':
            s = sis_sdscatlen(s, "\\n", 2);
            break;
        case '\r':
            s = sis_sdscatlen(s, "\\r", 2);
            break;
        case '\t':
            s = sis_sdscatlen(s, "\\t", 2);
            break;
        case '\a':
            s = sis_sdscatlen(s, "\\a", 2);
            break;
        case '\b':
            s = sis_sdscatlen(s, "\\b", 2);
            break;
        default:
            if (isprint(*p))
            {    s = sis_sdscatprintf(s, "%c", *p);}
            else
            {    s = sis_sdscatprintf(s, "\\x%02x", (unsigned char)*p);}
            break;
        }
        p++;
    }
    return sis_sdscatlen(s, "\"", 1);
}

/* Helper function for sis_sdssplitargs() that returns non zero if 'c'
 * is a valid hex digit. */
static int is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

/* Helper function for sis_sdssplitargs() that converts a hex digit into an
 * integer from 0 to 15 */
static int hex_digit_to_int(char c)
{
    switch (c)
    {
    case '0':
        return 0;
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    case '4':
        return 4;
    case '5':
        return 5;
    case '6':
        return 6;
    case '7':
        return 7;
    case '8':
        return 8;
    case '9':
        return 9;
    case 'a':
    case 'A':
        return 10;
    case 'b':
    case 'B':
        return 11;
    case 'c':
    case 'C':
        return 12;
    case 'd':
    case 'D':
        return 13;
    case 'e':
    case 'E':
        return 14;
    case 'f':
    case 'F':
        return 15;
    default:
        return 0;
    }
}

/* Split a line into arguments, where every argument can be in the
 * following programming-language REPL-alike form:
 *
 * foo bar "newline are supported\n" and "\xff\x00otherstuff"
 *
 * The number of arguments is stored into *argc, and an array
 * of s_sis_sds is returned.
 *
 * The caller should free the resulting array of s_sis_sds strings with
 * sis_sdsfreesplitres().
 *
 * Note that sis_sdscatrepr() is able to convert back a string into
 * a quoted string in the same format sis_sdssplitargs() is able to parse.
 *
 * The function returns the allocated tokens on success, even when the
 * input string is empty, or NULL if the input contains unbalanced
 * quotes or closed quotes followed by non space characters
 * as in: "foo"bar or "foo'
 */
#ifndef WARN_NOUSE_REALLOC
s_sis_sds *sis_sdssplitargs(const char *line, int *argc)
{
    const char *p = line;
    char *current = NULL;
    char **vector = NULL;

    *argc = 0;
    while (1)
    {
        /* skip blanks */
        while (*p && isspace(*p))
        {    p++;}
        if (*p)
        {
            /* get a token */
            int inq = 0;  /* set to 1 if we are in "quotes" */
            int insq = 0; /* set to 1 if we are in 'single quotes' */
            int done = 0;

            if (current == NULL)
            {    current = sis_sdsempty();}
            while (!done)
            {
                if (inq)
                {
                    if (*p == '\\' && *(p + 1) == 'x' &&
                        is_hex_digit(*(p + 2)) &&
                        is_hex_digit(*(p + 3)))
                    {
                        unsigned char byte;

                        byte = (hex_digit_to_int(*(p + 2)) * 16) +
                               hex_digit_to_int(*(p + 3));
                        current = sis_sdscatlen(current, (char *)&byte, 1);
                        p += 3;
                    }
                    else if (*p == '\\' && *(p + 1))
                    {
                        char c;

                        p++;
                        switch (*p)
                        {
                        case 'n':
                            c = '\n';
                            break;
                        case 'r':
                            c = '\r';
                            break;
                        case 't':
                            c = '\t';
                            break;
                        case 'b':
                            c = '\b';
                            break;
                        case 'a':
                            c = '\a';
                            break;
                        default:
                            c = *p;
                            break;
                        }
                        current = sis_sdscatlen(current, &c, 1);
                    }
                    else if (*p == '"')
                    {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (*(p + 1) && !isspace(*(p + 1)))
                        {    goto err;}
                        done = 1;
                    }
                    else if (!*p)
                    {
                        /* unterminated quotes */
                        goto err;
                    }
                    else
                    {
                        current = sis_sdscatlen(current, p, 1);
                    }
                }
                else if (insq)
                {
                    if (*p == '\\' && *(p + 1) == '\'')
                    {
                        p++;
                        current = sis_sdscatlen(current, "'", 1);
                    }
                    else if (*p == '\'')
                    {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (*(p + 1) && !isspace(*(p + 1)))
                        {    goto err;}
                        done = 1;
                    }
                    else if (!*p)
                    {
                        /* unterminated quotes */
                        goto err;
                    }
                    else
                    {
                        current = sis_sdscatlen(current, p, 1);
                    }
                }
                else
                {
                    switch (*p)
                    {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                    case '\0':
                        done = 1;
                        break;
                    case '"':
                        inq = 1;
                        break;
                    case '\'':
                        insq = 1;
                        break;
                    default:
                        current = sis_sdscatlen(current, p, 1);
                        break;
                    }
                }
                if (*p)
                {    p++;}
            }
            /* add the token to the vector */
            vector = sis_realloc(vector, ((*argc) + 1) * sizeof(char *));
            vector[*argc] = current;
            (*argc)++;
            current = NULL;
        }
        else
        {
            /* Even on empty input string return something not NULL. */
            if (vector == NULL)
            {    vector = sis_malloc(sizeof(void *));}
            return vector;
        }
    }

err:
    while ((*argc)--)
    {    sis_sdsfree(vector[*argc]);}
    sis_free(vector);
    if (current)
    {    sis_sdsfree(current);}
    *argc = 0;
    return NULL;
}
#endif
/* Modify the string substituting all the occurrences of the set of
 * characters specified in the 'from' string to the corresponding character
 * in the 'to' array.
 *
 * For instance: sis_sdsmapchars(mystring, "ho", "01", 2)
 * will have the effect of turning the string "hello" into "0ell1".
 *
 * The function returns the s_sis_sds string pointer, that is always the same
 * as the input pointer since no resize is needed. */
s_sis_sds sis_sdsmapchars(s_sis_sds s, const char *from, const char *to, size_t setlen)
{
    size_t j, i, l = sis_sdslen(s);

    for (j = 0; j < l; j++)
    {
        for (i = 0; i < setlen; i++)
        {
            if (s[j] == from[i])
            {
                s[j] = to[i];
                break;
            }
        }
    }
    return s;
}

/* Join an array of C strings using the specified separator (also a C string).
 * Returns the result as an s_sis_sds string. */
s_sis_sds sis_sdsjoin(char **argv, int argc, char *sep)
{
    s_sis_sds join = sis_sdsempty();
    int j;

    for (j = 0; j < argc; j++)
    {
        join = sis_sdscat(join, argv[j]);
        if (j != argc - 1)
        {    join = sis_sdscat(join, sep);}
    }
    return join;
}

/* Like sis_sdsjoin, but joins an array of SDS strings. */
s_sis_sds sis_sdsjoinsds(s_sis_sds *argv, int argc, const char *sep, size_t seplen)
{
    s_sis_sds join = sis_sdsempty();
    int j;

    for (j = 0; j < argc; j++)
    {
        join = sis_sdscatsds(join, argv[j]);
        if (j != argc - 1)
        {    join = sis_sdscatlen(join, sep, seplen);}
    }
    return join;
}

s_sis_sds_save *sis_sds_save_create(const char *cv_, const char *dv_)
{
    s_sis_sds_save *o = SIS_MALLOC(s_sis_sds_save, o);
    if (cv_)
    {
        o->current_v = sis_sdsnew(cv_);
    }
    if (dv_)
    {
        o->default_v = sis_sdsnew(dv_);
    }
    return o;
}
void sis_sds_save_destroy(s_sis_sds_save *sdss_)
{
    sis_sdsfree(sdss_->current_v);
    sis_sdsfree(sdss_->father_v);
    sis_sdsfree(sdss_->default_v);
    sis_free(sdss_);
}
void sis_sds_save_set(s_sis_sds_save *sdss_, const char *fv_)
{
    sis_sdsfree(sdss_->father_v);
    sdss_->father_v = NULL;
    if (fv_)
    {
        sdss_->father_v = sis_sdsnew(fv_);
    }
}
s_sis_sds sis_sds_save_get(s_sis_sds_save *sdss_)
{
    if (sdss_->current_v)
    {
        return sdss_->current_v;
    }
    if (sdss_->father_v)
    {
        return sdss_->father_v;
    }
    return sdss_->default_v;
}

#ifdef SDS_TEST_MAIN
#include <stdio.h>
#include "testhelp.h"
#include "limits.h"

#define UNUSED(x) (void)(x)
int _sds_debug(void)
{
    {
        s_sis_sds x = sis_sdsnew("foo"), y;

        test_cond("Create a string and obtain the length",
                  sis_sdslen(x) == 3 && memcmp(x, "foo\0", 4) == 0)

            sis_sdsfree(x);
        x = sis_sdsnewlen("foo", 2);
        test_cond("Create a string with specified length",
                  sis_sdslen(x) == 2 && memcmp(x, "fo\0", 3) == 0)

            x = sis_sdscat(x, "bar");
        test_cond("Strings concatenation",
                  sis_sdslen(x) == 5 && memcmp(x, "fobar\0", 6) == 0);

        x = sis_sdscpy(x, "a");
        test_cond("sis_sdscpy() against an originally longer string",
                  sis_sdslen(x) == 1 && memcmp(x, "a\0", 2) == 0)

            x = sis_sdscpy(x, "xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk");
        test_cond("sis_sdscpy() against an originally shorter string",
                  sis_sdslen(x) == 33 &&
                      memcmp(x, "xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk\0", 33) == 0)

            sis_sdsfree(x);
        x = sis_sdscatprintf(sis_sdsempty(), "%d", 123);
        test_cond("sis_sdscatprintf() seems working in the base case",
                  sis_sdslen(x) == 3 && memcmp(x, "123\0", 4) == 0)

            sis_sdsfree(x);
        x = sis_sdsnew("--");
        x = sis_sdscatfmt(x, "Hello %s World %I,%I--", "Hi!", LLONG_MIN, LLONG_MAX);
        test_cond("sis_sdscatfmt() seems working in the base case",
                  sis_sdslen(x) == 60 &&
                      memcmp(x, "--Hello Hi! World -9223372036854775808,"
                                "9223372036854775807--",
                             60) == 0)
            printf("[%s]\n", x);

        sis_sdsfree(x);
        x = sis_sdsnew("--");
        x = sis_sdscatfmt(x, "%u,%U--", UINT_MAX, ULLONG_MAX);
        test_cond("sis_sdscatfmt() seems working with unsigned numbers",
                  sis_sdslen(x) == 35 &&
                      memcmp(x, "--4294967295,18446744073709551615--", 35) == 0)

            sis_sdsfree(x);
        x = sis_sdsnew(" x ");
        sis_sdstrim(x, " x");
        test_cond("sis_sdstrim() works when all chars match",
                  sis_sdslen(x) == 0)

            sis_sdsfree(x);
        x = sis_sdsnew(" x ");
        sis_sdstrim(x, " ");
        test_cond("sis_sdstrim() works when a single char remains",
                  sis_sdslen(x) == 1 && x[0] == 'x')

            sis_sdsfree(x);
        x = sis_sdsnew("xxciaoyyy");
        sis_sdstrim(x, "xy");
        test_cond("sis_sdstrim() correctly trims characters",
                  sis_sdslen(x) == 4 && memcmp(x, "ciao\0", 5) == 0)

            y = sis_sdsdup(x);
        sis_sdsrange(y, 1, 1);
        test_cond("sis_sdsrange(...,1,1)",
                  sis_sdslen(y) == 1 && memcmp(y, "i\0", 2) == 0)

            sis_sdsfree(y);
        y = sis_sdsdup(x);
        sis_sdsrange(y, 1, -1);
        test_cond("sis_sdsrange(...,1,-1)",
                  sis_sdslen(y) == 3 && memcmp(y, "iao\0", 4) == 0)

            sis_sdsfree(y);
        y = sis_sdsdup(x);
        sis_sdsrange(y, -2, -1);
        test_cond("sis_sdsrange(...,-2,-1)",
                  sis_sdslen(y) == 2 && memcmp(y, "ao\0", 3) == 0)

            sis_sdsfree(y);
        y = sis_sdsdup(x);
        sis_sdsrange(y, 2, 1);
        test_cond("sis_sdsrange(...,2,1)",
                  sis_sdslen(y) == 0 && memcmp(y, "\0", 1) == 0)

            sis_sdsfree(y);
        y = sis_sdsdup(x);
        sis_sdsrange(y, 1, 100);
        test_cond("sis_sdsrange(...,1,100)",
                  sis_sdslen(y) == 3 && memcmp(y, "iao\0", 4) == 0)

            sis_sdsfree(y);
        y = sis_sdsdup(x);
        sis_sdsrange(y, 100, 100);
        test_cond("sis_sdsrange(...,100,100)",
                  sis_sdslen(y) == 0 && memcmp(y, "\0", 1) == 0)

            sis_sdsfree(y);
        sis_sdsfree(x);
        x = sis_sdsnew("foo");
        y = sis_sdsnew("foa");
        test_cond("sis_sdscmp(foo,foa)", sis_sdscmp(x, y) > 0)

            sis_sdsfree(y);
        sis_sdsfree(x);
        x = sis_sdsnew("bar");
        y = sis_sdsnew("bar");
        test_cond("sis_sdscmp(bar,bar)", sis_sdscmp(x, y) == 0)

            sis_sdsfree(y);
        sis_sdsfree(x);
        x = sis_sdsnew("aar");
        y = sis_sdsnew("bar");
        test_cond("sis_sdscmp(bar,bar)", sis_sdscmp(x, y) < 0)

            sis_sdsfree(y);
        sis_sdsfree(x);
        x = sis_sdsnewlen("\a\n\0foo\r", 7);
        y = sis_sdscatrepr(sis_sdsempty(), x, sis_sdslen(x));
        test_cond("sis_sdscatrepr(...data...)",
                  memcmp(y, "\"\\a\\n\\x00foo\\r\"", 15) == 0)

        {
            unsigned int oldfree;
            char *p;
            int step = 10, j, i;

            sis_sdsfree(x);
            sis_sdsfree(y);
            x = sis_sdsnew("0");
            test_cond("sis_sdsnew() free/len buffers", sis_sdslen(x) == 1 && sis_sdsavail(x) == 0);

            /* Run the test a few times in order to hit the first two
             * SDS header types. */
            for (i = 0; i < 10; i++)
            {
                int oldlen = sis_sdslen(x);
                x = sis_sds_addlen(x, step);
                int type = x[-1] & SDS_TYPE_MASK;

                test_cond("sis_sds_addlen() len", sis_sdslen(x) == oldlen);
                if (type != SDS_TYPE_5)
                {
                    test_cond("sis_sds_addlen() free", sis_sdsavail(x) >= step);
                    oldfree = sis_sdsavail(x);
                }
                p = x + oldlen;
                for (j = 0; j < step; j++)
                {
                    p[j] = 'A' + j;
                }
                sis_sds_incrlen(x, step);
            }
            test_cond("sis_sds_addlen() content",
                      memcmp("0ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJ", x, 101) == 0);
            test_cond("sis_sds_addlen() final length", sis_sdslen(x) == 101);

            sis_sdsfree(x);
        }
    }
    test_report() return 0;
}
#endif

#ifdef SDS_TEST_MAIN
int main(void)
{
    return _sds_debug();
}
#endif
