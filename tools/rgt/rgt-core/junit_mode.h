/** @file
 * @brief Test Environment: JUnit mode specific routines.
 *
 * Interface for output of control and regular messages
 * into the JUnit XML file.
 *
 * Copyright (C) 2017 Test Environment authors (see file AUTHORS in the
 * root directory of the distribution).
 *
 * Test Environment is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * Test Environment is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 *
 * @author Dmitry Izbitsky  <Dmitry.Izbitsky@oktetlabs.ru>
 */

#ifndef __TE_RGT_JUNIT_MODE_H__
#define __TE_RGT_JUNIT_MODE_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set callback pointers to refer functions implementing JUnit mode of
 * operation.
 *
 * @param ctrl_proc       Table of callbacks for processing control
 *                        log messages.
 * @param reg_proc        Callback for processing regular message.
 * @param root_proc       Callbacks for processing log start and end.
 */
extern void junit_mode_init(f_process_ctrl_log_msg
                                    ctrl_proc[CTRL_EVT_LAST][NT_LAST],
                            f_process_reg_log_msg  *reg_proc,
                            f_process_log_root root_proc[CTRL_EVT_LAST]);

#ifdef __cplusplus
}
#endif

#endif /* __TE_RGT_JUNIT_MODE_H__ */