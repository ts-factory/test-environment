/** @file
 * @brief API to deal with strings
 *
 * Function to operate the strings.
 *
 *
 * Copyright (C) 2017 Test Environment authors (see file AUTHORS
 * in the root directory of the distribution).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 *
 * @author Ivan Melnikov <Ivan.Melnikov@oktetlabs.ru>
 */

#include "te_config.h"
#include "te_defs.h"
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif


/* See description in te_str.h */
char *
te_str_upper(const char *src)
{
    char *dst;
    char *res;

    res = malloc(strlen(src) + 1);
    if (res == NULL)
        return NULL;

    for (dst = res; *src != '\0'; src++, dst++)
        *dst = toupper(*src);
    *dst = '\0';

    return res;
}

/* See description in te_str.h */
char *
te_str_lower(const char *src)
{
    char *dst;
    char *res;

    res = malloc(strlen(src) + 1);
    if (res == NULL)
        return NULL;

    for (dst = res; *src != '\0'; src++, dst++)
        *dst = tolower(*src);
    *dst = '\0';

    return res;
}

/* See description in te_str.h */
char *
te_str_concat(const char *first, const char *second)
{
    char *str;
    size_t len1;
    size_t len2;

    if (first == NULL)
        first = "";
    if (second == NULL)
        second = "";

    len1 = strlen(first);
    len2 = strlen(second);

    str = malloc(len1 + len2 + 1);
    if (str == NULL)
        return NULL;

    memcpy(str, first, len1);
    memcpy(&str[len1], second, len2);
    str[len1 + len2] = '\0';

    return str;
}
