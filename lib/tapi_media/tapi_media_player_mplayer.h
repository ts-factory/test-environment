/** @file
 * @brief Test API to 'mplayer' routines
 *
 * @defgroup tapi_media_player_mplayer Test API to control the mplayer media player
 * @ingroup tapi_media
 * @{
 *
 * Test API to control 'mplayer'.
 *
 * Copyright (C) 2016 Test Environment authors (see file AUTHORS
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
 * @author Ivan Melnikov <Ivan.Melnikov@oktetlabs.ru>
 *
 * $Id$
 */

#ifndef __TAPI_MEDIA_PLAYER_MPLAYER_H__
#define __TAPI_MEDIA_PLAYER_MPLAYER_H__

#include "tapi_media_player.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize media player access point hooks.
 *
 * @param player    The player access point
 */
extern void tapi_media_player_mplayer_init(tapi_media_player *player);

/**@} <!-- END tapi_media_player_mplayer --> */

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* __TAPI_MEDIA_PLAYER_MPLAYER_H__ */
