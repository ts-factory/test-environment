/** @file
 * @brief Test Environment: splitting raw log.
 *
 * Commong functions for splitting raw log into fragments and
 * merging fragments back into raw log.
 *
 *
 * Copyright (C) 2003-2018 OKTET Labs. All rights reserved.
 *
 * 
 *
 *
 * @author Dmitry Izbitsky  <Dmitry.Izbitsky@oktetlabs.ru>
 *
 * $Id$
 */

#include <stdlib.h>
#include <stdio.h>
#include <popt.h>

#include "te_config.h"
#include "te_defs.h"
#include "logger_api.h"

/* See the description in rgt_log_bundle_common.h */
int
file2file(FILE *out_f, FILE *in_f,
          off_t out_offset,
          off_t in_offset, off_t length)
{
#define BUF_SIZE  4096
    char   buf[BUF_SIZE];
    size_t bytes_read;
    size_t bytes_written;

    if (out_offset >= 0)
        fseeko(out_f, out_offset, SEEK_SET);
    if (in_offset >= 0)
        fseeko(in_f, in_offset, SEEK_SET);

    while (length > 0)
    {
        bytes_read = (length > BUF_SIZE ? BUF_SIZE : length);
        bytes_read = fread(buf, 1, bytes_read, in_f);
        if (ferror(in_f))
        {
            fprintf(stderr, "%s\n", "fread() failed");
            exit(1);
        }

        if (bytes_read > 0)
        {
            bytes_written = fwrite(buf, 1, bytes_read, out_f);
            if (bytes_written != bytes_read)
            {
                fprintf(stderr, "fwrite() returned %ld instead of %ld\n",
                        (long int)bytes_written, (long int)bytes_read);
                exit(1);
            }
            length -= bytes_read;
        }

        if (feof(in_f))
            break;
    }

    if (length > 0)
    {
        ERROR("Failed to copy last %llu bytes to file",
              (long long unsigned int)length);
        return -1;
    }

    return 0;
#undef BUF_SIZE
}
