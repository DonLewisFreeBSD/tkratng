/*
 * ratCompat.c --
 *
 *	This file contains compatibility functions.
 *
 * TkRat software and its included text is Copyright 1996-2004 by
 * Martin Forssén
 *
 * The full text of the legal notice is contained in the file called
 * COPYRIGHT, included with this distribution.
 */

#include "rat.h"

#ifndef HAVE_SNPRINTF

size_t
snprintf (char *buf, size_t buflen, const char *fmt, ...)
{
    va_list argList;
    int bytes_written;

    va_start(argList, fmt);
    bytes_written = vsprintf (buf, fmt, argList);
    va_end(argList);

    if (bytes_written >= buflen) {
	fprintf(stderr, "Buffer overflow in snprintf (%d > %d)\n",
		bytes_written+1, buflen);
        abort();
    }

    return bytes_written;
}

#endif /* HAVE_SNPRINTF */

#ifndef HAVE_STRLCPY

char*
strlcpy(char *dst, const char *src, size_t n)
{
    int i;

    for (i=0; src[i] && i<n-1; i++) {
	dst[i] = src[i];
    }
    dst[i] = '\0';
    return dst;
}

#endif /* HAVE_STRLCPY */

#ifndef HAVE_STRLCAT

char*
strlcat(char *dst, const char *src, size_t n)
{
    int i;
    const char *c;

    for (i=0; dst[i] && i<n-1; i++);
    for (c = src; *c && i<n-1; i++, c++) {
	dst[i] = *c;
    }
    dst[i] = '\0';
    return dst;
}

#endif /* HAVE_STRLCAT */
