/*
 * Copyright 2011-2016 "Silver Squirrel Software Handelsbolag"
 * Copyright 2023-2024 "John Högberg"
 *
 * This file is part of tibiarc.
 *
 * tibiarc is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * tibiarc is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with tibiarc. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __TRC_COMMON_H__
#define __TRC_COMMON_H__

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <malloc.h>
#include <stdio.h>

#ifndef MIN
#    define MIN(a, b) ((a) > (b) ? (b) : (a))
#endif

#ifndef MAX
#    define MAX(a, b) ((a) < (b) ? (b) : (a))
#endif

#ifndef ABS
#    define ABS(a) ((a) > 0 ? (a) : -(a))
#endif

#define CHECK_RANGE(a, b, c) (((a) >= (b)) && ((a) <= (c)))

#define ABORT_UNLESS(x) (!(x) ? abort() : (void)0)

#ifndef NDEBUG
#    define ASSERT(x) ABORT_UNLESS(x)
#else
#    define ASSERT(x) ((void)0)
#endif

#ifdef __GNUC__
#    define TRC_UNUSED __attribute__((unused))
#else
#    define TRC_UNUSED __pragma(warning(suppress : 4505))
#endif

#ifdef __cplusplus
#    define _Static_assert static_assert
#    define _Alignof alignof
#endif

TRC_UNUSED static void *checked_allocate(size_t elements, size_t size) {
    void *result = calloc(elements, size);

    /* There's nothing sensible we can do on allocation failures except
     * crash immediately, even when just used as a library. */
    ABORT_UNLESS(result);

    return result;
}

TRC_UNUSED static void *checked_aligned_allocate(size_t alignment,
                                                 size_t size) {
    /* While aligned_alloc is provided by C11, it isn't supported on Windows
     * and has to be emulated in user space.
     *
     * While MSVC provides _aligned_malloc to help with this, MinGW doesn't
     * have anything at all, so we'll do it ourselves as this kind of
     * allocation is very rare. */
    ABORT_UNLESS((alignment & -alignment) == alignment);
    ABORT_UNLESS(size % alignment == 0);
    ABORT_UNLESS(size < (SIZE_MAX - sizeof(ptrdiff_t)));
    ABORT_UNLESS(alignment < (SIZE_MAX - size - sizeof(ptrdiff_t)));

    if (alignment <= sizeof(ptrdiff_t)) {
        return checked_allocate(1, size);
    }

#if defined(_WIN32)
    uintptr_t base, actual;

    base = (uintptr_t)checked_allocate(1, size + sizeof(ptrdiff_t) + alignment);
    ABORT_UNLESS(base);

    actual = (base + sizeof(ptrdiff_t) + (alignment - 1)) & ~(alignment - 1);
    ((ptrdiff_t *)actual)[-1] = actual - base;

    return (void *)actual;
#else
    void *result;

    result = aligned_alloc(alignment, size);
    ABORT_UNLESS(result);

    return result;
#endif
}

TRC_UNUSED static void checked_aligned_deallocate(void *ptr) {
    if (ptr) {
#if defined(_WIN32)
        ptr = (void *)(((uintptr_t)ptr) - ((ptrdiff_t *)ptr)[-1]);
#endif
        free(ptr);
    }
}

TRC_UNUSED static void checked_deallocate(void *ptr) {
    if (ptr) {
        free(ptr);
    }
}

#include "report.h"

#endif /* __TRC_COMMON_H__ */
