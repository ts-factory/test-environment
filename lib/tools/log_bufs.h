/** @file
 * @brief API to collect long log messages
 *
 * Declaration of API to collect long log messages.
 *
 *
 * Copyright (C) 2005 Test Environment authors (see file AUTHORS
 * in the root directory of the distribution).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
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
 * @author Oleg Kravtosv <Oleg.Kravtosv@oktetlabs.ru>
 *
 * $Id$
 */

#ifndef __TE_LOG_BUFS_H__
#define __TE_LOG_BUFS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

/**
 * Declaration of te_log_buf type, which is defined
 * in the implementation, so user can allocate and operate only
 * with pointer to this data structure.
 */
struct te_log_buf;
typedef struct te_log_buf te_log_buf;


/* Log buffer related functions */

/**
 * Allocates a buffer to be used for accumulating log message.
 * Mainly used in tapi_snmp itself.
 *
 * @return Pointer to the buffer.
 *
 * @note the caller does not have to check the returned
 * value against NULL, the function blocks the caller until it
 * gets available buffer.
 *
 * @note This is thread safe function
 */
extern te_log_buf *te_log_buf_alloc();

/**
 * Appends format string to the log message, the behaviour of
 * the function is the same as ordinary printf-like function.
 *
 * @param buf  Pointer to the buffer allocated with te_log_buf_alloc()
 * @param fmt  Format string followed by parameters
 *
 * @return The number of characters appended
 *
 * @note This is NOT thread safe function, so you are not allowed
 * to append the same buffer from different threads simultaneously.
 */
extern int te_log_buf_append(te_log_buf *buf, const char *fmt, ...);

/**
 * Returns current log message accumulated in the buffer.
 *
 * @param buf  Pointer to the buffer
 *
 * @return log message
 */
extern const char *te_log_buf_get(te_log_buf *buf);

/**
 * Release buffer allocated by te_log_buf_alloc()
 *
 * @param ptr  Pointer to the buffer
 *
 * @note This is thread safe function
 */
extern void te_log_buf_free(te_log_buf *buf);

/**
 * Put @a argc/@a argv arguments enclosed in double quotes and separated
 * by comma to log buffer.
 *
 * @param buf   Pointer to the buffer allocated with @b te_log_buf_alloc()
 * @param argc  Number of arguments
 * @param argv  Array with arguments
 *
 * @return @b te_log_buf_get() return value after addition
 */
extern const char *te_args2log_buf(te_log_buf *buf,
                                   int argc, const char **argv);


/** Mapping of the bit number to string */
struct te_log_buf_bit2str {
    unsigned int    bit;
    const char     *str;
};

/**
 * Append bit mask converted to string to log buffer.
 *
 * Pipe '|' is used as a separator.
 *
 * @param buf       Pointer to the buffer
 * @param bit_mask  Bit mask
 * @param map       Bit to string map terminated by the element with #NULL
 *                  string
 *
 * @return te_log_buf_get() value.
 */
extern const char *te_bit_mask2log_buf(te_log_buf *buf,
                                       unsigned long long bit_mask,
                                       const struct te_log_buf_bit2str *map);

/**
 * Put ether address to log buffer.
 *
 * @param buf   Pointer to the buffer allocated with @b te_log_buf_alloc()
 * @param argc  Pointer to the ether address
 *
 * @return @b te_log_buf_get() return value after addition
 */
extern const char *te_ether_addr2log_buf(te_log_buf *buf,
                                         const uint8_t * mac_addr);

/**
 * Put IPv4 / IPv6 address to log buffer.
 *
 * @param buf            Pointer to the buffer allocated with @b te_log_buf_alloc()
 * @param ip_addr        Pointer to the IPv4 / IPv6 address
 * @param addr_str_len   Length of the string form for IPv4 / IPv6 address
 *
 * @return @b te_log_buf_get() return value after addition
 */
extern const char *te_ip_addr2log_buf(te_log_buf *buf, const void *ip_addr,
                                      int addr_str_len);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TE_LOG_BUFS_H__ */
